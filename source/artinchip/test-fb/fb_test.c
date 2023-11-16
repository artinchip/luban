// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>

/* Global macro and variables */

#define AICFB_LAYER_MAX_NUM	2
#define FB_DEV "/dev/fb0"

static const char sopts[] = "nscflLaAkKedw:h:m:v:u";
static const struct option lopts[] = {
	{"get_layer_num",	no_argument, NULL, 'n'},
	{"get_screen_size",	no_argument, NULL, 's'},
	{"get_layer_cap",	no_argument, NULL, 'c'},
	{"get_fb_layer",	no_argument, NULL, 'f'},
	{"get_layer",		no_argument, NULL, 'l'},
	{"set_layer",		no_argument, NULL, 'L'},
	{"get_alpha",		no_argument, NULL, 'a'},
	{"set_alpha",	  required_argument, NULL, 'A'},
	{"get_ck_cfg",		no_argument, NULL, 'k'},
	{"set_ck_cfg",	  required_argument, NULL, 'K'},
	{"enable",		no_argument, NULL, 'e'},
	{"disable",		no_argument, NULL, 'd'},
	{"id",		  required_argument, NULL, 'i'},
	{"width",	  required_argument, NULL, 'w'},
	{"height",	  required_argument, NULL, 'h'},
	{"mode",	  required_argument, NULL, 'm'},
	{"value",	  required_argument, NULL, 'v'},
	{"usage",		no_argument, NULL, 'u'},
	{0, 0, 0, 0}
};

/* Functions */

void usage(char *program)
{
	printf("Usage: %s [options]: \n", program);
	printf("\t -n, --get_layer_num \n");
	printf("\t -s, --get_screen_size \n");
	printf("\t -c, --get_layer_cap \n");
	printf("\t -f, --get_fb_layer \n");
	printf("\t -l, --get_layer \n");
	printf("\t -L, --set_layer\tneed other options: -i x -e/d -w y -h z\n");
	printf("\t -a, --get_alpha \n");
	printf("\t -A, --set_alpha\tneed other options: -e/d -m x -v y \n");
	printf("\t -k, --get_ck_cfg \n");
	printf("\t -K, --set_ck_cfg\tneed other options: -e/d -v x \n");
	printf("\t -e, --enable \n");
	printf("\t -d, --disable \n");
	printf("\t -i, --id\t\tneed an integer argument of Layer ID [0, 1]\n");
	printf("\t -w, --width\t\tneed an integer argument\n");
	printf("\t -h, --height\t\tneed an integer argument\n");
	printf("\t -m, --mode\t\tneed an integer argument [0, 2]\n");
	printf("\t -v, --value\t\tneed an integer argument [0, 255]\n");
	printf("\t -u, --usage \n");
	printf("\n");
	printf("Example: %s -l\n", program);
	printf("Example: %s -L -i 1 -e -w 800 -h 480\n", program);
	printf("Example: %s -A -e -w 800 -h 480\n", program);
	printf("Example: %s -K -e -v 0x3F\n", program);
}

/* Open a device file to be needed. */
int device_open(char *_fname, int _flag)
{
	s32 fd = -1;

	fd = open(_fname, _flag);
	if (fd < 0) {
		ERR("Failed to open %s", _fname);
		exit(0);
	}
	return fd;
}

int get_layer_num(int fd)
{
	int ret = 0;
	struct aicfb_layer_num n = {0};

	ret = ioctl(fd, AICFB_GET_LAYER_NUM, &n);
	if (ret < 0) {
		ERR("ioctl() return %d\n", ret);
	}
	else {
		printf("The number of video layer: %d\n", n.vi_num);
		printf("The number of UI layer: %d\n", n.ui_num);
	}
	return ret;
}

