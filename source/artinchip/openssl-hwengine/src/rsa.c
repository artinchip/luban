/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "hardware_engine.h"

#define RSA_CIPHER "rsa"

static RSA_METHOD *aic_rsa_meth = NULL;

static int aic_pub_do_crypt(int flen, const unsigned char *from,
			    unsigned char *to, RSA *rsa, int enc)
{
	struct kcapi_handle *handle = NULL;
	unsigned char *pubder = NULL;
	int len = 0, ok = 1, maxsize;

	len = i2d_RSAPublicKey(rsa, &pubder);
	if (len <= 0) {
		fprintf(stderr, "Get public key der failed.\n");
		return 0;
	}

	if (kcapi_akcipher_init(&handle, RSA_CIPHER, 0)) {
		fprintf(stderr, "Allocation of %s cipher failed\n", RSA_CIPHER);
		ok = 0;
		goto exit;
	}

	maxsize = kcapi_akcipher_setpubkey(handle, pubder, len);
	if (maxsize < flen) {
		fprintf(stderr, "Asymmetric cipher set public key failed\n");
		ok = 0;
		goto exit;
	}
	if (enc)
		ok = kcapi_akcipher_encrypt(handle, from, (size_t)flen, to,
					   (size_t)flen, 0);
	else
		ok = kcapi_akcipher_decrypt(handle, from, (size_t)flen, to,
					   (size_t)flen, 0);
exit:
	if (handle)
		kcapi_akcipher_destroy(handle);
	if (pubder)
		OPENSSL_free(pubder);
	return ok;
}

static int aic_pub_enc(int flen, const unsigned char *from, unsigned char *to,
		       RSA *rsa, int padding)
{
	int i = 0, num = 0;
	unsigned char *buf = NULL;

	num = BN_num_bytes(RSA_get0_n(rsa));
	if (num >256) {
		fprintf(stderr, "Artinchip ce not support algorithm.\n");
		goto out;
	}
	buf = OPENSSL_malloc(num);
	if (buf == NULL) {
		fprintf(stderr, "malloc failed \n");
		goto out;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		i = RSA_padding_add_PKCS1_type_2(buf, num, from, flen);
		break;
	case RSA_PKCS1_OAEP_PADDING:
		i = RSA_padding_add_PKCS1_OAEP(buf, num, from, flen, NULL, 0);
		break;
	case RSA_SSLV23_PADDING:
		i = RSA_padding_add_SSLv23(buf, num, from, flen);
		break;
	case RSA_NO_PADDING:
		i = RSA_padding_add_none(buf, num, from, flen);
		break;
	default:
		fprintf(stderr, "Artinchip ce not support padding type.\n");
		goto out;
	}

	if (i <= 0)
		goto out;

	i = aic_pub_do_crypt(num, buf, to, rsa, 1);
	if (i <= 0)
		fprintf(stderr, "aic pub enc failed.\n");

out:
	if (buf)
		OPENSSL_free(buf);

	if (i <= 0)
		return 0;

	return i;
}

static int aic_pub_dec(int flen, const unsigned char *from, unsigned char *to,
		       RSA *rsa, int padding)
{
	int r = 0, num = 0;
	unsigned char *buf = NULL;

	num = BN_num_bytes(RSA_get0_n(rsa));
	if (num >256) {
		fprintf(stderr, "Artinchip ce not support algorithm.\n");
		goto out;
	}
	buf = OPENSSL_malloc(num);
	if (buf == NULL) {
		fprintf(stderr, "malloc failed \n");
		goto out;
	}

	if (!aic_pub_do_crypt(flen, from, buf, rsa, 0)) {
		fprintf(stderr, "aic pub dec failed.\n");
		goto out;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		r = RSA_padding_check_PKCS1_type_1(to, num, buf, num, num);
		break;
	case RSA_X931_PADDING:
		r = RSA_padding_check_X931(to, num, buf, num, num);
		break;
	case RSA_NO_PADDING:
		r = RSA_padding_check_none(to, num, buf, num, num);
		break;
	default:
		fprintf(stderr, "Artinchip ce not support padding type.\n");
	}

out:
	if (buf)
		OPENSSL_free(buf);

	if (r <= 0)
		return 0;

	return r;
}

