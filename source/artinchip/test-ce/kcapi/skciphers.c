// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
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
#include <libgen.h>
#include <kcapi.h>
#include "util.h"
#include "app.h"

#define MAX_KEY_SIZE          64
#define MAX_IV_SIZE           16
#define MAX_FILE_NAME         512
#define MAX_CIPHER_NAME       32

/* Max 16 Pages is sent to kernel with libkcapi one shot api */
#define ONE_SHOT_SIZE (4096 * 16)
#define ONE_SHOT_SIZE2 (128)

struct sk_options {
	char cipher_name[MAX_CIPHER_NAME];
	int enc;
	uint8_t key[MAX_KEY_SIZE];
	int keylen;
	uint8_t iv[MAX_IV_SIZE];
	int ivlen;
	char in_file[MAX_FILE_NAME];
	char out_file[MAX_FILE_NAME];
	int alignment;
	int access;
	int stream_api;
	uint8_t key_flag;
	uint8_t iv_flag;
	uint8_t in_flag;
	uint8_t out_flag;
};

static int skcipher_do_crypt_sync_oneshot(struct sk_options *opts)
{
	struct kcapi_handle *handle = NULL;
	uint8_t *inbuf = NULL, *outbuf = NULL, *pin, *pout;
	FILE *fin = NULL, *fout = NULL;
	struct stat st;
	size_t bufsize, todo;
	ssize_t processed;
	int ret = 0, flags;
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);

	fin = fopen(opts->in_file, "rb");
	if (!fin) {
		printf("Failed to open %s\n", opts->in_file);
		ret = -EINVAL;
		goto err;
	}


	if (stat(opts->in_file, &st)) {
		printf("Failed to get file information.\n");
		ret = -EINVAL;
		goto err;
	}
	bufsize = st.st_size + pagesize;

	if ((st.st_size > ONE_SHOT_SIZE) && opts->iv_flag) {
		printf("File too large to use one shot API for CTR/CBC\n");
		ret = -EINVAL;
		goto err;
	}

	fout = fopen(opts->out_file, "wb");
	if (!fout) {
		printf("Failed to open %s\n", opts->out_file);
		ret = -EINVAL;
		goto err;
	}

	if (posix_memalign((void**)&inbuf, pagesize, bufsize)) {
		printf("Failed to allocate inbuf.\n");
		ret = -ENOMEM;
		goto err;
	}
	if (posix_memalign((void**)&outbuf, pagesize, bufsize)) {
		printf("Failed to allocate outbuf.\n");
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * Both input and output buffer page alignment is the best,
	 * here setup for test purpose
	 */
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

	flags = opts->access;
	if (kcapi_cipher_init(&handle, opts->cipher_name, flags)) {
		printf("Allocation of %s cipher failed\n", opts->cipher_name);
		ret = -1;
		goto err;
	}

	kcapi_cipher_setkey(handle, opts->key, opts->keylen);

	/* Do it in one shot */
	todo = fread(pin, 1, st.st_size, fin);
	if (opts->enc)
		processed = kcapi_cipher_encrypt(handle, pin, todo, opts->iv,
						 pout, todo, flags);
	else
		processed = kcapi_cipher_decrypt(handle, pin, todo, opts->iv,
						 pout, todo, flags);
	if (processed < 0) {
		printf("process error, ret %ld\n", processed);
		ret = -1;
		goto err;
	}
	fwrite(pout, 1, processed, fout);
err:
	if (handle)
		kcapi_cipher_destroy(handle);
	if (inbuf)
		free(inbuf);
	if (outbuf)
		free(outbuf);
	if (fin)
		fclose(fin);
	if (fout)
		fclose(fout);

	return ret;
}


/*
 * 如果想使用 vmsplice 进行 zerocopy，则每次发送的数据大小不要超过 1 page size
 *
 * 否则会自动改用 sendmsg 接口发送
 *
 * 如果设置的 iov 个数超过 16 个，也会使用 sendmsg 接口发送
 */
