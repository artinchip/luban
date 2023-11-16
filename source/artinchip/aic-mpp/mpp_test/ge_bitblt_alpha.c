/*
 * Copyright (C) 2022-2023 Artinchip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <signal.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include "mpp_ge.h"

#define ALIGN_8B(x) (((x) + (7)) & ~(7))

#define FB_DEV		"/dev/fb0"
#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"
#define SINGER_IMAGE	DATADIR"/singer_alpha.bmp"
#define URANDOM_DEV 	"/dev/urandom"

/* height and width of one sprite */
#define XSIZE 285
#define YSIZE 259

static int g_urandom_fd = 0;
static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
unsigned char *g_fb_buf = NULL;

static const char sopts[] = "n:u";
static const struct option lopts[] = {
	{"number",	required_argument, NULL, 'n'},
	{"usage",	no_argument,       NULL, 'u'},
};

static int urandom_open(void)
{
	g_urandom_fd = open(URANDOM_DEV, O_RDWR);
	if (g_urandom_fd == -1) {
		ERR("open %s", URANDOM_DEV);
		return -1;
	}
	return 0;
}

static void urandom_close(void)
{
	if (g_urandom_fd > 0)
		close(g_urandom_fd);
}

static int fb_open(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct aicfb_layer_data layer;

	g_fb_fd = open(FB_DEV, O_RDWR);
	if (g_fb_fd == -1) {
		ERR("open %s", FB_DEV);
		return -1;
	}

	if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		ERR("ioctl FBIOGET_FSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	if(ioctl(g_fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	g_screen_w = var.xres;
	g_screen_h = var.yres;
	g_fb_len = fix.smem_len;
	g_fb_phy = fix.smem_start;
	g_fb_stride = fix.line_length;
	g_fb_format = layer.buf.format;

	g_fb_buf = mmap(NULL, g_fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        g_fb_fd, 0);
	if (g_fb_buf == (unsigned char *)-1) {
		ERR("mmap framebuffer");
		close(g_fb_fd);
		g_fb_fd = -1;
		g_fb_buf = NULL;
		return -1;
	}

	DBG("screen_w = %d, screen_h = %d, stride = %d, format = %d\n",
			var.xres, var.yres, g_fb_stride, g_fb_format);
	return 0;
}

static void fb_close(void)
{
	if (!g_fb_buf) {
		munmap(g_fb_buf, g_fb_len);
	}
	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static int src_bmp_open(void)
{
	int fd = -1;

	fd = open(SINGER_IMAGE, O_RDWR);
	if (fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			SINGER_IMAGE, errno, strerror(errno));
		return -1;
	}
	lseek(fd, 54, SEEK_SET);

	return fd;
}

static void src_bmp_close(int fd)
{
	if (fd > 0)
		close(fd);
}

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-n, --number  the number of image to blt (default 3)\n");
	printf("\t-u, --usage\n\n");
}

