/*
 * wimlib.h
 *
 * External header for wimlib.
 *
 * This file contains extensive comments for generating documentation with
 * Doxygen.  The built HTML documentation can be viewed at
 * http://wimlib.sourceforge.net.
 */

/*
 * Copyright (C) 2012, 2013 Eric Biggers
 *
 * This file is part of wimlib, a library for working with WIM files.
 *
 * wimlib is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * wimlib is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wimlib; if not, see http://www.gnu.org/licenses/.
 */

/** \mainpage
 *
 * \section intro Introduction
 *
 * This is the documentation for the library interface of wimlib 1.5.0, a C
 * library for creating, modifying, extracting, and mounting files in the
 * Windows Imaging Format.  This documentation is intended for developers only.
 * If you have installed wimlib and want to know how to use the @b wimlib-imagex
 * program, please see the README file or manual pages.
 *
 * \section starting Getting Started
 *
 * wimlib uses the GNU autotools, so, on UNIX-like systems, it should be easy to
 * install with <code>configure && make && sudo make install</code>; however,
 * please see the README for more information about installing it.  To use
 * wimlib in a program after installing it, include @c wimlib.h and link your
 * program with @c -lwim.
 *
 * wimlib wraps up a WIM file in an opaque ::WIMStruct structure.  A ::WIMStruct
 * may represent either a stand-alone WIM or one part of a split WIM.
 *
 * All functions in wimlib's public API are prefixed with @c wimlib.  Most
 * return 0 on success and a positive error code on failure.  Use
 * wimlib_get_error_string() to get a string that describes an error code.
 * wimlib also can print error messages itself when an error happens, and these
 * may be more informative than the error code; to enable this, call
 * wimlib_set_print_errors().  Please note that this is for convenience only,
 * and some errors can occur without a message being printed.
 *
 * First before calling any other functions, you should call
 * wimlib_global_init() to initialize the library.
 *
 * To open an existing WIM, use wimlib_open_wim().
 *
 * To create a new WIM that initially contains no images, use
 * wimlib_create_new_wim().
 *
 * To add an image to a WIM file from a directory tree on your filesystem, call
 * wimlib_add_image().  This can be done with a ::WIMStruct gotten from
 * wimlib_open_wim() or from wimlib_create_new_wim().  On UNIX-like systems,
 * wimlib_add_image() can also capture a WIM image directly from a block device
 * containing a NTFS filesystem.
 *
 * To extract an image from a WIM file, call wimlib_extract_image().  This can
 * be done either to a directory, or, on UNIX-like systems, directly to a block
 * device containing a NTFS filesystem.
 *
 * To extract individual files or directories from a WIM image, rather than a
 * full image, call wimlib_extract_files().
 *
 * To programatically make changes to a WIM image without mounting it, call
 * wimlib_update_image().
 *
 * On UNIX-like systems supporting FUSE (such as Linux), wimlib supports
 * mounting WIM files either read-only or read-write.  Mounting is done using
 * wimlib_mount_image() and unmounting is done using wimlib_unmount_image().
 * Mounting can be done without root privileges because it is implemented using
 * FUSE (Filesystem in Userspace).  If wimlib is compiled with the
 * <code>--without-fuse</code> flag, these functions will be available but will
 * fail with ::WIMLIB_ERR_UNSUPPORTED.
 *
 * After creating or modifying a WIM file, you can write it to a file using
 * wimlib_write().  Alternatively,  if the WIM was originally read from a file
 * (using wimlib_open_wim() rather than wimlib_create_new_wim()), you can use
 * wimlib_overwrite() to overwrite the original file.  Still alternatively, you
 * can write a WIM directly to a file descriptor by calling wimlib_write_to_fd()
 * instead.
 *
 * wimlib supports a special "pipable" WIM format (which unfortunately is @b not
 * compatible with Microsoft's software).  To create a pipable WIM, call
 * wimlib_write(), wimlib_write_to_fd(), or wimlib_overwrite() with
 * ::WIMLIB_WRITE_FLAG_PIPABLE specified.  Pipable WIMs are pipable in both
 * directions, so wimlib_write_to_fd() can be used to write a pipable WIM to a
 * pipe, and wimlib_extract_image_from_pipe() can be used to apply an image from
 * a pipable WIM.  wimlib can also transparently open and operate on pipable WIM
 * s using a seekable file descriptor using the regular function calls (e.g.
 * wimlib_open_wim(), wimlib_extract_image()).
 *
 * Please note: merely by calling wimlib_add_image() or many of the other
 * functions in this library that operate on ::WIMStruct's, you are @b not
 * modifying the WIM file on disk.  Changes are not saved until you explicitly
 * call wimlib_write() or wimlib_overwrite().
 *
 * After you are done with the WIM file, use wimlib_free() to free all memory
 * associated with a ::WIMStruct and close all files associated with it.
 *
 * When you are completely done with using wimlib in your program, you should
 * call wimlib_global_cleanup().
 *
 * A number of functions take a pointer to a progress function of type
 * ::wimlib_progress_func_t.  This function will be called periodically during
 * the WIM operation(s) to report on the progress of the operation (for example,
 * how many bytes have been written so far).
 *
 * wimlib is thread-safe as long as different ::WIMStruct's are used, except for
 * the following exceptions:
 * - You must call wimlib_global_init() in one thread before calling any other
 *   functions.
 * - wimlib_set_print_errors() and wimlib_set_memory_allocator() both apply globally.
 * - wimlib_mount_image(), while it can be used to mount multiple WIMs
 *   concurrently in the same process, will daemonize the entire process when it
 *   does so for the first time.  This includes changing the working directory
 *   to the root directory.
 *
 * \section encodings Locales and character encodings
 *
 * To support Windows as well as UNIX-like systems, wimlib's API typically takes
 * and returns strings of ::wimlib_tchar, which are in a platform-dependent
 * encoding.
 *
 * On Windows, each ::wimlib_tchar is 2 bytes and is the same as a "wchar_t",
 * and the encoding is UTF-16LE.
 *
 * On UNIX-like systems, each ::wimlib_tchar is 1 byte and is simply a "char",
 * and the encoding is the locale-dependent multibyte encoding.  I recommend you
 * set your locale to a UTF-8 capable locale to avoid any issues.  Also, by
 * default, wimlib on UNIX will assume the locale is UTF-8 capable unless you
 * call wimlib_global_init() after having set your desired locale.
 *
 * \section Limitations
 *
 * This section documents some technical limitations of wimlib not already
 * documented in the man page for @b wimlib-imagex.
 *
 * - The old WIM format from Vista pre-releases is not supported.
 * - Compressed resource chunk sizes other than 32768 are not supported.  This
 *   doesn't seem to be a real problem because the chunk size always seems to be
 *   this value.
 * - wimlib does not provide a clone of the @b PEImg tool, or the @b Dism
 *   functionality other than that already present in @b ImageX, that allows you
 *   to make certain Windows-specific modifications to a Windows PE image, such
 *   as adding a driver or Windows component.  Such a tool possibly could be
 *   implemented on top of wimlib.
 */

#ifndef _WIMLIB_H
#define _WIMLIB_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>

/** Major version of the library (for example, the 1 in 1.2.5). */
#define WIMLIB_MAJOR_VERSION 1

/** Minor version of the library (for example, the 2 in 1.2.5). */
#define WIMLIB_MINOR_VERSION 5

/** Patch version of the library (for example, the 5 in 1.2.5). */
#define WIMLIB_PATCH_VERSION 0

/**
 * Opaque structure that represents a WIM file.  This is an in-memory structure
 * and need not correspond to a specific on-disk file.  However, a ::WIMStruct
 * obtained from wimlib_open_wim() depends on the underlying on-disk WIM file
 * continuing to exist so that data can be read from it as needed.
 *
 * Most functions in this library will work the same way regardless of whether a
 * given ::WIMStruct was obtained through wimlib_open_wim() or
 * wimlib_create_new_wim().  Exceptions are documented.
 *
 * Use wimlib_write() or wimlib_overwrite() to actually write an on-disk WIM
 * file from a ::WIMStruct.
 */
#ifndef WIMLIB_WIMSTRUCT_DECLARED
typedef struct WIMStruct WIMStruct;
#define WIMLIB_WIMSTRUCT_DECLARED
#endif

#ifdef __WIN32__
typedef wchar_t wimlib_tchar;
#else
/** See \ref encodings */
typedef char wimlib_tchar;
#endif

#ifdef __WIN32__
/** Path separator for WIM paths passed back to progress callbacks. */
#  define WIMLIB_WIM_PATH_SEPARATOR '\\'
#  define WIMLIB_WIM_PATH_SEPARATOR_STRING L"\\"
#else
/** Path separator for WIM paths passed back to progress callbacks. */
#  define WIMLIB_WIM_PATH_SEPARATOR '/'
#  define WIMLIB_WIM_PATH_SEPARATOR_STRING "/"
#endif

#ifdef __GNUC__
#  define _wimlib_deprecated __attribute__((deprecated))
#else
#  define _wimlib_deprecated
#endif

#define WIMLIB_GUID_LEN 16

/**
 * Specifies the compression type of a WIM file.
 */
enum wimlib_compression_type {
	/** An invalid compression type. */
	WIMLIB_COMPRESSION_TYPE_INVALID = -1,

	/** The WIM does not include any compressed resources. */
	WIMLIB_COMPRESSION_TYPE_NONE = 0,

	/** Compressed resources in the WIM use LZX compression. */
	WIMLIB_COMPRESSION_TYPE_LZX = 1,

	/** Compressed resources in the WIM use XPRESS compression. */
	WIMLIB_COMPRESSION_TYPE_XPRESS = 2,
};

/** Possible values of the first parameter to the user-supplied
 * ::wimlib_progress_func_t progress function */
enum wimlib_progress_msg {

	/** A WIM image is about to be extracted.  @p info will point to
	 * ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_IMAGE_BEGIN = 0,

	/** A file or directory tree within a WIM image (not the full image) is
	 * about to be extracted.  @p info will point to
	 * ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_TREE_BEGIN,

	/** The directory structure of the WIM image is about to be extracted.
	 * @p info will point to ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_DIR_STRUCTURE_BEGIN,

	/** The directory structure of the WIM image has been successfully
	 * extracted.  @p info will point to ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_DIR_STRUCTURE_END,

	/** The WIM image's files resources are currently being extracted.  @p
	 * info will point to ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_STREAMS,

	/** Starting to read a new part of a split pipable WIM over the pipe.
	 * @p info will point to ::wimlib_progress_info.extract.  */
	WIMLIB_PROGRESS_MSG_EXTRACT_SPWM_PART_BEGIN,

	/** All the WIM files and directories have been extracted, and
	 * timestamps are about to be applied.  @p info will point to
	 * ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_APPLY_TIMESTAMPS,

	/** A WIM image has been successfully extracted.  @p info will point to
	 * ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_IMAGE_END,

	/** A file or directory tree within a WIM image (not the full image) has
	 * been successfully extracted.  @p info will point to
	 * ::wimlib_progress_info.extract. */
	WIMLIB_PROGRESS_MSG_EXTRACT_TREE_END,

	/** The directory or NTFS volume is about to be scanned to build a tree
	 * of WIM dentries in-memory.  @p info will point to
	 * ::wimlib_progress_info.scan. */
	WIMLIB_PROGRESS_MSG_SCAN_BEGIN,

	/** A directory or file is being scanned.  @p info will point to
	 * ::wimlib_progress_info.scan, and its @p cur_path member will be
	 * valid.  This message is only sent if ::WIMLIB_ADD_FLAG_VERBOSE
	 * is passed to wimlib_add_image(). */
	WIMLIB_PROGRESS_MSG_SCAN_DENTRY,

	/** The directory or NTFS volume has been successfully scanned, and a
	 * tree of WIM dentries has been built in-memory. @p info will point to
	 * ::wimlib_progress_info.scan. */
	WIMLIB_PROGRESS_MSG_SCAN_END,

	/**
	 * File resources are currently being written to the WIM.
	 * @p info will point to ::wimlib_progress_info.write_streams. */
	WIMLIB_PROGRESS_MSG_WRITE_STREAMS,

	/**
	 * The metadata resource for each image is about to be written to the
	 * WIM. @p info will not be valid. */
	WIMLIB_PROGRESS_MSG_WRITE_METADATA_BEGIN,

	/**
	 * The metadata resource for each image has successfully been writen to
	 * the WIM.  @p info will not be valid. */
	WIMLIB_PROGRESS_MSG_WRITE_METADATA_END,

	/**
	 * The temporary file has successfully been renamed to the original WIM
	 * file.  Only happens when wimlib_overwrite() is called and the
	 * overwrite is not done in-place.
	 * @p info will point to ::wimlib_progress_info.rename. */
	WIMLIB_PROGRESS_MSG_RENAME,

	/** The contents of the WIM are being checked against the integrity
	 * table.  Only happens when wimlib_open_wim() is called with the
	 * ::WIMLIB_OPEN_FLAG_CHECK_INTEGRITY flag.  @p info will point to
	 * ::wimlib_progress_info.integrity. */
	WIMLIB_PROGRESS_MSG_VERIFY_INTEGRITY,

	/** An integrity table is being calculated for the WIM being written.
	 * Only happens when wimlib_write() or wimlib_overwrite() is called with
	 * the ::WIMLIB_WRITE_FLAG_CHECK_INTEGRITY flag.  @p info will point to
	 * ::wimlib_progress_info.integrity. */
	WIMLIB_PROGRESS_MSG_CALC_INTEGRITY,

	/** Reserved.  (Previously used for WIMLIB_PROGRESS_MSG_JOIN_STREAMS,
	 * but in wimlib v1.5.0 this was removed to simplify the code and now
	 * you'll get ::WIMLIB_PROGRESS_MSG_WRITE_STREAMS messages instead.)  */
	WIMLIB_PROGRESS_MSG_RESERVED,

	/** A wimlib_split() operation is in progress, and a new split part is
	 * about to be started.  @p info will point to
	 * ::wimlib_progress_info.split. */
	WIMLIB_PROGRESS_MSG_SPLIT_BEGIN_PART,

	/** A wimlib_split() operation is in progress, and a split part has been
	 * finished. @p info will point to ::wimlib_progress_info.split. */
	WIMLIB_PROGRESS_MSG_SPLIT_END_PART,

	/**
	 * A WIM update command is just about to be executed; @p info will point
	 * to ::wimlib_progress_info.update.
	 */
	WIMLIB_PROGRESS_MSG_UPDATE_BEGIN_COMMAND,

	/**
	 * A WIM update command has just been executed; @p info will point to
	 * ::wimlib_progress_info.update.
	 */
	WIMLIB_PROGRESS_MSG_UPDATE_END_COMMAND,

};

/** A pointer to this union is passed to the user-supplied
 * ::wimlib_progress_func_t progress function.  One (or none) of the structures
 * contained in this union will be applicable for the operation
 * (::wimlib_progress_msg) indicated in the first argument to the progress
 * function. */
union wimlib_progress_info {

	/* N.B. I wanted these to be anonymous structs, but Doxygen won't
	 * document them if they aren't given a name... */

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_WRITE_STREAMS. */
	struct wimlib_progress_info_write_streams {
		/** Number of bytes that are going to be written for all the
		 * streams combined.  This is the amount in uncompressed data.
		 * (The actual number of bytes will be less if the data is being
		 * written compressed.) */
		uint64_t total_bytes;

		/** Number of streams that are going to be written. */
		uint64_t total_streams;

		/** Number of uncompressed bytes that have been written so far.
		 * Will be 0 initially, and equal to @p total_bytes at the end.
		 * */
		uint64_t completed_bytes;

		/** Number of streams that have been written.  Will be 0
		 * initially, and equal to @p total_streams at the end. */
		uint64_t completed_streams;

		/** Number of threads that are being used to compress resources
		 * (if applicable).  */
		unsigned num_threads;

		/** The compression type being used to write the streams; either
		 * ::WIMLIB_COMPRESSION_TYPE_NONE,
		 * ::WIMLIB_COMPRESSION_TYPE_XPRESS, or
		 * ::WIMLIB_COMPRESSION_TYPE_LZX. */
		int	 compression_type;

		/** Number of split WIM parts from which streams are being
		 * written (may be 0 if irrelevant).  */
		unsigned total_parts;

		/** Number of split WIM parts from which streams have been
		 * written (may be 0 if irrelevant).  */
		unsigned completed_parts;
	} write_streams;

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_SCAN_BEGIN and
	 * ::WIMLIB_PROGRESS_MSG_SCAN_END. */
	struct wimlib_progress_info_scan {
		/** Directory or NTFS volume that is being scanned. */
		const wimlib_tchar *source;

		/** Path to the file or directory that is about to be scanned,
		 * relative to the root of the image capture or the NTFS volume.
		 * */
		const wimlib_tchar *cur_path;