static int skcipher_do_crypt_stream(struct sk_options *opts)
{
	struct kcapi_handle *handle = NULL;
	uint8_t *inbuf = NULL, *outbuf = NULL, *pin, *pout;
	FILE *fin = NULL, *fout = NULL;
	size_t todo;
	ssize_t processed;
	int ret = 0, flags;
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
	struct iovec iov;

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

	if (posix_memalign((void **)&inbuf, pagesize, ONE_SHOT_SIZE)) {
		printf("Failed to allocate inbuf.\n");
		ret = -ENOMEM;
		goto err;
	}
	if (posix_memalign((void **)&outbuf, pagesize, ONE_SHOT_SIZE)) {
		printf("Failed to allocate outbuf.\n");
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * Both input and output buffer page alignment is the best,
	 * here setup for test purpose
	 */
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

	flags = opts->access;
	if (kcapi_cipher_init(&handle, opts->cipher_name, flags)) {
		printf("Allocation of %s cipher failed\n", opts->cipher_name);
		ret = -1;
		goto err;
	}

	kcapi_cipher_setkey(handle, opts->key, opts->keylen);
	if (opts->enc)
		ret = kcapi_cipher_stream_init_enc(handle, opts->iv, NULL, 0);
	else
		ret = kcapi_cipher_stream_init_dec(handle, opts->iv, NULL, 0);

	while (1) {
		todo = fread(pin, 1, ONE_SHOT_SIZE, fin);
		if (todo <= 0)
			break;

		iov.iov_base = pin;
		iov.iov_len = todo;

		/*
		 * Stream API also support multi-thread operation:
		 * - kcapi_cipher_stream_update work in write data thread
		 * - kcapi_cipher_stream_op work in read data thread
		 */
		if (todo < ONE_SHOT_SIZE)
			processed = kcapi_cipher_stream_update_last(handle, &iov, 1);
		else
			processed = kcapi_cipher_stream_update(handle, &iov, 1);
		if (processed != todo) {
			printf("Send data error: %ld.\n", processed);
			ret = -1;
			break;
		}

		iov.iov_base = pout;
		iov.iov_len = processed;
		processed = kcapi_cipher_stream_op(handle, &iov, 1);
		if (processed < 0) {
			printf("Read result error: %ld.\n", processed);
			ret = -1;
			break;
		}
		fwrite(pout, 1, processed, fout);
	}
	kcapi_cipher_destroy(handle);

err:
	if (inbuf)
		free(inbuf);
	if (outbuf)
		free(outbuf);
	if (fin)
		fclose(fin);
	if (fout)
		fclose(fout);

	return ret;
}
static void usage_sk(struct subcmd_cfg *cmd)
{
	printf("Usage:\n");
	printf("  %s %s <options>\n", cmd->app_name, cmd->cmd_name);
	printf("    --help:         display this help.\n");
	printf("    --enc:          do encryption.\n");
	printf("    --dec:          do decryption.\n");
	printf("    --keyhex:       key hex string.\n");
	printf("    --keyfile:      key binary file.\n");
	if (cmd->ivlen) {
		printf("    --ivhex:        iv hex string.\n");
		printf("    --ivfile:       iv binary file.\n");
	}
	printf("    --in:           input data file.\n");
	printf("    --out:          output file.\n");
	printf("    --align-in:     input data buffer page alignment.\n");
	printf("    --align-out:    output data buffer page alignment.\n");
	printf("    --align:        input and output buffer page alignment.\n");
	printf("    --access-vmsplice:    app always use the sendmsg kernel interface.\n");
	printf("                          when --stream is enable, this setting will be ignored.\n");
	printf("    --access-sendmsg:     app always use the vmsplice zero copy kernel interface.\n");
	printf("                          when --stream is enable, this setting will be ignored.\n");
	printf("    --access-heuristic:   app to determine the optimal kernel access type.\n");
	printf("                          when --stream is enable, this setting will be ignored.\n");
	printf("    --stream:             app use stream api to perform crypto.\n");
}

static int check_options(struct sk_options *opts)
{
	if (!opts->key_flag) {
		printf("Key is required.\n");
		return -EINVAL;
	}

	if (opts->ivlen && !opts->iv_flag) {
		printf("IV is required.\n");
		return -EINVAL;
	}

	if (!opts->in_flag) {
		printf("input file is required.\n");
		return -EINVAL;
	}

	if (!opts->out_flag) {
		printf("output file is required.\n");
		return -EINVAL;
	}

	return 0;
}

static int skcipher_cmd_arg_parse(int argc, char *argv[],
				  struct sk_options *opts)
{
	int opt_index = 0;
	int c = 0;
	FILE *fp;

	static struct option cmd_opts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "enc", no_argument, 0, 'e' },
		{ "dec", no_argument, 0, 'd' },
		{ "keyhex", required_argument, 0, 'k' },
		{ "keyfile", required_argument, 0, 'K' },
		{ "ivhex", required_argument, 0, 'v' },
		{ "ivfile", required_argument, 0, 'I' },
		{ "in", required_argument, 0, 'i' },
		{ "out", required_argument, 0, 'o' },
		{ "align-in", no_argument, 0, 'a' },
		{ "align-out", no_argument, 0, 'L' },
		{ "align", no_argument, 0, 'A' },
		{ "access-heuristic", no_argument, 0, 'R' },
		{ "access-vmsplice", no_argument, 0, 'V' },
		{ "access-sendmsg", no_argument, 0, 'S' },
		{ "stream", no_argument, 0, 's' },
		{ "oneshot", no_argument, 0, 'O' },
		{ 0, 0, 0, 0 }
	};

	/* Use stream API as default */
	opts->stream_api = 1;
	while (1)
	{
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
		case 'k': /* keyhex */
			if (strlen(optarg) > MAX_KEY_SIZE * 2) {
				printf("HEX key string is too long.\n");
				return -EINVAL;
			}
			hex2bin(optarg, strlen(optarg), opts->key,
				MAX_KEY_SIZE);
			opts->key_flag = 1;
			break;
		case 'K': /* keyfile */
			fp = fopen(optarg, "rb");
			if (!fp) {
				printf("Failed to open %s\n", optarg);
				return -EINVAL;
			}
			fread(opts->key, 1, MAX_KEY_SIZE, fp);
			fclose(fp);
			fp = NULL;
			opts->key_flag = 1;
			break;
		case 'v': /* ivhex */
			if (strlen(optarg) > MAX_IV_SIZE) {
				printf("HEX iv string is too long.\n");
				return -EINVAL;
			}
			hex2bin(optarg, strlen(optarg), opts->iv,
				MAX_IV_SIZE);
			opts->iv_flag = 1;
			break;
		case 'I': /* ivfile */
			fp = fopen(optarg, "rb");
			if (!fp) {
				printf("Failed to open %s\n", optarg);
				return -EINVAL;
			}
			fread(opts->iv, 1, MAX_IV_SIZE, fp);
			fclose(fp);
			fp = NULL;
			opts->iv_flag = 1;
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
		case 's': /* stream */
			opts->stream_api = 1;
			break;
		case 'O': /* Oneshot */
			opts->stream_api = 0;
			break;
		case 'h':
			return 1;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

int skcipher_command(struct subcmd_cfg *cmd, int argc, char *argv[])
{
	struct sk_options sk_opts;
	int ret = -1;

	memset(&sk_opts, 0, sizeof(sk_opts));

	ret = skcipher_cmd_arg_parse(argc, argv, &sk_opts);
	if (ret > 0) {
		usage_sk(cmd);
		return 0;
	}
	if (ret < 0)
		return ret;

	if (check_options(&sk_opts))
		return -EINVAL;

	strcpy(sk_opts.cipher_name, cmd->cipher_name);
	sk_opts.keylen = cmd->keylen;
	sk_opts.ivlen = cmd->ivlen;

	if (sk_opts.stream_api)
		ret = skcipher_do_crypt_stream(&sk_opts);
	else
		ret = skcipher_do_crypt_sync_oneshot(&sk_opts);
	return ret;
}
