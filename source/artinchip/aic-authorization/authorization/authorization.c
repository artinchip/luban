// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/user.h>
#include <time.h>
#include <sys/utsname.h>
#include <linux/random.h>
#include <sys/syscall.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <kcapi.h>
#include "rsa_pad.h"
#include "authorization.h"

/*
 * Get PKCS1 ASN1 public key from X509 public key
 * SEQUENCE {
 *     SEQUENCE {
 *         OBJECT IDENTIFIER
 *         rsaEncryption (1 2 840 113549 1 1 1)
 *         NULL
 *     }
 *     BIT STRING, encapsulates {
 *         SEQUENCE {
 *             INTEGER
 *             INTEGER
 *         }
 *     }
 * }
 */
#define TAG_SEQ 0x30
#define TAG_BITSTRING 0x03

static int get_rsa_public_key_from_x509(unsigned char *der,
					struct ak_options *opts)
{
	uint8_t val, *p, *pder = der;
	uint32_t len;
	int ret, i, cnt;

	/* First SEQ TAG */
	val = *(pder++);
	opts->pk_len--;
	if (val != TAG_SEQ) {
		ret = -1;
		goto out;
	}
	/* SEQ LENGTH */
	val = *(pder++);
	opts->pk_len--;
	len = 0;
	if (val & 0x80) {
		cnt = val & 0x7f;
		for (i = 0; i < cnt; i++) {
			val = *(pder++);
			opts->pk_len--;
			len |= (val << (i * 8));
		}
	} else {
		len = val;
	}
	/* Skip Second SEQ */
	val = *(pder++);
	opts->pk_len--;
	if (val != TAG_SEQ) {
		ret = -1;
		goto out;
	}
	/* SEQ LENGTH */
	val = *(pder++);
	opts->pk_len--;
	len = 0;
	if (val & 0x80) {
		cnt = val & 0x7f;
		for (i = 0; i < cnt; i++) {
			val = *(pder++);
			opts->pk_len--;
			len |= (val << (i * 8));
		}
	} else {
		len = val;
	}
	pder += len;
	opts->pk_len -= len;

	/* Now should be BIT STRING */
	val = *(pder++);
	opts->pk_len--;
	if (val != TAG_BITSTRING) {
		ret = -1;
		goto out;
	}
	/* BIT STRING LENGTH */
	val = *(pder++);
	opts->pk_len--;
	len = 0;
	if (val & 0x80) {
		cnt = val & 0x7f;
		for (i = 0; i < cnt; i++) {
			val = *(pder++);
			opts->pk_len--;
			len |= (val << (i * 8));
		}
	} else {
		len = val;
	}

	val = *(pder++);
	opts->pk_len--;
	if (val != 0) {
		val = *(pder++);
		opts->pk_len--;
	}
	p = opts->pk_buf;
	memmove(p, pder, opts->pk_len);

	ret = 0;
out:
	return ret;
}

static int aic_priv_enc(int flen, const unsigned char *from, unsigned char *to,
		     struct ak_options *opts, char *cipher_name)
{
	struct kcapi_handle *handle = NULL;
	unsigned char *buf = NULL;
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
	int ret = 0, maxsize = 0;

	if (posix_memalign((void **)&buf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate buf.\n");
		ret = -ENOMEM;
		goto out;
	}
	if (kcapi_akcipher_init(&handle, cipher_name, 0)) {
		printf("Allocation of %s cipher failed\n", cipher_name);
		ret = -1;
		goto out;
	}

	maxsize = kcapi_akcipher_setkey(handle, opts->esk_buf, opts->esk_len);
	if (maxsize < flen) {
		printf("Asymmetric cipher set private key failed\n");
		ret = -EFAULT;
		goto out;
	}

	ret = rsa_padding_add_pkcs1_type_1(buf, maxsize, from, flen);
	if (ret < 0)
		goto out;

	ret = kcapi_akcipher_encrypt(handle, buf, (size_t)maxsize, to,
				    (size_t)maxsize, 0);
	if (ret < 0)
		printf("aic priv enc failed.\n");

out:
	if (handle)
		kcapi_akcipher_destroy(handle);
	if (buf)
		free(buf);

	return ret;
}

static int aic_pub_dec(int flen, const unsigned char *from, unsigned char *to,
		struct ak_options *opts, char *cipher_name)
{
	struct kcapi_handle *handle = NULL;
	unsigned char *buf = NULL;
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
	int ret = 0, maxsize = 0;

	if (posix_memalign((void **)&buf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate buf.\n");
		ret = -ENOMEM;
		goto out;
	}
	if (kcapi_akcipher_init(&handle, cipher_name, 0)) {
		printf("Allocation of %s cipher failed\n", cipher_name);
		ret = -1;
		goto out;
	}

	ret = get_rsa_public_key_from_x509(opts->pk_buf, opts);
	if (ret < 0) {
		printf("Get rsa public key from x509 failed.\n");
		goto out;
	}

	maxsize = kcapi_akcipher_setpubkey(handle, opts->pk_buf, opts->pk_len);
	if (maxsize < flen) {
		printf("Asymmetric cipher set public key failed\n");
		ret = -EFAULT;
		goto out;
	}

	ret = kcapi_akcipher_decrypt(handle, from, maxsize, buf, maxsize, 0);
	if (ret < 0) {
		printf("aic pub dec failed.\n");
		goto out;
	}

	ret = rsa_padding_check_pkcs1_type_1(to, maxsize, buf, maxsize, maxsize);

out:
	if (handle)
		kcapi_akcipher_destroy(handle);
	if (buf)
		free(buf);

	return ret;
}

int aic_rsa_priv_enc(int flen, unsigned char *from, unsigned char *to,
			struct ak_options *opts)
{
	return aic_priv_enc(flen, from, to, opts, "rsa");
}

int aic_rsa_pub_dec(int flen, unsigned char *from, unsigned char *to,
			struct ak_options *opts)
{
	return aic_pub_dec(flen, from, to, opts, "rsa");
}

int aic_hwp_rsa_priv_enc(int flen, unsigned char *from, unsigned char *to,
			struct ak_options *opts, char *algo)
{
	return aic_priv_enc(flen, from, to, opts, algo);
}
