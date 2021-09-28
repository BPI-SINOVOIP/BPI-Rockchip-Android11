/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef HASH_ALGS_H
#define HASH_ALGS_H

#include <stdio.h>

#include "util.h"

struct fsverity_hash_alg {
	const char *name;
	unsigned int digest_size;
	unsigned int block_size;
	struct hash_ctx *(*create_ctx)(const struct fsverity_hash_alg *alg);
};

extern const struct fsverity_hash_alg fsverity_hash_algs[];

struct hash_ctx {
	const struct fsverity_hash_alg *alg;
	void (*init)(struct hash_ctx *ctx);
	void (*update)(struct hash_ctx *ctx, const void *data, size_t size);
	void (*final)(struct hash_ctx *ctx, u8 *out);
	void (*free)(struct hash_ctx *ctx);
};

const struct fsverity_hash_alg *find_hash_alg_by_name(const char *name);
const struct fsverity_hash_alg *find_hash_alg_by_num(unsigned int num);
void show_all_hash_algs(FILE *fp);

/* The hash algorithm that fsverity-utils assumes when none is specified */
#define FS_VERITY_HASH_ALG_DEFAULT	FS_VERITY_HASH_ALG_SHA256

/*
 * Largest digest size among all hash algorithms supported by fs-verity.
 * This can be increased if needed.
 */
#define FS_VERITY_MAX_DIGEST_SIZE	64

static inline struct hash_ctx *hash_create(const struct fsverity_hash_alg *alg)
{
	return alg->create_ctx(alg);
}

static inline void hash_init(struct hash_ctx *ctx)
{
	ctx->init(ctx);
}

static inline void hash_update(struct hash_ctx *ctx,
			       const void *data, size_t size)
{
	ctx->update(ctx, data, size);
}

static inline void hash_final(struct hash_ctx *ctx, u8 *digest)
{
	ctx->final(ctx, digest);
}

static inline void hash_free(struct hash_ctx *ctx)
{
	if (ctx)
		ctx->free(ctx);
}

void hash_full(struct hash_ctx *ctx, const void *data, size_t size, u8 *digest);

#endif /* HASH_ALGS_H */
