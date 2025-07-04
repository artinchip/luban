// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2025 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/fb.h>
#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>
#include <stdbool.h>
#include <sys/time.h>

/* Global macro and variables */

#define AICFB_LAYER_MAX_NUM	2
#define FB_DEV "/dev/fb0"

static const char sopts[] = "nscflLaAkKedC:i:w:h:m:v:bru";
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
	{"crop_enable",   required_argument, NULL, 'C'},
	{"id",		  required_argument, NULL, 'i'},
	{"width",	  required_argument, NULL, 'w'},
	{"height",	  required_argument, NULL, 'h'},
	{"mode",	  required_argument, NULL, 'm'},
	{"value",	  required_argument, NULL, 'v'},
	{"colorblock",		no_argument, NULL, 'b'},
	{"repeat",		no_argument, NULL, 'r'},
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
	printf("\t -L, --set_layer\tneed other options: -i x -e/d -w y -h z -C 1/0\n");
	printf("\t -a, --get_alpha \n");
	printf("\t -A, --set_alpha\tneed other options: -e/d -m x -v y \n");
	printf("\t -k, --get_ck_cfg \n");
	printf("\t -K, --set_ck_cfg\tneed other options: -e/d -v x \n");
	printf("\t -e, --enable \n");
	printf("\t -d, --disable \n");
	printf("\t -C, --crop_en \n");
	printf("\t -i, --id\t\tneed an integer argument of Layer ID [0, 1]\n");
	printf("\t -w, --width\t\tneed an integer argument\n");
	printf("\t -h, --height\t\tneed an integer argument\n");
	printf("\t -m, --mode\t\tneed an integer argument [0, 2]\n");
	printf("\t -v, --value\t\tneed an integer argument [0, 255]\n");
	printf("\t -b, --colorblock\tshow a color-block image\n");
	printf("\t -r, --repeat\ttest vsync repeatedly\n");
	printf("\t -u, --usage \n");
	printf("\n");
	printf("Example: %s -l\n", program);
	printf("Example: %s -L -i 1 -e -w 800 -h 480 -C 1\n", program);
	printf("Example: %s -A -e -w 800 -h 480\n", program);
	printf("Example: %s -K -e -v 0x3F\n", program);
}

struct video_data_format {
	enum mpp_pixel_format format;
	char f_str[15];
	int pixel_bytes;
};

struct video_data_format g_vformat[] = {
	{MPP_FMT_XRGB_8888, "xrgb8888", 4},
	{MPP_FMT_ARGB_8888, "argb8888", 4},
	{MPP_FMT_ARGB_4444, "argb4444", 2},
	{MPP_FMT_ARGB_1555, "argb1555", 2},
	{MPP_FMT_RGB_565,   "rgb565"  , 2},
	{MPP_FMT_RGB_888,   "rgb888"  , 3},
};

static inline bool is_yuv_format(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
	case MPP_FMT_YUV400:
	case MPP_FMT_YUV420P:
	case MPP_FMT_YUV422P:
	case MPP_FMT_YUV420_128x16_TILE:
	case MPP_FMT_YUV420_64x32_TILE:
	case MPP_FMT_YUV422_128x16_TILE:
	case MPP_FMT_YUV422_64x32_TILE:
		return true;
	default:
		break;
	}
	return false;
}

