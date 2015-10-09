/*
 * write.c
 *
 * Support for writing WIM files; write a WIM file, overwrite a WIM file, write
 * compressed file resources, etc.
 */

/*
 * Copyright (C) 2012, 2013, 2014, 2015 Eric Biggers
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option) any
 * later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; if not, see http://www.gnu.org/licenses/.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if defined(HAVE_SYS_FILE_H) && defined(HAVE_FLOCK)
/* On BSD, this should be included before "wimlib/list.h" so that "wimlib/list.h" can
 * override the LIST_HEAD macro. */
#  include <sys/file.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "wimlib/alloca.h"
#include "wimlib/assert.h"
#include "wimlib/blob_table.h"
#include "wimlib/chunk_compressor.h"
#include "wimlib/endianness.h"
#include "wimlib/error.h"
#include "wimlib/file_io.h"
#include "wimlib/header.h"
#include "wimlib/inode.h"
#include "wimlib/integrity.h"
#include "wimlib/metadata.h"
#include "wimlib/paths.h"
#include "wimlib/progress.h"
#include "wimlib/resource.h"
#include "wimlib/solid.h"
#include "wimlib/win32.h" /* win32_rename_replacement() */
#include "wimlib/write.h"
#include "wimlib/xml.h"

/* Keep in sync with wimlib.h  */
#define WIMLIB_WRITE_MASK_PUBLIC (			  \
	WIMLIB_WRITE_FLAG_CHECK_INTEGRITY		| \
	WIMLIB_WRITE_FLAG_NO_CHECK_INTEGRITY		| \
	WIMLIB_WRITE_FLAG_PIPABLE			| \
	WIMLIB_WRITE_FLAG_NOT_PIPABLE			| \
	WIMLIB_WRITE_FLAG_RECOMPRESS			| \
	WIMLIB_WRITE_FLAG_FSYNC				| \
	WIMLIB_WRITE_FLAG_REBUILD			| \
	WIMLIB_WRITE_FLAG_SOFT_DELETE			| \
	WIMLIB_WRITE_FLAG_IGNORE_READONLY_FLAG		| \
	WIMLIB_WRITE_FLAG_SKIP_EXTERNAL_WIMS		| \
	WIMLIB_WRITE_FLAG_STREAMS_OK			| \
	WIMLIB_WRITE_FLAG_RETAIN_GUID			| \
	WIMLIB_WRITE_FLAG_SOLID				| \
	WIMLIB_WRITE_FLAG_SEND_DONE_WITH_FILE_MESSAGES	| \
	WIMLIB_WRITE_FLAG_NO_SOLID_SORT			| \
	WIMLIB_WRITE_FLAG_UNSAFE_COMPACT)

/* Internal use only */
#define WIMLIB_WRITE_FLAG_FILE_DESCRIPTOR	0x80000000
#define WIMLIB_WRITE_FLAG_APPEND		0x40000000
#define WIMLIB_WRITE_FLAG_NO_NEW_BLOBS		0x20000000

/* wimlib internal flags used when writing resources.  */
#define WRITE_RESOURCE_FLAG_RECOMPRESS		0x00000001
#define WRITE_RESOURCE_FLAG_PIPABLE		0x00000002
#define WRITE_RESOURCE_FLAG_SOLID		0x00000004
#define WRITE_RESOURCE_FLAG_SEND_DONE_WITH_FILE	0x00000008
#define WRITE_RESOURCE_FLAG_SOLID_SORT		0x00000010

static int
write_flags_to_resource_flags(int write_flags)
{
	int write_resource_flags = 0;

	if (write_flags & WIMLIB_WRITE_FLAG_RECOMPRESS)
		write_resource_flags |= WRITE_RESOURCE_FLAG_RECOMPRESS;

	if (write_flags & WIMLIB_WRITE_FLAG_PIPABLE)
		write_resource_flags |= WRITE_RESOURCE_FLAG_PIPABLE;

	if (write_flags & WIMLIB_WRITE_FLAG_SOLID)
		write_resource_flags |= WRITE_RESOURCE_FLAG_SOLID;

	if (write_flags & WIMLIB_WRITE_FLAG_SEND_DONE_WITH_FILE_MESSAGES)
		write_resource_flags |= WRITE_RESOURCE_FLAG_SEND_DONE_WITH_FILE;

	if ((write_flags & (WIMLIB_WRITE_FLAG_SOLID |
			    WIMLIB_WRITE_FLAG_NO_SOLID_SORT)) ==
	    WIMLIB_WRITE_FLAG_SOLID)
		write_resource_flags |= WRITE_RESOURCE_FLAG_SOLID_SORT;

	return write_resource_flags;
}

struct filter_context {
	int write_flags;
	WIMStruct *wim;
};

/*
 * Determine whether the specified blob should be filtered out from the write.
 *
 * Return values:
 *
 *  < 0 : The blob should be hard-filtered; that is, not included in the output
 *	  WIM file at all.
 *    0 : The blob should not be filtered out.
 *  > 0 : The blob should be soft-filtered; that is, it already exists in the
 *	  WIM file and may not need to be written again.
 */
static int
blob_filtered(const struct blob_descriptor *blob,
	      const struct filter_context *ctx)
{
	int write_flags;
	WIMStruct *wim;

	if (ctx == NULL)
		return 0;

	write_flags = ctx->write_flags;
	wim = ctx->wim;

	if (write_flags & WIMLIB_WRITE_FLAG_APPEND &&
	    blob->blob_location == BLOB_IN_WIM &&
	    blob->rdesc->wim == wim)
		return 1;

	if (write_flags & WIMLIB_WRITE_FLAG_SKIP_EXTERNAL_WIMS &&
	    blob->blob_location == BLOB_IN_WIM &&
	    blob->rdesc->wim != wim)
		return -1;

	return 0;
}

static bool
blob_hard_filtered(const struct blob_descriptor *blob,
		   struct filter_context *ctx)
{
	return blob_filtered(blob, ctx) < 0;
}

static inline bool
may_soft_filter_blobs(const struct filter_context *ctx)
{
	return ctx && (ctx->write_flags & WIMLIB_WRITE_FLAG_APPEND);
}

static inline bool
may_hard_filter_blobs(const struct filter_context *ctx)
{
	return ctx && (ctx->write_flags & WIMLIB_WRITE_FLAG_SKIP_EXTERNAL_WIMS);
}

static inline bool
may_filter_blobs(const struct filter_context *ctx)
{
	return (may_soft_filter_blobs(ctx) || may_hard_filter_blobs(ctx));
}

/* Return true if the specified blob is located in a WIM resource which can be
 * reused in the output WIM file, without being recompressed.  */
static bool
can_raw_copy(const struct blob_descriptor *blob, int write_resource_flags,
	     int out_ctype, u32 out_chunk_size)
{
	const struct wim_resource_descriptor *rdesc;

	/* Recompress everything if requested.  */
	if (write_resource_flags & WRITE_RESOURCE_FLAG_RECOMPRESS)
		return false;

	/* A blob not located in a WIM resource cannot be reused.  */
	if (blob->blob_location != BLOB_IN_WIM)
		return false;

	rdesc = blob->rdesc;

	/* In the case of an in-place compaction, always reuse resources located
	 * in the WIM being compacted.  */
	if (rdesc->wim->being_compacted)
		return true;

	/* Otherwise, only reuse compressed resources.  */
	if (out_ctype == WIMLIB_COMPRESSION_TYPE_NONE ||
	    !(rdesc->flags & (WIM_RESHDR_FLAG_COMPRESSED |
			      WIM_RESHDR_FLAG_SOLID)))
		return false;

	/* When writing a pipable WIM, we can only reuse pipable resources; and
	 * when writing a non-pipable WIM, we can only reuse non-pipable
	 * resources.  */
	if (rdesc->is_pipable !=
	    !!(write_resource_flags & WRITE_RESOURCE_FLAG_PIPABLE))
		return false;

	/* When writing a solid WIM, we can only reuse solid resources; and when
	 * writing a non-solid WIM, we can only reuse non-solid resources.  */
	if (!!(rdesc->flags & WIM_RESHDR_FLAG_SOLID) !=
	    !!(write_resource_flags & WRITE_RESOURCE_FLAG_SOLID))
		return false;

	/* Note: it is theoretically possible to copy chunks of compressed data
	 * between non-solid, solid, and pipable resources.  However, we don't
	 * currently implement this optimization because it would be complex and
	 * would usually go unused.  */

	if (rdesc->flags & WIM_RESHDR_FLAG_COMPRESSED) {
		/* To re-use a non-solid resource, it must use the desired
		 * compression type and chunk size.  */
		return (rdesc->compression_type == out_ctype &&
			rdesc->chunk_size == out_chunk_size);
	} else {
		/* Solid resource: Such resources may contain multiple blobs,
		 * and in general only a subset of them need to be written.  As
		 * a heuristic, re-use the raw data if more than two-thirds the
		 * uncompressed size is being written.  */

		/* Note: solid resources contain a header that specifies the
		 * compression type and chunk size; therefore we don't need to
		 * check if they are compatible with @out_ctype and
		 * @out_chunk_size.  */

		/* Did we already decide to reuse the resource?  */
		if (rdesc->raw_copy_ok)
			return true;

		struct blob_descriptor *res_blob;
		u64 write_size = 0;

		list_for_each_entry(res_blob, &rdesc->blob_list, rdesc_node)
			if (res_blob->will_be_in_output_wim)
				write_size += res_blob->size;

		return (write_size > rdesc->uncompressed_size * 2 / 3);
	}
}

static u32
reshdr_flags_for_blob(const struct blob_descriptor *blob)
{
	u32 reshdr_flags = 0;
	if (blob->is_metadata)
		reshdr_flags |= WIM_RESHDR_FLAG_METADATA;
	return reshdr_flags;
}

static void
blob_set_out_reshdr_for_reuse(struct blob_descriptor *blob)
{
	const struct wim_resource_descriptor *rdesc;

	wimlib_assert(blob->blob_location == BLOB_IN_WIM);
	rdesc = blob->rdesc;

	if (rdesc->flags & WIM_RESHDR_FLAG_SOLID) {
		blob->out_reshdr.offset_in_wim = blob->offset_in_res;
		blob->out_reshdr.uncompressed_size = 0;
		blob->out_reshdr.size_in_wim = blob->size;

		blob->out_res_offset_in_wim = rdesc->offset_in_wim;
		blob->out_res_size_in_wim = rdesc->size_in_wim;
		blob->out_res_uncompressed_size = rdesc->uncompressed_size;
	} else {
		blob->out_reshdr.offset_in_wim = rdesc->offset_in_wim;
		blob->out_reshdr.uncompressed_size = rdesc->uncompressed_size;
		blob->out_reshdr.size_in_wim = rdesc->size_in_wim;
	}
	blob->out_reshdr.flags = rdesc->flags;
}


/* Write the header for a blob in a pipable WIM.  */
static int
write_pwm_blob_header(const struct blob_descriptor *blob,
		      struct filedes *out_fd, bool compressed)
{
	struct pwm_blob_hdr blob_hdr;
	u32 reshdr_flags;
	int ret;

	wimlib_assert(!blob->unhashed);

	blob_hdr.magic = cpu_to_le64(PWM_BLOB_MAGIC);
	blob_hdr.uncompressed_size = cpu_to_le64(blob->size);
	copy_hash(blob_hdr.hash, blob->hash);
	reshdr_flags = reshdr_flags_for_blob(blob);
	if (compressed)
		reshdr_flags |= WIM_RESHDR_FLAG_COMPRESSED;
	blob_hdr.flags = cpu_to_le32(reshdr_flags);
	ret = full_write(out_fd, &blob_hdr, sizeof(blob_hdr));
	if (ret)
		ERROR_WITH_ERRNO("Write error");
	return ret;
}

struct write_blobs_progress_data {
	wimlib_progress_func_t progfunc;
	void *progctx;
	union wimlib_progress_info progress;
	u64 next_progress;
};

static int
do_write_blobs_progress(struct write_blobs_progress_data *progress_data,
			u64 complete_size, u32 complete_count, bool discarded)
{
	union wimlib_progress_info *progress = &progress_data->progress;
	int ret;

	if (discarded) {
		progress->write_streams.total_bytes -= complete_size;
		progress->write_streams.total_streams -= complete_count;
		if (progress_data->next_progress != ~(u64)0 &&
		    progress_data->next_progress > progress->write_streams.total_bytes)
		{
			progress_data->next_progress = progress->write_streams.total_bytes;
		}
	} else {
		progress->write_streams.completed_bytes += complete_size;
		progress->write_streams.completed_streams += complete_count;
	}

	if (progress->write_streams.completed_bytes >= progress_data->next_progress) {

		ret = call_progress(progress_data->progfunc,
				    WIMLIB_PROGRESS_MSG_WRITE_STREAMS,
				    progress,
				    progress_data->progctx);
		if (ret)
			return ret;

		set_next_progress(progress->write_streams.completed_bytes,
				  progress->write_streams.total_bytes,
				  &progress_data->next_progress);
	}
	return 0;
}

struct write_blobs_ctx {
	WIMStruct *wim;

	int image;

	struct filedes *out_fd;

	int write_flags;

	/* Blob table for the WIMStruct on whose behalf the blobs are being
	 * written.  */
	struct blob_table *blob_table;

	/* The list of written blobs which is being collected  */
	struct list_head blob_table_list;

	/* The maximum part size in bytes (for writing split WIMs)  */
	u64 max_part_size;

	/* Compression format to use.  */
	int out_ctype;

	/* Maximum uncompressed chunk size in compressed resources to use.  */
	u32 out_chunk_size;

	/* Flags that affect how the blobs will be written.  */
	int write_resource_flags;

	/* Data used for issuing WRITE_STREAMS progress.  */
	struct write_blobs_progress_data progress_data;

	struct filter_context *filter_ctx;

	/* Pointer to the chunk_compressor implementation being used for
	 * compressing chunks of data, or NULL if chunks are being written
	 * uncompressed.  */
	struct chunk_compressor *compressor;

	/* A buffer of size @out_chunk_size that has been loaned out from the
	 * chunk compressor and is currently being filled with the uncompressed
	 * data of the next chunk.  */
	u8 *cur_chunk_buf;

	/* Number of bytes in @cur_chunk_buf that are currently filled.  */
	size_t cur_chunk_buf_filled;

	/* List of blobs that currently have chunks being compressed.  */
	struct list_head blobs_being_compressed;