		/** True iff @p cur_path is being excluded from the image
		 * capture due to the capture configuration file. */
		bool excluded;

		/** Target path in the WIM.  Only valid on messages
		 * ::WIMLIB_PROGRESS_MSG_SCAN_BEGIN and
		 * ::WIMLIB_PROGRESS_MSG_SCAN_END. */
		const wimlib_tchar *wim_target_path;
	} scan;

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_EXTRACT_IMAGE_BEGIN,
	 * ::WIMLIB_PROGRESS_MSG_EXTRACT_DIR_STRUCTURE_BEGIN,
	 * ::WIMLIB_PROGRESS_MSG_EXTRACT_DIR_STRUCTURE_END,
	 * ::WIMLIB_PROGRESS_MSG_EXTRACT_STREAMS, and
	 * ::WIMLIB_PROGRESS_MSG_EXTRACT_IMAGE_END. */
	struct wimlib_progress_info_extract {
		/** Number of the image being extracted (1-based). */
		int image;

		/** Flags passed to to wimlib_extract_image() */
		int extract_flags;

		/** Full path to the WIM file being extracted. */
		const wimlib_tchar *wimfile_name;

		/** Name of the image being extracted. */
		const wimlib_tchar *image_name;

		/** Directory or NTFS volume to which the image is being
		 * extracted. */
		const wimlib_tchar *target;

		/** Reserved.  */
		const wimlib_tchar *cur_path;

		/** Number of bytes of uncompressed data that will be extracted.
		 * Takes into account hard links (they are not counted for each
		 * link.)  */
		uint64_t total_bytes;

		/** Number of bytes that have been written so far.  Will be 0
		 * initially, and equal to @p total_bytes at the end. */
		uint64_t completed_bytes;

		/** Number of streams that will be extracted.  This may more or
		 * less than the number of "files" to be extracted due to
		 * special cases (hard links, symbolic links, and alternate data
		 * streams.) */
		uint64_t num_streams;

		/** Path to the root dentry within the WIM for the tree that is
		 * being extracted.  Will be the empty string when extracting a
		 * full image. */
		const wimlib_tchar *extract_root_wim_source_path;

		/** Currently only used for
		 * ::WIMLIB_PROGRESS_MSG_EXTRACT_SPWM_PART_BEGIN.  */

		unsigned part_number;

		/** Currently only used for
		 * ::WIMLIB_PROGRESS_MSG_EXTRACT_SPWM_PART_BEGIN.  */
		unsigned total_parts;

		/** Currently only used for
		 * ::WIMLIB_PROGRESS_MSG_EXTRACT_SPWM_PART_BEGIN.  */
		uint8_t guid[WIMLIB_GUID_LEN];
	} extract;

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_RENAME. */
	struct wimlib_progress_info_rename {
		/** Name of the temporary file that the WIM was written to. */
		const wimlib_tchar *from;

		/** Name of the original WIM file to which the temporary file is
		 * being renamed. */
		const wimlib_tchar *to;
	} rename;

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_UPDATE_BEGIN_COMMAND and
	 * ::WIMLIB_PROGRESS_MSG_UPDATE_END_COMMAND. */
	struct wimlib_progress_info_update {
		/** Pointer to the update command that will be executed or has
		 * just been executed. */
		const struct wimlib_update_command *command;

		/** Number of update commands that have been completed so far.
		 */
		size_t completed_commands;

		/** Number of update commands that are being executed as part of
		 * this call to wimlib_update_image(). */
		size_t total_commands;
	} update;

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_VERIFY_INTEGRITY and
	 * ::WIMLIB_PROGRESS_MSG_CALC_INTEGRITY. */
	struct wimlib_progress_info_integrity {
		/** Number of bytes from the end of the WIM header to the end of
		 * the lookup table (the area that is covered by the SHA1
		 * integrity checks.) */
		uint64_t total_bytes;

		/** Number of bytes that have been SHA1-summed so far.  Will be
		 * 0 initially, and equal @p total_bytes at the end. */
		uint64_t completed_bytes;

		/** Number of chunks that the checksummed region is divided
		 * into. */
		uint32_t total_chunks;

		/** Number of chunks that have been SHA1-summed so far.   Will
		 * be 0 initially, and equal to @p total_chunks at the end. */
		uint32_t completed_chunks;

		/** Size of the chunks used for the integrity calculation. */
		uint32_t chunk_size;

		/** Filename of the WIM (only valid if the message is
		 * ::WIMLIB_PROGRESS_MSG_VERIFY_INTEGRITY). */
		const wimlib_tchar *filename;
	} integrity;

	/** Valid on messages ::WIMLIB_PROGRESS_MSG_SPLIT_BEGIN_PART and
	 * ::WIMLIB_PROGRESS_MSG_SPLIT_END_PART. */
	struct wimlib_progress_info_split {
		/** Total size of the original WIM's file and metadata resources
		 * (compressed). */
		uint64_t total_bytes;

		/** Number of bytes of file and metadata resources that have
		 * been copied out of the original WIM so far.  Will be 0
		 * initially, and equal to @p total_bytes at the end. */
		uint64_t completed_bytes;

		/** Number of the split WIM part that is about to be started
		 * (::WIMLIB_PROGRESS_MSG_SPLIT_BEGIN_PART) or has just been
		 * finished (::WIMLIB_PROGRESS_MSG_SPLIT_END_PART). */
		unsigned cur_part_number;

		/** Total number of split WIM parts that are being written.  */
		unsigned total_parts;

		/** Name of the split WIM part that is about to be started
		 * (::WIMLIB_PROGRESS_MSG_SPLIT_BEGIN_PART) or has just been
		 * finished (::WIMLIB_PROGRESS_MSG_SPLIT_END_PART). */
		const wimlib_tchar *part_name;
	} split;
};

/** A user-supplied function that will be called periodically during certain WIM
 * operations.  The first argument will be the type of operation that is being
 * performed or is about to be started or has been completed.  The second
 * argument will be a pointer to one of a number of structures depending on the
 * first argument.  It may be @c NULL for some message types.
 *
 * The return value of the progress function is currently ignored, but it may do
 * something in the future.  (Set it to 0 for now.)
 */
typedef int (*wimlib_progress_func_t)(enum wimlib_progress_msg msg_type,
				      const union wimlib_progress_info *info);

/** An array of these structures is passed to wimlib_add_image_multisource() to
 * specify the sources from which to create a WIM image. */
struct wimlib_capture_source {
	/** Absolute or relative path to a file or directory on the external
	 * filesystem to be included in the WIM image. */
	wimlib_tchar *fs_source_path;

	/** Destination path in the WIM image.  Leading and trailing slashes are
	 * ignored.  The empty string or @c NULL means the root directory of the
	 * WIM image. */
	wimlib_tchar *wim_target_path;

	/** Reserved; set to 0. */
	long reserved;
};

/** Structure that specifies a list of path patterns. */
struct wimlib_pattern_list {
	/** Array of patterns.  The patterns may be modified by library code,
	 * but the @p pats pointer itself will not.  See the man page for
	 * <b>wimlib-imagex capture</b> for more information about allowed
	 * patterns. */
	wimlib_tchar **pats;

	/** Number of patterns in the @p pats array. */
	size_t num_pats;

	/** Ignored; may be used by the calling code. */
	size_t num_allocated_pats;
};

/** A structure that contains lists of wildcards that match paths to treat
 * specially when capturing a WIM image. */
struct wimlib_capture_config {
	/** Paths matching any pattern this list are excluded from being
	 * captured, except if the same path appears in @p
	 * exclusion_exception_pats. */
	struct wimlib_pattern_list exclusion_pats;

	/** Paths matching any pattern in this list are never excluded from
	 * being captured. */
	struct wimlib_pattern_list exclusion_exception_pats;

	/** Reserved for future capture configuration options. */
	struct wimlib_pattern_list reserved1;

	/** Reserved for future capture configuration options. */
	struct wimlib_pattern_list reserved2;

	/** Library internal use only. */
	wimlib_tchar *_prefix;

	/** Library internal use only. */
	size_t _prefix_num_tchars;
};

/** Set or unset the WIM header flag that marks it read-only
 * (WIM_HDR_FLAG_READONLY in Microsoft's documentation), based on the
 * ::wimlib_wim_info.is_marked_readonly member of the @p info parameter.  This
 * is distinct from basic file permissions; this flag can be set on a WIM file
 * that is physically writable.  If this flag is set, all further operations to
 * modify the WIM will fail, except calling wimlib_overwrite() with
 * ::WIMLIB_WRITE_FLAG_IGNORE_READONLY_FLAG specified, which is a loophole that
 * allows you to set this flag persistently on the underlying WIM file.
 */
#define WIMLIB_CHANGE_READONLY_FLAG		0x00000001

/** Set the GUID (globally unique identifier) of the WIM file to the value
 * specified in ::wimlib_wim_info.guid of the @p info parameter. */
#define WIMLIB_CHANGE_GUID			0x00000002

/** Change the bootable image of the WIM to the value specified in
 * ::wimlib_wim_info.boot_index of the @p info parameter.  */
#define WIMLIB_CHANGE_BOOT_INDEX		0x00000004

/** Change the WIM_HDR_FLAG_RP_FIX flag of the WIM file to the value specified
 * in ::wimlib_wim_info.has_rpfix of the @p info parameter.  This flag generally
 * indicates whether an image in the WIM has been captured with reparse-point
 * fixups enabled.  wimlib also treats this flag as specifying whether to do
 * reparse-point fixups by default when capturing or applying WIM images.  */
#define WIMLIB_CHANGE_RPFIX_FLAG		0x00000008

/** General information about a WIM file. */
struct wimlib_wim_info {

	/** Globally unique identifier for the WIM file.  Note: all parts of a
	 * split WIM should have an identical value in this field.  */
	uint8_t  guid[WIMLIB_GUID_LEN];

	/** Number of images in the WIM.  */
	uint32_t image_count;

	/** 1-based index of the bootable image in the WIM, or 0 if no image is
	 * bootable.  */
	uint32_t boot_index;

	/** Version of the WIM file.  */
	uint32_t wim_version;

	/** Chunk size used for compression.  */
	uint32_t chunk_size;

	/** 1-based index of this part within a split WIM, or 1 if the WIM is
	 * standalone.  */
	uint16_t part_number;

	/** Total number of parts in the split WIM, or 1 if the WIM is
	 * standalone.  */
	uint16_t total_parts;

	/** One of the ::wimlib_compression_type values that specifies the
	 * method used to compress resources in the WIM.  */
	int32_t  compression_type;

	/** Size of the WIM file in bytes, excluding the XML data and integrity
	 * table.  */
	uint64_t total_bytes;

	/** 1 if the WIM has an integrity table.  Note: if the ::WIMStruct was
	 * created via wimlib_create_new_wim() rather than wimlib_open_wim(),
	 * this will always be 0, even if the ::WIMStruct was written to
	 * somewhere by calling wimlib_write() with the
	 * ::WIMLIB_WRITE_FLAG_CHECK_INTEGRITY flag specified. */
	uint32_t has_integrity_table : 1;

	/** 1 if the ::WIMStruct was created via wimlib_open_wim() rather than
	 * wimlib_create_new_wim(). */
	uint32_t opened_from_file : 1;

	/** 1 if the WIM is considered readonly for any reason. */
	uint32_t is_readonly : 1;

	/** 1 if reparse-point fixups are supposedly enabled for one or more
	 * images in the WIM.  */
	uint32_t has_rpfix : 1;

	/** 1 if the WIM is marked as read-only.  */
	uint32_t is_marked_readonly : 1;

	/** 1 if the WIM is part of a spanned set.  */
	uint32_t spanned : 1;

	uint32_t write_in_progress : 1;
	uint32_t metadata_only : 1;
	uint32_t resource_only : 1;

	/** 1 if the WIM is pipable (see ::WIMLIB_WRITE_FLAG_PIPABLE).  */
	uint32_t pipable : 1;
	uint32_t reserved_flags : 22;
	uint32_t reserved[9];
};

/** Information about a unique resource in the WIM file.
 */
struct wimlib_resource_entry {
	/** Uncompressed size of the resource in bytes. */
	uint64_t uncompressed_size;

	/** Compressed size of the resource in bytes.  This will be the same as
	 * @p uncompressed_size if the resource is uncompressed.  */
	uint64_t compressed_size;

	/** Offset, in bytes, of this resource from the start of the WIM file.
	 */
	uint64_t offset;

	/** SHA1 message digest of the resource's uncompressed contents.  */
	uint8_t sha1_hash[20];

	/** Which part number of the split WIM this resource is in.  This should
	 * be the same as the part number provided by wimlib_get_wim_info().  */
	uint32_t part_number;

	/** Number of times this resource is referenced over all WIM images.  */
	uint32_t reference_count;

	/** 1 if this resource is compressed.  */
	uint32_t is_compressed : 1;

	/** 1 if this resource is a metadata resource rather than a file
	 * resource.  */
	uint32_t is_metadata : 1;

	uint32_t is_free : 1;
	uint32_t is_spanned : 1;
	uint32_t reserved_flags : 28;
	uint64_t reserved[4];
};

/** A stream of a file in the WIM.  */
struct wimlib_stream_entry {
	/** Name of the stream, or NULL if the stream is unnamed. */
	const wimlib_tchar *stream_name;
	/** Location, size, etc. of the stream within the WIM file.  */
	struct wimlib_resource_entry resource;
	uint64_t reserved[4];
};

/** Structure passed to the wimlib_iterate_dir_tree() callback function.
 * Roughly, the information about a "file" in the WIM--- but really a directory
 * entry ("dentry") because hard links are allowed.  The hard_link_group_id
 * field can be used to distinguish actual file inodes.  */
struct wimlib_dir_entry {
	/** Name of the file, or NULL if this file is unnamed (only possible for
	 * the root directory) */
	const wimlib_tchar *filename;

	/** 8.3 DOS name of this file, or NULL if this file has no such name.
	 * */
	const wimlib_tchar *dos_name;

	/** Full path to this file within the WIM image.  */
	const wimlib_tchar *full_path;

	/** Depth of this directory entry, where 0 is the root, 1 is the root's
	 * children, ..., etc. */
	size_t depth;

	/** Pointer to the security descriptor for this file, in Windows
	 * SECURITY_DESCRIPTOR_RELATIVE format, or NULL if this file has no
	 * security descriptor.  */
	const char *security_descriptor;

	/** Length of the above security descriptor.  */
	size_t security_descriptor_size;

#define WIMLIB_FILE_ATTRIBUTE_READONLY            0x00000001
#define WIMLIB_FILE_ATTRIBUTE_HIDDEN              0x00000002
#define WIMLIB_FILE_ATTRIBUTE_SYSTEM              0x00000004
#define WIMLIB_FILE_ATTRIBUTE_DIRECTORY           0x00000010
#define WIMLIB_FILE_ATTRIBUTE_ARCHIVE             0x00000020
#define WIMLIB_FILE_ATTRIBUTE_DEVICE              0x00000040
#define WIMLIB_FILE_ATTRIBUTE_NORMAL              0x00000080
#define WIMLIB_FILE_ATTRIBUTE_TEMPORARY           0x00000100
#define WIMLIB_FILE_ATTRIBUTE_SPARSE_FILE         0x00000200
#define WIMLIB_FILE_ATTRIBUTE_REPARSE_POINT       0x00000400
#define WIMLIB_FILE_ATTRIBUTE_COMPRESSED          0x00000800
#define WIMLIB_FILE_ATTRIBUTE_OFFLINE             0x00001000
#define WIMLIB_FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#define WIMLIB_FILE_ATTRIBUTE_ENCRYPTED           0x00004000
#define WIMLIB_FILE_ATTRIBUTE_VIRTUAL             0x00010000
	/** File attributes, such as whether the file is a directory or not.
	 * These are the "standard" Windows FILE_ATTRIBUTE_* values, although in
	 * wimlib.h they are defined as WIMLIB_FILE_ATTRIBUTE_* for convenience
	 * on other platforms.  */
	uint32_t attributes;

#define WIMLIB_REPARSE_TAG_RESERVED_ZERO	0x00000000
#define WIMLIB_REPARSE_TAG_RESERVED_ONE		0x00000001
#define WIMLIB_REPARSE_TAG_MOUNT_POINT		0xA0000003
#define WIMLIB_REPARSE_TAG_HSM			0xC0000004
#define WIMLIB_REPARSE_TAG_HSM2			0x80000006
#define WIMLIB_REPARSE_TAG_DRIVER_EXTENDER	0x80000005
#define WIMLIB_REPARSE_TAG_SIS			0x80000007
#define WIMLIB_REPARSE_TAG_DFS			0x8000000A
#define WIMLIB_REPARSE_TAG_DFSR			0x80000012
#define WIMLIB_REPARSE_TAG_FILTER_MANAGER	0x8000000B
#define WIMLIB_REPARSE_TAG_SYMLINK		0xA000000C
	/** If the file is a reparse point (FILE_ATTRIBUTE_DIRECTORY set in the
	 * attributes), this will give the reparse tag.  This tells you whether
	 * the reparse point is a symbolic link, junction point, or some other,
	 * more unusual kind of reparse point.  */
	uint32_t reparse_tag;

