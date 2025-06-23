/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <stdio.h>
#include <string.h>
#include "huk_engine.h"

static int huk_cipher_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			   const unsigned char *iv, int enc);
static int huk_do_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			 const unsigned char *in, size_t inl);
static int huk_cipher_cleanup(EVP_CIPHER_CTX *ctx);

static struct huk_cipher huk_ciphers[] = {
	{
		.nid = NID_aes_128_ecb,
		.flags = EVP_CIPH_ECB_MODE,
		.block_size = AES_BLOCK_SIZE,
		.key_len = AES_KEY_SIZE_128,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_192_ecb,
		.flags = EVP_CIPH_ECB_MODE,
		.block_size = AES_BLOCK_SIZE,
		.key_len = AES_KEY_SIZE_192,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_256_ecb,
		.flags = EVP_CIPH_ECB_MODE,
		.block_size = AES_BLOCK_SIZE,
		.key_len = AES_KEY_SIZE_256,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_128_cbc,
		.flags = EVP_CIPH_CBC_MODE,
		.block_size = AES_BLOCK_SIZE,
		.key_len = AES_KEY_SIZE_128,
		.iv_len = AES_IV_LEN,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_192_cbc,
		.flags = EVP_CIPH_CBC_MODE,
		.block_size = AES_BLOCK_SIZE,
		.key_len = AES_KEY_SIZE_192,
		.iv_len = AES_IV_LEN,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_256_cbc,
		.flags = EVP_CIPH_CBC_MODE,
		.block_size = AES_BLOCK_SIZE,
		.key_len = AES_KEY_SIZE_256,
		.iv_len = AES_IV_LEN,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_128_ctr,
		.flags = EVP_CIPH_CTR_MODE | EVP_CIPH_NO_PADDING,
		.block_size = 1,
		.key_len = AES_KEY_SIZE_128,
		.iv_len = AES_IV_LEN,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_192_ctr,
		.flags = EVP_CIPH_CTR_MODE | EVP_CIPH_NO_PADDING,
		.block_size = 1,
		.key_len = AES_KEY_SIZE_192,
		.iv_len = AES_IV_LEN,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
	{
		.nid = NID_aes_256_ctr,
		.flags = EVP_CIPH_CTR_MODE | EVP_CIPH_NO_PADDING,
		.block_size = 1,
		.key_len = AES_KEY_SIZE_256,
		.iv_len = AES_IV_LEN,
		.ctx_size = sizeof(struct huk_ossl_cipher_ctx),
		.init = huk_cipher_init,
		.do_cipher = huk_do_cipher,
		.cleanup = huk_cipher_cleanup,
	},
};

static int known_cipher_nids[OSSL_NELEM(huk_ciphers)];

static int huk_cipher_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			   const unsigned char *iv, int enc)
{
	struct huk_ossl_cipher_ctx *c;
	int nid, ivlen, keylen;
	char *cipher_name;

	cipher_name = NULL;
	if (ctx == NULL || key == NULL) {
		fprintf(stderr, "Null Parameter\n");
		return 0;
	}
	if (EVP_CIPHER_CTX_cipher(ctx) == NULL) {
		fprintf(stderr, "Cipher object NULL\n");
		return 0;
	}

	c = EVP_CIPHER_CTX_get_cipher_data(ctx);
	nid= EVP_CIPHER_CTX_nid(ctx);
	keylen = 0;
	switch (nid) {
	case NID_aes_128_ecb:
		keylen = AES_KEY_SIZE_128;
		cipher_name = "huk-protected(ecb(aes))";
		break;
	case NID_aes_192_ecb:
		keylen = AES_KEY_SIZE_192;
		cipher_name = "huk-protected(ecb(aes))";
		break;
	case NID_aes_256_ecb:
		keylen = AES_KEY_SIZE_256;
		cipher_name = "huk-protected(ecb(aes))";
		break;
	case NID_aes_128_cbc:
		keylen = AES_KEY_SIZE_128;
		cipher_name = "huk-protected(cbc(aes))";
		break;
	case NID_aes_192_cbc:
		keylen = AES_KEY_SIZE_192;
		cipher_name = "huk-protected(cbc(aes))";
		break;
	case NID_aes_256_cbc:
		keylen = AES_KEY_SIZE_256;
		cipher_name = "huk-protected(cbc(aes))";
		break;
	case NID_aes_128_ctr:
		keylen = AES_KEY_SIZE_128;
		cipher_name = "huk-protected(ctr(aes))";
		break;
	case NID_aes_192_ctr:
		keylen = AES_KEY_SIZE_192;
		cipher_name = "huk-protected(ctr(aes))";
		break;
	case NID_aes_256_ctr:
		keylen = AES_KEY_SIZE_256;
		cipher_name = "huk-protected(ctr(aes))";
		break;
	default:
		fprintf(stderr, "Not supported NID %d\n", nid);
		return 0;
	}

	ivlen = EVP_CIPHER_CTX_iv_length(ctx);
	if (ivlen) {
		memcpy(c->iv, iv, ivlen);
		c->ivlen = ivlen;
	}

	if (kcapi_cipher_init(&c->handle, cipher_name, 0)) {
		fprintf(stderr, "Failed to allocate cipher %s \n", cipher_name);
		return 0;
	}
	kcapi_cipher_setkey(c->handle, key, keylen);

	c->enc = enc;
	return 1;
}