	/* List of blobs in the solid resource.  Blobs are moved here after
	 * @blobs_being_compressed only when writing a solid resource.  */
	struct list_head blobs_in_solid_resource;

	/* Current uncompressed offset in the blob being read.  */
	u64 cur_read_blob_offset;

	/* Uncompressed size of the blob currently being read.  */
	u64 cur_read_blob_size;

	/* Current uncompressed offset in the blob being written.  */
	u64 cur_write_blob_offset;

	/* Uncompressed size of resource currently being written.  */
	u64 cur_write_res_size;

	/* Array that is filled in with compressed chunk sizes as a resource is
	 * being written.  */
	u64 *chunk_csizes;

	/* Index of next entry in @chunk_csizes to fill in.  */
	size_t chunk_index;

	/* Number of entries in @chunk_csizes currently allocated.  */
	size_t num_alloc_chunks;

	/* Offset in the output file of the start of the chunks of the resource
	 * currently being written.  */
	u64 chunks_start_offset;
};

/* Reserve space for the chunk table and prepare to accumulate the chunk table
 * in memory.  */
static int
begin_chunk_table(struct write_blobs_ctx *ctx, u64 res_expected_size)
{
	u64 expected_num_chunks;
	u64 expected_num_chunk_entries;
	size_t reserve_size;
	int ret;

	/* Calculate the number of chunks and chunk entries that should be
	 * needed for the resource.  These normally will be the final values,
	 * but in SOLID mode some of the blobs we're planning to write into the
	 * resource may be duplicates, and therefore discarded, potentially
	 * decreasing the number of chunk entries needed.  */
	expected_num_chunks = DIV_ROUND_UP(res_expected_size, ctx->out_chunk_size);
	expected_num_chunk_entries = expected_num_chunks;
	if (!(ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID))
		expected_num_chunk_entries--;

	/* Make sure the chunk_csizes array is long enough to store the
	 * compressed size of each chunk.  */
	if (expected_num_chunks > ctx->num_alloc_chunks) {
		u64 new_length = expected_num_chunks + 50;

		if ((size_t)new_length != new_length) {
			ERROR("Resource size too large (%"PRIu64" bytes!",
			      res_expected_size);
			return WIMLIB_ERR_NOMEM;
		}

		FREE(ctx->chunk_csizes);
		ctx->chunk_csizes = MALLOC(new_length * sizeof(ctx->chunk_csizes[0]));
		if (ctx->chunk_csizes == NULL) {
			ctx->num_alloc_chunks = 0;
			return WIMLIB_ERR_NOMEM;
		}
		ctx->num_alloc_chunks = new_length;
	}

	ctx->chunk_index = 0;

	if (!(ctx->write_resource_flags & WRITE_RESOURCE_FLAG_PIPABLE)) {
		/* Reserve space for the chunk table in the output file.  In the
		 * case of solid resources this reserves the upper bound for the
		 * needed space, not necessarily the exact space which will
		 * prove to be needed.  At this point, we just use @chunk_csizes
		 * for a buffer of 0's because the actual compressed chunk sizes
		 * are unknown.  */
		reserve_size = expected_num_chunk_entries *
			       get_chunk_entry_size(res_expected_size,
						    0 != (ctx->write_resource_flags &
							  WRITE_RESOURCE_FLAG_SOLID));
		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID)
			reserve_size += sizeof(struct alt_chunk_table_header_disk);
		memset(ctx->chunk_csizes, 0, reserve_size);
		ret = full_write(ctx->out_fd, ctx->chunk_csizes, reserve_size);
		if (ret)
			return ret;
	}
	return 0;
}

static int
begin_write_resource(struct write_blobs_ctx *ctx, u64 res_expected_size)
{
	int ret;

	wimlib_assert(res_expected_size != 0);

	if (ctx->compressor != NULL) {
		ret = begin_chunk_table(ctx, res_expected_size);
		if (ret)
			return ret;
	}

	/* Output file descriptor is now positioned at the offset at which to
	 * write the first chunk of the resource.  */
	ctx->chunks_start_offset = ctx->out_fd->offset;
	ctx->cur_write_blob_offset = 0;
	ctx->cur_write_res_size = res_expected_size;
	return 0;
}

static int
end_chunk_table(struct write_blobs_ctx *ctx, u64 res_actual_size,
		u64 *res_start_offset_ret, u64 *res_store_size_ret)
{
	size_t actual_num_chunks;
	size_t actual_num_chunk_entries;
	size_t chunk_entry_size;
	int ret;

	actual_num_chunks = ctx->chunk_index;
	actual_num_chunk_entries = actual_num_chunks;
	if (!(ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID))
		actual_num_chunk_entries--;

	chunk_entry_size = get_chunk_entry_size(res_actual_size,
						0 != (ctx->write_resource_flags &
						      WRITE_RESOURCE_FLAG_SOLID));

	typedef le64 _may_alias_attribute aliased_le64_t;
	typedef le32 _may_alias_attribute aliased_le32_t;

	if (chunk_entry_size == 4) {
		aliased_le32_t *entries = (aliased_le32_t*)ctx->chunk_csizes;

		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
			for (size_t i = 0; i < actual_num_chunk_entries; i++)
				entries[i] = cpu_to_le32(ctx->chunk_csizes[i]);
		} else {
			u32 offset = ctx->chunk_csizes[0];
			for (size_t i = 0; i < actual_num_chunk_entries; i++) {
				u32 next_size = ctx->chunk_csizes[i + 1];
				entries[i] = cpu_to_le32(offset);
				offset += next_size;
			}
		}
	} else {
		aliased_le64_t *entries = (aliased_le64_t*)ctx->chunk_csizes;

		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
			for (size_t i = 0; i < actual_num_chunk_entries; i++)
				entries[i] = cpu_to_le64(ctx->chunk_csizes[i]);
		} else {
			u64 offset = ctx->chunk_csizes[0];
			for (size_t i = 0; i < actual_num_chunk_entries; i++) {
				u64 next_size = ctx->chunk_csizes[i + 1];
				entries[i] = cpu_to_le64(offset);
				offset += next_size;
			}
		}
	}

	size_t chunk_table_size = actual_num_chunk_entries * chunk_entry_size;
	u64 res_start_offset;
	u64 res_end_offset;

	if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_PIPABLE) {
		ret = full_write(ctx->out_fd, ctx->chunk_csizes, chunk_table_size);
		if (ret)
			goto write_error;
		res_end_offset = ctx->out_fd->offset;
		res_start_offset = ctx->chunks_start_offset;
	} else {
		res_end_offset = ctx->out_fd->offset;

		u64 chunk_table_offset;

		chunk_table_offset = ctx->chunks_start_offset - chunk_table_size;

		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
			struct alt_chunk_table_header_disk hdr;

			hdr.res_usize = cpu_to_le64(res_actual_size);
			hdr.chunk_size = cpu_to_le32(ctx->out_chunk_size);
			hdr.compression_format = cpu_to_le32(ctx->out_ctype);

			STATIC_ASSERT(WIMLIB_COMPRESSION_TYPE_XPRESS == 1);
			STATIC_ASSERT(WIMLIB_COMPRESSION_TYPE_LZX == 2);
			STATIC_ASSERT(WIMLIB_COMPRESSION_TYPE_LZMS == 3);

			ret = full_pwrite(ctx->out_fd, &hdr, sizeof(hdr),
					  chunk_table_offset - sizeof(hdr));
			if (ret)
				goto write_error;
			res_start_offset = chunk_table_offset - sizeof(hdr);
		} else {
			res_start_offset = chunk_table_offset;
		}

		ret = full_pwrite(ctx->out_fd, ctx->chunk_csizes,
				  chunk_table_size, chunk_table_offset);
		if (ret)
			goto write_error;
	}

	*res_start_offset_ret = res_start_offset;
	*res_store_size_ret = res_end_offset - res_start_offset;

	return 0;

write_error:
	ERROR_WITH_ERRNO("Write error");
	return ret;
}

/* Finish writing a WIM resource by writing or updating the chunk table (if not
 * writing the data uncompressed) and loading its metadata into @out_reshdr.  */
static int
end_write_resource(struct write_blobs_ctx *ctx, struct wim_reshdr *out_reshdr)
{
	int ret;
	u64 res_size_in_wim;
	u64 res_uncompressed_size;
	u64 res_offset_in_wim;

	wimlib_assert(ctx->cur_write_blob_offset == ctx->cur_write_res_size ||
		      (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID));
	res_uncompressed_size = ctx->cur_write_res_size;

	if (ctx->compressor) {
		ret = end_chunk_table(ctx, res_uncompressed_size,
				      &res_offset_in_wim, &res_size_in_wim);
		if (ret)
			return ret;
	} else {
		res_offset_in_wim = ctx->chunks_start_offset;
		res_size_in_wim = ctx->out_fd->offset - res_offset_in_wim;
	}
	out_reshdr->uncompressed_size = res_uncompressed_size;
	out_reshdr->size_in_wim = res_size_in_wim;
	out_reshdr->offset_in_wim = res_offset_in_wim;
	return 0;
}

/* Call when no more data from the file at @path is needed.  */
static int
done_with_file(const tchar *path, wimlib_progress_func_t progfunc, void *progctx)
{
	union wimlib_progress_info info;

	info.done_with_file.path_to_file = path;

	return call_progress(progfunc, WIMLIB_PROGRESS_MSG_DONE_WITH_FILE,
			     &info, progctx);
}

static int
do_done_with_blob(struct blob_descriptor *blob,
		  wimlib_progress_func_t progfunc, void *progctx)
{
	int ret;
	struct wim_inode *inode;
	tchar *cookie1;
	tchar *cookie2;

	if (!blob->may_send_done_with_file)
		return 0;

	inode = blob->file_inode;

	wimlib_assert(inode != NULL);
	wimlib_assert(inode->i_num_remaining_streams > 0);
	if (--inode->i_num_remaining_streams > 0)
		return 0;

	cookie1 = progress_get_streamless_path(blob->file_on_disk);
	cookie2 = progress_get_win32_path(blob->file_on_disk);

	ret = done_with_file(blob->file_on_disk, progfunc, progctx);

	progress_put_win32_path(cookie2);
	progress_put_streamless_path(cookie1);

	return ret;
}

/* Handle WIMLIB_WRITE_FLAG_SEND_DONE_WITH_FILE_MESSAGES mode.  */
static inline int
done_with_blob(struct blob_descriptor *blob, struct write_blobs_ctx *ctx)
{
	if (likely(!(ctx->write_resource_flags &
		     WRITE_RESOURCE_FLAG_SEND_DONE_WITH_FILE)))
		return 0;
	return do_done_with_blob(blob, ctx->progress_data.progfunc,
				 ctx->progress_data.progctx);
}

/* Begin processing a blob for writing.  */
static int
write_blob_begin_read(struct blob_descriptor *blob, void *_ctx)
{
	struct write_blobs_ctx *ctx = _ctx;
	int ret;

	wimlib_assert(blob->size > 0);

	ctx->cur_read_blob_offset = 0;
	ctx->cur_read_blob_size = blob->size;

	/* As an optimization, we allow some blobs to be "unhashed", meaning
	 * their SHA-1 message digests are unknown.  This is the case with blobs
	 * that are added by scanning a directory tree with wimlib_add_image(),
	 * for example.  Since WIM uses single-instance blobs, we don't know
	 * whether such each such blob really need to written until it is
	 * actually checksummed, unless it has a unique size.  In such cases we
	 * read and checksum the blob in this function, thereby advancing ahead
	 * of read_blob_list(), which will still provide the data again to
	 * write_blob_process_chunk().  This is okay because an unhashed blob
	 * cannot be in a WIM resource, which might be costly to decompress.  */
	if (ctx->blob_table != NULL && blob->unhashed && !blob->unique_size) {

		struct blob_descriptor *new_blob;

		ret = hash_unhashed_blob(blob, ctx->blob_table, &new_blob);
		if (ret)
			return ret;
		if (new_blob != blob) {
			/* Duplicate blob detected.  */

			if (new_blob->will_be_in_output_wim ||
			    blob_filtered(new_blob, ctx->filter_ctx))
			{
				/* The duplicate blob is already being included
				 * in the output WIM, or it would be filtered
				 * out if it had been.  Skip writing this blob
				 * (and reading it again) entirely, passing its
				 * output reference count to the duplicate blob
				 * in the former case.  */
				ret = do_write_blobs_progress(&ctx->progress_data,
							      blob->size, 1, true);
				list_del(&blob->write_blobs_list);
				if (new_blob->will_be_in_output_wim)
					new_blob->out_refcnt += blob->out_refcnt;
				if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID)
					ctx->cur_write_res_size -= blob->size;
				if (!ret)
					ret = done_with_blob(blob, ctx);
				free_blob_descriptor(blob);
				if (ret)
					return ret;
				return BEGIN_BLOB_STATUS_SKIP_BLOB;
			} else {
				/* The duplicate blob can validly be written,
				 * but was not marked as such.  Discard the
				 * current blob descriptor and use the
				 * duplicate, but actually freeing the current
				 * blob descriptor must wait until
				 * read_blob_list() has finished reading its
				 * data.  */
				list_replace(&blob->write_blobs_list,
					     &new_blob->write_blobs_list);
				blob->will_be_in_output_wim = 0;
				new_blob->out_refcnt = blob->out_refcnt;
				new_blob->will_be_in_output_wim = 1;
				new_blob->may_send_done_with_file = 0;
				blob = new_blob;
			}
		}
	}
	list_move_tail(&blob->write_blobs_list, &ctx->blobs_being_compressed);
	return 0;
}

/* Rewrite a blob that was just written compressed (as a non-solid WIM resource)
 * as uncompressed instead.  */
static int
write_blob_uncompressed(struct blob_descriptor *blob, struct filedes *out_fd)
{
	int ret;
	u64 begin_offset = blob->out_reshdr.offset_in_wim;
	u64 end_offset = out_fd->offset;

	if (filedes_seek(out_fd, begin_offset) == -1)
		return 0;

	ret = extract_blob_to_fd(blob, out_fd);
	if (ret) {
		/* Error reading the uncompressed data.  */
		if (out_fd->offset == begin_offset &&
		    filedes_seek(out_fd, end_offset) != -1)
		{
			/* Nothing was actually written yet, and we successfully
			 * seeked to the end of the compressed resource, so
			 * don't issue a hard error; just keep the compressed
			 * resource instead.  */
			WARNING("Recovered compressed resource of "
				"size %"PRIu64", continuing on.", blob->size);
			return 0;
		}
		return ret;
	}

	wimlib_assert(out_fd->offset - begin_offset == blob->size);

	/* We could ftruncate() the file to 'out_fd->offset' here, but there
	 * isn't much point.  Usually we will only be truncating by a few bytes
	 * and will just overwrite the data immediately.  */

	blob->out_reshdr.size_in_wim = blob->size;
	blob->out_reshdr.flags &= ~(WIM_RESHDR_FLAG_COMPRESSED |
				    WIM_RESHDR_FLAG_SOLID);
	return 0;
}