	/*  Number of (hard) links to this file.  */
	uint32_t num_links;

	/** Number of named data streams that this file has.  Normally 0.  */
	uint32_t num_named_streams;

	/** Roughly, the inode number of this file.  However, it may be 0 if
	 * @p num_links == 1.  */
	uint64_t hard_link_group_id;

	/** Time this file was created.  */
	struct timespec creation_time;

	/** Time this file was last written to.  */
	struct timespec last_write_time;

	/** Time this file was last accessed.  */
	struct timespec last_access_time;
	uint64_t reserved[16];

	/** Array of streams that make up this file.  The first entry will
	 * always exist and will correspond to the unnamed data stream (default
	 * file contents), so it will have @p stream_name == @c NULL.  There
	 * will then be @p num_named_streams additional entries that specify the
	 * named data streams, if any, each of which will have @p stream_name !=
	 * @c NULL.  */
	struct wimlib_stream_entry streams[];
};

/**
 * Type of a callback function to wimlib_iterate_dir_tree().  Must return 0 on
 * success.
 */
typedef int (*wimlib_iterate_dir_tree_callback_t)(const struct wimlib_dir_entry *dentry,
						  void *user_ctx);

/**
 * Type of a callback function to wimlib_iterate_lookup_table().  Must return 0
 * on success.
 */
typedef int (*wimlib_iterate_lookup_table_callback_t)(const struct wimlib_resource_entry *resource,
						      void *user_ctx);

/** For wimlib_iterate_dir_tree(): Iterate recursively on children rather than
 * just on the specified path. */
#define WIMLIB_ITERATE_DIR_TREE_FLAG_RECURSIVE 0x00000001

/** For wimlib_iterate_dir_tree(): Don't iterate on the file or directory
 * itself; only its children (in the case of a non-empty directory) */
#define WIMLIB_ITERATE_DIR_TREE_FLAG_CHILDREN  0x00000002



/*****************************
 * WIMLIB_ADD_FLAG_*
 *****************************/

/** Directly capture a NTFS volume rather than a generic directory.  This flag
 * cannot be combined with ::WIMLIB_ADD_FLAG_DEREFERENCE or
 * ::WIMLIB_ADD_FLAG_UNIX_DATA.   */
#define WIMLIB_ADD_FLAG_NTFS			0x00000001

/** Follow symlinks; archive and dump the files they point to.  Cannot be used
 * with ::WIMLIB_ADD_FLAG_NTFS. */
#define WIMLIB_ADD_FLAG_DEREFERENCE		0x00000002

/** Call the progress function with the message
 * ::WIMLIB_PROGRESS_MSG_SCAN_DENTRY when each directory or file is starting to
 * be scanned. */
#define WIMLIB_ADD_FLAG_VERBOSE			0x00000004

/** Mark the image being added as the bootable image of the WIM. */
#define WIMLIB_ADD_FLAG_BOOT			0x00000008

/** Store the UNIX owner, group, and mode.  This is done by adding a special
 * alternate data stream to each regular file, symbolic link, and directory to
 * contain this information.  Please note that this flag is for convenience
 * only; Microsoft's @a imagex.exe will not understand this special information.
 * This flag cannot be combined with ::WIMLIB_ADD_FLAG_NTFS.  */
#define WIMLIB_ADD_FLAG_UNIX_DATA			0x00000010

/** Do not capture security descriptors.  Only has an effect in NTFS capture
 * mode, or in Win32 native builds. */
#define WIMLIB_ADD_FLAG_NO_ACLS			0x00000020

/** Fail immediately if the full security descriptor of any file or directory
 * cannot be accessed.  Only has an effect in Win32 native builds.  The default
 * behavior without this flag is to first try omitting the SACL from the
 * security descriptor, then to try omitting the security descriptor entirely.
 * */
#define WIMLIB_ADD_FLAG_STRICT_ACLS		0x00000040

/** Call the progress function with the message
 * ::WIMLIB_PROGRESS_MSG_SCAN_DENTRY when a directory or file is excluded from
 * capture.  This is a subset of the messages provided by
 * ::WIMLIB_ADD_FLAG_VERBOSE. */
#define WIMLIB_ADD_FLAG_EXCLUDE_VERBOSE		0x00000080

/** Reparse-point fixups:  Modify absolute symbolic links (or junction points,
 * in the case of Windows) that point inside the directory being captured to
 * instead be absolute relative to the directory being captured, rather than the
 * current root; also exclude absolute symbolic links that point outside the
 * directory tree being captured.
 *
 * Without this flag, the default is to do this if WIM_HDR_FLAG_RP_FIX is set in
 * the WIM header or if this is the first image being added.
 * WIM_HDR_FLAG_RP_FIX is set if the first image in a WIM is captured with
 * reparse point fixups enabled and currently cannot be unset. */
#define WIMLIB_ADD_FLAG_RPFIX			0x00000100

/** Don't do reparse point fixups.  The default behavior is described in the
 * documentation for ::WIMLIB_ADD_FLAG_RPFIX. */
#define WIMLIB_ADD_FLAG_NORPFIX			0x00000200

#define WIMLIB_ADD_IMAGE_FLAG_NTFS		WIMLIB_ADD_FLAG_NTFS
#define WIMLIB_ADD_IMAGE_FLAG_DEREFERENCE	WIMLIB_ADD_FLAG_DEREFERENCE
#define WIMLIB_ADD_IMAGE_FLAG_VERBOSE		WIMLIB_ADD_FLAG_VERBOSE
#define WIMLIB_ADD_IMAGE_FLAG_BOOT		WIMLIB_ADD_FLAG_BOOT
#define WIMLIB_ADD_IMAGE_FLAG_UNIX_DATA		WIMLIB_ADD_FLAG_UNIX_DATA
#define WIMLIB_ADD_IMAGE_FLAG_NO_ACLS		WIMLIB_ADD_FLAG_NO_ACLS
#define WIMLIB_ADD_IMAGE_FLAG_STRICT_ACLS	WIMLIB_ADD_FLAG_STRICT_ACLS
#define WIMLIB_ADD_IMAGE_FLAG_EXCLUDE_VERBOSE	WIMLIB_ADD_FLAG_EXCLUDE_VERBOSE
#define WIMLIB_ADD_IMAGE_FLAG_RPFIX		WIMLIB_ADD_FLAG_RPFIX
#define WIMLIB_ADD_IMAGE_FLAG_NORPFIX		WIMLIB_ADD_FLAG_NORPFIX

/******************************
 * WIMLIB_DELETE_FLAG_*
 ******************************/

/** Do not issue an error if the path to delete does not exist. */
#define WIMLIB_DELETE_FLAG_FORCE			0x00000001

/** Delete the file or directory tree recursively; if not specified, an error is
 * issued if the path to delete is a directory. */
#define WIMLIB_DELETE_FLAG_RECURSIVE			0x00000002

/******************************
 * WIMLIB_EXPORT_FLAG_*
 ******************************/

/** See documentation for wimlib_export_image(). */
#define WIMLIB_EXPORT_FLAG_BOOT				0x00000001

/******************************
 * WIMLIB_EXTRACT_FLAG_*
 ******************************/

/** Extract the image directly to a NTFS volume rather than a generic directory.
 * This mode is only available if wimlib was compiled with libntfs-3g support;
 * if not, ::WIMLIB_ERR_UNSUPPORTED will be returned.  In this mode, the
 * extraction target will be interpreted as the path to a NTFS volume image (as
 * a regular file or block device) rather than a directory.  It will be opened
 * using libntfs-3g, and the image will be extracted to the NTFS filesystem's
 * root directory.  Note: this flag cannot be used when wimlib_extract_image()
 * is called with ::WIMLIB_ALL_IMAGES as the @p image.  */
#define WIMLIB_EXTRACT_FLAG_NTFS			0x00000001

/** When identical files are extracted from the WIM, always hard link them
 * together.  */
#define WIMLIB_EXTRACT_FLAG_HARDLINK			0x00000002

/** When identical files are extracted from the WIM, always symlink them
 * together.  */
#define WIMLIB_EXTRACT_FLAG_SYMLINK			0x00000004

/** This flag no longer does anything but is reserved for future use.  */
#define WIMLIB_EXTRACT_FLAG_VERBOSE			0x00000008

/** Read the WIM file sequentially while extracting the image.  */
#define WIMLIB_EXTRACT_FLAG_SEQUENTIAL			0x00000010

/** Extract special UNIX data captured with ::WIMLIB_ADD_FLAG_UNIX_DATA.
 * Cannot be used with ::WIMLIB_EXTRACT_FLAG_NTFS.  */
#define WIMLIB_EXTRACT_FLAG_UNIX_DATA			0x00000020

/** Do not extract security descriptors.  */
#define WIMLIB_EXTRACT_FLAG_NO_ACLS			0x00000040

/** Fail immediately if the full security descriptor of any file or directory
 * cannot be set exactly as specified in the WIM file.  On Windows, the default
 * behavior without this flag is to fall back to setting the security descriptor
 * with the SACL omitted, then only the default inherited security descriptor,
 * if we do not have permission to set the desired one.  */
#define WIMLIB_EXTRACT_FLAG_STRICT_ACLS			0x00000080

/** This is the extraction equivalent to ::WIMLIB_ADD_FLAG_RPFIX.  This forces
 * reparse-point fixups on, so absolute symbolic links or junction points will
 * be fixed to be absolute relative to the actual extraction root.  Reparse
 * point fixups are done by default if WIM_HDR_FLAG_RP_FIX is set in the WIM
 * header.  This flag may only be specified when extracting a full image (not a
 * file or directory tree within one).  */
#define WIMLIB_EXTRACT_FLAG_RPFIX			0x00000100

/** Force reparse-point fixups on extraction off, regardless of the state of the
 * WIM_HDR_FLAG_RP_FIX flag in the WIM header.  */
#define WIMLIB_EXTRACT_FLAG_NORPFIX			0x00000200

/** Extract the specified file to standard output.  This is only valid in an
 * extraction command that specifies the extraction of a regular file in the WIM
 * image.  */
#define WIMLIB_EXTRACT_FLAG_TO_STDOUT			0x00000400

/** Instead of ignoring files and directories with names that cannot be
 * represented on the current platform (note: Windows has more restrictions on
 * filenames than POSIX-compliant systems), try to replace characters or append
 * junk to the names so that they can be extracted in some form.  */
#define WIMLIB_EXTRACT_FLAG_REPLACE_INVALID_FILENAMES	0x00000800

/** On Windows, when there exist two or more files with the same case
 * insensitive name but different case sensitive names, try to extract them all
 * by appending junk to the end of them, rather than arbitrarily extracting only
 * one.  */
#define WIMLIB_EXTRACT_FLAG_ALL_CASE_CONFLICTS		0x00001000

/** Do not ignore failure to set timestamps on extracted files.  */
#define WIMLIB_EXTRACT_FLAG_STRICT_TIMESTAMPS		0x00002000

/** Do not ignore failure to set short names on extracted files.  */
#define WIMLIB_EXTRACT_FLAG_STRICT_SHORT_NAMES          0x00004000

/** Do not ignore failure to extract symbolic links (and junction points, on
 * Windows) due to permissions problems.  By default, such failures are ignored
 * since the default configuration of Windows only allows the Administrator to
 * create symbolic links.  */
#define WIMLIB_EXTRACT_FLAG_STRICT_SYMLINKS             0x00008000

/** TODO */
#define WIMLIB_EXTRACT_FLAG_RESUME			0x00010000


/******************************
 * WIMLIB_MOUNT_FLAG_*
 ******************************/

/** Mount the WIM image read-write rather than the default of read-only. */
#define WIMLIB_MOUNT_FLAG_READWRITE			0x00000001

/** Enable FUSE debugging by passing the @c -d flag to @c fuse_main().*/
#define WIMLIB_MOUNT_FLAG_DEBUG				0x00000002

/** Do not allow accessing alternate data streams in the mounted WIM image. */
#define WIMLIB_MOUNT_FLAG_STREAM_INTERFACE_NONE		0x00000004

/** Access alternate data streams in the mounted WIM image through extended file
 * attributes.  This is the default mode. */
#define WIMLIB_MOUNT_FLAG_STREAM_INTERFACE_XATTR	0x00000008

/** Access alternate data streams in the mounted WIM image by specifying the
 * file name, a colon, then the alternate file stream name. */
#define WIMLIB_MOUNT_FLAG_STREAM_INTERFACE_WINDOWS	0x00000010

/** Use UNIX file owners, groups, and modes if available in the WIM (see
 * ::WIMLIB_ADD_FLAG_UNIX_DATA). */
#define WIMLIB_MOUNT_FLAG_UNIX_DATA			0x00000020

/** Allow other users to see the mounted filesystem.  (this passes the @c
 * allow_other option to FUSE mount) */
#define WIMLIB_MOUNT_FLAG_ALLOW_OTHER			0x00000040

/******************************
 * WIMLIB_OPEN_FLAG_*
 ******************************/

/** Verify the WIM contents against the WIM's integrity table, if present.  This
 * causes the raw data of the WIM file, divided into 10 MB chunks, to be
 * checksummed and checked against the SHA1 message digests specified in the
 * integrity table.  ::WIMLIB_ERR_INTEGRITY is returned if there are any
 * mismatches.  */
#define WIMLIB_OPEN_FLAG_CHECK_INTEGRITY		0x00000001

/** Do not issue an error if the WIM is part of a split WIM.  */
#define WIMLIB_OPEN_FLAG_SPLIT_OK			0x00000002

/** Check if the WIM is writable and return ::WIMLIB_ERR_WIM_IS_READONLY if it
 * is not.  A WIM is considered writable only if it is writable at the
 * filesystem level, does not have the WIM_HDR_FLAG_READONLY flag set in its
 * header, and is not part of a spanned set.  It is not required to provide this
 * flag to make changes to the WIM, but with this flag you get the error sooner
 * rather than later. */
#define WIMLIB_OPEN_FLAG_WRITE_ACCESS			0x00000004

/******************************
 * WIMLIB_UNMOUNT_FLAG_*
 ******************************/

/** Include an integrity table in the WIM after it's been unmounted.  Ignored
 * for read-only mounts. */
#define WIMLIB_UNMOUNT_FLAG_CHECK_INTEGRITY		0x00000001

/** Unless this flag is given, changes to a read-write mounted WIM are
 * discarded.  Ignored for read-only mounts. */
#define WIMLIB_UNMOUNT_FLAG_COMMIT			0x00000002

/** See ::WIMLIB_WRITE_FLAG_REBUILD */
#define WIMLIB_UNMOUNT_FLAG_REBUILD			0x00000004

/** See ::WIMLIB_WRITE_FLAG_RECOMPRESS */
#define WIMLIB_UNMOUNT_FLAG_RECOMPRESS			0x00000008

/** Do a "lazy" unmount (detach filesystem immediately, even if busy) */
#define WIMLIB_UNMOUNT_FLAG_LAZY			0x00000010

/******************************
 * WIMLIB_UPDATE_FLAG_*
 ******************************/

/** Send ::WIMLIB_PROGRESS_MSG_UPDATE_BEGIN_COMMAND and
 * ::WIMLIB_PROGRESS_MSG_UPDATE_END_COMMAND messages. */
#define WIMLIB_UPDATE_FLAG_SEND_PROGRESS		0x00000001

/******************************
 * WIMLIB_WRITE_FLAG_*
 ******************************/

/** Include an integrity table in the WIM.
 *
 * For WIMs created with wimlib_open_wim(), the default behavior is to include
 * an integrity table if and only if one was present before.  For WIMs created
 * with wimlib_create_new_wim(), the default behavior is to not include an
 * integrity table.  */
#define WIMLIB_WRITE_FLAG_CHECK_INTEGRITY		0x00000001

/** Do not include an integrity table in the new WIM file.  This is the default
 * behavior, unless the WIM already included an integrity table.  */
#define WIMLIB_WRITE_FLAG_NO_CHECK_INTEGRITY		0x00000002

/** Write the WIM as "pipable".  After writing a WIM with this flag specified,
 * images from it can be applied directly from a pipe using
 * wimlib_extract_image_from_pipe().  See the documentation for the --pipable
 * flag of `wimlib-imagex capture' for more information.  Beware: WIMs written
 * with this flag will not be compatible with Microsoft's software.
 *
 * For WIMs created with wimlib_open_wim(), the default behavior is to write the
 * WIM as pipable if and only if it was pipable before.  For WIMs created with
 * wimlib_create_new_wim(), the default behavior is to write the WIM as
 * non-pipable.  */
