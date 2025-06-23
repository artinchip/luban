// SPDX-License-Identifier: Apache-2.0
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
#include "util.h"
#include "app.h"

#define MAX_KEY_SIZE          (16 * 1024)
#define MAX_FILE_NAME         512
#define MAX_CIPHER_NAME       32

#define RSA_KEY_TYPE_PRIV     0
#define RSA_KEY_TYPE_PUB      1

struct ak_options {
	char cipher_name[MAX_CIPHER_NAME];
	int enc;
	uint8_t *keybuf;
	int keylen;
	int keytype;
	char in_file[MAX_FILE_NAME];
	char out_file[MAX_FILE_NAME];
	int padding;
	int alignment;
	int access;
	uint8_t key_flag;
	uint8_t in_flag;
	uint8_t out_flag;
};

static int do_rsa(struct ak_options *opts)
{
	struct kcapi_handle *handle = NULL;
	uint8_t *inbuf = NULL, *outbuf = NULL, *buf = NULL, *pin, *pout;
	FILE *fin = NULL, *fout = NULL;
	int ret = 0, i = 0, flags;
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
	int todo, flen, maxsize = 0;

	fin = fopen(opts->in_file, "rb");
	if (!fin) {
		printf("Failed to open %s\n", opts->in_file);
		ret = -EINVAL;
		goto err;
	}

	fout = fopen(opts->out_file, "wb");
	if (!fout) {
		printf("Failed to open %s\n", opts->out_file);
		ret = -EINVAL;
		goto err;
	}

	if (posix_memalign((void **)&inbuf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate inbuf.\n");
		ret = -ENOMEM;
		goto err;
	}
	if (posix_memalign((void **)&outbuf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate outbuf.\n");
		ret = -ENOMEM;
		goto err;
	}
	if (posix_memalign((void **)&buf, pagesize, 2 * pagesize)) {
		printf("Failed to allocate buf.\n");
		ret = -ENOMEM;
		goto err;
	}
	flags = opts->access;
	if (kcapi_akcipher_init(&handle, opts->cipher_name, flags)) {
		printf("Allocation of %s cipher failed\n", opts->cipher_name);
		ret = -1;
		goto err;
	}

	if (opts->alignment == NO_ALIGNMENT) {
		pin = inbuf + 7;
		pout = outbuf + 1;
	} else if (opts->alignment == INPUT_ALIGNMENT) {
		pin = inbuf;
		pout = outbuf + 1;
	} else if (opts->alignment == OUTPUT_ALIGNMENT) {
		pin = inbuf + 7;
		pout = outbuf;
	} else {
		pin = inbuf;
		pout = outbuf;
	}

	if (opts->keytype == RSA_KEY_TYPE_PRIV) {
		maxsize = kcapi_akcipher_setkey(handle, opts->keybuf,
						opts->keylen);
		if (maxsize <= 0) {
			printf("Asymmetric cipher set pivate key failed\n");
			ret = -EFAULT;
			goto err;
		}
	} else if (opts->keytype == RSA_KEY_TYPE_PUB) {
		maxsize = kcapi_akcipher_setpubkey(handle, opts->keybuf,
						   opts->keylen);
		if (maxsize <= 0) {
			printf("Asymmetric cipher set public key failed\n");
			ret = -EFAULT;
			goto err;
		}
	} else {
		printf("No key is provided.\n");
		ret = -EINVAL;
		goto err;
	}

	flen = fread(pin, 1, maxsize, fin);

	if (i < 0)
		goto err;

	todo = maxsize;
	if (todo != maxsize) {
		printf("Input data(%d) is not equal to %d\n\n", todo, maxsize);
	}

	if (opts->enc) {
		if (opts->keytype == RSA_KEY_TYPE_PRIV) {
			switch (opts->padding) {
			case RSA_PKCS1_PADDING:
				i = rsa_padding_add_pkcs1_type_1(buf, maxsize, pin, flen);
				break;
			case RSA_NO_PADDING:
				i = rsa_padding_add_none(buf, maxsize, pin, flen);
				break;
			default:
				printf("unknow padding type.\n");
				goto err;
			}
		}

		if (opts->keytype == RSA_KEY_TYPE_PUB) {
			switch (opts->padding) {
			case RSA_PKCS1_PADDING:
				i = rsa_padding_add_pkcs1_type_2(buf, maxsize, pin, flen);
				break;
			case RSA_NO_PADDING:
				i = rsa_padding_add_none(buf, maxsize, pin, flen);
				break;
			default:
				printf("unknow padding type.\n");
				goto err;
			}
		}

		ret = kcapi_akcipher_encrypt(handle, buf, (size_t)maxsize, pout,
					     (size_t)maxsize, flags);
		fwrite(pout, 1, maxsize, fout);
	} else {
		ret = kcapi_akcipher_decrypt(handle, pin, (size_t)maxsize, pout,
					     (size_t)maxsize, flags);

		if (opts->keytype == RSA_KEY_TYPE_PRIV) {
			switch (opts->padding) {
			case RSA_PKCS1_PADDING:
				i = rsa_padding_check_pkcs1_type_2(buf, maxsize, pout, flen, maxsize);
				break;
			case RSA_NO_PADDING:
				i = rsa_padding_check_none(buf, maxsize, pout, flen, maxsize);
				break;
			default:
				printf("unknow padding type.\n");
				goto err;
			}
		}

		if (opts->keytype == RSA_KEY_TYPE_PUB) {
			switch (opts->padding) {
			case RSA_PKCS1_PADDING:
				i = rsa_padding_check_pkcs1_type_1(buf, maxsize, pout, flen, maxsize);
				break;
			case RSA_NO_PADDING:
				i = rsa_padding_check_none(buf, maxsize, pout, flen, maxsize);
				break;
			default:
				printf("unknow padding type.\n");
				goto err;
			}
		}

		if (i < 0)
			goto err;

		fwrite(buf, 1, i, fout);
	}
err:
	if (handle)
		kcapi_akcipher_destroy(handle);
	if (inbuf)
		free(inbuf);
	if (outbuf)
		free(outbuf);
	if (buf)
		free(buf);
	if (fin)
		fclose(fin);
	if (fout)
		fclose(fout);

	return ret;
}

static int check_options(struct ak_options *opts)
{
	if (opts->key_flag == 0) {
		printf("Key is required.\n");
		return -EINVAL;
	}
	if (opts->in_flag == 0) {
		printf("Input is required.\n");
		return -EINVAL;
	}
	if (opts->out_flag == 0) {
		printf("Outputis required.\n");
		return -EINVAL;
	}

	return 0;
}

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

static int get_rsa_public_key_from_x509(char *name, struct ak_options *opts)
{
	FILE *fp;
	uint8_t val, *p;
	uint32_t len;
	int ret, i, cnt;

	fp = fopen(name, "rb");
	if (!fp) {
		printf("Failed to open %s\n", name);
		return -EINVAL;
	}

	/* First SEQ TAG */
	ret = fread(&val, 1, 1, fp);
	if (ret <= 0)
		goto out;
	if (val != TAG_SEQ) {
		ret = -1;
		goto out;
	}
	/* SEQ LENGTH */
	ret = fread(&val, 1, 1, fp);
	if (ret <= 0)
		goto out;
	len = 0;
	if (val & 0x80) {
		cnt = val & 0x7f;
		for (i = 0; i < cnt; i++) {
			fread(&val, 1, 1, fp);
			len |= (val << (i * 8));
		}
	} else {
		len = val;
	}
	/* Skip Second SEQ */
	ret = fread(&val, 1, 1, fp);
	if (ret <= 0)
		goto out;
	if (val != TAG_SEQ) {
		ret = -1;
		goto out;
	}
	/* SEQ LENGTH */
	ret = fread(&val, 1, 1, fp);
	if (ret <= 0)
		goto out;
	len = 0;
	if (val & 0x80) {
		cnt = val & 0x7f;
		for (i = 0; i < cnt; i++) {
			fread(&val, 1, 1, fp);
			len |= (val << (i * 8));
		}
	} else {
		len = val;
	}
	fseek(fp, len, SEEK_CUR);

	/* Now should be BIT STRING */
	ret = fread(&val, 1, 1, fp);
	if (ret <= 0)
		goto out;
	if (val != TAG_BITSTRING) {
		ret = -1;
		goto out;
	}
	/* BIT STRING LENGTH */
	ret = fread(&val, 1, 1, fp);
	if (ret <= 0)
		goto out;
	len = 0;
	if (val & 0x80) {
		cnt = val & 0x7f;
		for (i = 0; i < cnt; i++) {
			fread(&val, 1, 1, fp);
			len |= (val << (i * 8));
		}
	} else {
		len = val;
	}

	opts->keylen = 0;
	p = opts->keybuf;
	ret = fread(p, 1, 1, fp);
	if (*p != 0) {
		p++;
		opts->keylen++;
	}
	opts->keylen += fread(p, 1, MAX_KEY_SIZE, fp);
	opts->keytype = RSA_KEY_TYPE_PUB;
	opts->key_flag = 1;
	
	ret = 0;
out:
	if (fp)
		fclose(fp);
	return ret;
}

static void usage_ak(struct subcmd_cfg *cmd)
{
	printf("Usage:\n");
	printf("  %s %s <options>\n", cmd->app_name, cmd->cmd_name);
	printf("    --help:         display this help.\n");
	printf("    --enc:          do encryption.\n");
	printf("    --dec:          do decryption.\n");
	printf("    --pubkey:       public key, DER format.\n");
	printf("    --privkey:      private key, DER format.\n");
	printf("    --in:           input data file.\n");
	printf("    --out:          output file.\n");
	printf("    --raw:          use no padding.\n");
	printf("    --pkcs:         use PKCS#1 v1.5 padding (default).\n");
	printf("    --align-in:     input data buffer page alignment.\n");
	printf("    --align-out:    output data buffer page alignment.\n");
	printf("    --align:        input and output buffer page alignment.\n");
	printf("    --access-vmsplice:    app always use the sendmsg kernel interface.\n");
	printf("    --access-sendmsg:     app always use the vmsplice zero copy kernel interface.\n");
	printf("    --access-heuristic:   app to determine the optimal kernel access type.\n");
}

static int akcipher_cmd_arg_parse(int argc, char *argv[],
				  struct ak_options *opts)
{
	int c = 0;
	FILE *fp;
	int opt_index = 0;
	opts->padding = RSA_PKCS1_PADDING;

	static struct option cmd_opts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "enc", no_argument, 0, 'e' },
		{ "dec", no_argument, 0, 'd' },
		{ "pubkey", required_argument, 0, 'p' },
		{ "privkey", required_argument, 0, 'P' },
		{ "in", required_argument, 0, 'i' },
		{ "out", required_argument, 0, 'o' },
		{ "raw", no_argument, 0, 'r' },
		{ "pkcs", no_argument, 0, 'c' },
		{ "align-in", no_argument, 0, 'a' },
		{ "align-out", no_argument, 0, 'L' },
		{ "align", no_argument, 0, 'A' },
		{ "access-heuristic", no_argument, 0, 'R' },
		{ "access-vmsplice", no_argument, 0, 'V' },
		{ "access-sendmsg", no_argument, 0, 'S' },
		{ 0, 0, 0, 0 }
	};

	while (1) {
		c = getopt_long_only(argc, argv, "h", cmd_opts, &opt_index);
		if (c < 0)
			break;

		switch (c) {
		case 'e': /* enc */
			opts->enc = 1;
			break;
		case 'd': /* dec */
			opts->enc = 0;
			break;
		case 'p': /* pubkey */
			get_rsa_public_key_from_x509(optarg, opts);
			break;
		case 'P': /* privkey */
			fp = fopen(optarg, "rb");
			if (!fp) {
				printf("Failed to open %s\n", optarg);
				return -EINVAL;
			}
			opts->keylen =
				fread(opts->keybuf, 1, MAX_KEY_SIZE, fp);
			fclose(fp);
			fp = NULL;
			opts->keytype = RSA_KEY_TYPE_PRIV;
			opts->key_flag = 1;
			break;
		case 'i': /* in */
			if (strlen(optarg) >= MAX_FILE_NAME) {
				printf("Input file name is too long.\n");
				return -EINVAL;
			}
			strcpy(opts->in_file, optarg);
			opts->in_flag = 1;
			break;
		case 'o': /* out */
			if (strlen(optarg) >= MAX_FILE_NAME) {
				printf("Output file name is too long.\n");
				return -EINVAL;
			}
			strcpy(opts->out_file, optarg);
			opts->out_flag = 1;
			break;
		case 'r': /* raw */
			opts->padding = RSA_NO_PADDING;
			break;
		case 'c': /* pkcs */
			opts->padding = RSA_PKCS1_PADDING;
			break;
		case 'A': /* align */
			opts->alignment = BIDIRECTION_ALIGNMENT;
			break;
		case 'a': /* align-in */
			opts->alignment = INPUT_ALIGNMENT;
			break;
		case 'L': /* align-out */
			opts->alignment = OUTPUT_ALIGNMENT;
			break;
		case 'R': /* access-heuristic */
			opts->access = KCAPI_ACCESS_HEURISTIC;
			break;
		case 'V': /* access-vmsplice */
			opts->access = KCAPI_ACCESS_VMSPLICE;
			break;
		case 'S': /* access-sendmsg */
			opts->access = KCAPI_ACCESS_SENDMSG;
			break;
		case 'h':
			return 1;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

int akcipher_command(struct subcmd_cfg *cmd, int argc, char *argv[])
{
	struct ak_options ak_opts;
	int ret = 0;

	memset(&ak_opts, 0, sizeof(ak_opts));

	ak_opts.keybuf = (uint8_t *)malloc(MAX_KEY_SIZE);
	if (!ak_opts.keybuf) {
		printf("Failed to malloc key buffer.\n");
		return -ENOMEM;
	}

	ret = akcipher_cmd_arg_parse(argc, argv, &ak_opts);
	if (ret > 0) {
		usage_ak(cmd);
		ret = 0;
		goto out;
	}
	if (ret < 0)
		goto out;

	if (check_options(&ak_opts)) {
		ret = -EINVAL;
		goto out;
	}

	strcpy(ak_opts.cipher_name, cmd->cipher_name);

	ret = do_rsa(&ak_opts);
out:
	if (ak_opts.keybuf)
		free(ak_opts.keybuf);
	return ret;
}
