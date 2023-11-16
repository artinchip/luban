/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Xiong Hao <hao.xiong@artinchip.com>
 */

#ifndef _RSA_PAD_H_
#define _RSA_PAD_H_

#define RSA_PKCS1_PADDING	1
#define RSA_NO_PADDING		2

#define RSA_PKCS1_PADDING_SIZE	11

/*
 * Reference openssl-1.1.1m rsa_pk1.c code and "RFC8017 : PKCS #1: RSA
 * Cryptography Specifications Version 2.2" implementation.
 */
int rsa_padding_add_pkcs1_type_1(unsigned char *to, int tlen,
				const unsigned char *from, int flen);
int rsa_padding_check_pkcs1_type_1(unsigned char *to, int tlen,
				   const unsigned char *from, int flen,
				   int num);
/*
 * PKCS#1 v1.5 encryption. See "RFC8017: PKCS #1: RSA Cryptography
 * Specifications Version 2.2" section 7.2.1.,
 */
int rsa_padding_add_pkcs1_type_2(unsigned char *to, int tlen,
				 const unsigned char *from, int flen);
/*
 * PKCS#1 v1.5 decryption. See "RFC8017: PKCS #1: RSA Cryptography
 * Specifications Version 2.2" section 7.2.2.,
 */
int rsa_padding_check_pkcs1_type_2(unsigned char *to, int tlen,
				   const unsigned char *from, int flen,
				   int num);
int rsa_padding_add_none(unsigned char *to, int tlen,
			const unsigned char *from, int flen);
int rsa_padding_check_none(unsigned char *to, int tlen,
			   const unsigned char *from, int flen, int num);

#endif