/* Returns true if the specified blob, which was written as a non-solid
 * resource, should be truncated from the WIM file and re-written uncompressed.
 * blob->out_reshdr must be filled in from the initial write of the blob.  */
static bool
should_rewrite_blob_uncompressed(const struct write_blobs_ctx *ctx,
				 const struct blob_descriptor *blob)
{
	/* If the compressed data is smaller than the uncompressed data, prefer
	 * the compressed data.  */
	if (blob->out_reshdr.size_in_wim < blob->out_reshdr.uncompressed_size)
		return false;

	/* If we're not actually writing compressed data, then there's no need
	 * for re-writing.  */
	if (!ctx->compressor)
		return false;

	/* If writing a pipable WIM, everything we write to the output is final
	 * (it might actually be a pipe!).  */
	if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_PIPABLE)
		return false;

	/* If the blob that would need to be re-read is located in a solid
	 * resource in another WIM file, then re-reading it would be costly.  So
	 * don't do it.
	 *
	 * Exception: if the compressed size happens to be *exactly* the same as
	 * the uncompressed size, then the blob *must* be written uncompressed
	 * in order to remain compatible with the Windows Overlay Filesystem
	 * Filter Driver (WOF).
	 *
	 * TODO: we are currently assuming that the optimization for
	 * single-chunk resources in maybe_rewrite_blob_uncompressed() prevents
	 * this case from being triggered too often.  To fully prevent excessive
	 * decompressions in degenerate cases, we really should obtain the
	 * uncompressed data by decompressing the compressed data we wrote to
	 * the output file.
	 */
	if (blob->blob_location == BLOB_IN_WIM &&
	    blob->size != blob->rdesc->uncompressed_size &&
	    blob->size != blob->out_reshdr.size_in_wim)
		return false;

	return true;
}

static int
maybe_rewrite_blob_uncompressed(struct write_blobs_ctx *ctx,
				struct blob_descriptor *blob)
{
	if (!should_rewrite_blob_uncompressed(ctx, blob))
		return 0;

	/* Regular (non-solid) WIM resources with exactly one chunk and
	 * compressed size equal to uncompressed size are exactly the same as
	 * the corresponding compressed data --- since there must be 0 entries
	 * in the chunk table and the only chunk must be stored uncompressed.
	 * In this case, there's no need to rewrite anything.  */
	if (ctx->chunk_index == 1 &&
	    blob->out_reshdr.size_in_wim == blob->out_reshdr.uncompressed_size)
	{
		blob->out_reshdr.flags &= ~WIM_RESHDR_FLAG_COMPRESSED;
		return 0;
	}

	return write_blob_uncompressed(blob, ctx->out_fd);
}

/* Write the next chunk of (typically compressed) data to the output WIM,
 * handling the writing of the chunk table.  */
static int
write_chunk(struct write_blobs_ctx *ctx, const void *cchunk,
	    size_t csize, size_t usize)
{
	int ret;
	struct blob_descriptor *blob;
	u32 completed_blob_count;
	u32 completed_size;

	blob = list_entry(ctx->blobs_being_compressed.next,
			  struct blob_descriptor, write_blobs_list);

	if (ctx->cur_write_blob_offset == 0 &&
	    !(ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID))
	{
		/* Starting to write a new blob in non-solid mode.  */

		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_PIPABLE) {
			ret = write_pwm_blob_header(blob, ctx->out_fd,
						    ctx->compressor != NULL);
			if (ret)
				return ret;
		}

		ret = begin_write_resource(ctx, blob->size);
		if (ret)
			return ret;
	}

	if (ctx->compressor != NULL) {
		/* Record the compresed chunk size.  */
		wimlib_assert(ctx->chunk_index < ctx->num_alloc_chunks);
		ctx->chunk_csizes[ctx->chunk_index++] = csize;

	       /* If writing a pipable WIM, before the chunk data write a chunk
		* header that provides the compressed chunk size.  */
		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_PIPABLE) {
			struct pwm_chunk_hdr chunk_hdr = {
				.compressed_size = cpu_to_le32(csize),
			};
			ret = full_write(ctx->out_fd, &chunk_hdr,
					 sizeof(chunk_hdr));
			if (ret)
				goto write_error;
		}
	}

	/* Write the chunk data.  */
	ret = full_write(ctx->out_fd, cchunk, csize);
	if (ret)
		goto write_error;

	ctx->cur_write_blob_offset += usize;

	completed_size = usize;
	completed_blob_count = 0;
	if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
		/* Wrote chunk in solid mode.  It may have finished multiple
		 * blobs.  */
		struct blob_descriptor *next_blob;

		while (blob && ctx->cur_write_blob_offset >= blob->size) {

			ctx->cur_write_blob_offset -= blob->size;

			if (ctx->cur_write_blob_offset)
				next_blob = list_entry(blob->write_blobs_list.next,
						      struct blob_descriptor,
						      write_blobs_list);
			else
				next_blob = NULL;

			ret = done_with_blob(blob, ctx);
			if (ret)
				return ret;
			list_move_tail(&blob->write_blobs_list, &ctx->blobs_in_solid_resource);
			completed_blob_count++;

			blob = next_blob;
		}
	} else {
		/* Wrote chunk in non-solid mode.  It may have finished a
		 * blob.  */
		if (ctx->cur_write_blob_offset == blob->size) {

			wimlib_assert(ctx->cur_write_blob_offset ==
				      ctx->cur_write_res_size);

			ret = end_write_resource(ctx, &blob->out_reshdr);
			if (ret)
				return ret;

			blob->out_reshdr.flags = reshdr_flags_for_blob(blob);
			if (ctx->compressor != NULL)
				blob->out_reshdr.flags |= WIM_RESHDR_FLAG_COMPRESSED;

			ret = maybe_rewrite_blob_uncompressed(ctx, blob);
			if (ret)
				return ret;

			wimlib_assert(blob->out_reshdr.uncompressed_size == blob->size);

			ctx->cur_write_blob_offset = 0;

			ret = done_with_blob(blob, ctx);
			if (ret)
				return ret;
			list_del(&blob->write_blobs_list);
			list_add(&blob->blob_table_list, ctx->blob_table_list);
			completed_blob_count++;
		}
	}

	return do_write_blobs_progress(&ctx->progress_data, completed_size,
				       completed_blob_count, false);

write_error:
	ERROR_WITH_ERRNO("Write error");
	return ret;
}

static int
prepare_chunk_buffer(struct write_blobs_ctx *ctx)
{
	/* While we are unable to get a new chunk buffer due to too many chunks
	 * already outstanding, retrieve and write the next compressed chunk. */
	while (!(ctx->cur_chunk_buf =
		 ctx->compressor->get_chunk_buffer(ctx->compressor)))
	{
		const void *cchunk;
		u32 csize;
		u32 usize;
		bool bret;
		int ret;

		bret = ctx->compressor->get_compression_result(ctx->compressor,
							       &cchunk,
							       &csize,
							       &usize);
		wimlib_assert(bret);

		ret = write_chunk(ctx, cchunk, csize, usize);
		if (ret)
			return ret;
	}
	return 0;
}

/* Process the next chunk of data to be written to a WIM resource.  */
static int
write_blob_process_chunk(const void *chunk, size_t size, void *_ctx)
{
	struct write_blobs_ctx *ctx = _ctx;
	int ret;
	const u8 *chunkptr, *chunkend;

	wimlib_assert(size != 0);

	if (ctx->compressor == NULL) {
		/* Write chunk uncompressed.  */
		 ret = write_chunk(ctx, chunk, size, size);
		 if (ret)
			 return ret;
		 ctx->cur_read_blob_offset += size;
		 return 0;
	}

	/* Submit the chunk for compression, but take into account that the
	 * @size the chunk was provided in may not correspond to the
	 * @out_chunk_size being used for compression.  */
	chunkptr = chunk;
	chunkend = chunkptr + size;
	do {
		size_t needed_chunk_size;
		size_t bytes_consumed;

		if (!ctx->cur_chunk_buf) {
			ret = prepare_chunk_buffer(ctx);
			if (ret)
				return ret;
		}

		if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
			needed_chunk_size = ctx->out_chunk_size;
		} else {
			needed_chunk_size = min(ctx->out_chunk_size,
						ctx->cur_chunk_buf_filled +
							(ctx->cur_read_blob_size -
							 ctx->cur_read_blob_offset));
		}

		bytes_consumed = min(chunkend - chunkptr,
				     needed_chunk_size - ctx->cur_chunk_buf_filled);

		memcpy(&ctx->cur_chunk_buf[ctx->cur_chunk_buf_filled],
		       chunkptr, bytes_consumed);

		chunkptr += bytes_consumed;
		ctx->cur_read_blob_offset += bytes_consumed;
		ctx->cur_chunk_buf_filled += bytes_consumed;

		if (ctx->cur_chunk_buf_filled == needed_chunk_size) {
			ctx->compressor->signal_chunk_filled(ctx->compressor,
							     ctx->cur_chunk_buf_filled);
			ctx->cur_chunk_buf = NULL;
			ctx->cur_chunk_buf_filled = 0;
		}
	} while (chunkptr != chunkend);
	return 0;
}

/* Finish processing a blob for writing.  It may not have been completely
 * written yet, as the chunk_compressor implementation may still have chunks
 * buffered or being compressed.  */
static int
write_blob_end_read(struct blob_descriptor *blob, int status, void *_ctx)
{
	struct write_blobs_ctx *ctx = _ctx;

	wimlib_assert(ctx->cur_read_blob_offset == ctx->cur_read_blob_size || status);

	if (!blob->will_be_in_output_wim) {
		/* The blob was a duplicate.  Now that its data has finished
		 * being read, it is being discarded in favor of the duplicate
		 * entry.  It therefore is no longer needed, and we can fire the
		 * DONE_WITH_FILE callback because the file will not be read
		 * again.
		 *
		 * Note: we can't yet fire DONE_WITH_FILE for non-duplicate
		 * blobs, since it needs to be possible to re-read the file if
		 * it does not compress to less than its original size.  */
		if (!status)
			status = done_with_blob(blob, ctx);
		free_blob_descriptor(blob);
	} else if (!status && blob->unhashed && ctx->blob_table != NULL) {
		/* The blob was not a duplicate and was previously unhashed.
		 * Since we passed COMPUTE_MISSING_BLOB_HASHES to
		 * read_blob_list(), blob->hash is now computed and valid.  So
		 * turn this blob into a "hashed" blob.  */
		list_del(&blob->unhashed_list);
		blob_table_insert(ctx->blob_table, blob);
		blob->unhashed = 0;
	}
	return status;
}

/*
 * Compute statistics about a list of blobs that will be written.
 *
 * Assumes the blobs are sorted such that all blobs located in each distinct WIM
 * (specified by WIMStruct) are together.
 *
 * For compactions, also verify that there are no overlapping resources.  This
 * really should be checked earlier, but for now it's easiest to check here.
 */
static int
tally_blob_list_stats(struct list_head *blob_list,
		      struct write_blobs_ctx *ctx)
{
	struct blob_descriptor *blob;
	WIMStruct *prev_wim_part = NULL;
	const struct wim_resource_descriptor *prev_rdesc = NULL;

	list_for_each_entry(blob, blob_list, write_blobs_list) {
		ctx->progress_data.progress.write_streams.total_streams++;
		ctx->progress_data.progress.write_streams.total_bytes += blob->size;
		if (blob->blob_location == BLOB_IN_WIM) {
			const struct wim_resource_descriptor *rdesc = blob->rdesc;
			WIMStruct *wim = rdesc->wim;

			if (unlikely(wim->being_compacted) && rdesc != prev_rdesc) {
				if (prev_rdesc != NULL &&
				    rdesc->offset_in_wim <
						prev_rdesc->offset_in_wim +
						prev_rdesc->size_in_wim)
				{
					WARNING("WIM file contains overlapping "
						"resources!  Compaction is not "
						"possible.");
					return WIMLIB_ERR_RESOURCE_ORDER;
				}
				prev_rdesc = rdesc;
			}
			if (prev_wim_part != wim && !blob->is_metadata) {
				prev_wim_part = wim;
				ctx->progress_data.progress.write_streams.total_parts++;
			}
		}
	}
	return 0;
}

/* Find blobs in @blob_list that can be copied to the output WIM in raw form
 * rather than compressed.  Delete these blobs from @blob_list and move them to
 * @raw_copy_blobs.  Return the total uncompressed size of the blobs that need
 * to be compressed.  */
static u64
find_raw_copy_blobs(struct list_head *blob_list, int write_resource_flags,
		    int out_ctype, u32 out_chunk_size,
		    struct list_head *raw_copy_blobs)
{
	struct blob_descriptor *blob, *tmp;
	u64 num_nonraw_bytes = 0;

	INIT_LIST_HEAD(raw_copy_blobs);

	/* Initialize temporary raw_copy_ok flag.  */
	list_for_each_entry(blob, blob_list, write_blobs_list)
		if (blob->blob_location == BLOB_IN_WIM)
			blob->rdesc->raw_copy_ok = 0;

	list_for_each_entry_safe(blob, tmp, blob_list, write_blobs_list) {
		if (can_raw_copy(blob, write_resource_flags,
				 out_ctype, out_chunk_size))
		{
			blob->rdesc->raw_copy_ok = 1;
			list_move_tail(&blob->write_blobs_list, raw_copy_blobs);
		} else {
			num_nonraw_bytes += blob->size;
		}
	}

	return num_nonraw_bytes;
}

/* Copy a raw compressed resource located in another WIM file to the WIM file
 * being written.  */
static int
write_raw_copy_resource(struct wim_resource_descriptor *in_rdesc,
			struct filedes *out_fd,
			struct list_head *blob_table_list)
{
	u64 cur_read_offset;
	u64 end_read_offset;
	u8 buf[BUFFER_SIZE];
	size_t bytes_to_read;
	int ret;
	struct filedes *in_fd;
	struct blob_descriptor *blob;
	u64 out_offset_in_wim;

