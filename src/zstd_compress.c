#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "wimlib/error.h"
#include "wimlib/compressor_ops.h"
#include "wimlib/util.h"

#include <zstd.h>

struct zstd_compressor {
	unsigned compression_level;
	ZSTD_CCtx *cctx;
};

static u64
zstd_get_needed_memory(size_t max_bufsize, unsigned compression_level,
		       bool destructive)
{
	return sizeof(struct zstd_compressor);
}

static int
zstd_create_compressor(size_t max_bufsize, unsigned compression_level,
		       bool destructive, void **c_ret)
{
	struct zstd_compressor *c;

	c = MALLOC(sizeof(*c));
	if (!c)
		goto oom;

	c->cctx = ZSTD_createCCtx();
	if (!c->cctx)
		goto oom;

	c->compression_level = compression_level / 5;
	c->compression_level = max(c->compression_level, 1);
	c->compression_level = min(c->compression_level, ZSTD_maxCLevel());

	*c_ret = c;
	return 0;

oom:
	FREE(c);
	return WIMLIB_ERR_NOMEM;
}

static size_t
zstd_compress(const void *in, size_t in_nbytes,
	      void *out, size_t out_nbytes_avail, void *_c)
{
	struct zstd_compressor *c = _c;
	size_t res;

	res = ZSTD_compressCCtx(c->cctx, out, out_nbytes_avail, in, in_nbytes,
				c->compression_level);

	if (ZSTD_isError(res))
		return 0;
	return res;
}

static void
zstd_free_compressor(void *_c)
{
	struct zstd_compressor *c = _c;

	FREE(c->cctx);
	FREE(c);
}

const struct compressor_ops zstd_compressor_ops = {
	.get_needed_memory	= zstd_get_needed_memory,
	.create_compressor	= zstd_create_compressor,
	.compress		= zstd_compress,
	.free_compressor	= zstd_free_compressor,
};

