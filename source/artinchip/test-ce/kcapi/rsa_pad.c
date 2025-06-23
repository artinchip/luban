// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Xiong Hao <hao.xiong@artinchip.com>
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
#include "util.h"
#include "app.h"

int rsa_padding_add_pkcs1_type_1(unsigned char *to, int tlen,
				const unsigned char *from, int flen)
{
	int j;
	unsigned char *p;

	if (flen > (tlen - RSA_PKCS1_PADDING_SIZE)) {
		printf("Data too large for key size.\n");
		return -1;
	}
	p = (unsigned char *)to;

	*(p++) = 0;
	*(p++) = 1; /* Private Key BT (Block Type) */

	/* pad out with 0xff data */
	j = tlen - 3 -flen;
	memset(p, 0xff, j);
	p += j;
	*(p++) = '\0';
	memcpy(p, from, (unsigned int)flen);

	return 0;
}

int rsa_padding_check_pkcs1_type_1(unsigned char *to, int tlen,
				   const unsigned char *from, int flen,
				   int num)
{
	int i, j;
	const unsigned char *p;

	p = from;

	/*
	 * The format if
	 * 00 || 01 || PS || D
	 * PS - padding string, at least 8 bytes of FF
	 * D  - data.
	 */

	if (num < RSA_PKCS1_PADDING_SIZE)
		return -1;

	/* Accept inputs with and without the leading 0-byte */
	if (num == flen) {
		if ((*p++) != 0x00) {
			printf("Invalid padding.\n");
			return -1;
		}
		flen--;
	}

	if ((num != (flen + 1)) || (*(p++) != 0x01)) {
		printf("Block type is not 01.\n");
		return -1;
	}

	/* scan over padding data */
	j = flen - 1;	/* one for type. */
	for (i = 0; i < j; i++) {
		if (*p != 0xff)	{	/* should decrypt to 0xff */
			if (*p == 0) {
				p++;
				break;
			} else {
				printf("Bad fixed header decrypt.\n");
				return -1;
			}
		}
		p++;
	}

	if (i == j) {
		printf("Null before block missing.\n");
		return -1;
	}

	if (i < 8) {
		printf("Bad pad byte count.\n");
		return -1;
	}
	i++;
	j -= i;
	if (j > tlen) {
		printf("Data too large.\n");
		return -1;
	}
	memcpy(to, p, (unsigned int)j);

	return j;
}

int rsa_padding_add_pkcs1_type_2(unsigned char *to, int tlen,
				 const unsigned char *from, int flen)
{
	int i, j;
	unsigned char *p;

	if (flen > (tlen - RSA_PKCS1_PADDING_SIZE)) {
		printf("Data too large for key size.\n");
		return -1;
	}

	p = (unsigned char *)to;

	*(p++) = 0;
	*(p++) = 2;	/* Public Key BT (Block Type) */

	/* pad out with non-zero random data */
	j = tlen - 3 - flen;
	srand((int)time(0));
	for (i = 0; i < j; i++) {
		*(p++) = 1 + rand() % 254;
	}

	*(p++) = '\0';

	memcpy(p, from, (unsigned int)flen);
	return 0;
}

int rsa_padding_check_pkcs1_type_2(unsigned char *to, int tlen,
				   const unsigned char *from, int flen,
				   int num)
{
	int i, j;
	const unsigned char *p;

	p = from;

	/*
	 * PKCS#1 v1.5 decryption. See "PKCS #1 v2.2: RSA Cryptography Standard"
	 * section 7.2.2.
	 */
	if (tlen <= 0 || flen <= 0)
		return -1;

	if (flen > num || num < RSA_PKCS1_PADDING_SIZE) {
		printf("pkcs decoding error.\n");
		return -1;
	}

	/* Accept inputs with and without the leading 0-byte */
	if (num == flen) {
		if ((*p++) != 0x00) {
			printf("Invalid padding.\n");
			return -1;
		}
		flen--;
	}

	if ((num != (flen + 1)) || (*(p++) != 0x02)) {
		printf("Block type is not 02.\n");
		return -1;
	}

	/* scan over padding data */
	j = flen - 1;	/* one for type. */
	for (i = 0; i < j; i++) {
		if (*p == 0) {
			p++;
			break;
		}
		p++;
	}

	if (i == j) {
		printf("Null before block missing.\n");
		return -1;
	}

	if (i < 8) {
		printf("Bad pad byte count.\n");
		return -1;
	}
	i++;
	j -= i;
	if (j > tlen) {
		printf("Data too large.\n");
		return -1;
	}
	memcpy(to, p, (unsigned int)j);

	return j;
}

int rsa_padding_add_none(unsigned char *to, int tlen,
			const unsigned char *from, int flen)
{
	if (flen > tlen) {
		printf("Data too large for key size.\n");
		return -1;
	}

	if (flen < tlen) {
		printf("Data too small for key size.\n");
		return -1;
	}
	memcpy(to, from, (unsigned int)flen);

	return 0;
}

int rsa_padding_check_none(unsigned char *to, int tlen,
			   const unsigned char *from, int flen, int num)
{
	if (flen > tlen) {
		printf("Data too large.\n");
		return -1;
	}

	memset(to, 0, tlen - flen);
	memcpy(to + tlen -flen, from, flen);

	return tlen;
}