	/* Copy the raw data.  */
	cur_read_offset = in_rdesc->offset_in_wim;
	end_read_offset = cur_read_offset + in_rdesc->size_in_wim;

	out_offset_in_wim = out_fd->offset;

	if (in_rdesc->is_pipable) {
		if (cur_read_offset < sizeof(struct pwm_blob_hdr))
			return WIMLIB_ERR_INVALID_PIPABLE_WIM;
		cur_read_offset -= sizeof(struct pwm_blob_hdr);
		out_offset_in_wim += sizeof(struct pwm_blob_hdr);
	}
	in_fd = &in_rdesc->wim->in_fd;
	wimlib_assert(cur_read_offset != end_read_offset);

	if (likely(!in_rdesc->wim->being_compacted) ||
	    in_rdesc->offset_in_wim > out_fd->offset) {
		do {
			bytes_to_read = min(sizeof(buf),
					    end_read_offset - cur_read_offset);

			ret = full_pread(in_fd, buf, bytes_to_read,
					 cur_read_offset);
			if (ret)
				return ret;

			ret = full_write(out_fd, buf, bytes_to_read);
			if (ret)
				return ret;

			cur_read_offset += bytes_to_read;

		} while (cur_read_offset != end_read_offset);
	} else {
		/* Optimization: the WIM file is being compacted and the
		 * resource being written is already in the desired location.
		 * Skip over the data instead of re-writing it.  */

		/* Due the earlier check for overlapping resources, it should
		 * never be the case that we already overwrote the resource.  */
		wimlib_assert(!(in_rdesc->offset_in_wim < out_fd->offset));

		if (-1 == filedes_seek(out_fd, out_fd->offset + in_rdesc->size_in_wim))
			return WIMLIB_ERR_WRITE;
	}

	list_for_each_entry(blob, &in_rdesc->blob_list, rdesc_node) {
		if (blob->will_be_in_output_wim) {
			blob_set_out_reshdr_for_reuse(blob);
			if (in_rdesc->flags & WIM_RESHDR_FLAG_SOLID)
				blob->out_res_offset_in_wim = out_offset_in_wim;
			else
				blob->out_reshdr.offset_in_wim = out_offset_in_wim;
			list_add_tail(&blob->blob_table_list, blob_table_list);
		}
	}
	return 0;
}

/* Copy a list of raw compressed resources located in other WIM file(s) to the
 * WIM file being written.  */
static int
write_raw_copy_resources(struct list_head *raw_copy_blobs,
			 struct filedes *out_fd,
			 struct list_head *blob_table_list,
			 struct write_blobs_progress_data *progress_data)
{
	struct blob_descriptor *blob;
	int ret;

	list_for_each_entry(blob, raw_copy_blobs, write_blobs_list)
		blob->rdesc->raw_copy_ok = 1;

	list_for_each_entry(blob, raw_copy_blobs, write_blobs_list) {
		if (blob->rdesc->raw_copy_ok) {
			/* Write each solid resource only one time.  */
			ret = write_raw_copy_resource(blob->rdesc, out_fd,
						      blob_table_list);
			if (ret)
				return ret;
			blob->rdesc->raw_copy_ok = 0;
		}
		ret = do_write_blobs_progress(progress_data, blob->size,
					      1, false);
		if (ret)
			return ret;
	}
	return 0;
}

/* Wait for and write all chunks pending in the compressor.  */
static int
finish_remaining_chunks(struct write_blobs_ctx *ctx)
{
	const void *cdata;
	u32 csize;
	u32 usize;
	int ret;

	if (ctx->compressor == NULL)
		return 0;

	if (ctx->cur_chunk_buf_filled != 0) {
		ctx->compressor->signal_chunk_filled(ctx->compressor,
						     ctx->cur_chunk_buf_filled);
	}

	while (ctx->compressor->get_compression_result(ctx->compressor, &cdata,
						       &csize, &usize))
	{
		ret = write_chunk(ctx, cdata, csize, usize);
		if (ret)
			return ret;
	}
	return 0;
}

static inline bool
blob_is_in_file(const struct blob_descriptor *blob)
{
	return blob->blob_location == BLOB_IN_FILE_ON_DISK
#ifdef __WIN32__
	    || blob->blob_location == BLOB_IN_WINNT_FILE_ON_DISK
	    || blob->blob_location == BLOB_WIN32_ENCRYPTED
#endif
	   ;
}

static void
init_done_with_file_info(struct list_head *blob_list)
{
	struct blob_descriptor *blob;

	list_for_each_entry(blob, blob_list, write_blobs_list) {
		if (blob_is_in_file(blob)) {
			blob->file_inode->i_num_remaining_streams = 0;
			blob->may_send_done_with_file = 1;
		} else {
			blob->may_send_done_with_file = 0;
		}
	}

	list_for_each_entry(blob, blob_list, write_blobs_list)
		if (blob->may_send_done_with_file)
			blob->file_inode->i_num_remaining_streams++;
}

static int
finish_pending_blobs(struct write_blobs_ctx *ctx)
{
	int ret;
	
	ret = finish_remaining_chunks(ctx);
	if (ret)
		return ret;

	if (ctx->write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
		struct wim_reshdr reshdr;
		struct blob_descriptor *blob;
		u64 offset_in_res;

		ret = end_write_resource(&ctx, &reshdr);
		if (ret)
			goto out_destroy_context;

		offset_in_res = 0;
		list_for_each_entry(blob, &ctx.blobs_in_solid_resource, write_blobs_list) {
			blob->out_reshdr.size_in_wim = blob->size;
			blob->out_reshdr.flags = reshdr_flags_for_blob(blob) |
						 WIM_RESHDR_FLAG_SOLID;
			blob->out_reshdr.uncompressed_size = 0;
			blob->out_reshdr.offset_in_wim = offset_in_res;
			blob->out_res_offset_in_wim = reshdr.offset_in_wim;
			blob->out_res_size_in_wim = reshdr.size_in_wim;
			blob->out_res_uncompressed_size = reshdr.uncompressed_size;
			list_add_tail(&blob->blob_table_list, blob_table_list);
			offset_in_res += blob->size;
		}
		INIT_LIST_HEAD(&ctx.blobs_in_solid_resource);
		wimlib_assert(offset_in_res == reshdr.uncompressed_size);
	}

	return 0;
}

static void
destroy_compressor(struct write_blobs_ctx *ctx)
{
	if (ctx.compressor) {
		ctx.compressor->destroy(ctx.compressor);
		ctx.compressor = NULL;
	}
}

static int
init_compressor(struct write_blobs_ctx *ctx, int out_ctype, u32 out_chunk_size,
		unsigned num_threads)
{
	int ret;

	if (ctx->compressor &&
	    ctx->compressor.out_ctype == out_ctype &&
	    ctx->compressor.out_chunk_size == out_chunk_size)
		return 0;

	destroy_compressor(ctx);

	/* Unless uncompressed output was required, allocate a chunk_compressor
	 * to do compression.  There are serial and parallel implementations of
	 * the chunk_compressor interface.  We default to parallel using the
	 * specified number of threads, unless the upper bound on the number
	 * bytes needing to be compressed is less than a heuristic value.  */
	if (out_ctype != WIMLIB_COMPRESSION_TYPE_NONE) {

	#ifdef ENABLE_MULTITHREADED_COMPRESSION
		if (num_nonraw_bytes > max(2000000, out_chunk_size)) {
			ret = new_parallel_chunk_compressor(out_ctype,
							    out_chunk_size,
							    num_threads, 0,
							    &ctx.compressor);
			if (ret > 0) {
				WARNING("Couldn't create parallel chunk compressor: %"TS".\n"
					"          Falling back to single-threaded compression.",
					wimlib_get_error_string(ret));
			}
		}
	#endif

		if (ctx.compressor == NULL) {
			return new_serial_chunk_compressor(out_ctype, out_chunk_size,
							   &ctx.compressor);
		}
	}

	return 0;
}

static int
read_blob_list_and_write(struct list_head *blob_list,
			 const struct read_blob_callbacks *cbs,
			 struct write_blobs_ctx *ctx)
{
	int ret;

	ret = read_blob_list(blob_list,
			     offsetof(struct blob_descriptor, write_blobs_list),
			     &cbs,
			     BLOB_LIST_ALREADY_SORTED |
				VERIFY_BLOB_HASHES |
				COMPUTE_MISSING_BLOB_HASHES);

	if (!ret)
		ret = finish_pending_blobs(&ctx);
	return ret;
}

static int
write_blobs(WIMStruct *wim,
	    int image,
	    struct list_head *metadata_blob_list,
	    struct list_head *file_blob_list,
	    int write_flags,
	    unsigned num_threads,
	    u64 max_part_size,
	    struct filter_ctx *filter_ctx)
{
	int ret;
	struct write_blobs_ctx ctx = {};
	struct list_head raw_copy_blobs;
	u64 num_nonraw_bytes;
	const struct read_blob_callbacks cbs = {
		.begin_blob	= write_blob_begin_read,
		.consume_chunk	= write_blob_process_chunk,
		.end_blob	= write_blob_end_read,
		.ctx		= &ctx,
	};
	LIST_HEAD(tmp_list);

	memset(&ctx, 0, sizeof(ctx));

	ctx.wim = wim;
	ctx.out_fd = wim->out_fd;
	ctx.image = image;
	ctx.blob_table = blob_table;
	INIT_LIST_HEAD(&ctx.blob_table_list);
	INIT_LIST_HEAD(&ctx.blobs_being_compressed);
	INIT_LIST_HEAD(&ctx.blobs_in_solid_resource);
	ctx.max_part_size = max_part_size;
	ctx.write_resource_flags = write_flags_to_resource_flags(write_flags);
	ctx.filter_ctx = filter_ctx;
	ctx.progress_data.progfunc = wim->progfunc;
	ctx.progress_data.progctx = progctx;
	ctx.progress_data.progress.write_streams.num_threads = ctx.compressor->num_threads;

	/*
	 * We normally sort the blobs to write by a "sequential" order that is
	 * optimized for reading.  But when using solid compression, we instead
	 * sort the blobs by file extension and file name (when applicable; and
	 * we don't do this for blobs from solid resources) so that similar
	 * files are grouped together, which improves the compression ratio.
	 * This is somewhat of a hack since a blob does not necessarily
	 * correspond one-to-one with a filename, nor is there any guarantee
	 * that two files with similar names or extensions are actually similar
	 * in content.  A potential TODO is to sort the blobs based on some
	 * measure of similarity of their actual contents.
	 */

	ret = sort_blob_list_by_sequential_order(file_blob_list,
						 offsetof(struct blob_descriptor,
							  write_blobs_list));
	if (ret)
		return ret;

	ret = tally_blob_list_stats(metadata_blob_list, &ctx);
	if (ret)
		return ret;

	ret = tally_blob_list_stats(file_blob_list, &ctx);
	if (ret)
		return ret;

	if (write_resource_flags & WRITE_RESOURCE_FLAG_SOLID_SORT) {
		ret = sort_blob_list_for_solid_compression(file_blob_list);
		if (unlikely(ret))
			WARNING("Failed to sort blobs for solid compression. Continuing anyways.");
	}

	/* If needed, set auxiliary information so that we can detect when the
	 * library has finished using each external file.  */
	if (unlikely(write_resource_flags & WRITE_RESOURCE_FLAG_SEND_DONE_WITH_FILE))
		init_done_with_file_info(file_blob_list);

	num_nonraw_bytes = find_raw_copy_blobs(file_blob_list, write_resource_flags,
					       out_ctype, out_chunk_size,
					       &raw_copy_blobs);

	/* Copy any compressed resources for which the raw data can be reused
	 * without decompression.  */
	ret = write_raw_copy_resources(&raw_copy_blobs, ctx.out_fd,
				       blob_table_list, &ctx.progress_data);

	if (ret || num_nonraw_bytes == 0)
		goto out_destroy_context;

	ret = call_progress(wim->progfunc, WIMLIB_PROGRESS_MSG_WRITE_STREAMS,
			    &ctx.progress_data.progress, wim->progctx);
	if (ret)
		goto out_destroy_context;

	if (!list_empty(metadata_blob_list)) {
		ret = init_compressor(ctx, wim->out_compression_type, wim->out_chunk_size);
		if (ret)
			goto out_destroy_context;
		ret = read_blob_list_and_write(file_blob_list, &cbs, &ctx);
		if (ret)
			goto out_destroy_context;
	}

	if (!list_empty(file_blob_list)) {

		int ctype;
		u32 chunk_size;

		if (write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
			ctype = wim->out_compression_type;
			chunk_size = wim->out_chunk_size;
		} else {
			ctype = wim->out_solid_compression_type;
			chunk_size = wim->out_solid_chunk_size;
		}

		ret = init_compressor(ctx, ctype, chunk_size);
		if (ret)
			goto out_destroy_context;

		if (write_resource_flags & WRITE_RESOURCE_FLAG_SOLID) {
			ret = begin_write_resource(&ctx, num_nonraw_bytes);
			if (ret)
				goto out_destroy_context;
		}

		ret = read_blob_list_and_write(file_blob_list, &cbs, &ctx);
		if (ret)
			goto out_destroy_context;
	}

out_destroy_context:
	FREE(ctx.chunk_csizes);
	destroy_compressor(&ctx);
	return ret;
}

/* Write the contents of the specified buffer as a WIM resource.  */
int
write_uncompressed_resource(const void *buf,
			    size_t buf_size,
			    bool is_metadata,
			    struct filedes *out_fd,
			    struct wim_reshdr *out_reshdr,
			    int write_resource_flags)
{
	int ret;

	out_reshdr->offset_in_wim = out_fd->offset;
	out_reshdr->size_in_wim = buf_size;
	out_reshdr->uncompressed_size = buf_size;
	out_reshdr->flags = 0;
	if (is_metadata)
		out_reshdr->flags |= WIM_RESHDR_FLAG_METADATA;

	write_pwm_blob_header

	return full_write(out_fd, buf, buf_size);
}

struct blob_size_table {
	struct hlist_head *array;
	size_t num_entries;
	size_t capacity;
};

static int
init_blob_size_table(struct blob_size_table *tab, size_t capacity)
{
	tab->array = CALLOC(capacity, sizeof(tab->array[0]));
	if (tab->array == NULL)
		return WIMLIB_ERR_NOMEM;
	tab->num_entries = 0;
	tab->capacity = capacity;
	return 0;
}

