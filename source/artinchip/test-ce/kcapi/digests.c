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
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <kcapi.h>
#include "util.h"
#include "app.h"

#define MAX_KEY_SIZE          (16 * 1024)
#define MAX_FILE_NAME         512
#define MAX_CIPHER_NAME       32

struct md_options {
	char cipher_name[MAX_CIPHER_NAME];
	uint8_t *hmac_key;
	uint32_t keylen;
	int hmac;
	char in_file[MAX_FILE_NAME];
	char out_file[MAX_FILE_NAME];
	int alignment;
	int hexout;
	uint8_t key_flag;
	uint8_t in_flag;
	uint8_t out_flag;
};

int check_filetype(int fd, struct stat *sb, const char *filename)
{
	int ret = fstat(fd, sb);
	if (ret) {
		fprintf(stderr, "fstat() failed: %s", strerror(errno));
		return -errno;
	}

	/* Do not return an error in case we cannot validate the data. */
	if ((sb->st_mode & S_IFMT) != S_IFREG &&
	    (sb->st_mode & S_IFMT) != S_IFLNK) {
		fprintf(stderr, "%s is no regular file or symlink", filename);
		return -EINVAL;
	}

	return 0;
}
static int mmap_file(const char *filename, uint8_t **memory, off_t *size,
		     size_t *mapped, off_t offset)
{
	int fd = -1;
	int ret = 0;
	struct stat sb;

	fd = open(filename, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open file %s: %s\n", filename,
		        strerror(errno));
		return -EIO;
	}

	if (*size) {
		if ((*size - offset) < (off_t)*mapped )
			*mapped = (size_t)(*size - offset);
	} else {
		ret = check_filetype(fd, &sb, filename);
		if (ret)
			goto out;
		*size = sb.st_size;
		if (*size <= (off_t)*mapped) {
			*mapped = (size_t)*size;
		if (*size == 0)
			goto out;
		}
	}

	*memory = mmap(NULL, *mapped, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd,
		       offset);
	if (*memory == MAP_FAILED) {
		*memory = NULL;
		ret = -errno;
		goto out;
	}
	madvise(*memory, *mapped, MADV_SEQUENTIAL | MADV_WILLNEED);

out:
	close(fd);
	return ret;
}

static int do_hash(struct md_options *opts)
{
	struct kcapi_handle *handle;
	off_t offset = 0, size = 0;
	const char *hashname;
	uint8_t *memblk = NULL;
	uint8_t *pdata;
	size_t mapped, left;
	uint32_t todo, mdsiz;
	uint8_t md[64];
	ssize_t ret = 0;
	FILE *fp = NULL;

	hashname = opts->cipher_name;
	ret = kcapi_md_init(&handle, hashname, 0);
	if (ret) {
		fprintf(stderr, "Allocation of %s cipher failed (ret=%d)\n",
			hashname, (int)ret);
		return -EFAULT;
	}

	if (opts->hmac) {
		ret = kcapi_md_setkey(handle, opts->hmac_key, opts->keylen);
		if (ret) {
			fprintf(stderr, "Setting HMAC key for %s failed (%d)\n",
				hashname, (int)ret);
			kcapi_md_destroy(handle);
			return -EINVAL;
		}
	}

	/* Mapping file in 16M segments */
	mapped = 16 << 20;
	do {
		ret = mmap_file(opts->in_file, &memblk, &size, &mapped, offset);
		if (ret) {
			fprintf(stderr, "Failed to map file %s\n",
				opts->in_file);
			return (int)ret;
		}
		/* Compute hash */
		pdata = memblk;
		left = mapped;
		do {
			todo = (left > INT_MAX) ? INT_MAX : (uint32_t)left;

			ret = kcapi_md_update(handle, pdata, todo);
			if (ret < 0) {
				munmap(memblk, mapped);
				goto out;
			}

			left -= todo;
			pdata += todo;
		} while (left);

		munmap(memblk, mapped);
		offset = offset + (off_t)mapped;
	} while (offset < size);

	mdsiz = kcapi_md_digestsize(handle);
	ret = kcapi_md_final(handle, md, sizeof(md));
	if (ret <= 0)
		goto out;

	if (opts->out_flag) {
		fp = fopen(opts->out_file, "wb");
		if (!fp)
			fprintf(stderr, "Failed to open %s\n", opts->out_file);
	} 
	if (fp) {
		fwrite(md, 1, mdsiz, fp);
		fclose(fp);
	} else if (!opts->hexout) {
		/* Default output format */
		printf("%s:\t", hashname);
		bin2print(md, mdsiz);
		printf("\n");
	}

	if (opts->hexout) {
		/* output format for CI test */
		printf("HEX:");
		bin2print(md, mdsiz);
		printf("\n");
	}

out:
	kcapi_md_destroy(handle);
	return (int)ret;
}

static int check_options(struct md_options *opts)
{
	if (opts->hmac && opts->key_flag == 0) {
		printf("Key is required.\n");
		return -EINVAL;
	}
	if (opts->in_flag == 0) {
		printf("Input is required.\n");
		return -EINVAL;
	}

	return 0;
}

static void usage_md(struct subcmd_cfg *cmd)
{
	printf("Usage:\n");
	printf("  %s %s <options>\n", cmd->app_name, cmd->cmd_name);
	printf("    --help:         display this help.\n");
	printf("    --keyfile:      HMAC key file\n");
	printf("    --in:           binary input data file.\n");
	printf("    --out:          binary output file name if provided.\n");
	printf("    --hexout:       hex output.\n");
}

static int digest_cmd_arg_parse(int argc, char *argv[], struct md_options *opts)
{
	int c = 0;
	FILE *fp;
	int opt_index = 0;

	static struct option cmd_opts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "keyfile", required_argument, 0, 'k' },
		{ "in", required_argument, 0, 'i' },
		{ "out", required_argument, 0, 'o' },
		{ "hexout", no_argument, 0, 'H' },
		{ 0, 0, 0, 0 }
	};

	while (1) {
		c = getopt_long_only(argc, argv, "h", cmd_opts, &opt_index);
		if (c < 0)
			break;

		switch (c) {
		case 'k': /* keyfile*/
			fp = fopen(optarg, "rb");
			if (!fp) {
				printf("Failed to open %s\n", optarg);
				return -EINVAL;
			}
			opts->keylen =
				fread(opts->hmac_key, 1, MAX_KEY_SIZE, fp);
			fclose(fp);
			fp = NULL;
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
		case 'H':
			opts->hexout = 1;
			break;
		case 'h':
			return 1;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

int digest_command(struct subcmd_cfg *cmd, int argc, char *argv[])
{
	struct md_options md_opts;
	int ret = 0;

	memset(&md_opts, 0, sizeof(md_opts));

	if (cmd->keylen)
		md_opts.hmac = 1;

	if (md_opts.hmac) {
		md_opts.hmac_key = (uint8_t *)malloc(MAX_KEY_SIZE);
		if (!md_opts.hmac_key) {
			printf("Failed to malloc key buffer.\n");
			return -ENOMEM;
		}
	}

	ret = digest_cmd_arg_parse(argc, argv, &md_opts);
	if (ret > 0) {
		usage_md(cmd);
		ret = 0;
		goto out;
	}
	if (ret < 0)
		goto out;

	if (check_options(&md_opts)) {
		ret = -EINVAL;
		goto out;
	}

	strcpy(md_opts.cipher_name, cmd->cipher_name);

	ret = do_hash(&md_opts);
out:
	if (md_opts.hmac_key)
		free(md_opts.hmac_key);
	return ret;
}