int get_layer_cap(int fd)
{
	int i;
	int ret = 0;
	struct aicfb_layer_capability cap = {0};

	for (i = 0; i < AICFB_LAYER_MAX_NUM; i++) {
		memset(&cap, 0, sizeof(struct aicfb_layer_capability));
		cap.layer_id = i;
		ret = ioctl(fd, AICFB_GET_LAYER_CAPABILITY, &cap);
		if (ret < 0) {
			ERR("ioctl() return %d\n", ret);
			return ret;
		}

		printf("\n--------------- Layer %d ---------------\n", i);
		printf("Type: %s\n", cap.layer_type == AICFB_LAYER_TYPE_VIDEO ?
			"Video" : "UI");
		printf("Max Width: %d (%#x)\n", cap.max_width, cap.max_width);
		printf("Max height: %d (%#x)\n", cap.max_height, cap.max_height);
		printf("Flag: %#x\n", cap.cap_flags);
		printf("---------------------------------------\n");
	}
	return ret;
}

int get_one_layer_cfg(int fd, int cmd, int id)
{
	int ret = 0;
	struct aicfb_layer_data layer = {0};

	layer.layer_id = id;
	ret = ioctl(fd, cmd, &layer);
	if (ret < 0) {
		ERR("ioctl() return %d\n", ret);
		return ret;
	}

	printf("\n--------------- Layer %d ---------------\n", layer.layer_id);
	printf("Enable: %d\n", layer.enable);
	printf("Layer ID: %d\n", layer.layer_id);
	printf("Rect ID: %d\n", layer.rect_id);
	printf("Scale Size: w %d, h %d\n",
		layer.scale_size.width, layer.scale_size.height);
	printf("Position: x %d, y %d\n", layer.pos.x, layer.pos.y);
	printf("Buffer: \n");
	printf("\tPixel format: %#x\n", layer.buf.format);
	printf("\tPhysical addr: %#x %#x %#x\n",
		layer.buf.phy_addr[0], layer.buf.phy_addr[1],
		layer.buf.phy_addr[2]);
	printf("\tStride: %d %d %d\n", layer.buf.stride[0],
		layer.buf.stride[1], layer.buf.stride[2]);
	printf("\tSize: w %d, h %d\n",
		layer.buf.size.width, layer.buf.size.height);
	printf("\tCrop enable: %d\n", layer.buf.crop_en);
	printf("\tCrop: x %d, y %d, w %d, h %d\n",
		layer.buf.crop.x, layer.buf.crop.y,
		layer.buf.crop.width, layer.buf.crop.height);
	printf("\tFlag: %#x\n", layer.buf.flags);
	printf("---------------------------------------\n");

	return 0;
}

int get_fb_layer_cfg(int fd)
{
	return get_one_layer_cfg(fd, AICFB_GET_FB_LAYER_CONFIG, 0);
}

int get_layer_cfg(int fd)
{
	int i;

	for (i = 0; i < AICFB_LAYER_MAX_NUM; i++)
		get_one_layer_cfg(fd, AICFB_GET_LAYER_CONFIG, i);

	return 0;
}

int set_layer_cfg(int fd, int id, int enable, int width, int height)
{
	int ret = 0;
	struct aicfb_layer_data layer = {0};

	if ((id < 0) || (enable < 0) || (width < 0) || (height < 0)) {
		ERR("Invalid argument.\n");
		return -1;
	}

	layer.layer_id = id;
	layer.enable = enable;
	layer.scale_size.width = layer.buf.size.width;
	layer.scale_size.height = layer.buf.size.height;
	ret = ioctl(fd, AICFB_UPDATE_LAYER_CONFIG, &layer);
	if (ret < 0)
		ERR("ioctl() return %d\n", ret);

	return ret;
}

int get_layer_alpha(int fd)
{
	int i;
	int ret = 0;
	struct aicfb_alpha_config alpha = {0};

	for (i = 1; i < AICFB_LAYER_MAX_NUM; i++) {
		alpha.layer_id = i;
		ret = ioctl(fd, AICFB_GET_ALPHA_CONFIG, &alpha);
		if (ret < 0) {
			ERR("ioctl() return %d\n", ret);
			return ret;
		}

		printf("\n--------------- Layer %d ---------------\n", i);
		printf("Alpha enable: %d\n", alpha.enable);
		printf("Alpla mode: %d (0, pixel; 1, global; 2, mix)\n",
			alpha.mode);
		printf("Alpha value: %d (%#x)\n", alpha.value, alpha.value);
		printf("---------------------------------------\n");
	}

	return 0;
}

