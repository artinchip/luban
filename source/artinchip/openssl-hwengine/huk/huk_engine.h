/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#ifndef OPENSSL_HUK_ENGINE_H
#define OPENSSL_HUK_ENGINE_H

#include <openssl/engine.h>
#include <kcapi.h>

#define trace_log() {printf("%s, line:%d\n", __func__, __LINE__);}
# define OSSL_NELEM(x)    (sizeof(x)/sizeof((x)[0]))

#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16
#endif
#define AES_KEY_SIZE_128 16
#define AES_KEY_SIZE_192 24
#define AES_KEY_SIZE_256 32
#define AES_IV_LEN 16

struct huk_ossl_cipher_ctx {
	struct kcapi_handle *handle;
	unsigned char iv[AES_IV_LEN];
	int ivlen;
	int enc;
};

struct huk_cipher {
	int nid;
	EVP_CIPHER *cipher;
	int block_size; /* bytes */
	int key_len; /* bytes */
	int iv_len;
	int flags;
	int (*init)(EVP_CIPHER_CTX *ctx, const unsigned char *key,
		    const unsigned char *iv, int enc);
	int (*do_cipher)(EVP_CIPHER_CTX *ctx, unsigned char *out,
			 const unsigned char *in, size_t inl);
	int (*cleanup)(EVP_CIPHER_CTX *);
	int ctx_size;
	int (*set_asn1_parameters)(EVP_CIPHER_CTX *, ASN1_TYPE *);
	int (*get_asn1_parameters)(EVP_CIPHER_CTX *, ASN1_TYPE *);
	int (*ctrl)(EVP_CIPHER_CTX *, int type, int arg, void *ptr);
};

int huk_skciphers(ENGINE *e, const EVP_CIPHER **cipher, const int **nids,
		  int nid);
#endif
