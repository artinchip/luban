// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2023 Artinchip Technology Co., Ltd.
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
#include "authorization.h"
#include "test_aic_hw_authorization.h"

void hexdump(const char *msg, const uint8_t* data, int len)
{
	int i, plen;
	char msgbuf[32];
	const uint8_t *p;

	memset(msgbuf, ' ', 16);
	msgbuf[16] = '\0';
	strncpy(msgbuf, msg, strlen(msg) > 16 ? 16 : strlen(msg));
	printf("%s", msgbuf);

	memset(msgbuf, ' ', 16);
	p = data;
	do {
		if (len > 16)
			plen = 16;
		else
			plen = len;
		for (i = 0; i < plen; ++i)
			printf("%.2x ", p[i]);
		printf("\n");

		p = p + plen;
		len -= plen;
		if (len > 0)
			printf("%s", msgbuf);
	} while (len > 0);
}

int app_hw_authorization_check(unsigned char *from, int flen,
				unsigned char *esk, int esk_len,
				unsigned char *pk, int pk_len, char *algo)
{
	struct ak_options opts = {0};
	uint8_t *inbuf = NULL, *outbuf = NULL;
	uint8_t esk_buf[esk_len];
	uint8_t pk_buf[pk_len];
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
	int ret = 0, rlen, nonce;

	if (posix_memalign((void **)&inbuf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate inbuf.\n");
		ret = -ENOMEM;
		goto out;
	}
	if (posix_memalign((void **)&outbuf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate outbuf.\n");
		ret = -ENOMEM;
		goto out;
	}

	// 1. Set RSA key parameters
	memcpy(esk_buf, esk, esk_len);
	memcpy(pk_buf, pk, pk_len);
	opts.esk_buf = esk_buf;
	opts.esk_len = esk_len;
	opts.pk_buf = pk_buf;
	opts.pk_len = pk_len;

	// 2. Nonce private key encryption
	rlen = aic_hwp_rsa_priv_enc(flen, from, outbuf, &opts, algo);
	if (rlen < 0) {
		printf("aic_hwp_rsa_priv_enc failed.\n");
		goto out;
	}
	memcpy(inbuf, outbuf, rlen);
	memset(outbuf, 0, 2 * pagesize);

	// 3. EncNonce public key decryption
	rlen = aic_rsa_pub_dec(rlen, inbuf, outbuf, &opts);
	if (rlen < 0) {
		printf("aic_rsa_pub_dec failed.\n");
		goto out;
	}

	// 4. compare Nonce and DecNonce
	if (memcmp(from, outbuf, rlen))
	{
		hexdump("Expect", (unsigned char *)&nonce, rlen);
		hexdump("Got Result", (unsigned char *)outbuf, rlen);
		printf("App %s stop.\n", algo);
		ret = -1;
	} else {
		printf("App %s running.\n", algo);
		ret = 0;
	}

out:
	if (inbuf)
		free(inbuf);
	if (outbuf)
		free(outbuf);

	return ret;
}

int main()
{
	int ret = 0;
	int nonce, flen, esk_len, pk_len;
	unsigned char *esk, *pk;
	char *algo;

	esk = rsa_private_key2048_encrypted_der;
	esk_len = rsa_private_key2048_encrypted_der_len;
	pk = rsa_public_key2048_der;
	pk_len = rsa_public_key2048_der_len;
	while(1) {
		nonce = rand();
		flen = sizeof(nonce);
		algo = PNK_PROTECTED_RSA;
		ret = app_hw_authorization_check((unsigned char *)&nonce, flen,
						esk, esk_len, pk, pk_len, algo);
		if (ret < 0) {
			printf("Application %s not authorization.\n", algo);
		}

		nonce = rand();
		flen = sizeof(nonce);
		algo = PSK0_PROTECTED_RSA;
		ret = app_hw_authorization_check((unsigned char *)&nonce, flen,
						esk, esk_len, pk, pk_len, algo);
		if (ret < 0) {
			printf("Application %s not authorization.\n", algo);
		}

		nonce = rand();
		flen = sizeof(nonce);
		algo = PSK1_PROTECTED_RSA;
		ret = app_hw_authorization_check((unsigned char *)&nonce, flen,
						esk, esk_len, pk, pk_len, algo);
		if (ret < 0) {
			printf("Application %s not authorization.\n", algo);
		}

		nonce = rand();
		flen = sizeof(nonce);
		algo = PSK2_PROTECTED_RSA;
		ret = app_hw_authorization_check((unsigned char *)&nonce, flen,
						esk, esk_len, pk, pk_len, algo);
		if (ret < 0) {
			printf("Application %s not authorization.\n", algo);
		}

		nonce = rand();
		flen = sizeof(nonce);
		algo = PSK3_PROTECTED_RSA;
		ret = app_hw_authorization_check((unsigned char *)&nonce, flen,
						esk, esk_len, pk, pk_len, algo);
		if (ret < 0) {
			printf("Application %s not authorization.\n", algo);
		}

		sleep(2);
	}

	return 0;
}