static void
destroy_blob_size_table(struct blob_size_table *tab)
{
	FREE(tab->array);
}

static int
blob_size_table_insert(struct blob_descriptor *blob, void *_tab)
{
	struct blob_size_table *tab = _tab;
	size_t pos;
	struct blob_descriptor *same_size_blob;

	if (blob->is_metadata)
		return 0;

	pos = hash_u64(blob->size) % tab->capacity;
	blob->unique_size = 1;
	hlist_for_each_entry(same_size_blob, &tab->array[pos], hash_list_2) {
		if (same_size_blob->size == blob->size) {
			blob->unique_size = 0;
			same_size_blob->unique_size = 0;
			break;
		}
	}

	hlist_add_head(&blob->hash_list_2, &tab->array[pos]);
	tab->num_entries++;
	return 0;
}

struct find_blobs_ctx {
	WIMStruct *wim;
	int write_flags;
	struct list_head blob_list;
	struct blob_size_table blob_size_tab;
};

static void
reference_blob_for_write(struct blob_descriptor *blob,
			 struct list_head *blob_list, u32 nref)
{
	if (!blob->will_be_in_output_wim) {
		blob->out_refcnt = 0;
		list_add_tail(&blob->write_blobs_list, blob_list);
		blob->will_be_in_output_wim = 1;
	}
	blob->out_refcnt += nref;
}

static int
fully_reference_blob_for_write(struct blob_descriptor *blob, void *_blob_list)
{
	struct list_head *blob_list = _blob_list;
	blob->will_be_in_output_wim = 0;
	reference_blob_for_write(blob, blob_list, blob->refcnt);
	return 0;
}

static int
inode_find_blobs_to_reference(const struct wim_inode *inode,
			      const struct blob_table *table,
			      struct list_head *blob_list)
{
	wimlib_assert(inode->i_nlink > 0);

	for (unsigned i = 0; i < inode->i_num_streams; i++) {
		struct blob_descriptor *blob;
		const u8 *hash;

		blob = stream_blob(&inode->i_streams[i], table);
		if (blob) {
			reference_blob_for_write(blob, blob_list, inode->i_nlink);
		} else {
			hash = stream_hash(&inode->i_streams[i]);
			if (!is_zero_hash(hash))
				return blob_not_found_error(inode, hash);
		}
	}
	return 0;
}

static int
do_blob_set_not_in_output_wim(struct blob_descriptor *blob, void *_ignore)
{
	blob->will_be_in_output_wim = 0;
	return 0;
}

static int
image_find_blobs_to_reference(WIMStruct *wim)
{
	struct wim_image_metadata *imd;
	struct wim_inode *inode;
	struct blob_descriptor *blob;
	struct list_head *blob_list;
	int ret;

	imd = wim_get_current_image_metadata(wim);

	image_for_each_unhashed_blob(blob, imd)
		blob->will_be_in_output_wim = 0;

	blob_list = wim->private;
	image_for_each_inode(inode, imd) {
		ret = inode_find_blobs_to_reference(inode,
						    wim->blob_table,
						    blob_list);
		if (ret)
			return ret;
	}
	return 0;
}

static int
prepare_unfiltered_list_of_blobs_in_output_wim(WIMStruct *wim,
					       int image,
					       int blobs_ok,
					       struct list_head *blob_list_ret)
{
	int ret;
	int i;
	struct wim_image_metadata *imd;
	struct blob_descriptor *blob;

	INIT_LIST_HEAD(blob_list_ret);

	if (blobs_ok && (image == WIMLIB_ALL_IMAGES ||
			 (image == 1 && wim->hdr.image_count == 1)))
	{
		/* Fast case:  Assume that all blobs are being written and that
		 * the reference counts are correct.  */
		for_blob_in_table(wim->blob_table,
				  fully_reference_blob_for_write,
				  blob_list_ret);
		for (i = 0; i < wim->hdr.image_count; i++) {
			imd = wim->image_metadata[i];
			image_for_each_unhashed_blob(blob, imd)
				fully_reference_blob_for_write(blob, blob_list_ret);
		}
	} else {
		/* Slow case:  Walk through the images being written and
		 * determine the blobs referenced.  */
		for_blob_in_table(wim->blob_table,
				  do_blob_set_not_in_output_wim, NULL);
		wim->private = blob_list_ret;
		ret = for_image(wim, image, image_find_blobs_to_reference);
		if (ret)
			return ret;
	}

	/* Reference metadata resources  */
	for (i = (image == WIMLIB_ALL_IMAGES ? 1 : image);
	     i <= (image == WIMLIB_ALL_IMAGES ? wim->hdr.image_count : image);
	     i++)
	{
		imd = wim->image_metadata[i];
		blob = imd->metadata_blob;
		blob->will_be_in_output_wim = 0;
		reference_blob_for_write(blob, blob_list_ret, 1);
	}

	return 0;
}

struct insert_other_if_hard_filtered_ctx {
	struct blob_size_table *tab;
	struct filter_context *filter_ctx;
};

static int
insert_other_if_hard_filtered(struct blob_descriptor *blob, void *_ctx)
{
	struct insert_other_if_hard_filtered_ctx *ctx = _ctx;

	if (!blob->will_be_in_output_wim &&
	    blob_hard_filtered(blob, ctx->filter_ctx))
		blob_size_table_insert(blob, ctx->tab);
	return 0;
}

static int
determine_blob_size_uniquity(struct list_head *blob_list,
			     struct blob_table *table,
			     struct filter_context *filter_ctx)
{
	int ret;
	struct blob_size_table tab;
	struct blob_descriptor *blob;

	ret = init_blob_size_table(&tab, 9001);
	if (ret)
		return ret;

	if (may_hard_filter_blobs(filter_ctx)) {
		struct insert_other_if_hard_filtered_ctx ctx = {
			.tab = &tab,
			.filter_ctx = filter_ctx,
		};
		for_blob_in_table(table, insert_other_if_hard_filtered, &ctx);
	}

	list_for_each_entry(blob, blob_list, write_blobs_list)
		blob_size_table_insert(blob, &tab);

	destroy_blob_size_table(&tab);
	return 0;
}

static void
filter_blob_list_for_write(struct list_head *blob_list,
			   struct filter_context *filter_ctx)
{
	struct blob_descriptor *blob, *tmp;

	list_for_each_entry_safe(blob, tmp, blob_list, write_blobs_list) {
		int status = blob_filtered(blob, filter_ctx);

		if (status == 0) {
			/* Not filtered.  */
			continue;
		} else {
			if (status > 0) {
				/* Soft filtered.  */
			} else {
				/* Hard filtered.  */
				blob->will_be_in_output_wim = 0;
				list_del(&blob->blob_table_list);
			}
			list_del(&blob->write_blobs_list);
		}
	}
}

/*
 * prepare_blob_list_for_write() -
 *
 * Prepare the list of blobs to write for writing a WIM containing the specified
 * image(s) with the specified write flags.
 *
 * @wim
 *	The WIMStruct on whose behalf the write is occurring.
 *
 * @image
 *	Image(s) from the WIM to write; may be WIMLIB_ALL_IMAGES.
 *
 * @write_flags
 *	WIMLIB_WRITE_FLAG_* flags for the write operation:
 *
 *	STREAMS_OK:  For writes of all images, assume that all blobs in the blob
 *	table of @wim and the per-image lists of unhashed blobs should be taken
 *	as-is, and image metadata should not be searched for references.  This
 *	does not exclude filtering with APPEND and SKIP_EXTERNAL_WIMS, below.
 *
 *	APPEND:  Blobs already present in @wim shall not be returned in
 *	@blob_list_ret.
 *
 *	SKIP_EXTERNAL_WIMS:  Blobs already present in a WIM file, but not @wim,
 *	shall be returned in neither @blob_list_ret nor @blob_table_list_ret.
 *
 * @blob_list_ret
 *	List of blobs, linked by write_blobs_list, that need to be written will
 *	be returned here.
 *
 *	Note that this function assumes that unhashed blobs will be written; it
 *	does not take into account that they may become duplicates when actually
 *	hashed.
 *
 * @filter_ctx_ret
 *	A context for queries of blob filter status with blob_filtered() is
 *	returned in this location.
 *
 * In addition, @will_be_in_output_wim will be set to 1 in all blobs inserted
 * into @blob_table_list_ret and to 0 in all blobs in the blob table of @wim not
 * inserted into @blob_table_list_ret.
 *
 * Still furthermore, @unique_size will be set to 1 on all blobs in
 * @blob_list_ret that have unique size among all blobs in @blob_list_ret and
 * among all blobs in the blob table of @wim that are ineligible for being
 * written due to filtering.
 *
 * Returns 0 on success; nonzero on read error, memory allocation error, or
 * otherwise.
 */
static int
prepare_blob_list_for_write(WIMStruct *wim, int image, int write_flags,
			    struct list_head *blob_list_ret,
			    struct filter_context *filter_ctx_ret)
{
	int ret;

	filter_ctx_ret->write_flags = write_flags;
	filter_ctx_ret->wim = wim;

	ret = prepare_unfiltered_list_of_blobs_in_output_wim(
				wim,
				image,
				write_flags & WIMLIB_WRITE_FLAG_STREAMS_OK,
				blob_list_ret);
	if (ret)
		return ret;

	ret = determine_blob_size_uniquity(blob_list_ret, wim->blob_table,
					   filter_ctx_ret);
	if (ret)
		return ret;

	if (may_filter_blobs(filter_ctx_ret))
		filter_blob_list_for_write(blob_list_ret, filter_ctx_ret);

	return 0;
}

/*static int*/
/*prepare_metadata_resources(WIMStruct *wim, int image, int write_flags,*/
			   /*struct list_head *blob_list)*/
/*{*/
	/*int ret;*/
	/*int start_image;*/
	/*int end_image;*/
	/*int write_resource_flags;*/

	/*write_resource_flags = write_flags_to_resource_flags(write_flags);*/

	/*if (image == WIMLIB_ALL_IMAGES) {*/
		/*start_image = 1;*/
		/*end_image = wim->hdr.image_count;*/
	/*} else {*/
		/*start_image = image;*/
		/*end_image = image;*/
	/*}*/

	/*for (int i = start_image; i <= end_image; i++) {*/
		/*struct wim_image_metadata *imd;*/

		/*imd = wim->image_metadata[i - 1];*/
		/*if (!is_image_metadata_in_any_wim(imd)) {*/
			/* The image was modified from the original, or was
			 * newly added, so we have to build and write a new
			 * metadata resource.  */
			/*ret = write_metadata_resource(wim, i,*/
						      /*write_resource_flags);*/
		/*} else if (is_image_metadata_in_wim(imd, wim) &&*/
			   /*(write_flags & (WIMLIB_WRITE_FLAG_UNSAFE_COMPACT |*/
					   /*WIMLIB_WRITE_FLAG_APPEND)))*/
		/*{*/
			/* The metadata resource is already in the WIM file.
			 * For appends, we don't need to write it at all.  For
			 * compactions, we re-write existing metadata resources
			 * along with the existing file resources, not here.  */
			/*if (write_flags & WIMLIB_WRITE_FLAG_APPEND)*/
				/*blob_set_out_reshdr_for_reuse(imd->metadata_blob);*/
			/*ret = 0;*/
		/*} else {*/
			/* The metadata resource is in a WIM file other than the
			 * one being written to.  We need to rewrite it,
			 * possibly compressed differently; but rebuilding the
			 * metadata itself isn't necessary.  */
			/*ret = write_wim_resource(imd->metadata_blob,*/
						 /*&wim->out_fd,*/
						 /*wim->out_compression_type,*/
						 /*wim->out_chunk_size,*/
						 /*write_resource_flags);*/
		/*}*/
		/*if (ret)*/
			/*return ret;*/
		/*imd->metadata_blob->out_refcnt = 1;*/
	/*}*/

	/*return 0;*/
/*}*/

static int
open_wim_writable(WIMStruct *wim, const tchar *path, int open_flags)
{
	int raw_fd = topen(path, open_flags | O_BINARY, 0644);
	if (raw_fd < 0) {
		ERROR_WITH_ERRNO("Failed to open \"%"TS"\" for writing", path);
		return WIMLIB_ERR_OPEN;
	}
	filedes_init(&wim->out_fd, raw_fd);
	return 0;
}

static int
close_wim_writable(WIMStruct *wim, int write_flags)
{
	int ret = 0;

	if (!(write_flags & WIMLIB_WRITE_FLAG_FILE_DESCRIPTOR))
		if (filedes_valid(&wim->out_fd))
			if (filedes_close(&wim->out_fd))
				ret = WIMLIB_ERR_WRITE;
	filedes_invalidate(&wim->out_fd);
	return ret;
}

static int
cmp_blobs_by_out_rdesc(const void *p1, const void *p2)
{
	const struct blob_descriptor *blob1, *blob2;

	blob1 = *(const struct blob_descriptor**)p1;
	blob2 = *(const struct blob_descriptor**)p2;

	if (blob1->out_reshdr.flags & WIM_RESHDR_FLAG_SOLID) {
		if (blob2->out_reshdr.flags & WIM_RESHDR_FLAG_SOLID) {
			if (blob1->out_res_offset_in_wim != blob2->out_res_offset_in_wim)
				return cmp_u64(blob1->out_res_offset_in_wim,
					       blob2->out_res_offset_in_wim);
		} else {
			return 1;
		}
	} else {
		if (blob2->out_reshdr.flags & WIM_RESHDR_FLAG_SOLID)
			return -1;
	}
	return cmp_u64(blob1->out_reshdr.offset_in_wim,
		       blob2->out_reshdr.offset_in_wim);
}

static int
blob_add_if_in_same_wim(struct blob_descriptor *blob, void *_wim)
{
	WIMStruct *wim = _wim;
	struct list_head *blob_table_list = wim->private;

	if (blob->blob_location == BLOB_IN_WIM && blob->rdesc->wim == wim) {
		list_add(&blob->blob_table_list, blob_table_list);
		blob_set_out_reshdr_for_reuse(blob);
	}
	return 0;
}

static int
write_blob_table(WIMStruct *wim, int write_flags,
		 struct list_head *blob_table_list)
{
	int ret;

	/* If doing an append, add and prepare blob descriptors for existing
	 * blobs in the WIM file.  */
	if (write_flags & WIMLIB_WRITE_FLAG_APPEND) {
		wim->private = blob_table_list;
		for_blob_in_table(wim->blob_table, blob_add_if_in_same_wim, wim);
	}

