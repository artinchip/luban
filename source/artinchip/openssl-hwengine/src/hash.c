// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include "hardware_engine.h"

static int aic_digest_init(EVP_MD_CTX *ctx);
static int aic_digest_update(EVP_MD_CTX *ctx, const void *data, size_t count);
static int aic_digest_final(EVP_MD_CTX *ctx, unsigned char *md);
static int aic_digest_cleanup(EVP_MD_CTX *ctx);

static struct aic_digest md_list[] = {
	{
		.nid = NID_md5,
		.result_size = MD5_DIGEST_LENGTH,
		.input_blocksize = MD5_CBLOCK,
		.ctx_size = sizeof(struct aic_ossl_digest_ctx),
		.init = aic_digest_init,
		.update = aic_digest_update,
		.final = aic_digest_final,
		.cleanup = aic_digest_cleanup,
	},
	{
		.nid = NID_sha1,
		.result_size = SHA_DIGEST_LENGTH,
		.input_blocksize = SHA_CBLOCK,
		.ctx_size = sizeof(struct aic_ossl_digest_ctx),
		.init = aic_digest_init,
		.update = aic_digest_update,
		.final = aic_digest_final,
		.cleanup = aic_digest_cleanup,
	},
	{
		.nid = NID_sha224,
		.result_size = SHA224_DIGEST_LENGTH,
		.input_blocksize = SHA256_CBLOCK,
		.ctx_size = sizeof(struct aic_ossl_digest_ctx),
		.init = aic_digest_init,
		.update = aic_digest_update,
		.final = aic_digest_final,
		.cleanup = aic_digest_cleanup,
	},
	{
		.nid = NID_sha256,
		.result_size = SHA256_DIGEST_LENGTH,
		.input_blocksize = SHA256_CBLOCK,
		.ctx_size = sizeof(struct aic_ossl_digest_ctx),
		.init = aic_digest_init,
		.update = aic_digest_update,
		.final = aic_digest_final,
		.cleanup = aic_digest_cleanup,
	},
	{
		.nid = NID_sha384,
		.result_size = SHA384_DIGEST_LENGTH,
		.input_blocksize = SHA512_CBLOCK,
		.ctx_size = sizeof(struct aic_ossl_digest_ctx),
		.init = aic_digest_init,
		.update = aic_digest_update,
		.final = aic_digest_final,
		.cleanup = aic_digest_cleanup,
	},
	{
		.nid = NID_sha512,
		.result_size = SHA512_DIGEST_LENGTH,
		.input_blocksize = SHA512_CBLOCK,
		.ctx_size = sizeof(struct aic_ossl_digest_ctx),
		.init = aic_digest_init,
		.update = aic_digest_update,
		.final = aic_digest_final,
		.cleanup = aic_digest_cleanup,
	},
};

static int known_digest_nids[OSSL_NELEM(md_list)];

static int aic_digest_init(EVP_MD_CTX *ctx)
{
	struct aic_ossl_digest_ctx *c;
	char *hashname = NULL;
	const EVP_MD *md;
	int nid;

	hashname = NULL;
	if (ctx == NULL) {
		fprintf(stderr, "Null Parameter\n");
		return 0;
	}
	md = EVP_MD_CTX_md(ctx);
	if (md == NULL) {
		fprintf(stderr, "Cipher object NULL\n");
		return 0;
	}

	c = EVP_MD_CTX_md_data(ctx);
	c->result_size = EVP_MD_size(md);
	nid= EVP_MD_nid(md);
	switch (nid) {
	case NID_md5:
		hashname = "md5";
		break;
	case NID_sha1:
		hashname = "sha1";
		break;
	case NID_sha224:
		hashname = "sha224";
		break;
	case NID_sha256:
		hashname = "sha256";
		break;
	case NID_sha384:
		hashname = "sha384";
		break;
	case NID_sha512:
		hashname = "sha512";
		break;
	default:
		fprintf(stderr, "Unknown NID %d\n", nid);
		return 0;
	}

	if (kcapi_md_init(&c->handle, hashname, 0)) {
		fprintf(stderr, "Failed to allocate %s\n", hashname);
		return 0;
	}
	return 1;
}

static int aic_digest_update(EVP_MD_CTX *ctx, const void *data, size_t count)
{
	struct aic_ossl_digest_ctx *c;
	ssize_t ret = 0;

	c = EVP_MD_CTX_md_data(ctx);
	ret = kcapi_md_update(c->handle, data, count);
	if (ret < 0)
		return 0;
	return 1;
}

static int aic_digest_final(EVP_MD_CTX *ctx, unsigned char *md)
{
	struct aic_ossl_digest_ctx *c;
	ssize_t ret = 0;

	c = EVP_MD_CTX_md_data(ctx);

	ret = kcapi_md_final(c->handle, md, c->result_size);
	if (ret < 0)
		return 0;
	return 1;
}

static int aic_digest_cleanup(EVP_MD_CTX *ctx)
{
	struct aic_ossl_digest_ctx *c;

	c = EVP_MD_CTX_md_data(ctx);
	kcapi_md_destroy(c->handle);
	c->handle = 0;
	return 1;
}

static EVP_MD *aic_digest_alloc(struct aic_digest *d)
{
	EVP_MD *md;

	if (d->digest)
		return d->digest;

	if (!(md = EVP_MD_meth_new(d->nid, NID_undef)) ||
	    !EVP_MD_meth_set_result_size(md, d->result_size) ||
	    !EVP_MD_meth_set_input_blocksize(md, d->input_blocksize) ||
	    !EVP_MD_meth_set_app_datasize(md, d->ctx_size) ||
	    !EVP_MD_meth_set_flags(md, d->flags) ||
	    !EVP_MD_meth_set_init(md, d->init) ||
	    !EVP_MD_meth_set_update(md, d->update) ||
	    !EVP_MD_meth_set_final(md, d->final) ||
	    !EVP_MD_meth_set_copy(md, d->copy) ||
	    !EVP_MD_meth_set_cleanup(md, d->cleanup) ||
	    !EVP_MD_meth_set_ctrl(md, d->ctrl)) {
		EVP_MD_meth_free(md);
		md = NULL;
	}

	d->digest = md;
	return md;
}

void aic_digests_free(void)
{
	EVP_MD *digest;
	int i;

	for (i = 0; i < OSSL_NELEM(md_list); i++) {
		digest = md_list[i].digest;
		if (digest)
			EVP_MD_meth_free(digest);
		md_list[i].digest = NULL;
	}
}

int aic_digests(ENGINE *e, const EVP_MD **digest, const int **nids, int nid)
{
	int i;

	if (!digest) {
		int *n = known_digest_nids;

		*nids = n;
		for (i = 0; i < OSSL_NELEM(md_list); i++)
			*n++ = md_list[i].nid;
		return i;
	}

	for (i = 0; i < OSSL_NELEM(md_list); i++) {
		if (nid == md_list[i].nid) {
			*digest = aic_digest_alloc(&md_list[i]);
			return 1;
		}
	}
	*digest = NULL;
	return 0;
}