static inline bool is_rgb_format(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_RGB_888:
	case MPP_FMT_RGB_565:
		return true;
	default:
		break;
	}
	return false;
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
	} else {
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

int set_layer_cfg(int fd, int id, int enable, int width, int height, int crop_enable)
{
	int ret = 0;
	struct aicfb_layer_data layer = {0};
	struct video_data_format vlayer = {0};

	if ((id != AICFB_LAYER_TYPE_UI && id != AICFB_LAYER_TYPE_VIDEO) || (enable < 0) ||
	    (width < 0) || (height < 0)) {
		ERR("Invalid argument.\n");
		return -1;
	}

	if(id == AICFB_LAYER_TYPE_UI) {
		/* Get the current configuration of UI layer */
		layer.layer_id = id;
		ret = ioctl(fd, AICFB_GET_FB_LAYER_CONFIG, &layer);
		if (ret < 0) {
			ERR("ioctl() return %d\n", ret);
			return ret;
		}
		if (width > layer.buf.size.width || height > layer.buf.size.height) {
			ERR("Width %d x Height %d is out of range.\n", width, height);
			return -1;
		}

		/* Set the configuration of UI layer */
		layer.enable      = enable;
		layer.buf.crop_en = crop_enable;
		layer.buf.crop.x  = 0;
		layer.buf.crop.y  = 0;
		layer.buf.crop.width = width;
		layer.buf.crop.height = height;
		ret = ioctl(fd, AICFB_UPDATE_LAYER_CONFIG, &layer);
		if (ret < 0)
			ERR("ioctl() return %d\n", ret);
	}

	if(id == AICFB_LAYER_TYPE_VIDEO) {
		/* Get the current configuration of Video layer */
		layer.layer_id = id;
		ret = ioctl(fd, AICFB_GET_LAYER_CONFIG, &layer);
		if (ret < 0) {
			ERR("ioctl() return %d\n", ret);
			return ret;
		}
		if (width > layer.buf.size.width || height > layer.buf.size.height) {
			ERR("Width %d x Height %d is out of range.\n", width, height);
			return -1;
		}

		/* Get the current configuration of Video layer */
		if(is_yuv_format(layer.buf.format)) {
			layer.scale_size.width  = width;
			layer.scale_size.height = height;
		}

		if(is_rgb_format(layer.buf.format)) {
			layer.buf.stride[0] = width * vlayer.pixel_bytes;
		}

		layer.enable      = enable;
		layer.buf.crop_en = crop_enable;
		layer.buf.crop.x  = 0;
		layer.buf.crop.y  = 0;
		layer.buf.crop.width  = width;
		layer.buf.crop.height = height;

		ret = ioctl(fd, AICFB_UPDATE_LAYER_CONFIG, &layer);
		if (ret < 0)
			ERR("ioctl() return %d\n", ret);
	}
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

int show_color_block(int fd)
{
	struct fb_fix_screeninfo fix = {0};
	struct fb_var_screeninfo var = {0};

	int i, j, color, step, blk_line, pixel_size, width, height, blk_height;
	int colors24[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
	int steps24[]  = {0x010000, 0x000100, 0x000001, 0x010101};
	int colors16[] = {0xF800, 0x07E0, 0x001F, 0xFFDF};
	int steps16[]  = {0x0800, 0x0020, 0x0001, 0x0841};
	int *colors = colors24;
	int *steps  = steps24;
	char *fb_buf = NULL, *line1 = NULL, *line2 = NULL;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		ERR("ioctl FBIOGET_FSCREENINFO");
		return -1;
	}
	if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		return -1;
	}

	pixel_size = var.bits_per_pixel / 8;
	if (pixel_size == 2) {
		colors = colors16;
		steps  = steps16;
	}

	fb_buf = (char *)mmap(NULL, fix.smem_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
	if (fb_buf == MAP_FAILED) {
		ERR("mmap framebuffer");
		return -1;
	}
	memset(fb_buf, 0, fix.smem_len);

	width  = var.xres;
	height = var.yres;
	printf("Framebuf: size %d, width %d, height %d, bits per pixel %d\n",
		fix.smem_len, width, height, var.bits_per_pixel);

	blk_height = height / 4;
	line1 = fb_buf;
	for (i = 0; i < height; i++) {
		blk_line = i / blk_height;
		color = colors[blk_line];
		step = steps[blk_line];
		for (j = 0; j < width; j++) {
			memcpy(&line1[j * pixel_size], &color, pixel_size);

			if (pixel_size == 2) {
			if (j && (j % 4 == 0)) /* Enlarge the step range for RGB564 */
				color -= step;
			} else {
				color -= step;
			}

			if (color == 0) {
				color = colors[blk_line];
				if (pixel_size == 2)
					j += 4;
				else
					j++;
			}
		}
		line1 += width * pixel_size;
	}

	/* Draw the location line */

	line1 = &fb_buf[width * pixel_size] + pixel_size;
	line2 = &fb_buf[width * (height - 2) * pixel_size - 101 * pixel_size];
	for (i = 0; i < 100; i++) {
		memcpy(&line1[i * pixel_size], &colors[3], pixel_size);
		memcpy(&line2[i * pixel_size], &colors[3], pixel_size);
	}

	line1 = &fb_buf[width * pixel_size] + pixel_size;
	line2 = &fb_buf[width * (height - 101) * pixel_size - pixel_size];
	for (i = 0; i < 100; i++) {
		memcpy(&line1[0], &colors[3], pixel_size);
		line1 += width * pixel_size;
		memcpy(&line2[0], &colors[3], pixel_size);
		line2 += width * pixel_size;
	}

	munmap(fb_buf, fix.smem_len);
	return 0;
}

static int test_vsync_repeat(int fd)
{
	int ret, i = 0;
	struct timeval start, end;
	unsigned long time_us;

	do {
		gettimeofday(&start, NULL);
		ret = ioctl(fd, AICFB_WAIT_FOR_VSYNC, NULL);
		if (ret) {
			ERR("ioctl WAIT_FOR_VSYNC timeout, DE not working\n");
			break;
		}
		gettimeofday(&end, NULL);

		time_us = (end.tv_sec - start.tv_sec) * 1000000 +
			  (end.tv_usec - start.tv_usec);
		DBG("wait vsync time %ld us\n", time_us);
	} while (i++ < 5);

	return ret;
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
	int crop_enable = 0;

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
		case 'C':
			crop_enable = str2int(optarg);
			continue;
		case 'm':
			mode = str2int(optarg);
			continue;
		case 'v':
			value = str2int(optarg);
			continue;
		case 'b':
			show_color_block(dev_fd);
			goto end;
		case 'r':
			ret = test_vsync_repeat(dev_fd);
			goto end;
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
			ret = set_layer_cfg(dev_fd, layer_id, enable, width, height, crop_enable);
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