#define WIMLIB_WRITE_FLAG_PIPABLE			0x00000004

/** Do not write the WIM as "pipable".  This is the default behavior, unless the
 * WIM was pipable already.  */
#define WIMLIB_WRITE_FLAG_NOT_PIPABLE			0x00000008

/** Recompress all resources, even if they could otherwise be copied from a
 * different WIM with the same compression type (in the case of
 * wimlib_export_image() being called previously).  This flag is also valid in
 * the @p wim_write_flags of wimlib_join(), in which case all resources included
 * in the joined WIM file will be recompressed.  */
#define WIMLIB_WRITE_FLAG_RECOMPRESS			0x00000010

/** Call fsync() just before the WIM file is closed.  */
#define WIMLIB_WRITE_FLAG_FSYNC				0x00000020

/** wimlib_overwrite() only:  Re-build the entire WIM file rather than appending
 * data to it if possible.  */
#define WIMLIB_WRITE_FLAG_REBUILD			0x00000040

/** wimlib_overwrite() only:  Specifying this flag overrides the default
 * behavior of wimlib_overwrite() after one or more calls to
 * wimlib_delete_image(), which is to rebuild the entire WIM.  With this flag,
 * only minimal changes to correctly remove the image from the WIM will be
 * taken.  In particular, all streams will be left alone, even if they are no
 * longer referenced.  This is probably not what you want, because almost no
 * space will be saved by deleting an image in this way. */
#define WIMLIB_WRITE_FLAG_SOFT_DELETE			0x00000080

/** wimlib_overwrite() only:  Allow overwriting the WIM even if the readonly
 * flag is set in the WIM header.  This can be used in combination with
 * wimlib_set_wim_info() with the ::WIMLIB_CHANGE_READONLY_FLAG flag to actually
 * set the readonly flag on the on-disk WIM file.  */
#define WIMLIB_WRITE_FLAG_IGNORE_READONLY_FLAG		0x00000100

/******************************
 * WIMLIB_INIT_FLAG_*
 ******************************/

/** Assume that strings are represented in UTF-8, even if this is not the
 * locale's character encoding.  Not used on Windows.  */
#define WIMLIB_INIT_FLAG_ASSUME_UTF8			0x00000001

/** Windows-only: do not attempt to acquire additional privileges (currently
 * SeBackupPrivilege, SeRestorePrivilege, SeSecurityPrivilege, and
 * SeTakeOwnershipPrivilege) when initializing the library.  This is intended
 * for the case where the calling program manages these privileges itself.
 * Note: no error is issued if privileges cannot be acquired, although related
 * errors may be reported later, depending on if the operations performed
 * actually require additional privileges or not.  */
#define WIMLIB_INIT_FLAG_DONT_ACQUIRE_PRIVILEGES	0x00000002

/** Specification of an update to perform on a WIM image. */
struct wimlib_update_command {

	/** The specific type of update to perform. */
	enum wimlib_update_op {
		/** Add a new file or directory tree to the WIM image in a
		 * certain location. */
		WIMLIB_UPDATE_OP_ADD = 0,

		/** Delete a file or directory tree from the WIM image. */
		WIMLIB_UPDATE_OP_DELETE,

		/** Rename a file or directory tree in the WIM image. */
		WIMLIB_UPDATE_OP_RENAME,
	} op;
	union {
		/** Data for a ::WIMLIB_UPDATE_OP_ADD operation. */
		struct wimlib_add_command {
			/** Filesystem path to the file or directory tree to
			 * add. */
			wimlib_tchar *fs_source_path;
			/** Path, specified from the root of the WIM image, at
			 * which to add the file or directory tree within the
			 * WIM image. */
			wimlib_tchar *wim_target_path;

			/** Configuration for excluded files.  @c NULL means
			 * exclude no files. */
			struct wimlib_capture_config *config;

			/** Bitwise OR of WIMLIB_ADD_FLAG_* flags. */
			int add_flags;
		} add;
		/** Data for a ::WIMLIB_UPDATE_OP_DELETE operation. */
		struct wimlib_delete_command {
			/** Path, specified from the root of the WIM image, for
			 * the file or directory tree within the WIM image to be
			 * deleted. */
			wimlib_tchar *wim_path;
			/** Bitwise OR of WIMLIB_DELETE_FLAG_* flags. */
			int delete_flags;
		} delete;
		/** Data for a ::WIMLIB_UPDATE_OP_RENAME operation. */
		struct wimlib_rename_command {
			/** Path, specified from the root of the WIM image, for
			 * the source file or directory tree within the WIM
			 * image. */
			wimlib_tchar *wim_source_path;
			/** Path, specified from the root of the WIM image, for
			 * the destination file or directory tree within the WIM
			 * image. */
			wimlib_tchar *wim_target_path;
			/** Reserved; set to 0. */
			int rename_flags;
		} rename;
	};
};

/** Specification of a file or directory tree to extract from a WIM image. */
struct wimlib_extract_command {
	/** Path to file or directory tree within the WIM image to extract.  It
	 * must be provided as an absolute path from the root of the WIM image.
	 * The path separators may be either forward slashes or backslashes. */
	wimlib_tchar *wim_source_path;

	/** Filesystem path to extract the file or directory tree to. */
	wimlib_tchar *fs_dest_path;

	/** Bitwise or of zero or more of the WIMLIB_EXTRACT_FLAG_* flags. */
	int extract_flags;
};

/**
 * Possible values of the error code returned by many functions in wimlib.
 *
 * See the documentation for each wimlib function to see specifically what error
 * codes can be returned by a given function, and what they mean.
 */
enum wimlib_error_code {
	WIMLIB_ERR_SUCCESS = 0,
	WIMLIB_ERR_ALREADY_LOCKED,
	WIMLIB_ERR_DECOMPRESSION,
	WIMLIB_ERR_DELETE_STAGING_DIR,
	WIMLIB_ERR_FILESYSTEM_DAEMON_CRASHED,
	WIMLIB_ERR_FORK,
	WIMLIB_ERR_FUSE,
	WIMLIB_ERR_FUSERMOUNT,
	WIMLIB_ERR_ICONV_NOT_AVAILABLE,
	WIMLIB_ERR_IMAGE_COUNT,
	WIMLIB_ERR_IMAGE_NAME_COLLISION,
	WIMLIB_ERR_INTEGRITY,
	WIMLIB_ERR_INVALID_CAPTURE_CONFIG,
	WIMLIB_ERR_INVALID_CHUNK_SIZE,
	WIMLIB_ERR_INVALID_COMPRESSION_TYPE,
	WIMLIB_ERR_INVALID_ENCRYPTED_STREAM,
	WIMLIB_ERR_INVALID_HEADER,
	WIMLIB_ERR_INVALID_IMAGE,
	WIMLIB_ERR_INVALID_INTEGRITY_TABLE,
	WIMLIB_ERR_INVALID_LOOKUP_TABLE_ENTRY,
	WIMLIB_ERR_INVALID_METADATA_RESOURCE,
	WIMLIB_ERR_INVALID_MULTIBYTE_STRING,
	WIMLIB_ERR_INVALID_OVERLAY,
	WIMLIB_ERR_INVALID_PARAM,
	WIMLIB_ERR_INVALID_PART_NUMBER,
	WIMLIB_ERR_INVALID_PIPABLE_WIM,
	WIMLIB_ERR_INVALID_REPARSE_DATA,
	WIMLIB_ERR_INVALID_RESOURCE_HASH,
	WIMLIB_ERR_INVALID_SECURITY_DATA,
	WIMLIB_ERR_INVALID_UNMOUNT_MESSAGE,
	WIMLIB_ERR_INVALID_UTF16_STRING,
	WIMLIB_ERR_INVALID_UTF8_STRING,
	WIMLIB_ERR_IS_DIRECTORY,
	WIMLIB_ERR_LIBXML_UTF16_HANDLER_NOT_AVAILABLE,
	WIMLIB_ERR_LINK,
	WIMLIB_ERR_MKDIR,
	WIMLIB_ERR_MQUEUE,
	WIMLIB_ERR_NOMEM,
	WIMLIB_ERR_NOTDIR,
	WIMLIB_ERR_NOTEMPTY,
	WIMLIB_ERR_NOT_A_REGULAR_FILE,
	WIMLIB_ERR_NOT_A_WIM_FILE,
	WIMLIB_ERR_NOT_PIPABLE,
	WIMLIB_ERR_NO_FILENAME,
	WIMLIB_ERR_NTFS_3G,
	WIMLIB_ERR_OPEN,
	WIMLIB_ERR_OPENDIR,
	WIMLIB_ERR_PATH_DOES_NOT_EXIST,
	WIMLIB_ERR_READ,
	WIMLIB_ERR_READLINK,
	WIMLIB_ERR_RENAME,
	WIMLIB_ERR_REOPEN,
	WIMLIB_ERR_REPARSE_POINT_FIXUP_FAILED,
	WIMLIB_ERR_RESOURCE_NOT_FOUND,
	WIMLIB_ERR_RESOURCE_ORDER,
	WIMLIB_ERR_SET_ATTRIBUTES,
	WIMLIB_ERR_SET_REPARSE_DATA,
	WIMLIB_ERR_SET_SECURITY,
	WIMLIB_ERR_SET_SHORT_NAME,
	WIMLIB_ERR_SET_TIMESTAMPS,
	WIMLIB_ERR_SPECIAL_FILE,
	WIMLIB_ERR_SPLIT_INVALID,
	WIMLIB_ERR_SPLIT_UNSUPPORTED,
	WIMLIB_ERR_STAT,
	WIMLIB_ERR_TIMEOUT,
	WIMLIB_ERR_UNEXPECTED_END_OF_FILE,
	WIMLIB_ERR_UNICODE_STRING_NOT_REPRESENTABLE,
	WIMLIB_ERR_UNKNOWN_VERSION,
	WIMLIB_ERR_UNSUPPORTED,
	WIMLIB_ERR_VOLUME_LACKS_FEATURES,
	WIMLIB_ERR_WIM_IS_READONLY,
	WIMLIB_ERR_WRITE,
	WIMLIB_ERR_XML,
};


/** Used to indicate no WIM image or an invalid WIM image. */
#define WIMLIB_NO_IMAGE		0

/** Used to specify all images in the WIM. */
#define WIMLIB_ALL_IMAGES	(-1)

/**
 * Appends an empty image to a WIM file.  This empty image will initially
 * contain no files or directories, although if written without further
 * modifications, a root directory will be created automatically for it.  After
 * calling this function, you can use wimlib_update_image() to add files to the
 * new WIM image.  This gives you slightly more control over making the new
 * image compared to calling wimlib_add_image() or
 * wimlib_add_image_multisource() directly.
 *
 * @param wim
 *	Pointer to the ::WIMStruct for the WIM file to which the image is to be
 *	added.
 * @param name
 *	Name to give the new image.  If @c NULL or empty, the new image is given
 *	no name.  If nonempty, it must specify a name that does not already
 *	exist in @p wim.
 * @param new_idx_ret
 *	If non-<code>NULL</code>, the index of the newly added image is returned
 *	in this location.
 *
 * @return 0 on success; nonzero on failure.  The possible error codes are:
 *
 * @retval ::WIMLIB_ERR_IMAGE_NAME_COLLISION
 *	There is already an image in @p wim named @p name.
 * @retval ::WIMLIB_ERR_NOMEM
 *	Failed to allocate the memory needed to add the new image.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	The WIM file is considered read-only because of any of the reasons
 *	mentioned in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS
 *	flag.
 */
extern int
wimlib_add_empty_image(WIMStruct *wim,
		       const wimlib_tchar *name,
		       int *new_idx_ret);

/**
 * Adds an image to a WIM file from an on-disk directory tree or NTFS volume.
 *
 * The directory tree or NTFS volume is scanned immediately to load the dentry
 * tree into memory, and file attributes and symbolic links are read.  However,
 * actual file data is not read until wimlib_write() or wimlib_overwrite() is
 * called.
 *
 * See the manual page for the @b wimlib-imagex program for more information
 * about the "normal" capture mode versus the NTFS capture mode (entered by
 * providing the flag ::WIMLIB_ADD_FLAG_NTFS).
 *
 * Note that @b no changes are committed to the underlying WIM file (if
 * any) until wimlib_write() or wimlib_overwrite() is called.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file to which the image will be
 * 	added.
 * @param source
 * 	A path to a directory or unmounted NTFS volume that will be captured as
 * 	a WIM image.
 * @param name
 *	Name to give the new image.  If @c NULL or empty, the new image is given
 *	no name.  If nonempty, it must specify a name that does not already
 *	exist in @p wim.
 * @param config
 * 	Capture configuration that specifies files, directories, or path globs
 * 	to exclude from being captured.  If @c NULL, a dummy configuration where
 * 	no paths are treated specially is used.
 * @param add_flags
 * 	Bitwise OR of flags prefixed with WIMLIB_ADD_FLAG.
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.  The progress messages that will be
 * 	received are ::WIMLIB_PROGRESS_MSG_SCAN_BEGIN,
 * 	::WIMLIB_PROGRESS_MSG_SCAN_END, and, if ::WIMLIB_ADD_FLAG_VERBOSE was
 * 	included in @p add_flags, also ::WIMLIB_PROGRESS_MSG_SCAN_DENTRY.
 *
 * @return 0 on success; nonzero on error.  On error, changes to @p wim are
 * discarded so that it appears to be in the same state as when this function
 * was called.
 *
 * This function is implemented by calling wimlib_add_empty_image(), then
 * calling wimlib_update_image() with a single "add" command, so any error code
 * returned by wimlib_add_empty_image() may be returned, as well as any error
 * codes returned by wimlib_update_image() other than ones documented as only
 * being returned specifically by an update involving delete or rename commands.
 */
extern int
wimlib_add_image(WIMStruct *wim,
		 const wimlib_tchar *source,
		 const wimlib_tchar *name,
		 const struct wimlib_capture_config *config,
		 int add_flags,
		 wimlib_progress_func_t progress_func);

/** This function is equivalent to wimlib_add_image() except it allows for
 * multiple sources to be combined into a single WIM image.  This is done by
 * specifying the @p sources and @p num_sources parameters instead of the @p
 * source parameter of wimlib_add_image().  The rest of the parameters are the
 * same as wimlib_add_image().  See the documentation for <b>wimlib-imagex
 * capture</b> for full details on how this mode works.
 *
 * In addition to the error codes that wimlib_add_image() can return,
 * wimlib_add_image_multisource() can return ::WIMLIB_ERR_INVALID_OVERLAY
 * when trying to overlay a non-directory on a directory or when otherwise
 * trying to overlay multiple conflicting files to the same location in the WIM
 * image.  It will also return ::WIMLIB_ERR_INVALID_PARAM if
 * ::WIMLIB_ADD_FLAG_NTFS was specified in @p add_flags but there
 * was not exactly one capture source with the target being the root directory.
 * (In this respect, there is no advantage to using
 * wimlib_add_image_multisource() instead of wimlib_add_image() when requesting
 * NTFS mode.) */
extern int
wimlib_add_image_multisource(WIMStruct *wim,
			     const struct wimlib_capture_source *sources,
			     size_t num_sources,
			     const wimlib_tchar *name,
			     const struct wimlib_capture_config *config,
			     int add_flags,
			     wimlib_progress_func_t progress_func);

/**
 * Creates a ::WIMStruct for a new WIM file.
 *
 * This only creates an in-memory structure for a WIM that initially contains no
 * images.  No on-disk file is created until wimlib_write() is called.
 *
 * @param ctype
 * 	The type of compression to be used in the new WIM file.  Must be
 * 	::WIMLIB_COMPRESSION_TYPE_NONE, ::WIMLIB_COMPRESSION_TYPE_LZX, or
 * 	::WIMLIB_COMPRESSION_TYPE_XPRESS.
 * @param wim_ret
 * 	On success, a pointer to an opaque ::WIMStruct for the new WIM file is
 * 	written to the memory location pointed to by this paramater.  The
 * 	::WIMStruct must be freed using using wimlib_free() when finished with
 * 	it.
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_INVALID_COMPRESSION_TYPE
 * 	@p ctype was not ::WIMLIB_COMPRESSION_TYPE_NONE,
 * 	::WIMLIB_COMPRESSION_TYPE_LZX, or ::WIMLIB_COMPRESSION_TYPE_XPRESS.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate needed memory.
 */
extern int
wimlib_create_new_wim(int ctype, WIMStruct **wim_ret);