static int aic_priv_do_crypt(int flen, const unsigned char *from,
			     unsigned char *to, RSA *rsa, int enc)
{
	struct kcapi_handle *handle = NULL;
	unsigned char *privder = NULL;
	int len, ok = 1, maxsize;

	len = i2d_RSAPrivateKey(rsa, &privder);
	if (len <= 0) {
		fprintf(stderr, "Get private key der failed.\n");
		return 0;
	}

	if (kcapi_akcipher_init(&handle, RSA_CIPHER, 0)) {
		fprintf(stderr, "Allocation of %s cipher failed\n", RSA_CIPHER);
		ok = 0;
		goto exit;
	}

	maxsize = kcapi_akcipher_setkey(handle, privder, len);
	if (maxsize < flen) {
		fprintf(stderr, "Asymmetric cipher set private key failed\n");
		ok = 0;
		goto exit;
	}
	if (enc)
		ok = kcapi_akcipher_encrypt(handle, from, (size_t)flen, to,
					 (size_t)flen, 0);
	else
		ok = kcapi_akcipher_decrypt(handle, from, (size_t)flen, to,
					    (size_t)flen, 0);
exit:
	if (handle)
		kcapi_akcipher_destroy(handle);
	if (privder)
		OPENSSL_free(privder);
	return ok;
}

static int aic_rsa_priv_enc(int flen, const unsigned char *from,
			    unsigned char *to, RSA *rsa, int padding)
{
	int i = 0, num = 0;
	unsigned char *buf = NULL;

	num = BN_num_bytes(RSA_get0_n(rsa));
	if (num >256) {
		fprintf(stderr, "Artinchip ce not support algorithm.\n");
		goto out;
	}
	buf = OPENSSL_malloc(num);
	if (buf == NULL) {
		fprintf(stderr, "malloc failed \n");
		goto out;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		i = RSA_padding_add_PKCS1_type_1(buf, num, from, flen);
		break;
	case RSA_X931_PADDING:
		i = RSA_padding_add_X931(buf, num, from, flen);
		break;
	case RSA_NO_PADDING:
		i = RSA_padding_add_none(buf, num, from, flen);
		break;
	case RSA_SSLV23_PADDING:
	default:
		fprintf(stderr, "Artinchip ce not support padding type.\n");
		goto out;
	}

	i =  aic_priv_do_crypt(num, buf, to, rsa, 1);
	if (i <= 0)
		fprintf(stderr, "aic priv enc failed.\n");

out:
	if (buf)
		OPENSSL_free(buf);

	if (i <= 0)
		return 0;
	return i;
}

static int aic_rsa_priv_dec(int flen, const unsigned char *from,
			    unsigned char *to, RSA *rsa, int padding)
{
	int r = 0, num = 0;
	unsigned char *buf = NULL;

	num = BN_num_bytes(RSA_get0_n(rsa));
	if (num >256) {
		fprintf(stderr, "Artinchip ce not support algorithm.\n");
		goto out;
	}
	buf = OPENSSL_malloc(num);
	if (buf == NULL) {
		fprintf(stderr, "malloc failed \n");
		goto out;
	}

	if (!aic_priv_do_crypt(num, buf, to, rsa, 0)) {
		fprintf(stderr, "aic priv dec failed.\n");
		goto out;;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		r = RSA_padding_check_PKCS1_type_2(to, num, buf, num, flen);
		break;
	case RSA_PKCS1_OAEP_PADDING:
		r = RSA_padding_check_PKCS1_OAEP(to, num, buf, num, num, NULL, 0);
		break;
	case RSA_SSLV23_PADDING:
		r = RSA_padding_check_SSLv23(to, num, buf, num, num);
		break;
	case RSA_NO_PADDING:
		r = RSA_padding_check_none(to, num, buf, num, num);
		break;
	default:
		fprintf(stderr, "Artinchip ce not support padding type.\n");
	}

out:
	if (buf)
		OPENSSL_free(buf);

	if (r <= 0)
		return 0;

	return r;
}

static int aic_rsa_init(RSA *rsa)
{
	return 1;
}

static int aic_rsa_finish(RSA *rsa)
{
	return 1;
}

RSA_METHOD *aic_get_rsa_method(void)
{
	if (aic_rsa_meth)
		return aic_rsa_meth;

	aic_rsa_meth = RSA_meth_new("Artinchip Async RSA method", 0);

	if (!aic_rsa_meth)
		return NULL;

	if (!RSA_meth_set_pub_enc(aic_rsa_meth, aic_pub_enc) ||
	    !RSA_meth_set_pub_dec(aic_rsa_meth, aic_pub_dec) ||
	    !RSA_meth_set_priv_enc(aic_rsa_meth, aic_rsa_priv_enc) ||
	    !RSA_meth_set_priv_dec(aic_rsa_meth, aic_rsa_priv_dec) ||
	    !RSA_meth_set_init(aic_rsa_meth, aic_rsa_init) ||
	    !RSA_meth_set_finish(aic_rsa_meth, aic_rsa_finish)) {
		fprintf(stderr, "Failed to setup rsa method.\n");
		return NULL;
	}

	return aic_rsa_meth;
}

void aic_rsa_free(void)
{
	RSA_meth_free(aic_rsa_meth);
	aic_rsa_meth = NULL;
}