	ret = sort_blob_list(blob_table_list,
			     offsetof(struct blob_descriptor, blob_table_list),
			     cmp_blobs_by_out_rdesc);
	if (ret)
		return ret;

	return write_blob_table_from_blob_list(blob_table_list,
					       &wim->out_fd,
					       wim->out_hdr.part_number,
					       &wim->out_hdr.blob_table_reshdr,
					       write_flags_to_resource_flags(write_flags));
}

/*
 * Finish writing a WIM file: write the blob table, xml data, and integrity
 * table, then overwrite the WIM header.
 *
 * The output file descriptor is closed on success, except when writing to a
 * user-specified file descriptor (WIMLIB_WRITE_FLAG_FILE_DESCRIPTOR set).
 */
static int
finish_write(WIMStruct *wim, int image, int write_flags,
	     struct list_head *blob_table_list)
{
	int write_resource_flags;
	off_t old_blob_table_end = 0;
	struct integrity_table *old_integrity_table = NULL;
	off_t new_blob_table_end;
	u64 xml_totalbytes;
	int ret;

	write_resource_flags = write_flags_to_resource_flags(write_flags);

	/* In the WIM header, there is room for the resource entry for a
	 * metadata resource labeled as the "boot metadata".  This entry should
	 * be zeroed out if there is no bootable image (boot_idx 0).  Otherwise,
	 * it should be a copy of the resource entry for the image that is
	 * marked as bootable.  */
	if (wim->out_hdr.boot_idx == 0) {
		zero_reshdr(&wim->out_hdr.boot_metadata_reshdr);
	} else {
		copy_reshdr(&wim->out_hdr.boot_metadata_reshdr,
			    &wim->image_metadata[
				wim->out_hdr.boot_idx - 1]->metadata_blob->out_reshdr);
	}

	/* If appending to a WIM file containing an integrity table, we'd like
	 * to re-use the information in the old integrity table instead of
	 * recalculating it.  But we might overwrite the old integrity table
	 * when we expand the XML data.  Read it into memory just in case.  */
	if ((write_flags & (WIMLIB_WRITE_FLAG_APPEND |
			    WIMLIB_WRITE_FLAG_CHECK_INTEGRITY)) ==
		(WIMLIB_WRITE_FLAG_APPEND |
		 WIMLIB_WRITE_FLAG_CHECK_INTEGRITY)
	    && wim_has_integrity_table(wim))
	{
		old_blob_table_end = wim->hdr.blob_table_reshdr.offset_in_wim +
				     wim->hdr.blob_table_reshdr.size_in_wim;
		(void)read_integrity_table(wim,
					   old_blob_table_end - WIM_HEADER_DISK_SIZE,
					   &old_integrity_table);
		/* If we couldn't read the old integrity table, we can still
		 * re-calculate the full integrity table ourselves.  Hence the
		 * ignoring of the return value.  */
	}

	/* Write blob table if needed.  */
	if (!(write_flags & WIMLIB_WRITE_FLAG_NO_NEW_BLOBS)) {
		ret = write_blob_table(wim, write_flags, blob_table_list);
		if (ret) {
			free_integrity_table(old_integrity_table);
			return ret;
		}
	}

	/* Write XML data.  */
	xml_totalbytes = wim->out_fd.offset;
	if (0)//write_flags & WIMLIB_WRITE_FLAG_USE_EXISTING_TOTALBYTES)
		xml_totalbytes = WIM_TOTALBYTES_USE_EXISTING;
	ret = write_wim_xml_data(wim, image, xml_totalbytes,
				 &wim->out_hdr.xml_data_reshdr,
				 write_resource_flags);
	if (ret) {
		free_integrity_table(old_integrity_table);
		return ret;
	}

	/* Write integrity table if needed.  */
	if (write_flags & WIMLIB_WRITE_FLAG_CHECK_INTEGRITY) {
		if (write_flags & WIMLIB_WRITE_FLAG_NO_NEW_BLOBS) {
			/* The XML data we wrote may have overwritten part of
			 * the old integrity table, so while calculating the new
			 * integrity table we should temporarily update the WIM
			 * header to remove the integrity table reference.   */
			struct wim_header checkpoint_hdr;
			memcpy(&checkpoint_hdr, &wim->out_hdr, sizeof(struct wim_header));
			zero_reshdr(&checkpoint_hdr.integrity_table_reshdr);
			checkpoint_hdr.flags |= WIM_HDR_FLAG_WRITE_IN_PROGRESS;
			ret = write_wim_header(&checkpoint_hdr, &wim->out_fd, 0);
			if (ret) {
				free_integrity_table(old_integrity_table);
				return ret;
			}
		}

		new_blob_table_end = wim->out_hdr.blob_table_reshdr.offset_in_wim +
				     wim->out_hdr.blob_table_reshdr.size_in_wim;

		ret = write_integrity_table(wim,
					    new_blob_table_end,
					    old_blob_table_end,
					    old_integrity_table);
		free_integrity_table(old_integrity_table);
		if (ret)
			return ret;
	} else {
		/* No integrity table.  */
		zero_reshdr(&wim->out_hdr.integrity_table_reshdr);
	}

	/* Now that all information in the WIM header has been determined, the
	 * preliminary header written earlier can be overwritten, the header of
	 * the existing WIM file can be overwritten, or the final header can be
	 * written to the end of the pipable WIM.  */
	wim->out_hdr.flags &= ~WIM_HDR_FLAG_WRITE_IN_PROGRESS;
	if (write_flags & WIMLIB_WRITE_FLAG_PIPABLE)
		ret = write_wim_header(&wim->out_hdr, &wim->out_fd, wim->out_fd.offset);
	else
		ret = write_wim_header(&wim->out_hdr, &wim->out_fd, 0);
	if (ret)
		return ret;

	if (unlikely(write_flags & WIMLIB_WRITE_FLAG_UNSAFE_COMPACT)) {
		/* Truncate any data the compaction freed up.  */
		if (ftruncate(wim->out_fd.fd, wim->out_fd.offset)) {
			ERROR_WITH_ERRNO("Failed to truncate the output WIM file");
			return WIMLIB_ERR_WRITE;
		}
	}

	/* Possibly sync file data to disk before closing.  On POSIX systems, it
	 * is necessary to do this before using rename() to overwrite an
	 * existing file with a new file.  Otherwise, data loss would occur if
	 * the system is abruptly terminated when the metadata for the rename
	 * operation has been written to disk, but the new file data has not.
	 */
	if (write_flags & WIMLIB_WRITE_FLAG_FSYNC) {
		if (fsync(wim->out_fd.fd)) {
			ERROR_WITH_ERRNO("Error syncing data to WIM file");
			return WIMLIB_ERR_WRITE;
		}
	}

	if (close_wim_writable(wim, write_flags)) {
		ERROR_WITH_ERRNO("Failed to close the output WIM file");
		return WIMLIB_ERR_WRITE;
	}

	return 0;
}

#if defined(HAVE_SYS_FILE_H) && defined(HAVE_FLOCK)

/* Set advisory lock on WIM file (if not already done so)  */
int
lock_wim_for_append(WIMStruct *wim)
{
	if (wim->locked_for_append)
		return 0;
	if (!flock(wim->in_fd.fd, LOCK_EX | LOCK_NB)) {
		wim->locked_for_append = 1;
		return 0;
	}
	if (errno != EWOULDBLOCK)
		return 0;
	return WIMLIB_ERR_ALREADY_LOCKED;
}

/* Remove advisory lock on WIM file (if present)  */
void
unlock_wim_for_append(WIMStruct *wim)
{
	if (wim->locked_for_append) {
		flock(wim->in_fd.fd, LOCK_UN);
		wim->locked_for_append = 0;
	}
}
#endif

/*
 * write_pipable_wim():
 *
 * Perform the intermediate stages of creating a "pipable" WIM (i.e. a WIM
 * capable of being applied from a pipe).
 *
 * Pipable WIMs are a wimlib-specific modification of the WIM format such that
 * images can be applied from them sequentially when the file data is sent over
 * a pipe.  In addition, a pipable WIM can be written sequentially to a pipe.
 * The modifications made to the WIM format for pipable WIMs are:
 *
 * - Magic characters in header are "WLPWM\0\0\0" (wimlib pipable WIM) instead
 *   of "MSWIM\0\0\0".  This lets wimlib know that the WIM is pipable and also
 *   stops other software from trying to read the file as a normal WIM.
 *
 * - The header at the beginning of the file does not contain all the normal
 *   information; in particular it will have all 0's for the blob table and XML
 *   data resource entries.  This is because this information cannot be
 *   determined until the blob table and XML data have been written.
 *   Consequently, wimlib will write the full header at the very end of the
 *   file.  The header at the end, however, is only used when reading the WIM
 *   from a seekable file (not a pipe).
 *
 * - An extra copy of the XML data is placed directly after the header.  This
 *   allows image names and sizes to be determined at an appropriate time when
 *   reading the WIM from a pipe.  This copy of the XML data is ignored if the
 *   WIM is read from a seekable file (not a pipe).
 *
 * - Solid resources are not allowed.  Each blob is always stored in its own
 *   resource.
 *
 * - The format of resources, or blobs, has been modified to allow them to be
 *   used before the "blob table" has been read.  Each blob is prefixed with a
 *   `struct pwm_blob_hdr' that is basically an abbreviated form of `struct
 *   blob_descriptor_disk' that only contains the SHA-1 message digest,
 *   uncompressed blob size, and flags that indicate whether the blob is
 *   compressed.  The data of uncompressed blobs then follows literally, while
 *   the data of compressed blobs follows in a modified format.  Compressed
 *   blobs do not begin with a chunk table, since the chunk table cannot be
 *   written until all chunks have been compressed.  Instead, each compressed
 *   chunk is prefixed by a `struct pwm_chunk_hdr' that gives its size.
 *   Furthermore, the chunk table is written at the end of the resource instead
 *   of the start.  Note: chunk offsets are given in the chunk table as if the
 *   `struct pwm_chunk_hdr's were not present; also, the chunk table is only
 *   used if the WIM is being read from a seekable file (not a pipe).
 *
 * - Metadata blobs always come before non-metadata blobs.  (This does not by
 *   itself constitute an incompatibility with normal WIMs, since this is valid
 *   in normal WIMs.)
 *
 * - At least up to the end of the blobs, all components must be packed as
 *   tightly as possible; there cannot be any "holes" in the WIM.  (This does
 *   not by itself consititute an incompatibility with normal WIMs, since this
 *   is valid in normal WIMs.)
 *
 * Note: the blob table, XML data, and header at the end are not used when
 * applying from a pipe.  They exist to support functionality such as image
 * application and export when the WIM is *not* read from a pipe.
 *
 *   Layout of pipable WIM:
 *
 * ---------+----------+--------------------+----------------+--------------+-----------+--------+
 * | Header | XML data | Metadata resources | File resources |  Blob table  | XML data  | Header |
 * ---------+----------+--------------------+----------------+--------------+-----------+--------+
 *
 *   Layout of normal WIM:
 *
 * +--------+-----------------------------+-------------------------+
 * | Header | File and metadata resources |  Blob table  | XML data |
 * +--------+-----------------------------+-------------------------+
 *
 * An optional integrity table can follow the final XML data in both normal and
 * pipable WIMs.  However, due to implementation details, wimlib currently can
 * only include an integrity table in a pipable WIM when writing it to a
 * seekable file (not a pipe).
 *
 * Do note that since pipable WIMs are not supported by Microsoft's software,
 * wimlib does not create them unless explicitly requested (with
 * WIMLIB_WRITE_FLAG_PIPABLE) and as stated above they use different magic
 * characters to identify the file.
 */

static bool
should_default_to_solid_compression(WIMStruct *wim, int write_flags)
{
	return wim->out_hdr.wim_version == WIM_VERSION_SOLID &&
		!(write_flags & (WIMLIB_WRITE_FLAG_SOLID |
				 WIMLIB_WRITE_FLAG_PIPABLE)) &&
		wim_has_solid_resources(wim);
}

struct split_context {
	union wimlib_progress_info split_progress;
	tchar *swm_name_buf;
	tchar *swm_suffix;
	size_t swm_base_name_len;
};

