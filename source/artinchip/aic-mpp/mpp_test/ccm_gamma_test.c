/*
 * Copyright (C) 2023-2024 ArtinChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ArtInChip
 */
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <stdbool.h>

#include <getopt.h>
#include "artinchip/sample_base.h"
#include "video/artinchip_fb.h"

#define FB_DEV "/dev/fb0"
#define FLAGS_NONE  0x00
#define FLAGS_CCM   (0x1 << 1)
#define FLAGS_GAMMA (0x1 << 2)

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-m, --mode, ");
	printf("\t0(default): disable ccm and gamma, 1: just enable ccm\n");
	printf("\t\t2: just enable gamma, 3: enable ccm and gamma\n");
	printf("\t-c, --ccm \n");
	printf("\t-r, --gammared \n");
	printf("\t-g, --gammagreen \n");
	printf("\t-b, --gammablue \n");
	printf("\t-u, --usage \n");
	printf("\n");
}

static void gamma_lut_parse(unsigned int channel, unsigned int num,
                            struct aicfb_gamma_config *gamma)
{
	int len, i, index;
	char str[3] = {0};
	unsigned int val[4] = {0};
	unsigned int gamma_lut;

	len = strlen(optarg);
	if (len != 16 * 2) {
		ERR("Invaild gamma table, table len %d\n", len);
		return;
	}

	for (i = 0; i < 16; i++) {
		strncpy(str, optarg + (i * 2), 2);

		index = i % 4;
		val[index] = strtoll(str, NULL, 16);

		if (index == 3) {
			/* Convert into gamma lut format */
			gamma_lut = (val[0] << 0)  | (val[1] << 8) |
						(val[2] << 16) | (val[3] << 24);
			gamma->gamma_lut[channel][num * 4 + i / 4] = gamma_lut;
		}
	}
}

static int device_open(char *_fname, int _flag)
{
	s32 fd = -1;

	fd = open(_fname, _flag);
	if (fd < 0) {
		ERR("Failed to open %s", _fname);
		exit(0);
	}
	return fd;
}

int main(int argc, char **argv)
{
	int dev_fd = -1;
	int c, ret = 0, mode = 0, num = 0;
	int len, i;
	char str[5] = {0};
	struct aicfb_ccm_config ccm;
	struct aicfb_gamma_config gamma;
	unsigned int flags = FLAGS_NONE;

	const char sopts[] = "m:c:n:r:g:b:u";
	const struct option lopts[] = {
		{"mode",       required_argument, NULL, 'm'},
		{"ccm",        required_argument, NULL, 'c'},
		{"num",        required_argument, NULL, 'n'},
		{"gammared",   required_argument, NULL, 'r'},
		{"gammagreen", required_argument, NULL, 'g'},
		{"gammablue",  required_argument, NULL, 'b'},
		{"usage",            no_argument, NULL, 'u'},
		{0, 0, 0, 0}
	};

	dev_fd = device_open(FB_DEV, O_RDWR);
	if (dev_fd < 0) {
		ERR("Failed to open %s, return %d\n", FB_DEV, dev_fd);
		return -1;
	}

	ioctl(dev_fd, AICFB_GET_CCM_CONFIG, &ccm);
	ioctl(dev_fd, AICFB_GET_GAMMA_CONFIG, &gamma);

	optind = 0;
	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'u':
			usage(argv[0]);
			return 0;
		case 'm':
			mode = str2int(optarg);
			switch (mode) {
			case 0:
				ccm.enable = 0;
				gamma.enable = 0;
				break;
			case 1:
				gamma.enable = 0;
				break;
			case 2:
				ccm.enable = 0;
				break;
			default:
				break;
			}

			break;
		case 'n':
			num = str2int(optarg);
			break;
		case 'c':
			len = strlen(optarg);
			if (len != 12 * 4) {
				ERR("Invaild ccm table, table len %d\n", len);
				break;
			}

			for (i = 0; i < 12; i++) {
				strncpy(str, optarg + (i * 4), 4);

				ccm.ccm_table[i] = strtoll(str, NULL, 16);
			}

			flags |= FLAGS_CCM;
			break;
		case 'r':
			gamma_lut_parse(GAMMA_RED, num, &gamma);
			break;
		case 'g':
			gamma_lut_parse(GAMMA_GREEN, num, &gamma);
 			break;
		case 'b':
			gamma_lut_parse(GAMMA_BLUE, num, &gamma);

			if (num == 3)
				flags |= FLAGS_GAMMA;

			break;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			usage(argv[0]);
			return 0;
		}
    }

	if (flags & FLAGS_GAMMA)
		gamma.enable = 1;

	ioctl(dev_fd, AICFB_UPDATE_GAMMA_CONFIG, &gamma);

	if (flags & FLAGS_CCM)
		ccm.enable = 1;

	ioctl(dev_fd, AICFB_UPDATE_CCM_CONFIG, &ccm);

	close(dev_fd);
	return 0;
}