static int dmabuf_request_one(int fd, int len)
{
	int ret;
	struct dma_heap_allocation_data data = {0};

	if (len < 0) {
		ERR("Invalid len %d\n", len);
		return -1;
	}

	data.fd = 0;
	data.len = len;
	data.fd_flags = O_RDWR | O_CLOEXEC;
	data.heap_flags = 0;
	ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &data);
	if (ret < 0) {
		ERR("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	DBG("Get dma_heap fd: %d\n", data.fd);

	return data.fd;
}

static int draw_src_dmabuf(int dmabuf_fd, int fd, int len)
{
	unsigned char *dmabuf = NULL, *buf = NULL;
	int ret = 0, i= 0;
	unsigned int width = XSIZE;
	unsigned int line_length, data_length;

	dmabuf = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
	if (dmabuf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	data_length = width * 4;
	line_length = ALIGN_8B(width * 4);
	buf = dmabuf;

	for (i = 0; i < YSIZE; i++) {
		ret = read(fd, buf, data_length);
		if (ret != data_length) {
			ERR("read(%d) return %d. errno: %d[%s]\n", data_length,
				ret, errno, strerror(errno));
			return -1;
		}
		buf += line_length;
	}

	munmap(dmabuf, len);
	return 0;
}

static void ge_set_bitblt(int src_fd, struct ge_bitblt *blt)
{
	int width = XSIZE;
	int height = YSIZE;
	unsigned int value[5] = {0};
	unsigned int dst_x, dst_y, ctrl_flags, scale;
	int scale_flag;

	do {
		read(g_urandom_fd, value, sizeof(value));

		ctrl_flags = value[0] % 6;
		scale_flag = value[1];
		scale = value[2] % 2 + 1;

		if (scale_flag > 0) {
			dst_x = value[3] % (g_screen_w - (width * scale));
			dst_y = value[4] % (g_screen_h - (width * scale));
		} else {
			scale = scale + ctrl_flags;
			dst_x = value[3] % (g_screen_w - (width / scale));
			dst_y = value[4] % (g_screen_h - (width / scale));
		}

	} while (0);

	/* source buffer */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = src_fd;
	blt->src_buf.stride[0] = ALIGN_8B(width * 4);
	blt->src_buf.size.width = width;
	blt->src_buf.size.height = height;
	blt->src_buf.format = MPP_FMT_ARGB_8888;
	blt->src_buf.crop_en = 0;

	/* dstination buffer */
	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.phy_addr[0] = g_fb_phy;
	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;

	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = dst_x;
	blt->dst_buf.crop.y = dst_y;

	if (scale_flag > 0) {
		width = width * scale;
		height = height * scale;
	} else {
		width = width / scale;
		height = height / scale;
	}

	blt->dst_buf.crop.width = width;
	blt->dst_buf.crop.height = height;

	blt->ctrl.alpha_en = 1;
	blt->ctrl.alpha_rules = GE_PD_NONE;

	/* rotation or filp */
	switch (ctrl_flags) {
	case 0:
		blt->ctrl.flags = MPP_ROTATION_0;
		break;
	case 1:
		blt->ctrl.flags = MPP_ROTATION_90;
		blt->dst_buf.crop.width = height;
		blt->dst_buf.crop.height = width;
		break;
	case 2:
		blt->ctrl.flags = MPP_ROTATION_180;
		break;
	case 3:
		blt->ctrl.flags = MPP_ROTATION_270;
		blt->dst_buf.crop.width = height;
		blt->dst_buf.crop.height = width;
		break;
	case 4:
		blt->ctrl.flags = MPP_FLIP_H;
		break;
	case 5:
		blt->ctrl.flags = MPP_FLIP_V;
		break;
	default:
		break;
	};
}

int main(int argc, char **argv)
{
	struct mpp_ge *ge = NULL;
	int ret = 0, i = 0;
	int src_fd = -1;
	int src_dmabuf_fd = -1;
	int heap_fd = -1;
	int fsize = 0;
	int num = 3;
	struct ge_bitblt blt = {0};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 'u':
			usage(argv[0]);
			goto EXIT;
		case 'n':
			num = str2int(optarg);
			break;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			goto EXIT;
		}
	}

	ge = mpp_ge_open();
	if (!ge) {
		ERR("open ge device\n");
		exit(1);
	}

	if (fb_open()) {
		fb_close();
		mpp_ge_close(ge);
		ERR("fb_open\n");
		exit(1);
	}

	if (urandom_open()) {
		fb_close();
		mpp_ge_close(ge);
		ERR("urandom_open()\n");
		exit(1);
	}

	heap_fd = open(DMA_HEAP_DEV, O_RDWR);
	if (heap_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		goto EXIT;
	}

	src_fd = src_bmp_open();
	if (src_fd < 0) {
		ERR("Failed to open bmp file, errno: %d[%s]\n",
			errno, strerror(errno));
		goto EXIT;
	}

	fsize = ALIGN_8B(XSIZE * 4) * YSIZE;
	src_dmabuf_fd = dmabuf_request_one(heap_fd, fsize);
	if (src_dmabuf_fd < 0) {
		ERR("Failed to request dmabuf\n");
		goto EXIT;
	}

	draw_src_dmabuf(src_dmabuf_fd, src_fd, fsize);
	mpp_ge_add_dmabuf(ge, src_dmabuf_fd);

	do {
		ge_set_bitblt(src_dmabuf_fd, &blt);

		ret = mpp_ge_bitblt(ge, &blt);
		if (ret) {
			ERR("bitblt task failed:%d\n", ret);
			goto EXIT;
		}

		ret = mpp_ge_emit(ge);
		if (ret) {
			ERR("emit task failed:%d\n", ret);
			goto EXIT;
		}

		ret = mpp_ge_sync(ge);
		if (ret) {
			ERR("sync task failed:%d\n", ret);
			goto EXIT;
		}

		i++;
	} while(i < num);

EXIT:
	if (src_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, src_dmabuf_fd);
		close(src_dmabuf_fd);
	}

	if (heap_fd > 0)
		close(heap_fd);

	if (ge)
		mpp_ge_close(ge);

	fb_close();
	src_bmp_close(src_fd);
	urandom_close();
	return 0;
}