static int
write_wim(WIMStruct *wim, const void *path_or_fd, int image,
	  int write_flags, unsigned num_threads, u64 part_size)
{
	int ret;
	int write_resource_flags;
	struct list_head blob_list;
	struct filter_context filter_ctx;
	struct split_context ctx;

	/* A valid image (or all images) must be specified.  */
	if (image != WIMLIB_ALL_IMAGES &&
	     (image < 1 || image > wim->hdr.image_count))
		return WIMLIB_ERR_INVALID_IMAGE;

	/* Make sure the WIMStruct has the needed information attached (e.g.  is
	 * not a resource-only WIM, such as a non-first part of a split WIM). */
	if (!wim_has_metadata(wim))
		return WIMLIB_ERR_METADATA_NOT_FOUND;

	/* Check for contradictory flags.  */
	if ((write_flags & (WIMLIB_WRITE_FLAG_CHECK_INTEGRITY |
			    WIMLIB_WRITE_FLAG_NO_CHECK_INTEGRITY))
				== (WIMLIB_WRITE_FLAG_CHECK_INTEGRITY |
				    WIMLIB_WRITE_FLAG_NO_CHECK_INTEGRITY))
		return WIMLIB_ERR_INVALID_PARAM;

	if ((write_flags & (WIMLIB_WRITE_FLAG_PIPABLE |
			    WIMLIB_WRITE_FLAG_NOT_PIPABLE))
				== (WIMLIB_WRITE_FLAG_PIPABLE |
				    WIMLIB_WRITE_FLAG_NOT_PIPABLE))
		return WIMLIB_ERR_INVALID_PARAM;

	/* A split WIM can't be written to a file descriptor.  */
	if (part_size != 0 && (write_flags & WIMLIB_WRITE_FLAG_FILE_DESCRIPTOR))
		return WIMLIB_ERR_INVALID_PARAM;

	/* Only wimlib_overwrite() accepts UNSAFE_COMPACT.  */
	if (write_flags & WIMLIB_WRITE_FLAG_UNSAFE_COMPACT)
		return WIMLIB_ERR_INVALID_PARAM;

	/* Include an integrity table by default if no preference was given and
	 * the WIM already had an integrity table.  */
	if (!(write_flags & (WIMLIB_WRITE_FLAG_CHECK_INTEGRITY |
			     WIMLIB_WRITE_FLAG_NO_CHECK_INTEGRITY))) {
		if (wim_has_integrity_table(wim))
			write_flags |= WIMLIB_WRITE_FLAG_CHECK_INTEGRITY;
	}

	/* Write a pipable WIM by default if no preference was given and the WIM
	 * was already pipable.  */
	if (!(write_flags & (WIMLIB_WRITE_FLAG_PIPABLE |
			     WIMLIB_WRITE_FLAG_NOT_PIPABLE))) {
		if (wim_is_pipable(wim))
			write_flags |= WIMLIB_WRITE_FLAG_PIPABLE;
	}

	if ((write_flags & (WIMLIB_WRITE_FLAG_PIPABLE |
			    WIMLIB_WRITE_FLAG_SOLID))
				    == (WIMLIB_WRITE_FLAG_PIPABLE |
					WIMLIB_WRITE_FLAG_SOLID))
	{
		ERROR("Solid compression is unsupported in pipable WIMs");
		return WIMLIB_ERR_INVALID_PARAM;
	}

	/* Start initializing the new file header.  */
	memset(&wim->out_hdr, 0, sizeof(wim->out_hdr));

	/* Set the magic number.  */
	if (write_flags & WIMLIB_WRITE_FLAG_PIPABLE) {
		WARNING("Creating a pipable WIM, which will "
			"be incompatible\n"
			"          with Microsoft's software (WIMGAPI/ImageX/DISM).");

		/* For efficiency, when wimlib adds an image to the WIM with
		 * wimlib_add_image(), the SHA-1 message digests of files are
		 * not calculated; instead, they are calculated while the files
		 * are being written.  However, this does not work when writing
		 * a pipable WIM, since when writing a blob to a pipable WIM,
		 * its SHA-1 message digest needs to be known before the blob
		 * data is written.  Therefore, before getting much farther, we
		 * need to pre-calculate the SHA-1 message digests of all blobs
		 * that will be written.  */
		ret = wim_checksum_unhashed_blobs(wim);
		if (ret)
			return ret;

		wim->out_hdr.magic = PWM_MAGIC;
	} else {
		wim->out_hdr.magic = WIM_MAGIC;
	}

	/* Set the version number.  */
	if ((write_flags & WIMLIB_WRITE_FLAG_SOLID) ||
	    wim->out_compression_type == WIMLIB_COMPRESSION_TYPE_LZMS)
		wim->out_hdr.wim_version = WIM_VERSION_SOLID;
	else
		wim->out_hdr.wim_version = WIM_VERSION_DEFAULT;

	/* Default to solid compression if it is valid in the chosen WIM file
	 * format and the WIMStruct references any solid resources.  This is
	 * useful when exporting an image from a solid WIM.  */
	if (should_default_to_solid_compression(wim, write_flags))
		write_flags |= WIMLIB_WRITE_FLAG_SOLID;

	/* Set the header flags.  */
	wim->out_hdr.flags = (wim->hdr.flags & (WIM_HDR_FLAG_RP_FIX |
						WIM_HDR_FLAG_READONLY));
	if (wim->out_compression_type != WIMLIB_COMPRESSION_TYPE_NONE) {
		wim->out_hdr.flags |= WIM_HDR_FLAG_COMPRESSION;
		switch (wim->out_compression_type) {
		case WIMLIB_COMPRESSION_TYPE_XPRESS:
			wim->out_hdr.flags |= WIM_HDR_FLAG_COMPRESS_XPRESS;
			break;
		case WIMLIB_COMPRESSION_TYPE_LZX:
			wim->out_hdr.flags |= WIM_HDR_FLAG_COMPRESS_LZX;
			break;
		case WIMLIB_COMPRESSION_TYPE_LZMS:
			wim->out_hdr.flags |= WIM_HDR_FLAG_COMPRESS_LZMS;
			break;
		}
	}

	/* Set the chunk size.  */
	wim->out_hdr.chunk_size = wim->out_chunk_size;

	/* Set the GUID.  */
	if (write_flags & WIMLIB_WRITE_FLAG_RETAIN_GUID)
		copy_guid(wim->out_hdr.guid, wim->hdr.guid);
	else
		generate_guid(wim->out_hdr.guid);

	/* Set the image count.  */
	if (image == WIMLIB_ALL_IMAGES)
		wim->out_hdr.image_count = wim->hdr.image_count;
	else
		wim->out_hdr.image_count = 1;

	/* Set the boot index.  */
	if (image == WIMLIB_ALL_IMAGES)
		wim->out_hdr.boot_idx = wim->hdr.boot_idx;
	else if (image == wim->hdr.boot_idx)
		wim->out_hdr.boot_idx = 1;
	else
		wim->out_hdr.boot_idx = 0;

	if (part_size) {
		tchar *dot;
		size_t swm_name_len;

		memset(&split_progress, 0, sizeof(split_progress));
		split_progress.split.cur_part_number = 1;
		split_progress.split.total_bytes = 0; // TODO
		split_progress.split.completed_bytes = 0; // TODO
		split_progress.split.total_parts = 0; // TODO
		swm_name_len = tstrlen((const tchar *)path_or_fd);
		swm_name_buf = alloca((swm_name_len + 20) * sizeof(tchar));
		tstrcpy(swm_name_buf, (const tchar *)path_or_fd);
		dot = tstrchr(swm_name_buf, T('.'));
		if (dot) {
			swm_base_name_len = dot - swm_name_buf;
			swm_suffix = alloca((tstrlen(dot) + 1) * sizeof(tchar));
			tstrcpy(swm_suffix, dot);
		} else {
			swm_base_name_len = swm_name_len;
			swm_suffix = alloca(1 * sizeof(tchar));
			swm_suffix[0] = T('\0');
		}

		wim->out_hdr.flags |= WIM_HDR_FLAG_SPANNED;
	} else {
		wim->out_hdr.part_number = 1;
		wim->out_hdr.total_parts = 1;
	}

	ret = prepare_blob_list_for_write(wim, image, write_flags,
					  &blob_list, &filter_ctx);
	if (ret)
		return ret;

	write_resource_flags = write_flags_to_resource_flags(write_flags);

	for (;;) {
		/* Writing a new WIM part  */

		LIST_HEAD(blob_table_list);

		if (part_size) {
			u16 part_number = split_progress.split.cur_part_number;

			if (part_number != 1) {
				tsprintf(swm_name_buf + swm_base_name_len,
					 T("%u%"TS), part_number, swm_suffix);
			}
			split_progress.split.part_name = swm_name_buf;

			ret = call_progress(wim->progfunc,
					    WIMLIB_PROGRESS_MSG_SPLIT_BEGIN_PART,
					    &split_progress,
					    wim->progctx);
			if (ret)
				goto out_cleanup;

			wim->out_hdr.part_number = part_number;
			path_or_fd = swm_name_buf;
		}

		/* Set up the output file descriptor.  */
		if (write_flags & WIMLIB_WRITE_FLAG_FILE_DESCRIPTOR) {
			/* File descriptor was explicitly provided.  */
			int fd = *(const int *)path_or_fd;

			if (fd < 0)
				return WIMLIB_ERR_INVALID_PARAM;
			filedes_init(&wim->out_fd, fd);
			if (!filedes_is_seekable(&wim->out_fd)) {
				/* The file descriptor is a pipe.  */
				ret = WIMLIB_ERR_INVALID_PARAM;
				if (!(write_flags & WIMLIB_WRITE_FLAG_PIPABLE))
					goto out_cleanup;
				if (write_flags & WIMLIB_WRITE_FLAG_CHECK_INTEGRITY) {
					ERROR("Can't include integrity check when "
					      "writing pipable WIM to pipe!");
					goto out_cleanup;
				}
			}
		} else {
			/* Writing to an on-disk file  */
			const tchar *path = path_or_fd;
			if (!path || !*path)
				return WIMLIB_ERR_INVALID_PARAM;
			ret = open_wim_writable(wim, path,
						O_TRUNC | O_CREAT | O_RDWR);
			if (ret)
				goto out_cleanup;
		}

		/* Write the initial header.  This is merely a "dummy" header
		 * since it doesn't have resource entries filled in yet, so it
		 * will be overwritten later (unless writing a pipable WIM).  */
		if (!(write_flags & WIMLIB_WRITE_FLAG_PIPABLE))
			wim->out_hdr.flags |= WIM_HDR_FLAG_WRITE_IN_PROGRESS;
		ret = write_wim_header(&wim->out_hdr, &wim->out_fd, wim->out_fd.offset);
		wim->out_hdr.flags &= ~WIM_HDR_FLAG_WRITE_IN_PROGRESS;
		if (ret)
			goto out_cleanup;

		/* If it's a pipable WIM, write the initial XML data.  */
		if (write_flags & WIMLIB_WRITE_FLAG_PIPABLE) {
			struct wim_reshdr xml_reshdr;

			ret = write_wim_xml_data(wim, image, WIM_TOTALBYTES_OMIT,
						 &xml_reshdr, write_resource_flags);
			if (ret)
				goto out_cleanup;
		}

		/* Write blobs  */
		ret = write_blob_list(&blob_list,
				      &wim->out_fd,
				      write_resource_flags,
				      (write_resource_flags & WIMLIB_WRITE_RES
		ret = write_file_data_blobs(wim, &blob_list, &blob_table_list,
					    write_resource_flags, num_threads,
					    part_size ? part_size : UINT64_MAX,
					    &filter_ctx);
		if (ret)
			goto out_cleanup;

		/* Write blob table, XML data, and (optional) integrity table.  */
		ret = finish_write(wim, image, write_flags, &blob_table_list);
		if (ret)
			goto out_cleanup;

		if (part_size) {
			// progress.split.completed_bytes += swm_info->parts[part_number - 1].size;

			ret = call_progress(wim->progfunc,
					    WIMLIB_PROGRESS_MSG_SPLIT_END_PART,
					    &split_progress,
					    wim->progctx);
			if (ret)
				return ret;
			split_progress.split.cur_part_number++;
		}
	}

out_cleanup:
	(void)close_wim_writable(wim, write_flags);
	return ret;
}

/* API function documented in wimlib.h  */
WIMLIBAPI int
wimlib_write(WIMStruct *wim, const tchar *path, int image, int write_flags,
	     unsigned num_threads)
{
	if (write_flags & ~WIMLIB_WRITE_MASK_PUBLIC)
		return WIMLIB_ERR_INVALID_PARAM;

	return write_wim(wim, path, image, write_flags, num_threads, 0);
}

/* API function documented in wimlib.h  */
WIMLIBAPI int
wimlib_write_to_fd(WIMStruct *wim, int fd, int image, int write_flags,
		   unsigned num_threads)
{
	if (write_flags & ~WIMLIB_WRITE_MASK_PUBLIC)
		return WIMLIB_ERR_INVALID_PARAM;

	write_flags |= WIMLIB_WRITE_FLAG_FILE_DESCRIPTOR;

	return write_wim(wim, &fd, image, write_flags, num_threads, 0);
}

/* API function documented in wimlib.h  */
WIMLIBAPI int
wimlib_split(WIMStruct *wim, const tchar *swm_name, u64 part_size,
	     int write_flags)
{
	if (write_flags & ~WIMLIB_WRITE_MASK_PUBLIC)
		return WIMLIB_ERR_INVALID_PARAM;

	if (!part_size)
		return WIMLIB_ERR_INVALID_PARAM;

	write_flags |= WIMLIB_WRITE_FLAG_RETAIN_GUID;

	return write_wim(wim, swm_name, WIMLIB_ALL_IMAGES, write_flags,
			 0, part_size);
}

/* Might we need to write blobs for at least one image?  */
static bool
any_images_modified(WIMStruct *wim)
{
	for (int i = 0; i < wim->hdr.image_count; i++)
		if (!is_image_metadata_in_wim(wim->image_metadata[i], wim))
			return true;
	return false;
}

static int
check_resource_offset(struct blob_descriptor *blob, void *_wim)
{
	const WIMStruct *wim = _wim;
	off_t end_offset = *(const off_t*)wim->private;

	if (blob->blob_location == BLOB_IN_WIM &&
	    blob->rdesc->wim == wim &&
	    blob->rdesc->offset_in_wim + blob->rdesc->size_in_wim > end_offset)
		return WIMLIB_ERR_RESOURCE_ORDER;
	return 0;
}

/* Make sure no file or metadata resources are located after the XML data (or
 * integrity table if present)--- otherwise we can't safely append to the WIM
 * file and we return WIMLIB_ERR_RESOURCE_ORDER.  */
static int
check_resource_offsets(WIMStruct *wim, off_t end_offset)
{
	int ret;
	unsigned i;

	wim->private = &end_offset;
	ret = for_blob_in_table(wim->blob_table, check_resource_offset, wim);
	if (ret)
		return ret;

	for (i = 0; i < wim->hdr.image_count; i++) {
		ret = check_resource_offset(wim->image_metadata[i]->metadata_blob, wim);
		if (ret)
			return ret;
	}
	return 0;
}

/*
 * Overwrite a WIM, possibly appending new resources to it.
 *
 * A WIM looks like (or is supposed to look like) the following:
 *
 *                   Header (212 bytes)
 *                   Resources for metadata and files (variable size)
 *                   Blob table (variable size)
 *                   XML data (variable size)
 *                   Integrity table (optional) (variable size)
 *
 * If we are not adding any new files or metadata, then the blob table is
 * unchanged--- so we only need to overwrite the XML data, integrity table, and
 * header.  This operation is potentially unsafe if the program is abruptly
 * terminated while the XML data or integrity table are being overwritten, but
 * before the new header has been written.  To partially alleviate this problem,
 * we write a temporary header after the XML data has been written.  This may
 * prevent the WIM from becoming corrupted if the program is terminated while
 * the integrity table is being calculated (but no guarantees, due to write
 * re-ordering...).
 *
 * If we are adding new blobs, including new file data as well as any metadata
 * for any new images, then the blob table needs to be changed, and those blobs
 * need to be written.  In this case, we try to perform a safe update of the WIM
 * file by writing the blobs *after* the end of the previous WIM, then writing
 * the new blob table, XML data, and (optionally) integrity table following the
 * new blobs.  This will produce a layout like the following:
 *
 *                   Header (212 bytes)
 *                   (OLD) Resources for metadata and files (variable size)
 *                   (OLD) Blob table (variable size)
 *                   (OLD) XML data (variable size)
 *                   (OLD) Integrity table (optional) (variable size)
 *                   (NEW) Resources for metadata and files (variable size)
 *                   (NEW) Blob table (variable size)
 *                   (NEW) XML data (variable size)
 *                   (NEW) Integrity table (optional) (variable size)
 *
 * At all points, the WIM is valid as nothing points to the new data yet.  Then,
 * the header is overwritten to point to the new blob table, XML data, and
 * integrity table, to produce the following layout:
 *
 *                   Header (212 bytes)
 *                   Resources for metadata and files (variable size)
 *                   Nothing (variable size)
 *                   Resources for metadata and files (variable size)
 *                   Blob table (variable size)
 *                   XML data (variable size)
 *                   Integrity table (optional) (variable size)
 *
 * This function allows an image to be appended to a large WIM very quickly, and
 * is crash-safe except in the case of write re-ordering, but the disadvantage
 * is that a small hole is left in the WIM where the old blob table, xml data,
 * and integrity table were.  (These usually only take up a small amount of
 * space compared to the blobs, however.)
 *
 * Finally, this function also supports "compaction" overwrites as an
 * alternative to the normal "append" overwrites described above.  In a
 * compaction, data is written starting immediately from the end of the header.
 * All existing resources are written first, in order by file offset.  New
 * resources are written afterwards, and at the end any extra data is truncated
 * from the file.  The advantage of this approach is that is that the WIM file
 * ends up fully optimized, without any holes remaining.  The main disadavantage
 * is that this operation is fundamentally unsafe and cannot be interrupted
 * without data corruption.  Consequently, compactions are only ever done when
 * explicitly requested by the library user with the flag
 * WIMLIB_WRITE_FLAG_UNSAFE_COMPACT.  (Another disadvantage is that a compaction
 * can be much slower than an append.)
 */
static int
overwrite_wim_inplace(WIMStruct *wim, int write_flags, unsigned num_threads)
{
	int ret;
	off_t old_wim_end;
	struct list_head blob_list;
	LIST_HEAD(blob_table_list);
	struct filter_context filter_ctx;

	/* Include an integrity table by default if no preference was given and
	 * the WIM already had an integrity table.  */
	if (!(write_flags & (WIMLIB_WRITE_FLAG_CHECK_INTEGRITY |
			     WIMLIB_WRITE_FLAG_NO_CHECK_INTEGRITY)))
		if (wim_has_integrity_table(wim))
			write_flags |= WIMLIB_WRITE_FLAG_CHECK_INTEGRITY;

	/* Start preparing the updated file header.  */
	memcpy(&wim->out_hdr, &wim->hdr, sizeof(wim->out_hdr));

	/* If using solid compression, the version number must be set to
	 * WIM_VERSION_SOLID.  */
	if (write_flags & WIMLIB_WRITE_FLAG_SOLID)
		wim->out_hdr.wim_version = WIM_VERSION_SOLID;

	/* Default to solid compression if it is valid in the chosen WIM file
	 * format and the WIMStruct references any solid resources.  This is
	 * useful when updating a solid WIM.  */
	if (should_default_to_solid_compression(wim, write_flags))
		write_flags |= WIMLIB_WRITE_FLAG_SOLID;

	if (unlikely(write_flags & WIMLIB_WRITE_FLAG_UNSAFE_COMPACT)) {

		/* In-place compaction  */

		WARNING("The WIM file \"%"TS"\" is being compacted in place.\n"
			"          Do *not* interrupt the operation, or else "
			"the WIM file will be\n"
			"          corrupted!", wim->filename);
		wim->being_compacted = 1;
		old_wim_end = WIM_HEADER_DISK_SIZE;

		ret = prepare_blob_list_for_write(wim, WIMLIB_ALL_IMAGES,
						  write_flags, &blob_list,
						  &filter_ctx);
		if (ret)
			goto out;

		if (wim_has_metadata(wim)) {
			/* Add existing metadata resources to be compacted along
			 * with the file resources.  */
			for (int i = 0; i < wim->hdr.image_count; i++) {
				struct wim_image_metadata *imd = wim->image_metadata[i];
				if (is_image_metadata_in_wim(imd, wim)) {
					fully_reference_blob_for_write(imd->metadata_blob,
								       &blob_list);
				}
			}
		}
	} else {
		u64 old_blob_table_end, old_xml_begin, old_xml_end;

		/* Set additional flags for append.  */
		write_flags |= WIMLIB_WRITE_FLAG_APPEND |
			       WIMLIB_WRITE_FLAG_STREAMS_OK;

		/* Make sure there is no data after the XML data, except
		 * possibily an integrity table.  If this were the case, then
		 * this data would be overwritten.  */
		old_xml_begin = wim->hdr.xml_data_reshdr.offset_in_wim;
		old_xml_end = old_xml_begin + wim->hdr.xml_data_reshdr.size_in_wim;
		old_blob_table_end = wim->hdr.blob_table_reshdr.offset_in_wim +
				     wim->hdr.blob_table_reshdr.size_in_wim;
		if (wim_has_integrity_table(wim) &&
		    wim->hdr.integrity_table_reshdr.offset_in_wim < old_xml_end) {
			WARNING("Didn't expect the integrity table to be "
				"before the XML data");
			ret = WIMLIB_ERR_RESOURCE_ORDER;
			goto out;
		}

		if (old_blob_table_end > old_xml_begin) {
			WARNING("Didn't expect the blob table to be after "
				"the XML data");
			ret = WIMLIB_ERR_RESOURCE_ORDER;
			goto out;
		}
		/* Set @old_wim_end, which indicates the point beyond which we
		 * don't allow any file and metadata resources to appear without
		 * returning WIMLIB_ERR_RESOURCE_ORDER (due to the fact that we
		 * would otherwise overwrite these resources). */
		if (!wim->image_deletion_occurred && !any_images_modified(wim)) {
			/* If no images have been modified and no images have
			 * been deleted, a new blob table does not need to be
			 * written.  We shall write the new XML data and
			 * optional integrity table immediately after the blob
			 * table.  Note that this may overwrite an existing
			 * integrity table. */
			old_wim_end = old_blob_table_end;
			write_flags |= WIMLIB_WRITE_FLAG_NO_NEW_BLOBS;
		} else if (wim_has_integrity_table(wim)) {
			/* Old WIM has an integrity table; begin writing new
			 * blobs after it. */
			old_wim_end = wim->hdr.integrity_table_reshdr.offset_in_wim +
				      wim->hdr.integrity_table_reshdr.size_in_wim;
		} else {
			/* No existing integrity table; begin writing new blobs
			 * after the old XML data. */
			old_wim_end = old_xml_end;
		}

		ret = check_resource_offsets(wim, old_wim_end);
		if (ret)
			goto out;

		ret = prepare_blob_list_for_write(wim, WIMLIB_ALL_IMAGES,
						  write_flags, &blob_list,
						  &filter_ctx);
		if (ret)
			goto out;

		if (write_flags & WIMLIB_WRITE_FLAG_NO_NEW_BLOBS)
			wimlib_assert(list_empty(&blob_list));
	}

	ret = open_wim_writable(wim, wim->filename, O_RDWR);
	if (ret)
		goto out;

	ret = lock_wim_for_append(wim);
	if (ret)
		goto out_close_wim;

	/* Set WIM_HDR_FLAG_WRITE_IN_PROGRESS flag in header. */
	wim->hdr.flags |= WIM_HDR_FLAG_WRITE_IN_PROGRESS;
	ret = write_wim_header_flags(wim->hdr.flags, &wim->out_fd);
	wim->hdr.flags &= ~WIM_HDR_FLAG_WRITE_IN_PROGRESS;
	if (ret) {
		ERROR_WITH_ERRNO("Error updating WIM header flags");
		goto out_unlock_wim;
	}

	if (filedes_seek(&wim->out_fd, old_wim_end) == -1) {
		ERROR_WITH_ERRNO("Can't seek to end of WIM");
		ret = WIMLIB_ERR_WRITE;
		goto out_restore_hdr;
	}

	ret = write_file_data_blobs(wim, &blob_list, &blob_table_list,
				    write_flags_to_resource_flags(write_flags),
				    num_threads, UINT64_MAX, &filter_ctx);
	if (ret)
		goto out_truncate;

	ret = write_metadata_resources(wim, WIMLIB_ALL_IMAGES, write_flags,
				       &blob_table_list);
	if (ret)
		goto out_truncate;

	ret = finish_write(wim, WIMLIB_ALL_IMAGES, write_flags,
			   &blob_table_list);
	if (ret)
		goto out_truncate;

	unlock_wim_for_append(wim);
	return 0;

out_truncate:
	if (!(write_flags & (WIMLIB_WRITE_FLAG_NO_NEW_BLOBS |
			     WIMLIB_WRITE_FLAG_UNSAFE_COMPACT))) {
		WARNING("Truncating \"%"TS"\" to its original size "
			"(%"PRIu64" bytes)", wim->filename, old_wim_end);
		/* Return value of ftruncate() is ignored because this is
		 * already an error path.  */
		(void)ftruncate(wim->out_fd.fd, old_wim_end);
	}
out_restore_hdr:
	(void)write_wim_header_flags(wim->hdr.flags, &wim->out_fd);
out_unlock_wim:
	unlock_wim_for_append(wim);
out_close_wim:
	(void)close_wim_writable(wim, write_flags);
out:
	wim->being_compacted = 0;
	return ret;
}

static int
overwrite_wim_via_tmpfile(WIMStruct *wim, int write_flags, unsigned num_threads)
{
	size_t wim_name_len;
	int ret;

	/* Write the WIM to a temporary file in the same directory as the
	 * original WIM. */
	wim_name_len = tstrlen(wim->filename);
	tchar tmpfile[wim_name_len + 10];
	tmemcpy(tmpfile, wim->filename, wim_name_len);
	randomize_char_array_with_alnum(tmpfile + wim_name_len, 9);
	tmpfile[wim_name_len + 9] = T('\0');

	ret = wimlib_write(wim, tmpfile, WIMLIB_ALL_IMAGES,
			   write_flags |
				WIMLIB_WRITE_FLAG_FSYNC |
				WIMLIB_WRITE_FLAG_RETAIN_GUID,
			   num_threads);
	if (ret) {
		tunlink(tmpfile);
		return ret;
	}

	if (filedes_valid(&wim->in_fd)) {
		filedes_close(&wim->in_fd);
		filedes_invalidate(&wim->in_fd);
	}

	/* Rename the new WIM file to the original WIM file.  Note: on Windows
	 * this actually calls win32_rename_replacement(), not _wrename(), so
	 * that removing the existing destination file can be handled.  */
	ret = trename(tmpfile, wim->filename);
	if (ret) {
		ERROR_WITH_ERRNO("Failed to rename `%"TS"' to `%"TS"'",
				 tmpfile, wim->filename);
	#ifdef __WIN32__
		if (ret < 0)
	#endif
		{
			tunlink(tmpfile);
		}
		return WIMLIB_ERR_RENAME;
	}

	union wimlib_progress_info progress;
	progress.rename.from = tmpfile;
	progress.rename.to = wim->filename;
	return call_progress(wim->progfunc, WIMLIB_PROGRESS_MSG_RENAME,
			     &progress, wim->progctx);
}

/* Determine if the specified WIM file may be updated in-place rather than by
 * writing and replacing it with an entirely new file.  */
static bool
can_overwrite_wim_inplace(const WIMStruct *wim, int write_flags)
{
	/* REBUILD flag forces full rebuild.  */
	if (write_flags & WIMLIB_WRITE_FLAG_REBUILD)
		return false;

	/* Image deletions cause full rebuild by default.  */
	if (wim->image_deletion_occurred &&
	    !(write_flags & WIMLIB_WRITE_FLAG_SOFT_DELETE))
		return false;

	/* Pipable WIMs cannot be updated in place, nor can a non-pipable WIM be
	 * turned into a pipable WIM in-place.  */
	if (wim_is_pipable(wim) || (write_flags & WIMLIB_WRITE_FLAG_PIPABLE))
		return false;

	/* The default compression type and compression chunk size selected for
	 * the output WIM must be the same as those currently used for the WIM.
	 */
	if (wim->compression_type != wim->out_compression_type)
		return false;
	if (wim->chunk_size != wim->out_chunk_size)
		return false;

	return true;
}

/* API function documented in wimlib.h  */
WIMLIBAPI int
wimlib_overwrite(WIMStruct *wim, int write_flags, unsigned num_threads)
{
	int ret;
	u32 orig_hdr_flags;

	if (write_flags & ~WIMLIB_WRITE_MASK_PUBLIC)
		return WIMLIB_ERR_INVALID_PARAM;

	if (!wim->filename)
		return WIMLIB_ERR_NO_FILENAME;

	if (unlikely(write_flags & WIMLIB_WRITE_FLAG_UNSAFE_COMPACT)) {
		/*
		 * In UNSAFE_COMPACT mode:
		 *	- RECOMPRESS is forbidden
		 *	- REBUILD is ignored
		 *	- SOFT_DELETE and NO_SOLID_SORT are implied
		 */
		if (write_flags & WIMLIB_WRITE_FLAG_RECOMPRESS)
			return WIMLIB_ERR_COMPACTION_NOT_POSSIBLE;
		write_flags &= ~WIMLIB_WRITE_FLAG_REBUILD;
		write_flags |= WIMLIB_WRITE_FLAG_SOFT_DELETE;
		write_flags |= WIMLIB_WRITE_FLAG_NO_SOLID_SORT;
	}

	orig_hdr_flags = wim->hdr.flags;
	if (write_flags & WIMLIB_WRITE_FLAG_IGNORE_READONLY_FLAG)
		wim->hdr.flags &= ~WIM_HDR_FLAG_READONLY;
	ret = can_modify_wim(wim);
	wim->hdr.flags = orig_hdr_flags;
	if (ret)
		return ret;

	if (can_overwrite_wim_inplace(wim, write_flags)) {
		ret = overwrite_wim_inplace(wim, write_flags, num_threads);
		if (ret != WIMLIB_ERR_RESOURCE_ORDER)
			return ret;
		WARNING("Falling back to re-building entire WIM");
	}
	if (write_flags & WIMLIB_WRITE_FLAG_UNSAFE_COMPACT)
		return WIMLIB_ERR_COMPACTION_NOT_POSSIBLE;
	return overwrite_wim_via_tmpfile(wim, write_flags, num_threads);
}