/**
 * Deletes an image, or all images, from a WIM file.
 *
 * All streams referenced by the image(s) being deleted are removed from the
 * lookup table of the WIM if they are not referenced by any other images in the
 * WIM.
 *
 * Please note that @b no changes are committed to the underlying WIM file (if
 * any) until wimlib_write() or wimlib_overwrite() is called.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for the WIM file that contains the image(s)
 * 	being deleted.
 * @param image
 * 	The number of the image to delete, or ::WIMLIB_ALL_IMAGES to delete all
 * 	images.
 * @return 0 on success; nonzero on failure.  On failure, @p wim is guaranteed
 * to be left unmodified only if @p image specified a single image.  If instead
 * @p image was ::WIMLIB_ALL_IMAGES and @p wim contained more than one image, it's
 * possible for some but not all of the images to have been deleted when a
 * failure status is returned.
 *
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p image does not exist in the WIM and is not ::WIMLIB_ALL_IMAGES.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	The WIM file is considered read-only because of any of the reasons
 *	mentioned in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS
 *	flag.
 *
 * This function can additionally return ::WIMLIB_ERR_DECOMPRESSION,
 * ::WIMLIB_ERR_INVALID_METADATA_RESOURCE, ::WIMLIB_ERR_NOMEM,
 * ::WIMLIB_ERR_READ, or ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE, all of which
 * indicate failure (for different reasons) to read the metadata resource for an
 * image that needed to be deleted.
 */
extern int
wimlib_delete_image(WIMStruct *wim, int image);

/**
 * Exports an image, or all the images, from a WIM file, into another WIM file.
 *
 * The destination image is made to share the same dentry tree and security data
 * structure as the source image.  This places some restrictions on additional
 * functions that may be called.  wimlib_mount_image() may not be called on
 * either the source image or the destination image without an intervening call
 * to a function that un-shares the images, such as wimlib_free() on @p
 * dest_wim, or wimlib_delete_image() on either the source or destination image.
 * Furthermore, you may not call wimlib_free() on @p src_wim before calling
 * wimlib_write() or wimlib_overwrite() on @p dest_wim because @p dest_wim will
 * have references back to @p src_wim.
 *
 * If this function fails, all changes to @p dest_wim are rolled back.
 *
 * Please note that no changes are committed to the underlying WIM file of @p
 * dest_wim (if any) until wimlib_write() or wimlib_overwrite() is called.
 *
 * @param src_wim
 * 	Pointer to the ::WIMStruct for a stand-alone WIM or part 1 of a split
 * 	WIM that contains the image(s) being exported.
 * @param src_image
 * 	The image to export from @p src_wim.  Can be the number of an image, or
 * 	::WIMLIB_ALL_IMAGES to export all images.
 * @param dest_wim
 * 	Pointer to the ::WIMStruct for a WIM file that will receive the images
 * 	being exported.
 * @param dest_name
 * 	The name to give the exported image in the new WIM file.  If left @c
 * 	NULL, the name from @p src_wim is used.  This parameter must be left @c
 * 	NULL if @p src_image is ::WIMLIB_ALL_IMAGES and @p src_wim contains more
 * 	than one image; in that case, the names are all taken from the @p
 * 	src_wim.  (This is allowed even if one or more images being exported has
 * 	no name.)
 * @param dest_description
 * 	The description to give the exported image in the new WIM file.  If left
 * 	@c NULL, the description from the @p src_wim is used.  This parameter must
 * 	be left @c NULL if @p src_image is ::WIMLIB_ALL_IMAGES and @p src_wim contains
 * 	more than one image; in that case, the descriptions are all taken from
 * 	@p src_wim.  (This is allowed even if one or more images being exported
 * 	has no description.)
 * @param export_flags
 * 	::WIMLIB_EXPORT_FLAG_BOOT if the image being exported is to be made
 * 	bootable, or 0 if which image is marked as bootable in the destination
 * 	WIM is to be left unchanged.  If @p src_image is ::WIMLIB_ALL_IMAGES and
 * 	there are multiple images in @p src_wim, specifying
 * 	::WIMLIB_EXPORT_FLAG_BOOT is valid only if one of the exported images is
 * 	currently marked as bootable in @p src_wim; if that is the case, then
 * 	that image is marked as bootable in the destination WIM.
 * @param additional_swms
 * 	Array of pointers to the ::WIMStruct for each additional part in the
 * 	split WIM.  Ignored if @p num_additional_swms is 0.  The pointers do not
 * 	need to be in any particular order, but they must include all parts of
 * 	the split WIM other than the first part, which must be provided in the
 * 	@p wim parameter.
 * @param num_additional_swms
 * 	Number of additional WIM parts provided in the @p additional_swms array.
 * 	This number should be one less than the total number of parts in the
 * 	split WIM.  Set to 0 if the WIM is a standalone WIM.
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_IMAGE_NAME_COLLISION
 * 	One or more of the names being given to an exported image was already in
 * 	use in the destination WIM.
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p src_image does not exist in @p src_wim.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 * 	::WIMLIB_EXPORT_FLAG_BOOT was specified in @p flags, @p src_image was
 * 	::WIMLIB_ALL_IMAGES, @p src_wim contains multiple images, and no images in
 * 	@p src_wim are marked as bootable; or @p dest_name and/or @p
 * 	dest_description were non-<code>NULL</code>, @p src_image was
 * 	::WIMLIB_ALL_IMAGES, and @p src_wim contains multiple images.
 * @retval ::WIMLIB_ERR_NOMEM
 *	Failed to allocate needed memory.
 * @retval ::WIMLIB_ERR_SPLIT_INVALID
 * 	The source WIM is a split WIM, but the parts specified do not form a
 * 	complete split WIM because they do not include all the parts of the
 * 	split WIM, there are duplicate parts, or not all the parts are in fact
 * 	from the same split WIM.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	@p dest_wim is considered read-only because of any of the reasons
 *	mentioned in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS
 *	flag.
 *
 * This function can additionally return ::WIMLIB_ERR_DECOMPRESSION,
 * ::WIMLIB_ERR_INVALID_METADATA_RESOURCE, ::WIMLIB_ERR_NOMEM,
 * ::WIMLIB_ERR_READ, or ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE, all of which
 * indicate failure (for different reasons) to read the metadata resource for an
 * image in @p src_wim that needed to be exported.
 */
extern int
wimlib_export_image(WIMStruct *src_wim, int src_image,
		    WIMStruct *dest_wim,
		    const wimlib_tchar *dest_name,
		    const wimlib_tchar *dest_description,
		    int export_flags,
		    WIMStruct **additional_swms,
		    unsigned num_additional_swms,
		    wimlib_progress_func_t progress_func);

/**
 * Extract zero or more files or directory trees from a WIM image.
 *
 * This generalizes the single-image extraction functionality of
 * wimlib_extract_image() to allow extracting only the specified subsets of the
 * image.
 *
 * @param wim
 *	Pointer to the ::WIMStruct for a standalone WIM file, or part 1 of a
 *	split WIM.
 *
 * @param image
 *	The 1-based number of the image in @p wim from which the files or
 *	directory trees are to be extracted.  It cannot be ::WIMLIB_ALL_IMAGES.
 *
 * @param cmds
 *	An array of ::wimlib_extract_command structures that specifies the
 *	extractions to perform.
 *
 * @param num_cmds
 *	Number of commands in the @p cmds array.
 *
 * @param default_extract_flags
 *	Default extraction flags; the behavior shall be as if these flags had
 *	been specified in the ::wimlib_extract_command.extract_flags member in
 *	each extraction command, in combination with any flags already present.
 *
 * @param additional_swms
 * 	Array of pointers to the ::WIMStruct for each additional part in the
 * 	split WIM.  Ignored if @p num_additional_swms is 0.  The pointers do not
 * 	need to be in any particular order, but they must include all parts of
 * 	the split WIM other than the first part, which must be provided in the
 * 	@p wim parameter.
 *
 * @param num_additional_swms
 * 	Number of additional WIM parts provided in the @p additional_swms array.
 * 	This number should be one less than the total number of parts in the
 * 	split WIM.  Set to 0 if the WIM is a standalone WIM.
 *
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.
 *
 * @return 0 on success; nonzero on error.  The possible error codes include
 * most of those documented as returned by wimlib_extract_image() as well as the
 * following additional error codes:
 *
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 *	@p image was ::WIMLIB_ALL_IMAGES (or was not otherwise a valid image in
 *	the WIM file).
 * @retval ::WIMLIB_ERR_PATH_DOES_NOT_EXIST
 *	The ::wimlib_extract_command.wim_source_path member in one of the
 *	extract commands did not exist in the WIM.
 * @retval ::WIMLIB_ERR_NOT_A_REGULAR_FILE
 *	::WIMLIB_EXTRACT_FLAG_TO_STDOUT was specified for an extraction command
 *	in which ::wimlib_extract_command.wim_source_path existed but was not a
 *	regular file or directory.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 *	::WIMLIB_EXTRACT_FLAG_HARDLINK or ::WIMLIB_EXTRACT_FLAG_SYMLINK was
 *	specified for some commands but not all; or
 *	::wimlib_extract_command.fs_dest_path was @c NULL or the empty string
 *	for one or more commands; or ::WIMLIB_EXTRACT_FLAG_RPFIX was specified
 *	for a command in which ::wimlib_extract_command.wim_source_path did not
 *	specify the root directory of the WIM image.
 */
extern int
wimlib_extract_files(WIMStruct *wim,
		     int image,
		     const struct wimlib_extract_command *cmds,
		     size_t num_cmds,
		     int default_extract_flags,
		     WIMStruct **additional_swms,
		     unsigned num_additional_swms,
		     wimlib_progress_func_t progress_func);

/**
 * Extracts an image, or all images, from a standalone or split WIM file to a
 * directory or directly to a NTFS volume image.
 *
 * The exact behavior of how wimlib extracts files from a WIM image is
 * controllable by the @p extract_flags parameter, but there also are
 * differences depending on the platform (UNIX-like vs Windows).  See the manual
 * page for <b>wimlib-imagex apply</b> for more information, including about the
 * special "NTFS volume extraction mode" entered by providing
 * ::WIMLIB_EXTRACT_FLAG_NTFS.
 *
 * All extracted data is SHA1-summed, and ::WIMLIB_ERR_INVALID_RESOURCE_HASH is
 * returned if any resulting SHA1 message digests do not match the values
 * provided in the WIM file.  Therefore, if this function is successful, you can
 * be fairly sure that any compressed data in the WIM was uncompressed
 * correctly.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a standalone WIM file, or part 1 of a
 * 	split WIM.
 * @param image
 * 	The image to extract.  Can be the number of an image, or ::WIMLIB_ALL_IMAGES
 * 	to specify that all images are to be extracted.  ::WIMLIB_ALL_IMAGES cannot
 * 	be used if ::WIMLIB_EXTRACT_FLAG_NTFS is specified in @p extract_flags.
 * @param target
 * 	Directory to extract the WIM image(s) to (created if it does not already
 * 	exist); or, with ::WIMLIB_EXTRACT_FLAG_NTFS in @p extract_flags, the
 * 	path to the unmounted NTFS volume to extract the image to.
 * @param extract_flags
 * 	Bitwise OR of the flags prefixed with WIMLIB_EXTRACT_FLAG.
 * @param additional_swms
 * 	Array of pointers to the ::WIMStruct for each additional part in the
 * 	split WIM.  Ignored if @p num_additional_swms is 0.  The pointers do not
 * 	need to be in any particular order, but they must include all parts of
 * 	the split WIM other than the first part, which must be provided in the
 * 	@p wim parameter.
 * @param num_additional_swms
 * 	Number of additional WIM parts provided in the @p additional_swms array.
 * 	This number should be one less than the total number of parts in the
 * 	split WIM.  Set to 0 if the WIM is a standalone WIM.
 *
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_DECOMPRESSION
 * 	Failed to decompress a resource to be extracted, or failed to decompress
 * 	a needed metadata resource.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 * 	Both ::WIMLIB_EXTRACT_FLAG_HARDLINK and ::WIMLIB_EXTRACT_FLAG_SYMLINK
 * 	were specified in @p extract_flags; or both
 * 	::WIMLIB_EXTRACT_FLAG_STRICT_ACLS and ::WIMLIB_EXTRACT_FLAG_NO_ACLS were
 * 	specified in @p extract_flags; or both ::WIMLIB_EXTRACT_FLAG_RPFIX and
 * 	::WIMLIB_EXTRACT_FLAG_NORPFIX were specified in @p extract_flags; or
 * 	::WIMLIB_EXTRACT_FLAG_RESUME was specified in @p extract_flags; or if
 * 	::WIMLIB_EXTRACT_FLAG_NTFS was specified in @p extract_flags and
 * 	@p image was ::WIMLIB_ALL_IMAGES.
 * @retval ::WIMLIB_ERR_INVALID_METADATA_RESOURCE
 *	The metadata resource for an image being extracted was invalid.
 * @retval ::WIMLIB_ERR_INVALID_RESOURCE_HASH
 * 	The SHA1 message digest of an extracted stream did not match the SHA1
 * 	message digest given in the WIM file.
 * @retval ::WIMLIB_ERR_LINK
 * 	Failed to create a symbolic link or a hard link.
 * @retval ::WIMLIB_ERR_MKDIR
 * 	Failed create a directory.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate needed memory.
 * @retval ::WIMLIB_ERR_OPEN
 * 	Could not create a file, or failed to open an already-extracted file.
 * @retval ::WIMLIB_ERR_READ
 * 	Failed to read data from the WIM file associated with @p wim.
 * @retval ::WIMLIB_ERR_READLINK
 *	Failed to determine the target of a symbolic link in the WIM.
 * @retval ::WIMLIB_ERR_REPARSE_POINT_FIXUP_FAILED
 *	Failed to fix the target of an absolute symbolic link (e.g. if the
 *      target would have exceeded the maximum allowed length).  (Only if
 *      reparse data was supported by the extraction mode and
 *      ::WIMLIB_EXTRACT_FLAG_STRICT_SYMLINKS was specified in @p extract_flags.)
 * @retval ::WIMLIB_ERR_RESOURCE_NOT_FOUND
 *	One of the files or directories that needed to be extracted referenced a
 *	stream not present in the WIM's lookup table (or in any of the lookup
 *	tables of the split WIM	parts).
 * @retval ::WIMLIB_ERR_SET_ATTRIBUTES
 *	Failed to set attributes on a file.
 * @retval ::WIMLIB_ERR_SET_REPARSE_DATA
 *	Failed to set reparse data on a file (only if reparse data was supported
 *	by the extraction mode).
 * @retval ::WIMLIB_ERR_SET_SECURITY
 *	Failed to set security descriptor on a file
 *	(only if ::WIMLIB_EXTRACT_FLAG_STRICT_ACLS was specified in @p
 *	extract_flags).
 * @retval ::WIMLIB_ERR_SET_SHORT_NAME
 *	Failed to set the short name of a file (only if
 *	::WIMLIB_EXTRACT_FLAG_STRICT_SHORT_NAMES was specified in @p extract_flags).
 * @retval ::WIMLIB_ERR_SET_TIMESTAMPS
 *	Failed to set timestamps on a file (only if
 *	::WIMLIB_EXTRACT_FLAG_STRICT_TIMESTAMPS was specified in @p extract_flags).
 * @retval ::WIMLIB_ERR_SPLIT_INVALID
 * 	The WIM is a split WIM, but the parts specified do not form a complete
 * 	split WIM because they do not include all the parts of the original WIM,
 * 	there are duplicate parts, or not all the parts have the same GUID and
 * 	compression type.
 * @retval ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE
 * 	Unexpected end-of-file occurred when reading data from the WIM file
 * 	associated with @p wim.
 * @retval ::WIMLIB_ERR_UNSUPPORTED
 *	A requested extraction flag, or the data or metadata that must be
 *	extracted to support it, is unsupported in the build and configuration
 *	of wimlib, or on the current platform or extraction mode or target
 *	volume.  Flags affected by this include ::WIMLIB_EXTRACT_FLAG_NTFS,
 *	::WIMLIB_EXTRACT_FLAG_UNIX_DATA, ::WIMLIB_EXTRACT_FLAG_STRICT_ACLS,
 *	::WIMLIB_EXTRACT_FLAG_STRICT_SHORT_NAMES,
 *	::WIMLIB_EXTRACT_FLAG_STRICT_TIMESTAMPS,
 *	::WIMLIB_EXTRACT_FLAG_STRICT_SYMLINKS, ::WIMLIB_EXTRACT_FLAG_SYMLINK,
 *	and ::WIMLIB_EXTRACT_FLAG_HARDLINK.  For example, if
 *	::WIMLIB_EXTRACT_FLAG_STRICT_SHORT_NAMES is specified in @p
 *	extract_flags,
 *	::WIMLIB_ERR_UNSUPPORTED will be returned if the WIM image contains one
 *	or more files with short names, but extracting short names is not
 *	supported --- on Windows, this occurs if the target volume does not
 *	support short names, while on non-Windows, this occurs if
 *	::WIMLIB_EXTRACT_FLAG_NTFS was not specified in @p extract_flags.
 * @retval ::WIMLIB_ERR_WRITE
 * 	Failed to write data to a file being extracted.
 */