static int huk_do_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			 const unsigned char *in, size_t inlen)
{
	struct huk_ossl_cipher_ctx *c = EVP_CIPHER_CTX_get_cipher_data(ctx);
	ssize_t processed;

	if (c->enc)
		processed = kcapi_cipher_encrypt(c->handle, in, inlen, c->iv,
						 out, inlen, 0);
	else
		processed = kcapi_cipher_decrypt(c->handle, in, inlen, c->iv,
						 out, inlen, 0);
	if ( processed < 0 || processed != inlen)
		return 0;
	return 1;
}

static int huk_cipher_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct huk_ossl_cipher_ctx *c = EVP_CIPHER_CTX_get_cipher_data(ctx);

	kcapi_cipher_destroy(c->handle);
	c->handle = 0;
	return 1;
}

static EVP_CIPHER *huk_cipher_cfg(struct huk_cipher *c)
{
	int flags = c->flags;
	int block_size = c->block_size;
	EVP_CIPHER *cipher;

	trace_log();
	if (c->cipher)
		return c->cipher;

	switch (flags & EVP_CIPH_MODE) {
	case EVP_CIPH_CTR_MODE:
		OPENSSL_assert(block_size == 1);
		OPENSSL_assert(flags & EVP_CIPH_NO_PADDING);
		break;
	default:
		OPENSSL_assert(block_size != 1);
		OPENSSL_assert(!(flags & EVP_CIPH_NO_PADDING));
	}

	if (!(cipher = EVP_CIPHER_meth_new(c->nid, block_size, c->key_len)) ||
	    !EVP_CIPHER_meth_set_iv_length(cipher, c->iv_len) ||
	    !EVP_CIPHER_meth_set_flags(cipher, flags) ||
	    !EVP_CIPHER_meth_set_init(cipher, c->init) ||
	    !EVP_CIPHER_meth_set_do_cipher(cipher, c->do_cipher) ||
	    !EVP_CIPHER_meth_set_cleanup(cipher, c->cleanup) ||
	    !EVP_CIPHER_meth_set_impl_ctx_size(cipher, c->ctx_size) ||
	    !EVP_CIPHER_meth_set_set_asn1_params(cipher,
						 c->set_asn1_parameters) ||
	    !EVP_CIPHER_meth_set_get_asn1_params(cipher,
						 c->get_asn1_parameters) ||
	    !EVP_CIPHER_meth_set_ctrl(cipher, c->ctrl)) {
		EVP_CIPHER_meth_free(cipher);
		cipher = NULL;
	}
	c->cipher = cipher;
	return c->cipher;
}

int huk_skciphers(ENGINE *e, const EVP_CIPHER **cipher, const int **nids,
		  int nid)
{
	int i;

	if (!cipher) {
		int *n = known_cipher_nids;

		*nids = n;
		for (i = 0; i < OSSL_NELEM(huk_ciphers); i++)
			*n++ = huk_ciphers[i].nid;
		return i;
	}

	for (i = 0; i < OSSL_NELEM(huk_ciphers); i++) {
		if (nid == huk_ciphers[i].nid) {
			*cipher = huk_cipher_cfg(&huk_ciphers[i]);
			return 1;
		}
	}
	*cipher = NULL;
	return 0;
}
