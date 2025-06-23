/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Xiong Hao <hao.xiong@artinchip.com>
 */

#ifndef _LIC_H_
#define _LIC_H_

#define MAX_KEY_SIZE          (16 * 1024)
#define MAX_FILE_NAME         512
#define MAX_CIPHER_NAME       32

#define PNK_PROTECTED_RSA	"pnk-protected(rsa)"
#define PSK0_PROTECTED_RSA	"psk0-protected(rsa)"
#define PSK1_PROTECTED_RSA	"psk1-protected(rsa)"
#define PSK2_PROTECTED_RSA	"psk2-protected(rsa)"
#define PSK3_PROTECTED_RSA	"psk3-protected(rsa)"

struct ak_options {
	char cipher_name[MAX_CIPHER_NAME];
	char esk_name[MAX_FILE_NAME];
	char pk_name[MAX_FILE_NAME];
	uint8_t *esk_buf;
	uint8_t *pk_buf;
	int esk_len;
	int pk_len;
};

int aic_rsa_priv_enc(int flen, unsigned char *from, unsigned char *to,
			struct ak_options *opts);

int aic_rsa_pub_dec(int flen, unsigned char *from, unsigned char *to,
			struct ak_options *opts);

int aic_hwp_rsa_priv_enc(int flen, unsigned char *from, unsigned char *to,
			struct ak_options *opts, char *algo);

#endif