extern int
wimlib_extract_image(WIMStruct *wim, int image,
		     const wimlib_tchar *target,
		     int extract_flags,
		     WIMStruct **additional_swms,
		     unsigned num_additional_swms,
		     wimlib_progress_func_t progress_func);

/**
 * Since wimlib v1.5.0:  Extract one or more images from a pipe on which a
 * pipable WIM is being sent.
 *
 * See the documentation for ::WIMLIB_WRITE_FLAG_PIPABLE for more information
 * about pipable WIMs.
 *
 * This function operates in a special way to read the WIM fully sequentially.
 * As a result, there is no ::WIMStruct is made visible to library users, and
 * you cannot call wimlib_open_wim() on the pipe.  (You can, however, use
 * wimlib_open_wim() to transparently open a pipable WIM if it's available as a
 * seekable file, not a pipe.)
 *
 * @param pipe_fd
 *	File descriptor, which may be a pipe, opened for reading and positioned
 *	at the start of the pipable WIM.
 * @param image_num_or_name
 *	String that specifies the 1-based index or name of the image to extract.
 *	It is translated to an image index using the same rules that
 *	wimlib_resolve_image() uses.  However, unlike wimlib_extract_image(),
 *	only a single image (not all images) can be specified.  Alternatively,
 *	specify @p NULL here to use the first image in the WIM if it contains
 *	exactly one image but otherwise return @p WIMLIB_ERR_INVALID_IMAGE.
 * @param target
 *	Same as the corresponding parameter to wimlib_extract_image().
 * @param extract_flags
 *	Same as the corresponding parameter to wimlib_extract_image(), except
 *	for the following exceptions:  ::WIMLIB_EXTRACT_FLAG_SEQUENTIAL is
 *	always implied, since data is always read from @p pipe_fd sequentially
 *	in this mode; also, ::WIMLIB_EXTRACT_FLAG_TO_STDOUT is invalid and will
 *	result in ::WIMLIB_ERR_INVALID_PARAM being returned.
 * @param progress_func
 *	Same as the corresponding parameter to wimlib_extract_image(), except
 *	::WIMLIB_PROGRESS_MSG_EXTRACT_SPWM_PART_BEGIN messages will also be
 *	received.
 *
 * @return 0 on success; nonzero on error.  The possible error codes include
 * those returned by wimlib_extract_image() as well as the following:
 *
 * @retval ::WIMLIB_ERR_INVALID_PIPABLE_WIM
 *	Data read from the pipable WIM was invalid.
 * @retval ::WIMLIB_ERR_NOT_PIPABLE
 *	The WIM being piped in a @p pipe_fd is a normal WIM, not a pipable WIM.
 */
extern int
wimlib_extract_image_from_pipe(int pipe_fd,
			       const wimlib_tchar *image_num_or_name,
			       const wimlib_tchar *target, int extract_flags,
			       wimlib_progress_func_t progress_func);

/**
 * Extracts the XML data of a WIM file to a file stream.  Every WIM file
 * includes a string of XML that describes the images contained in the WIM.
 * This function works on standalone WIMs as well as split WIM parts.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.
 * @param fp
 * 	@c stdout, or a FILE* opened for writing, to extract the data to.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 * 	@p wim is not a ::WIMStruct that was created by wimlib_open_wim().
 * @retval ::WIMLIB_ERR_NOMEM
 * @retval ::WIMLIB_ERR_READ
 * @retval ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE
 * 	Failed to read the XML data from the WIM.
 * @retval ::WIMLIB_ERR_WRITE
 * 	Failed to completely write the XML data to @p fp.
 */
extern int
wimlib_extract_xml_data(WIMStruct *wim, FILE *fp);

/**
 * Frees all memory allocated for a WIMStruct and closes all files associated
 * with it.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.
 *
 * @return This function has no return value.
 */
extern void
wimlib_free(WIMStruct *wim);

/**
 * Converts a ::wimlib_compression_type value into a string.
 *
 * @param ctype
 * 	::WIMLIB_COMPRESSION_TYPE_NONE, ::WIMLIB_COMPRESSION_TYPE_LZX,
 * 	::WIMLIB_COMPRESSION_TYPE_XPRESS, or another value.
 *
 * @return
 * 	A statically allocated string: "None", "LZX", "XPRESS", or "Invalid",
 * 	respectively.
 */
extern const wimlib_tchar *
wimlib_get_compression_type_string(int ctype);

/**
 * Converts an error code into a string describing it.
 *
 * @param code
 * 	The error code returned by one of wimlib's functions.
 *
 * @return
 * 	Pointer to a statically allocated string describing the error code,
 * 	or @c NULL if the error code is not valid.
 */
extern const wimlib_tchar *
wimlib_get_error_string(enum wimlib_error_code code);

/**
 * Returns the description of the specified image.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.  It may be either a
 * 	standalone WIM or a split WIM part.
 * @param image
 * 	The number of the image, numbered starting at 1.
 *
 * @return
 * 	The description of the image, or @c NULL if there is no such image, or
 * 	@c NULL if the specified image has no description.  The description
 * 	string is in library-internal memory and may not be modified or freed;
 * 	in addition, the string will become invalid if the description of the
 * 	image is changed, the image is deleted, or the ::WIMStruct is destroyed.
 */
extern const wimlib_tchar *
wimlib_get_image_description(const WIMStruct *wim, int image);

/**
 * Returns the name of the specified image.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.  It may be either a
 * 	standalone WIM or a split WIM part.
 * @param image
 * 	The number of the image, numbered starting at 1.
 *
 * @return
 * 	The name of the image, or @c NULL if there is no such image, or an empty
 * 	string if the image is unnamed.  The name string is in
 * 	library-internal memory and may not be modified or freed; in addition,
 * 	the string will become invalid if the name of the image is changed, the
 * 	image is deleted, or the ::WIMStruct is destroyed.
 */
extern const wimlib_tchar *
wimlib_get_image_name(const WIMStruct *wim, int image);


/**
 * Get basic information about a WIM file.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.  It may be for either a
 * 	standalone WIM or part of a split WIM.
 * @param info
 *	A ::wimlib_wim_info structure that will be filled in with information
 *	about the WIM file.
 * @return
 *	0
 */
extern int
wimlib_get_wim_info(WIMStruct *wim, struct wimlib_wim_info *info);

/**
 * Initialization function for wimlib.  Call before using any other wimlib
 * function except wimlib_set_print_errors().  (However, currently this is not
 * strictly necessary and it will be called automatically if not done manually,
 * but you should not rely on this behavior.)
 *
 * @param init_flags
 *	Bitwise OR of ::WIMLIB_INIT_FLAG_ASSUME_UTF8 and/or
 *	::WIMLIB_INIT_FLAG_DONT_ACQUIRE_PRIVILEGES.
 *
 * @return 0; other error codes may be returned in future releases.
 */
extern int
wimlib_global_init(int init_flags);

/**
 * Cleanup function for wimlib.  You are not required to call this function, but
 * it will release any global resources allocated by the library.
 */
extern void
wimlib_global_cleanup(void);

/**
 * Determines if an image name is already used by some image in the WIM.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.
 * @param name
 * 	The name to check.
 *
 * @return
 * 	@c true if there is already an image in @p wim named @p name; @c false
 * 	if there is no image named @p name in @p wim.  If @p name is @c NULL or
 * 	the empty string, @c false is returned.
 */
extern bool
wimlib_image_name_in_use(const WIMStruct *wim, const wimlib_tchar *name);

/**
 * Iterate through a file or directory tree in the WIM image.  By specifying
 * appropriate flags and a callback function, you can get the attributes of a
 * file in the WIM, get a directory listing, or even get a listing of the entire
 * WIM image.
 *
 * @param wim
 *	Pointer to the ::WIMStruct for a standalone WIM file, or part 1 of a
 *	split WIM.
 *
 * @param image
 *	The 1-based number of the image in @p wim that contains the files or
 *	directories to iterate over, or ::WIMLIB_ALL_IMAGES to repeat the same
 *	iteration on all images in the WIM.
 *
 * @param path
 *	Path in the WIM image at which to do the iteration.
 *
 * @param flags
 *	Bitwise OR of ::WIMLIB_ITERATE_DIR_TREE_FLAG_RECURSIVE and/or
 *	::WIMLIB_ITERATE_DIR_TREE_FLAG_CHILDREN.
 *
 * @param cb
 *	A callback function that will receive each directory entry.
 *
 * @param user_ctx
 *	An extra parameter that will always be passed to the callback function
 *	@p cb.
 *
 * @return Normally, returns 0 if all calls to @p cb returned 0; otherwise the
 * first nonzero value that was returned from @p cb.  However, additional error
 * codes may be returned, including the following:
 *
 * @retval ::WIMLIB_ERR_PATH_DOES_NOT_EXIST
 *	@p path did not exist in the WIM image.
 * @retval ::WIMLIB_ERR_NOMEM
 *	Failed to allocate memory needed to create a ::wimlib_dir_entry.
 *
 * This function can additionally return ::WIMLIB_ERR_DECOMPRESSION,
 * ::WIMLIB_ERR_INVALID_METADATA_RESOURCE, ::WIMLIB_ERR_NOMEM,
 * ::WIMLIB_ERR_READ, or ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE, all of which
 * indicate failure (for different reasons) to read the metadata resource for an
 * image over which iteration needed to be done.
 */
extern int
wimlib_iterate_dir_tree(WIMStruct *wim, int image, const wimlib_tchar *path,
			int flags,
			wimlib_iterate_dir_tree_callback_t cb, void *user_ctx);

/**
 * Iterate through the lookup table of a WIM file.  This can be used to directly
 * get a listing of the unique resources contained in a WIM file.  Both file
 * resources and metadata resources are included.
 *
 * @param wim
 *	Pointer to the ::WIMStruct of a standalone WIM file or a split WIM part.
 *	Note: metadata resources will only be included if the WIM is standalone
 *	or the first part of the split WIM.
 *
 * @param flags
 *	Reserved; set to 0.
 *
 * @param cb
 *	A callback function that will receive each resource.
 *
 * @param user_ctx
 *	An extra parameter that will always be passed to the callback function
 *	@p cb.
 *
 * @return 0 if all calls to @p cb returned 0; otherwise the first nonzero value
 * that was returned from @p cb.
 */
extern int
wimlib_iterate_lookup_table(WIMStruct *wim, int flags,
			    wimlib_iterate_lookup_table_callback_t cb,
			    void *user_ctx);

/**
 * Joins a split WIM into a stand-alone one-part WIM.
 *
 * @param swms
 * 	An array of strings that gives the filenames of all parts of the split
 * 	WIM.  No specific order is required, but all parts must be included with
 * 	no duplicates.
 * @param num_swms
 * 	Number of filenames in @p swms.
 * @param swm_open_flags
 *	Open flags for the split WIM parts (e.g.
 *	::WIMLIB_OPEN_FLAG_CHECK_INTEGRITY).  Note: WIMLIB_OPEN_FLAG_SPLIT_OK is
 *	automatically added to the value specified here.
 * @param wim_write_flags
 * 	Bitwise OR of relevant flags prefixed with WIMLIB_WRITE_FLAG, which will
 * 	be used to write the joined WIM.
 * @param output_path
 * 	The path to write the joined WIM file to.
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.
 *
 * @return 0 on success; nonzero on error.  This function may return most error
 * codes that can be returned by wimlib_open_wim() and wimlib_write(), as well
 * as the following error code:
 *
 * @retval ::WIMLIB_ERR_SPLIT_INVALID
 * 	The split WIMs do not form a valid WIM because they do not include all
 * 	the parts of the original WIM, there are duplicate parts, or not all the
 * 	parts have the same GUID and compression type.
 *
 * Note: wimlib_export_image() can provide similar functionality to
 * wimlib_join(), since it is possible to export all images from a split WIM.
 * Actually, wimlib_join() currently calls wimlib_export_image internally.
 */
extern int
wimlib_join(const wimlib_tchar * const *swms,
	    unsigned num_swms,
	    const wimlib_tchar *output_path,
	    int swm_open_flags,
	    int wim_write_flags,
	    wimlib_progress_func_t progress_func);

/**
 * Compress a chunk of a WIM resource using LZX compression.
 *
 * This function is exported for convenience only and should only be used by
 * library clients looking to make use of wimlib's compression code for another
 * purpose.
 *
 * @param chunk
 * 	Uncompressed data of the chunk.
 * @param chunk_size
 * 	Size of the uncompressed chunk, in bytes.
 * @param out
 * 	Pointer to output buffer of size at least (@p chunk_size - 1) bytes.
 *
 * @return
 * 	The size of the compressed data written to @p out in bytes, or 0 if the
 * 	data could not be compressed to (@p chunk_size - 1) bytes or fewer.
 *
 * As a special requirement, the compression code is optimized for the WIM
 * format and therefore requires (@p chunk_size <= 32768).
 */
extern unsigned
wimlib_lzx_compress(const void *chunk, unsigned chunk_size, void *out);

/**
 * Decompresses a block of LZX-compressed data as used in the WIM file format.
 *
 * Note that this will NOT work unmodified for LZX as used in the cabinet
 * format, which is not the same as in the WIM format!
 *
 * This function is exported for convenience only and should only be used by
 * library clients looking to make use of wimlib's compression code for another
 * purpose.
 *
 * @param compressed_data
 * 	Pointer to the compressed data.
 *
 * @param compressed_len
 * 	Length of the compressed data, in bytes.
 *
 * @param uncompressed_data
 * 	Pointer to the buffer into which to write the uncompressed data.
 *
 * @param uncompressed_len
 * 	Length of the uncompressed data.  It must be 32768 bytes or less.
 *
 * @return
 * 	0 on success; non-zero on failure.
 */
extern int
wimlib_lzx_decompress(const void *compressed_data, unsigned compressed_len,
		      void *uncompressed_data, unsigned uncompressed_len);


/**
 * Mounts an image in a WIM file on a directory read-only or read-write.
 *
 * As this is implemented using FUSE (Filesystme in UserSpacE), this is not
 * supported if wimlib was configured with @c --without-fuse.  This includes
 * Windows builds of wimlib; ::WIMLIB_ERR_UNSUPPORTED will be returned in such
 * cases.
 *
 * Calling this function daemonizes the process, unless
 * ::WIMLIB_MOUNT_FLAG_DEBUG was specified or an early occur occurs.  If the
 * mount is read-write (::WIMLIB_MOUNT_FLAG_READWRITE specified), modifications
 * to the WIM are staged in a temporary directory.
 *
 * It is safe to mount multiple images from the same underlying WIM file
 * read-only at the same time, but only if different ::WIMStruct's are used.  It
 * is @b not safe to mount multiple images from the same WIM file read-write at
 * the same time.
 *
 * wimlib_mount_image() cannot be used on an image that was exported with
 * wimlib_export_image() while the dentry trees for both images are still in
 * memory.  In addition, wimlib_mount_image() may not be used to mount an image
 * that already has modifications pending (e.g. an image added with
 * wimlib_add_image()).
 *
 * @param wim
 * 	Pointer to the ::WIMStruct containing the image to be mounted.
 * @param image
 * 	The number of the image to mount, indexed starting from it.  It must be
 * 	an existing, single image.
 * @param dir
 * 	The path to an existing empty directory to mount the image on.
 * @param mount_flags
 * 	Bitwise OR of the flags prefixed with WIMLIB_MOUNT_FLAG.
 * @param additional_swms
 * 	Array of pointers to the ::WIMStruct for each additional part in the
 * 	split WIM.  Ignored if @p num_additional_swms is 0.  The pointers do not
 * 	need to be in any particular order, but they must include all parts of
 * 	the split WIM other than the first part, which must be provided in the
 * 	@p wim parameter.
 * @param num_additional_swms
 * 	Number of additional WIM parts provided in the @p additional_swms array.
 * 	This number should be one less than the total number of parts in the
 * 	split WIM.  Set to 0 if the WIM is a standalone WIM.
 * @param staging_dir
 * 	If non-NULL, the name of a directory in which the staging directory will
 * 	be created.  Ignored if ::WIMLIB_MOUNT_FLAG_READWRITE is not specified
 * 	in @p mount_flags.  If left @c NULL, the staging directory is created in
 * 	the same directory as the WIM file that @p wim was originally read from.
 *
 * @return 0 on success; nonzero on error.
 *
 * @retval ::WIMLIB_ERR_ALREADY_LOCKED
 * 	A read-write mount was requested, but an an exclusive advisory lock on
 * 	the on-disk WIM file could not be acquired because another thread or
 * 	process has mounted an image from the WIM read-write or is currently
 * 	modifying the WIM in-place.
 * @retval ::WIMLIB_ERR_DECOMPRESSION
 * 	Could not decompress the metadata resource for @p image in @p wim.
 * @retval ::WIMLIB_ERR_FUSE
 * 	A non-zero status was returned by @c fuse_main().
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p image does not specify an existing, single image in @p wim.
 * @retval ::WIMLIB_ERR_INVALID_METADATA_RESOURCE
 *	The metadata resource for @p image is invalid.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 * 	@p image is shared among multiple ::WIMStruct's as a result of a call to
 * 	wimlib_export_image(), or @p image has been added with
 * 	wimlib_add_image().
 * @retval ::WIMLIB_ERR_MKDIR
 * 	::WIMLIB_MOUNT_FLAG_READWRITE was specified in @p mount_flags, but the
 * 	staging directory could not be created.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate needed memory.
 * @retval ::WIMLIB_ERR_NOTDIR
 * 	Could not determine the current working directory.
 * @retval ::WIMLIB_ERR_READ
 * 	Failed to read data from the WIM file associated with @p wim.
 * @retval ::WIMLIB_ERR_RESOURCE_NOT_FOUND
 *	One of the dentries in the image referenced a stream not present in the
 *	WIM's lookup table (or in any of the lookup tables of the split WIM
 *	parts).
 * @retval ::WIMLIB_ERR_SPLIT_INVALID
 * 	The WIM is a split WIM, but the parts specified do not form a complete
 * 	split WIM because they do not include all the parts of the original WIM,
 * 	there are duplicate parts, or not all the parts have the same GUID and
 * 	compression type.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	::WIMLIB_MOUNT_FLAG_READWRITE was specified in @p mount_flags, but @p
 *	wim is considered read-only because of any of the reasons mentioned in
 *	the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS flag.
 * @retval ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE
 * 	Unexpected end-of-file occurred while reading data from the WIM file
 * 	associated with @p wim.
 * @retval ::WIMLIB_ERR_UNSUPPORTED
 * 	Mounting is not supported, either because the platform is Windows, or
 * 	because the platform is UNIX-like and wimlib was compiled with @c
 * 	--without-fuse.
 */