int set_layer_alpha(int fd, int enable, int mode, int val)
{
	int ret = 0;
	struct aicfb_alpha_config alpha = {0};

	if ((enable < 0) || (mode < 0) || (val < 0)) {
		ERR("Invalid argument.\n");
		return -1;
	}

	alpha.layer_id = 1;
	alpha.enable = enable;
	alpha.mode = mode;
	alpha.value = val;
	ret = ioctl(fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);
	if (ret < 0)
		ERR("ioctl() return %d\n", ret);

	return ret;
}

int get_ck_cfg(int fd)
{
	int i;
	int ret = 0;
	struct aicfb_ck_config ck = {0};

	for (i = 1; i < AICFB_LAYER_MAX_NUM; i++) {
		ck.layer_id = i;
		ret = ioctl(fd, AICFB_GET_CK_CONFIG, &ck);
		if (ret < 0) {
			ERR("ioctl() return %d\n", ret);
			return ret;
		}

		printf("\n--------------- Layer %d ---------------\n", i);
		printf("Color key enable: %d\n", ck.enable);
		printf("Color key value: R %#x, G %#x, B %#x\n",
			(ck.value >> 16) & 0xFF, (ck.value >> 8) & 0xFF,
			ck.value & 0xFF);
		printf("---------------------------------------\n");
	}
	return 0;
}

int set_ck_cfg(int fd, int enable, int val)
{
	int ret = 0;
	struct aicfb_ck_config ck = {0};

	if ((enable < 0) || (val < 0)) {
		ERR("Invalid argument.\n");
		return -1;
	}

	ck.layer_id = 1;
	ck.enable = enable;
	ck.value = val;
	ret = ioctl(fd, AICFB_UPDATE_CK_CONFIG, &ck);
	if (ret < 0)
		ERR("ioctl() return %d\n", ret);

	return ret;
}

int get_screen_size(int fd)
{
	int ret = 0;
	struct mpp_size s = {0};

	ret = ioctl(fd, AICFB_GET_SCREEN_SIZE, &s);
	if (ret < 0) {
		ERR("ioctl() return %d\n", ret);
		return ret;
	}

	printf("Screen width: %d (%#x)\n", s.width, s.width);
	printf("Screen height: %d (%#x)\n", s.height, s.height);
	return 0;
}

int main(int argc, char **argv)
{
	int dev_fd = -1;
	int ret = 0;
	int c = 0;
	int layer_id = 0;
	int enable = 0;
	int mode = 0;
	int width = 0;
	int height = 0;
	int value = 0;

	dev_fd = device_open(FB_DEV, O_RDWR);
	if (dev_fd < 0) {
		ERR("Failed to open %s, return %d\n", FB_DEV, dev_fd);
		return -1;
	}

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'n':
			ret = get_layer_num(dev_fd);
			goto end;
		case 's':
			ret = get_screen_size(dev_fd);
			goto end;
		case 'c':
			ret = get_layer_cap(dev_fd);
			goto end;
		case 'f':
			ret = get_fb_layer_cfg(dev_fd);
			goto end;
		case 'l':
			ret = get_layer_cfg(dev_fd);
			goto end;
		case 'a':
			ret = get_layer_alpha(dev_fd);
			goto end;
		case 'k':
			ret = get_ck_cfg(dev_fd);
			goto end;
		case 'e':
			enable = 1;
			continue;
		case 'd':
			enable = 0;
			continue;
		case 'i':
			layer_id = str2int(optarg);
			continue;
		case 'w':
			width = str2int(optarg);
			continue;
		case 'h':
			height = str2int(optarg);
			continue;
		case 'm':
			mode = str2int(optarg);
			continue;
		case 'v':
			value = str2int(optarg);
			continue;
		case 'u':
			usage(argv[0]);
			goto end;
		default:
			continue;
		}
	}

	optind = 0;
	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'L':
			ret = set_layer_cfg(dev_fd, layer_id, enable, width, height);
			goto end;
		case 'A':
			ret = set_layer_alpha(dev_fd, enable, mode, value);
			goto end;
		case 'K':
			ret = set_ck_cfg(dev_fd, enable, value);
			goto end;
		default:
			continue;
		}
	}

end:
	if (dev_fd > 0)
		close(dev_fd);

	return ret;
}
