// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <stdint.h>

#include <video/tp2825B.h>

#include <artinchip/sample_base.h>

/* Global macro and variables */

#define TP2825_DEV			"/dev/tp2825"
#define TP2825_MAX_CH			DIFF_VIN34
#define TP2825_MAX_MODE			TP2802_960P25
#define UNDEFINED			"Undefined"

static const char sopts[] = "m:c:h";
static const struct option lopts[] = {
	{"mode",	  required_argument, NULL, 'm'},
	{"channel",	  required_argument, NULL, 'c'},
	{"usage",		no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static char *tp2825_modes[] = {
	"720P60",		// 0x00
	"720P50",		// 0x01
	"1080P30",		// 0x02
	"1080P25",		// 0x03
	"720P30",		// 0x04
	"720P25",		// 0x05
	"SD",			// 0x06
	"INVALID_FORMAT",	// 0x07
	"PAL",			// 0x08
	"NTSC",			// 0x09
	UNDEFINED,
	UNDEFINED,
	"720P30V2",		// 0x0C
	"720P25V2",		// 0x0D
	UNDEFINED,
	UNDEFINED,		// 0x0F

	UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, // 0x10~0x13
	UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, // 0x14~0x17
	UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, // 0x18~0x1B
	UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, // 0x1C~0x1F

	"3M18",			// 0x20, 2048x1536@18.75 for TVI
	"5M12",			// 0x21, 2592x1944@12.5 for TVI
	"4M15",			// 0x22, 2688x1520@15 for TVI
	"3M20",			// 0x23, 2048x1536@20 for TVI
	"4M12",			// 0x24, 2688x1520@12.5 for TVI
	"6M10",			// 0x25, 3200x1800@10 for TVI
	"QHD30",		// 0x26, 2560x1440@30 for TVI/HDA/HDC
	"QHD25",		// 0x27, 2560x1440@25 for TVI/HDA/HDC
	"QHD15",		// 0x28, 2560x1440@15 for HDA
	"QXGA18",		// 0x29, 2048x1536@18 for HDA/TVI
	"QXGA30",		// 0x2A, 2048x1536@30 for HDA
	"QXGA25",		// 0x2B, 2048x1536@25 for HDA
	"4M30",			// 0x2C, 2688x1520@30 for TVI(for future)
	"4M25",			// 0x2D, 2688x1520@25 for TVI(for future)
	"5M20",			// 0x2E, 2592x1944@20 for TVI/HDA
	"8M15",			// 0x2f, 3840x2160@15 for TVI

	"8M12",			// 0x30, 3840x2160@12.5 for TVI
	"1080P15",		// 0x31, 1920x1080@15 for TVI
	"1080P60",		// 0x32, 1920x1080@60 for TVI
	"960P30",		// 0x33, 1280x960@30 for TVI
	"1080P20",		// 0x34, 1920x1080@20 for TVI
	"1080P50",		// 0x35, 1920x1080@50 for TVI
	"720P14",		// 0x36, 1280x720@14 for TVI
	"720P30HDR",		// 0x37, 1280x720@30 for TVI
	"6M20",			// 0x38, 2960x1920@20 for CVI
	"8M15V2",		// 0x39, 3264x2448@15 for TVI
	"5M20V2",		// 0x3a, 2960x1664@20 for TVI
	"8M7",			// 0x3b, 3720x2160@7.5 for AHD
	"3M20V2",		// 0x3c, 2304x1296@20 for TVI
	"1080P2750",		// 0x3d, 1920x1080@27.5 for TVI
	"1080P2700",		// 0x3e, 1920x1080@27.5 for TVI
	UNDEFINED,

	UNDEFINED,
	"1080P12",		// 0x41, 1920x1080@12.5
	"5M12V2",		// 0x42, 2960x1664@12.5 for TVI
	"960P25",		// 0x43, 1280x960@25 for AHD
};

/* Functions */

void usage(char *program)
{
	printf("Usage: %s [options]: \n", program);
	printf("\t -m, --mode\t\toutput mode\n");
	printf("\t -c, --channel\t\tthe number of video channel, [0, 3]\n");
	printf("\t -h, --usage \n");
	printf("\n");
	printf("Example: %s -m 0xC -c 1\n", program);
}

/* Open a device file to be needed. */
static int device_open(char *_fname, int _flag)
{
	s32 fd = -1;

	fd = open(_fname, _flag);
	if (fd < 0) {
		ERR("Failed to open %s errno: %d[%s]\n",
			_fname, errno, strerror(errno));
		exit(0);
	}
	return fd;
}

static int tp2825_get_ch(int fd, uint8_t *ch)
{
	tp2802_register tp_reg = {0};
	int ret = 0;

	ret = ioctl(fd, TP2802_GET_VIDEO_INPUT, &tp_reg);
	if (ret < 0) {
		ERR("Failed to get current video channel. return %d\n", ret);
		return -1;
	}
	*ch = tp_reg.value;
	return 0;
}

static int tp2825_set_ch(int fd, uint8_t ch)
{
	tp2802_register tp_reg = {0};
	int ret = 0;

	tp_reg.value = ch;
	ret = ioctl(fd, TP2802_SET_VIDEO_INPUT, &tp_reg);
	if (ret < 0) {
		ERR("Failed to set the video channel. return %d\n", ret);
		return -1;
	}
	return 0;
}

static int tp2825_get_mode(int fd, tp2802_video_mode *out)
{
	int ret = 0;
	tp2802_video_mode vmode = {0};
	tp2802_video_mode *pmode = &vmode;

	if (out)
		pmode = out;

	ret = tp2825_get_ch(fd, &pmode->ch);
	if (ret < 0)
		return -1;

	ret = ioctl(fd, TP2802_GET_VIDEO_MODE, pmode);
	if (ret < 0) {
		ERR("Failed to get current video mode. return %d\n", ret);
		return -1;
	}
	printf("Current mode: Chip%d, ch%d, mode %s, std %d\n",
		pmode->chip, pmode->ch, tp2825_modes[pmode->mode], pmode->std);

	return 0;
}

static int tp2825_set_mode(int fd, uint32_t ch, uint32_t mode)
{
	tp2802_video_mode vmode = {0};
	int ret = 0;

	ret = tp2825_get_mode(fd, &vmode);
	if (ret < 0)
		return -1;

	if ((vmode.ch == ch) && (vmode.mode == mode)) {
		printf("Don't need change anything.\n");
		return 0;
	}
	printf("\tChange ch %d -> %d, mode %s -> %s\n",
		vmode.ch, ch,
		tp2825_modes[vmode.mode], tp2825_modes[mode]);

	if (vmode.ch != ch) {
		ret = tp2825_set_ch(fd, ch);
		if (ret < 0)
			return -1;
	}

	if (vmode.mode != mode) {
		vmode.ch = ch;
		vmode.mode = mode;
		ret = ioctl(fd, TP2802_SET_VIDEO_MODE, &vmode);
		if (ret < 0) {
			ERR("Failed to set video mode. return %d\n", ret);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int c, ret = 0, changed = 0;
	uint32_t mode = TP2802_PAL, ch = 0;
	int dev_fd = -1;

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'm':
			mode = str2int(optarg);
			if (mode > TP2825_MAX_MODE) {
				ERR("Invalid mode: %s\n", optarg);
				return -1;
			}
			if (!strcmp(tp2825_modes[mode], UNDEFINED)) {
				ERR("%s mode: %s\n", UNDEFINED, optarg);
				return -1;
			}
			changed = 1;
			break;
		case 'c':
			ch = str2int(optarg);
			if (ch > TP2825_MAX_CH) {
				ERR("Invalid channel: %s\n", optarg);
				return -1;
			}
			changed = 1;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			break;
		}
	}

	dev_fd = device_open(TP2825_DEV, O_RDWR);
	if (dev_fd < 0)
		return -1;

	if (changed)
		ret = tp2825_set_mode(dev_fd, ch, mode);
	else
		ret = tp2825_get_mode(dev_fd, NULL);

	if (dev_fd > 0)
		close(dev_fd);

	return ret;
}