extern int
wimlib_mount_image(WIMStruct *wim,
		   int image,
		   const wimlib_tchar *dir,
		   int mount_flags,
		   WIMStruct **additional_swms,
		   unsigned num_additional_swms,
		   const wimlib_tchar *staging_dir);

/**
 * Opens a WIM file and creates a ::WIMStruct for it.
 *
 * @param wim_file
 * 	The path to the WIM file to open.
 *
 * @param open_flags
 * 	Bitwise OR of flags prefixed with WIMLIB_OPEN_FLAG.
 *
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.  Currently, the only messages sent
 * 	will be ::WIMLIB_PROGRESS_MSG_VERIFY_INTEGRITY, and only if
 * 	::WIMLIB_OPEN_FLAG_CHECK_INTEGRITY was specified in @p open_flags.
 *
 * @param wim_ret
 * 	On success, a pointer to an opaque ::WIMStruct for the opened WIM file
 * 	is written to the memory location pointed to by this parameter.  The
 * 	::WIMStruct can be freed using using wimlib_free() when finished with
 * 	it.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_IMAGE_COUNT
 * 	The WIM is not the non-first part of a split WIM, and the number of
 * 	metadata resources found in the WIM did not match the image count given
 * 	in the WIM header, or the number of &lt;IMAGE&gt; elements in the XML
 * 	data for the WIM did not match the image count given in the WIM header.
 * @retval ::WIMLIB_ERR_INTEGRITY
 * 	::WIMLIB_OPEN_FLAG_CHECK_INTEGRITY was specified in @p open_flags and @p
 * 	wim_file contains an integrity table, but the SHA1 message digest for a
 * 	chunk of the WIM does not match the corresponding message digest given
 * 	in the integrity table.
 * @retval ::WIMLIB_ERR_INVALID_CHUNK_SIZE
 * 	Resources in @p wim_file are compressed, but the chunk size is not 32768.
 * @retval ::WIMLIB_ERR_INVALID_COMPRESSION_TYPE
 * 	The header of @p wim_file says that resources in the WIM are compressed,
 * 	but the header flag indicating LZX or XPRESS compression is not set.
 * @retval ::WIMLIB_ERR_INVALID_HEADER
 * 	The header of @p wim_file was otherwise invalid.
 * @retval ::WIMLIB_ERR_INVALID_INTEGRITY_TABLE
 * 	::WIMLIB_OPEN_FLAG_CHECK_INTEGRITY was specified in @p open_flags and @p
 * 	wim_file contains an integrity table, but the integrity table is
 * 	invalid.
 * @retval ::WIMLIB_ERR_INVALID_LOOKUP_TABLE_ENTRY
 * 	The lookup table for the WIM contained duplicate entries that are not
 * 	for metadata resources, or it contained an entry with a SHA1 message
 * 	digest of all 0's.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 *	@p wim_ret was @c NULL.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocated needed memory.
 * @retval ::WIMLIB_ERR_NOT_A_WIM_FILE
 * 	@p wim_file does not begin with the expected magic characters.
 * @retval ::WIMLIB_ERR_OPEN
 * 	Failed to open the file @p wim_file for reading.
 * @retval ::WIMLIB_ERR_READ
 * 	Failed to read data from @p wim_file.
 * @retval ::WIMLIB_ERR_SPLIT_UNSUPPORTED
 * 	@p wim_file is a split WIM, but ::WIMLIB_OPEN_FLAG_SPLIT_OK was not
 * 	specified in @p open_flags.
 * @retval ::WIMLIB_ERR_UNEXPECTED_END_OF_FILE
 *	Unexpected end-of-file while reading data from @p wim_file.
 * @retval ::WIMLIB_ERR_UNKNOWN_VERSION
 * 	A number other than 0x10d00 is written in the version field of the WIM
 * 	header of @p wim_file.  (May be a pre-Vista WIM.)
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	::WIMLIB_OPEN_FLAG_WRITE_ACCESS was specified but the WIM file was
 *	considered read-only because of any of the reasons mentioned in the
 *	documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS flag.
 * @retval ::WIMLIB_ERR_XML
 * 	The XML data for @p wim_file is invalid.
 */
extern int
wimlib_open_wim(const wimlib_tchar *wim_file,
		int open_flags,
		WIMStruct **wim_ret,
		wimlib_progress_func_t progress_func);

/**
 * Overwrites the file that the WIM was originally read from, with changes made.
 * This only makes sense for ::WIMStruct's obtained from wimlib_open_wim()
 * rather than wimlib_create_new_wim().
 *
 * There are two ways that a WIM may be overwritten.  The first is to do a full
 * rebuild.  In this mode, the new WIM is written to a temporary file and then
 * renamed to the original file after it is has been completely written.  The
 * temporary file is made in the same directory as the original WIM file.  A
 * full rebuild may take a while, but can be used even if images have been
 * modified or deleted, will produce a WIM with no holes, and has little chance
 * of unintentional data loss because the temporary WIM is fsync()ed before
 * being renamed to the original WIM.
 *
 * The second way to overwrite a WIM is by appending to the end of it and
 * overwriting the header.  This can be much faster than a full rebuild, but the
 * disadvantage is that some space will be wasted.  Writing a WIM in this mode
 * begins with writing any new file resources *after* everything in the old WIM,
 * even though this will leave a hole where the old lookup table, XML data, and
 * integrity were.  This is done so that the WIM remains valid even if the
 * operation is aborted mid-write.  The WIM header is only overwritten at the
 * very last moment, and up until that point the WIM will be seen as the old
 * version.
 *
 * By default, wimlib_overwrite() does the append-style overwrite described
 * above, unless resources in the WIM are arranged in an unusual way or if
 * images have been deleted from the WIM.  Use the flag
 * ::WIMLIB_WRITE_FLAG_REBUILD to explicitly request a full rebuild, and use the
 * ::WIMLIB_WRITE_FLAG_SOFT_DELETE to request the in-place overwrite even if
 * images have been deleted from the WIM.
 *
 * In the temporary-file overwrite mode, no changes are made to the WIM on
 * failure, and the temporary file is deleted if possible.  Abnormal termination
 * of the program will result in the temporary file being orphaned.  In the
 * direct append mode, the WIM is truncated to the original length on failure;
 * and while abnormal termination of the program will result in extra data
 * appended to the original WIM, it should still be a valid WIM.
 *
 * If this function completes successfully, no more functions should be called
 * on @p wim other than wimlib_free().  You must use wimlib_open_wim() to read
 * the WIM file anew.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for the WIM file to write.  There may have
 * 	been in-memory changes made to it, which are then reflected in the
 * 	output file.
 * @param write_flags
 * 	Bitwise OR of relevant flags prefixed with WIMLIB_WRITE_FLAG.
 * @param num_threads
 * 	Number of threads to use for compression (see wimlib_write()).
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.
 *
 * @return 0 on success; nonzero on error.  This function may return any value
 * returned by wimlib_write() as well as the following error codes:
 * @retval ::WIMLIB_ERR_ALREADY_LOCKED
 * 	The WIM was going to be modified in-place (with no temporary file), but
 * 	an exclusive advisory lock on the on-disk WIM file could not be acquired
 * 	because another thread or process has mounted an image from the WIM
 * 	read-write or is currently modifying the WIM in-place.
 * @retval ::WIMLIB_ERR_NO_FILENAME
 * 	@p wim corresponds to a WIM created with wimlib_create_new_wim() rather
 * 	than a WIM read with wimlib_open_wim().
 * @retval ::WIMLIB_ERR_RENAME
 * 	The temporary file that the WIM was written to could not be renamed to
 * 	the original filename of @p wim.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	The WIM file is considered read-only because of any of the reasons
 *	mentioned in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS
 *	flag.
 */
extern int
wimlib_overwrite(WIMStruct *wim, int write_flags, unsigned num_threads,
		 wimlib_progress_func_t progress_func);

/**
 * Prints information about one image, or all images, contained in a WIM.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.
 * @param image
 * 	The image about which to print information.  Can be the number of an
 * 	image, or ::WIMLIB_ALL_IMAGES to print information about all images in the
 * 	WIM.
 *
 * @return This function has no return value.  No error checking is done when
 * printing the information.  If @p image is invalid, an error message is
 * printed.
 */
extern void
wimlib_print_available_images(const WIMStruct *wim, int image);

/**
 * Deprecated in favor of wimlib_get_wim_info(), which provides the information
 * in a way that can be accessed programatically.
 */
extern void
wimlib_print_header(const WIMStruct *wim) _wimlib_deprecated;

/**
 * Deprecated in favor of wimlib_iterate_dir_tree(), which provides the
 * information in a way that can be accessed programatically.
 */
extern int
wimlib_print_metadata(WIMStruct *wim, int image) _wimlib_deprecated;

/**
 * Translates a string specifying the name or number of an image in the WIM into
 * the number of the image.  The images are numbered starting at 1.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.
 * @param image_name_or_num
 * 	A string specifying the name or number of an image in the WIM.  If it
 * 	parses to a positive integer, this integer is taken to specify the
 * 	number of the image, indexed starting at 1.  Otherwise, it is taken to
 * 	be the name of an image, as given in the XML data for the WIM file.  It
 * 	also may be the keyword "all" or the string "*", both of which will
 * 	resolve to ::WIMLIB_ALL_IMAGES.
 * 	<br/> <br/>
 * 	There is no way to search for an image actually named "all", "*", or an
 * 	integer number, or an image that has no name.  However, you can use
 * 	wimlib_get_image_name() to get the name of any image.
 *
 * @return
 * 	If the string resolved to a single existing image, the number of that
 * 	image, indexed starting at 1, is returned.  If the keyword "all" or "*"
 * 	was specified, ::WIMLIB_ALL_IMAGES is returned.  Otherwise,
 * 	::WIMLIB_NO_IMAGE is returned.  If @p image_name_or_num was @c NULL or
 * 	the empty string, ::WIMLIB_NO_IMAGE is returned, even if one or more
 * 	images in @p wim has no name.
 */
extern int
wimlib_resolve_image(WIMStruct *wim,
		     const wimlib_tchar *image_name_or_num);

/**
 * Changes the description of an image in the WIM.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.  It may be either a
 * 	standalone WIM or part of a split WIM; however, you should set the same
 * 	description on all parts of a split WIM.
 * @param image
 * 	The number of the image for which to change the description.
 * @param description
 * 	The new description to give the image.  It may be @c NULL, which
 * 	indicates that the image is to be given no description.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p image does not specify a single existing image in @p wim.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate the memory needed to duplicate the @p description
 * 	string.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	@p wim is considered read-only because of any of the reasons mentioned
 *	in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS flag.
 */
extern int
wimlib_set_image_descripton(WIMStruct *wim, int image,
			    const wimlib_tchar *description);

/**
 * Set basic information about a WIM.
 *
 * @param wim
 *	A WIMStruct for a standalone WIM file.
 * @param info
 *	A struct ::wimlib_wim_info that contains the information to set.  Only
 *	the information explicitly specified in the @p which flags need be
 *	valid.
 * @param which
 *	Flags that specify which information to set.  This is a bitwise OR of
 *	::WIMLIB_CHANGE_READONLY_FLAG, ::WIMLIB_CHANGE_GUID,
 *	::WIMLIB_CHANGE_BOOT_INDEX, and/or ::WIMLIB_CHANGE_RPFIX_FLAG.
 *
 * @return 0 on success; nonzero on failure.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	The WIM file is considered read-only because of any of the reasons
 *	mentioned in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS
 *	flag.  However, as a special case, if you are using
 *	::WIMLIB_CHANGE_READONLY_FLAG to unset the readonly flag, then this
 *	function will not fail due to the readonly flag being previously set.
 * @retval ::WIMLIB_ERR_IMAGE_COUNT
 *	::WIMLIB_CHANGE_BOOT_INDEX was specified, but
 *	::wimlib_wim_info.boot_index did not specify 0 or a valid 1-based image
 *	index in the WIM.
 */
extern int
wimlib_set_wim_info(WIMStruct *wim, const struct wimlib_wim_info *info,
		    int which);

/**
 * Changes what is written in the \<FLAGS\> element in the WIM XML data
 * (something like "Core" or "Ultimate")
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.  It may be either a
 * 	standalone WIM or part of a split WIM; however, you should set the same
 * 	\<FLAGS\> element on all parts of a split WIM.
 * @param image
 * 	The number of the image for which to change the description.
 * @param flags
 * 	The new \<FLAGS\> element to give the image.  It may be @c NULL, which
 * 	indicates that the image is to be given no \<FLAGS\> element.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p image does not specify a single existing image in @p wim.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate the memory needed to duplicate the @p flags string.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	@p wim is considered read-only because of any of the reasons mentioned
 *	in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS flag.
 */
extern int
wimlib_set_image_flags(WIMStruct *wim, int image, const wimlib_tchar *flags);

/**
 * Changes the name of an image in the WIM.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM file.  It may be either a
 * 	standalone WIM or part of a split WIM; however, you should set the same
 * 	name on all parts of a split WIM.
 * @param image
 * 	The number of the image for which to change the name.
 * @param name
 *	New name to give the new image.  If @c NULL or empty, the new image is
 *	given no name.  If nonempty, it must specify a name that does not
 *	already exist in @p wim.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_IMAGE_NAME_COLLISION
 * 	There is already an image named @p name in @p wim.
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p image does not specify a single existing image in @p wim.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate the memory needed to duplicate the @p name string.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	@p wim is considered read-only because of any of the reasons mentioned
 *	in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS flag.
 */
extern int wimlib_set_image_name(WIMStruct *wim, int image,
				 const wimlib_tchar *name);

/**
 * Set the functions that wimlib uses to allocate and free memory.
 *
 * These settings are global and not per-WIM.
 *
 * The default is to use the default @c malloc() and @c free() from the C
 * library.
 *
 * Please note that some external functions, such as those in @c libntfs-3g, may
 * use the standard memory allocation functions.
 *
 * @param malloc_func
 * 	A function equivalent to @c malloc() that wimlib will use to allocate
 * 	memory.  If @c NULL, the allocator function is set back to the default
 * 	@c malloc() from the C library.
 * @param free_func
 * 	A function equivalent to @c free() that wimlib will use to free memory.
 * 	If @c NULL, the free function is set back to the default @c free() from
 * 	the C library.
 * @param realloc_func
 * 	A function equivalent to @c realloc() that wimlib will use to reallocate
 * 	memory.  If @c NULL, the free function is set back to the default @c
 * 	realloc() from the C library.
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_UNSUPPORTED
 * 	wimlib was compiled with the @c --without-custom-memory-allocator flag,
 * 	so custom memory allocators are unsupported.
 */
extern int
wimlib_set_memory_allocator(void *(*malloc_func)(size_t),
			    void (*free_func)(void *),
			    void *(*realloc_func)(void *, size_t));

/**
 * Sets whether wimlib is to print error messages to @c stderr when a function
 * fails.  These error messages may provide information that cannot be
 * determined only from the error code that is returned.  Not every error will
 * result in an error message being printed.
 *
 * This setting is global and not per-WIM.
 *
 * By default, error messages are not printed.
 *
 * This can be called before wimlib_global_init().
 *
 * @param show_messages
 * 	@c true if error messages are to be printed; @c false if error messages
 * 	are not to be printed.
 *
 * @return 0 on success; nonzero on error.
 * @retval ::WIMLIB_ERR_UNSUPPORTED
 * 	@p show_messages was @c true, but wimlib was compiled with the @c
 * 	--without-error-messages option.   Therefore, error messages cannot be
 * 	shown.
 */
extern int
wimlib_set_print_errors(bool show_messages);

/**
 * Splits a WIM into multiple parts.
 *
 * @param wim
 * 	The ::WIMStruct for the WIM to split.  It must be a standalone, one-part
 * 	WIM.
 * @param swm_name
 * 	Name of the SWM file to create.  This will be the name of the first
 * 	part.  The other parts will have the same name with 2, 3, 4, ..., etc.
 * 	appended before the suffix.
 * @param part_size
 * 	The maximum size per part, in bytes.  Unfortunately, it is not
 * 	guaranteed that this will really be the maximum size per part, because
 * 	some file resources in the WIM may be larger than this size, and the WIM
 * 	file format provides no way to split up file resources among multiple
 * 	WIMs.
 * @param write_flags
 * 	Bitwise OR of relevant flags prefixed with @c WIMLIB_WRITE_FLAG.  These
 * 	flags will be used to write each split WIM part.  Specify 0 here to get
 * 	the default behavior.
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation
 * 	(::WIMLIB_PROGRESS_MSG_SPLIT_BEGIN_PART and
 * 	::WIMLIB_PROGRESS_MSG_SPLIT_END_PART).
 *
 * @return 0 on success; nonzero on error.  This function may return most error
 * codes that can be returned by wimlib_write() as well as the following error
 * codes:
 *
 * @retval ::WIMLIB_ERR_SPLIT_UNSUPPORTED
 * 	@p wim was not part 1 of a stand-alone WIM.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 * 	@p swm_name was not a nonempty string, or @p part_size was 0.
 *
 * Note: the WIM's uncompressed and compressed resources are not checksummed
 * when they are copied from the joined WIM to the split WIM parts, nor are
 * compressed resources re-compressed (unless explicitly requested with
 * ::WIMLIB_WRITE_FLAG_RECOMPRESS).
 */
extern int
wimlib_split(WIMStruct *wim,
	     const wimlib_tchar *swm_name,
	     uint64_t part_size,
	     int write_flags,
	     wimlib_progress_func_t progress_func);

/**
 * Unmounts a WIM image that was mounted using wimlib_mount_image().
 *
 * The image to unmount is specified by the path to the mountpoint, not the
 * original ::WIMStruct passed to wimlib_mount_image(), which should not be
 * touched and also may have been allocated in a different process.
 *
 * To unmount the image, the thread calling this function communicates with the
 * thread that is managing the mounted WIM image.  This function blocks until it
 * is known whether the unmount succeeded or failed.  In the case of a
 * read-write mounted WIM, the unmount is not considered to have succeeded until
 * all changes have been saved to the underlying WIM file.
 *
 * @param dir
 * 	The directory that the WIM image was mounted on.
 * @param unmount_flags
 * 	Bitwise OR of the flags ::WIMLIB_UNMOUNT_FLAG_CHECK_INTEGRITY,
 * 	::WIMLIB_UNMOUNT_FLAG_COMMIT, ::WIMLIB_UNMOUNT_FLAG_REBUILD, and/or
 * 	::WIMLIB_UNMOUNT_FLAG_RECOMPRESS.  None of these flags affect read-only
 * 	mounts.
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.  Currently, only
 * 	::WIMLIB_PROGRESS_MSG_WRITE_STREAMS will be sent.
 *
 * @return 0 on success; nonzero on error.
 *
 * @retval ::WIMLIB_ERR_DELETE_STAGING_DIR
 * 	The filesystem daemon was unable to remove the staging directory and the
 * 	temporary files that it contains.
 * @retval ::WIMLIB_ERR_FILESYSTEM_DAEMON_CRASHED
 * 	The filesystem daemon appears to have terminated before sending an exit
 * 	status.
 * @retval ::WIMLIB_ERR_FORK
 * 	Could not @c fork() the process.
 * @retval ::WIMLIB_ERR_FUSERMOUNT
 * 	The @b fusermount program could not be executed or exited with a failure
 * 	status.
 * @retval ::WIMLIB_ERR_MQUEUE
 * 	Could not open a POSIX message queue to communicate with the filesystem
 * 	daemon servicing the mounted filesystem, could not send a message
 * 	through the queue, or could not receive a message through the queue.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate needed memory.
 * @retval ::WIMLIB_ERR_OPEN
 * 	The filesystem daemon could not open a temporary file for writing the
 * 	new WIM.
 * @retval ::WIMLIB_ERR_READ
 * 	A read error occurred when the filesystem daemon tried to a file from
 * 	the staging directory
 * @retval ::WIMLIB_ERR_RENAME
 * 	The filesystem daemon failed to rename the newly written WIM file to the
 * 	original WIM file.
 * @retval ::WIMLIB_ERR_UNSUPPORTED
 * 	Mounting is not supported, either because the platform is Windows, or
 * 	because the platform is UNIX-like and wimlib was compiled with @c
 * 	--without-fuse.
 * @retval ::WIMLIB_ERR_WRITE
 * 	A write error occurred when the filesystem daemon was writing to the new
 * 	WIM file, or the filesystem daemon was unable to flush changes that had
 * 	been made to files in the staging directory.
 */
extern int
wimlib_unmount_image(const wimlib_tchar *dir,
		     int unmount_flags,
		     wimlib_progress_func_t progress_func);

/**
 * Update a WIM image by adding, deleting, and/or renaming files or directories.
 *
 * @param wim
 *	Pointer to the ::WIMStruct for the WIM file to update.
 * @param image
 *	The 1-based index of the image in the WIM to update.  It cannot be
 *	::WIMLIB_ALL_IMAGES.
 * @param cmds
 *	An array of ::wimlib_update_command's that specify the update operations
 *	to perform.
 * @param num_cmds
 *	Number of commands in @p cmds.
 * @param update_flags
 *	::WIMLIB_UPDATE_FLAG_SEND_PROGRESS or 0.
 * @param progress_func
 *	If non-NULL, a function that will be called periodically with the
 *	progress of the current operation.
 *
 * @return 0 on success; nonzero on error.  On failure, some but not all of the
 * update commands may have been executed.  No individual update command will
 * have been partially executed.  Possible error codes include:
 *
 * @retval ::WIMLIB_ERR_DECOMPRESSION
 *	Failed to decompress the metadata resource from @p image in @p wim.
 * @retval ::WIMLIB_ERR_INVALID_CAPTURE_CONFIG
 *	The capture configuration structure specified for an add command was
 *	invalid.
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 *	@p image did not specify a single, existing image in @p wim.
 * @retval ::WIMLIB_ERR_INVALID_OVERLAY
 *	Attempted to perform an add command that conflicted with previously
 *	existing files in the WIM when an overlay was attempted.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 *	An unknown operation type was specified in the update commands; or,
 *	attempted to execute an add command where ::WIMLIB_ADD_FLAG_NTFS was set
 *	in the @p add_flags, but the same image had previously already been
 *	added from a NTFS volume; or, both ::WIMLIB_ADD_FLAG_RPFIX and
 *	::WIMLIB_ADD_FLAG_NORPFIX were specified in the @p add_flags for one add
 *	command; or, ::WIMLIB_ADD_FLAG_NTFS or ::WIMLIB_ADD_FLAG_RPFIX were
 *	specified in the @p add_flags for an add command in which @p
 *	wim_target_path was not the root directory of the WIM image.
 * @retval ::WIMLIB_ERR_INVALID_REPARSE_DATA
 *	(Windows only):  While executing an add command, tried to capture a
 *	reparse point with invalid data.
 * @retval ::WIMLIB_ERR_INVALID_RESOURCE_SIZE
 *	The metadata resource for @p image in @p wim is invalid.
 * @retval ::WIMLIB_ERR_INVALID_SECURITY_DATA
 *	The security data for image @p image in @p wim is invalid.
 * @retval ::WIMLIB_ERR_IS_DIRECTORY
 *	A delete command without ::WIMLIB_DELETE_FLAG_RECURSIVE specified was
 *	for a WIM path that corresponded to a directory; or, a rename command
 *	attempted to rename a directory to a non-directory.
 * @retval ::WIMLIB_ERR_NOMEM
 *	Failed to allocate needed memory.
 * @retval ::WIMLIB_ERR_NOTDIR
 *	A rename command attempted to rename a directory to a non-directory; or,
 *	an add command was executed that attempted to set the root of the WIM
 *	image as a non-directory; or, a path component used as a directory in a
 *	rename command was not, in fact, a directory.
 * @retval ::WIMLIB_ERR_NOTEMPTY
 *	A rename command attempted to rename a directory to a non-empty
 *	directory.
 * @retval ::WIMLIB_ERR_NTFS_3G
 *	While executing an add command with ::WIMLIB_ADD_FLAG_NTFS specified, an
 *	error occurred while reading data from the NTFS volume using libntfs-3g.
 * @retval ::WIMLIB_ERR_OPEN
 *	Failed to open a file to be captured while executing an add command.
 * @retval ::WIMLIB_ERR_OPENDIR
 *	Failed to open a directory to be captured while executing an add command.
 * @retval ::WIMLIB_ERR_PATH_DOES_NOT_EXIST
 *	A delete command without ::WIMLIB_DELETE_FLAG_FORCE specified was for a
 *	WIM path that did not exist; or, a rename command attempted to rename a
 *	file that does not exist.
 * @retval ::WIMLIB_ERR_READ
 *	Failed to read the metadata resource for @p image in @p wim; or, while
 *	executing an add command, failed to read data from a file or directory
 *	to be captured.
 * @retval ::WIMLIB_ERR_READLINK
 *	While executing an add command, failed to read the target of a symbolic
 *	link or junction point.
 * @retval ::WIMLIB_ERR_REPARSE_POINT_FIXUP_FAILED
 *	(Windows only) Failed to perform a reparse point fixup because of
 *	problems with the data of a reparse point.
 * @retval ::WIMLIB_ERR_SPECIAL_FILE
 *	While executing an add command, attempted to capture a file that was not
 *	a supported file type (e.g. a device file).
 * @retval ::WIMLIB_ERR_STAT
 *	While executing an add command, failed to get attributes for a file or
 *	directory.
 * @retval ::WIMLIB_ERR_UNSUPPORTED
 * 	::WIMLIB_ADD_FLAG_NTFS was specified in the @p add_flags for an update
 * 	command, but wimlib was configured with the @c --without-ntfs-3g flag;
 * 	or, the platform is Windows and either the ::WIMLIB_ADD_FLAG_UNIX_DATA
 * 	or the ::WIMLIB_ADD_FLAG_DEREFERENCE flags were specified in the @p
 * 	add_flags for an update command.
 * @retval ::WIMLIB_ERR_WIM_IS_READONLY
 *	The WIM file is considered read-only because of any of the reasons
 *	mentioned in the documentation for the ::WIMLIB_OPEN_FLAG_WRITE_ACCESS
 *	flag.
 */
extern int
wimlib_update_image(WIMStruct *wim,
		    int image,
		    const struct wimlib_update_command *cmds,
		    size_t num_cmds,
		    int update_flags,
		    wimlib_progress_func_t progress_func);

/**
 * Writes a standalone WIM to a file.
 *
 * This brings in resources from any external locations, such as directory trees
 * or NTFS volumes scanned with wimlib_add_image(), or other WIM files via
 * wimlib_export_image(), and incorporates them into a new on-disk WIM file.
 *
 * @param wim
 * 	Pointer to the ::WIMStruct for a WIM.  There may have been in-memory
 * 	changes made to it, which are then reflected in the output file.
 * @param path
 * 	The path to the file to write the WIM to.
 * @param image
 * 	The 1-based index of the image inside the WIM to write.  Use
 * 	::WIMLIB_ALL_IMAGES to include all images.
 * @param write_flags
 * 	Bitwise OR of any of the flags prefixed with @c WIMLIB_WRITE_FLAG.
 * @param num_threads
 * 	Number of threads to use for compressing data.  If 0, the number of
 * 	threads is taken to be the number of online processors.  Note: if no
 * 	data compression needs to be done, no additional threads will be created
 * 	regardless of this parameter (e.g. if writing an uncompressed WIM, or
 * 	exporting an image from a compressed WIM to another WIM of the same
 * 	compression type without ::WIMLIB_WRITE_FLAG_RECOMPRESS specified in @p
 * 	write_flags).
 * @param progress_func
 * 	If non-NULL, a function that will be called periodically with the
 * 	progress of the current operation.  The possible messages are
 * 	::WIMLIB_PROGRESS_MSG_WRITE_METADATA_BEGIN,
 * 	::WIMLIB_PROGRESS_MSG_WRITE_METADATA_END, and
 * 	::WIMLIB_PROGRESS_MSG_WRITE_STREAMS.
 *
 * @return 0 on success; nonzero on error.
 *
 * @retval ::WIMLIB_ERR_DECOMPRESSION
 * 	Failed to decompress a metadata or file resource in @p wim.
 * @retval ::WIMLIB_ERR_INVALID_IMAGE
 * 	@p image does not specify a single existing image in @p wim, and is not
 * 	::WIMLIB_ALL_IMAGES.
 * @retval ::WIMLIB_ERR_INVALID_RESOURCE_HASH
 * 	A file that had previously been scanned for inclusion in the WIM by
 * 	wimlib_add_image() was concurrently modified, so it failed the SHA1
 * 	message digest check.
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 * 	@p path was @c NULL.
 * @retval ::WIMLIB_ERR_INVALID_METADATA_RESOURCE
 *	The metadata resource for @p image in @p wim is invalid.
 * @retval ::WIMLIB_ERR_NOMEM
 * 	Failed to allocate needed memory.
 * @retval ::WIMLIB_ERR_OPEN
 * 	Failed to open @p path for writing, or some file resources in @p wim
 * 	refer to files in the outside filesystem, and one of these files could
 * 	not be opened for reading.
 * @retval ::WIMLIB_ERR_READ
 * 	An error occurred when trying to read data from the WIM file associated
 * 	with @p wim, or some file resources in @p wim refer to files in the
 * 	outside filesystem, and a read error occurred when reading one of these
 * 	files.
 * @retval ::WIMLIB_ERR_SPLIT_UNSUPPORTED
 * 	@p wim is part of a split WIM, not a standalone WIM.
 * @retval ::WIMLIB_ERR_WRITE
 * 	An error occurred when trying to write data to the new WIM file.
 */
extern int
wimlib_write(WIMStruct *wim,
	     const wimlib_tchar *path,
	     int image,
	     int write_flags,
	     unsigned num_threads,
	     wimlib_progress_func_t progress_func);

/**
 * Since wimlib v1.5.0:  Same as wimlib_write(), but write the WIM directly to a
 * file descriptor, which need not be seekable if the write is done in a special
 * pipable WIM format by providing ::WIMLIB_WRITE_FLAG_PIPABLE in @p
 * write_flags.  This can, for example, allow capturing a WIM image and
 * streaming it over the network.  See the documentation for
 * ::WIMLIB_WRITE_FLAG_PIPABLE for more information about pipable WIMs.
 *
 * The file descriptor @p fd will @b not be closed when the write is complete;
 * the calling code is responsible for this.
 *
 * Returns 0 on success; nonzero on failure.  The possible error codes include
 * those that can be returned by wimlib_write() as well as the following:
 *
 * @retval ::WIMLIB_ERR_INVALID_PARAM
 *	@p fd was not seekable, but ::WIMLIB_WRITE_FLAG_PIPABLE was not
 *	specified in @p write_flags.
 */
extern int
wimlib_write_to_fd(WIMStruct *wim,
		   int fd,
		   int image,
		   int write_flags,
		   unsigned num_threads,
		   wimlib_progress_func_t progress_func);

/**
 * This function is equivalent to wimlib_lzx_compress(), but instead compresses
 * the data using "XPRESS" compression.
 */
extern unsigned
wimlib_xpress_compress(const void *chunk, unsigned chunk_size, void *out);

/**
 * This function is equivalent to wimlib_lzx_decompress(), but instead assumes
 * the data is compressed using "XPRESS" compression.
 */
extern int
wimlib_xpress_decompress(const void *compressed_data, unsigned compressed_len,
			 void *uncompressed_data, unsigned uncompressed_len);

#endif /* _WIMLIB_H */
