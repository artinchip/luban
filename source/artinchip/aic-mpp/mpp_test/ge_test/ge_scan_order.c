/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#include <signal.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "mpp_ge.h"

#define FB_DEV		"/dev/fb0"
#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"
#define SCAN_ORDER_BMP	DATADIR"/scan_order.bmp"

#define ALIGN_8B(x) (((x) + (7)) & ~(7))

#define UPPER_LEFT  	0
#define UPPER_RIGHT 	1
#define LOWER_RIGHT 	2
#define LOWER_LEFT  	3
#define STRAIGHT_LEFT   4
#define DIRECTLY_ABOVE  5
#define STRAIGHT_RIGHT  6
#define DIRECTLY_BELOW  7

#define MMAP_CHECK(x) ((x) == ((unsigned char *) - 1))

/* height and width of one sprite */
#define XSIZE 1024
#define YSIZE 600

static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
static unsigned char *g_fb_buf = NULL;

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

	if (ioctl(g_fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	g_screen_w = var.xres;
	g_screen_h = var.yres;
	g_fb_len = fix.smem_len;
	g_fb_phy = fix.smem_start;
	g_fb_stride = layer.buf.stride[0];
	g_fb_format = layer.buf.format;

	g_fb_buf = mmap(NULL, g_fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        g_fb_fd, 0);
	if (g_fb_buf == MAP_FAILED) {
		ERR("mmap framebuffer");
		close(g_fb_fd);
		g_fb_fd = -1;
		g_fb_buf = NULL;
		return -1;
	}

	memset(g_fb_buf, 0, g_fb_len);

	DBG("screen_w = %d, screen_h = %d, stride = %d, format = %d\n",
			var.xres, var.yres, g_fb_stride, g_fb_format);
	return 0;
}

static void fb_close(void)
{
	if (!g_fb_buf)
		munmap(g_fb_buf, g_fb_len);

	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-r, --region the  region of screen (default 0),The range of region is 0~7\n");
	printf("\t-m, --mode   scan order flags for GE ctrl (default 0)\n");
	printf("\t    --mode=0 scan order flags = MPP_LR_TB\n");
	printf("\t    --mode=1 scan order flags = MPP_RL_TB\n");
	printf("\t    --mode=2 scan order flags = MPP_LR_BT\n");
	printf("\t    --mode=3 scan order flags = MPP_RL_BT\n");

	printf("\t-u, --usage\n");
	printf("\t-h, --help\n\n");
}

static void help(void)
{
	printf("\r--This is ge bitblt operation using scan order function.\n");
	printf("\r--Scan order function is used when src and dst memory overlap.\n");
	printf("\r--Scan order flags defaults to MPP_LR_TB whether the memory overlaps or not.\n");
	printf("\r--When scan order is not set to MPP_LR_TB,rot0 and dither are not supported,only rgb format is supported.\n\n");

	printf("\r--There are eight types of memory overlap in screen memory for reference.\n\n");
	printf("\r--In the case of SRC and DST memory overlap,it is recommended to set scan order flags as follows.\n");
	printf("\r--If the DST layer is in the upper left  corner of the SRC layer,scan order flags is recommended to be MPP_LR_TB.\n");
	printf("\r--If the DST layer is in the upper right corner of the SRC layer,scan order flags is recommended to be MPP_RL_TB.\n");
	printf("\r--If the DST layer is in the lower left  corner of the SRC layer,scan order flags is recommended to be MPP_LR_BT.\n");
	printf("\r--If the DST layer is in the lower right corner of the SRC layer,scan order flags is recommended to be MPP_RL_BT.\n");

	printf("\r--If the DST layer is in the directly above the SRC layer,scan order flags is recommended to be MPP_LR_TB or MPP_RL_TB.\n");
	printf("\r--If the DST layer is in the directly below the SRC layer,scan order flags is recommended to be MPP_LR_BT or MPP_RL_BT.\n");
	printf("\r--If the DST layer is on the straight left  side of the SRC layer,scan order flags is recommended to be MPP_LR_TB or MPP_LR_BT.\n");
	printf("\r--If the DST layer is on the straight right side of the SRC layer,scan order flags is recommended to be MPP_RL_TB or MPP_RL_BT.\n\n");

	printf("\r--Please use parameter -u or --usage to list other parameters.\n\n");
}

static int src_bmp_open(const char *bmp_path)
{
	int src_fd = -1;

	src_fd = open(bmp_path, O_RDWR);
	if (src_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			bmp_path, errno, strerror(errno));
		return -1;
	}

	lseek(src_fd, 54, SEEK_SET);

	return src_fd;
}

static void src_bmp_close(int src_fd)
{
	if (src_fd > 0)
		close(src_fd);
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
	int ret = 0, i = 0;
	unsigned int width = XSIZE;
	unsigned int line_length, data_length;

	dmabuf = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
	if (dmabuf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	data_length = ALIGN_8B(width * 3);
	line_length = ALIGN_8B(width * 3);
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

static void ge_fill_background_set(struct ge_bitblt *blt, int src_dma_fd)
{
	int width = XSIZE;
	int height = YSIZE;

	/* source buffer */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = src_dma_fd;
	blt->src_buf.stride[0] = ALIGN_8B(width * 3);
	blt->src_buf.size.width = width;
	blt->src_buf.size.height = height;
	blt->src_buf.format =  MPP_FMT_RGB_888;

	blt->src_buf.crop_en = 0;
	blt->src_buf.crop.x = 0;
	blt->src_buf.crop.y = 0;

	/*  buffer */
	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.phy_addr[0] = g_fb_phy;
	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;

	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = g_screen_w/4;
	blt->dst_buf.crop.y = g_screen_h/4;
	blt->dst_buf.crop.width = g_screen_w/2;
	blt->dst_buf.crop.height = g_screen_h/2;

	blt->ctrl.flags = MPP_FLIP_V;
}

static int ge_fill_background(struct mpp_ge *ge, struct ge_bitblt *blt, int src_dma_fd)
{
	int ret = 0;

	ge_fill_background_set(blt, src_dma_fd);

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("emit task failed: %d\n", ret);
		return ret;
	}

	return ret;
}

static void ge_scan_one_order(struct ge_bitblt *blt, unsigned int mode, unsigned int dstx, unsigned int dsty)
{
	/* source buffer */
	blt->src_buf.buf_type = MPP_PHY_ADDR;
	blt->src_buf.phy_addr[0] = g_fb_phy;
	blt->src_buf.stride[0] = g_fb_stride;
	blt->src_buf.size.width = g_screen_w;
	blt->src_buf.size.height = g_screen_h;
	blt->src_buf.format = g_fb_format;

	blt->src_buf.crop_en = 1;
	blt->src_buf.crop.x = g_screen_w/4;
	blt->src_buf.crop.y = g_screen_h/4;
	blt->src_buf.crop.width = g_screen_w/2;
	blt->src_buf.crop.height = g_screen_h/2;

	/* destination buffer */
	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.phy_addr[0] = g_fb_phy;
	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;

	/* only in this case,
	 * dst_buf.crop.width==src_buf.size.width ||
	 * dst_buf.crop.height==src_buf.size.height
	 * scan order flags can use perameters MPP_RL_TB,MPP_LR_BT,MPP_RL_BT
	*/
	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = dstx;
	blt->dst_buf.crop.y = dsty;
	blt->dst_buf.crop.width = g_screen_w/2;
	blt->dst_buf.crop.height = g_screen_h/2;

	/* scan order flage */
	blt->ctrl.flags = mode;
}

static int ge_scan_order(struct mpp_ge *ge, struct ge_bitblt *blt, unsigned int mode, unsigned int dest_area)
{
	int ret = 0;
	unsigned int dstx = 0;
	unsigned int dsty = 0;

	/* choice dst region */
	switch (dest_area) {
	case UPPER_LEFT:
		dstx = 0;
		dsty = 0;
		break;
	case UPPER_RIGHT:
		dstx = g_screen_w/2;
		dsty = 0;
		break;
	case LOWER_RIGHT:
		dstx = g_screen_w/2;
		dsty = g_screen_h/2;
		break;
	case LOWER_LEFT:
		dstx = 0;
		dsty = g_screen_h/2;
		break;
	case STRAIGHT_LEFT:
		dstx = 0;
		dsty = g_screen_h/4;
		break;
	case DIRECTLY_ABOVE:
		dstx = g_screen_w/4;
		dsty = 0;
		break;
	case STRAIGHT_RIGHT:
		dstx = g_screen_w/2;
		dsty = g_screen_h/4;
		break;
	case DIRECTLY_BELOW:
		dstx = g_screen_w/4;
		dsty = g_screen_h/2;
		break;
	default:
		break;
	}

	ge_scan_one_order(blt, (mode<<16), dstx, dsty);

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("mpp_ge_bitblt failed:%d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("emit task failed:%d\n", ret);
		return ret;
	}

	return ret;
}

int main(int argc, char **argv)
{
	struct mpp_ge *ge = NULL;
	int ret = 0;
	unsigned int region = 0;
	unsigned int mode = 0;
	int heap_fd = -1;
	int src_fd = -1;
	int fsize = 0;
	int src_dmabuf_fd = -1;
	struct ge_bitblt blt = {0};

	const char sopts[] = "uhr:m:";
	const struct option lopts[] = {
		{"usage",	no_argument,       NULL, 'u'},
		{"help",	no_argument,       NULL, 'h'},
		{"region",	required_argument, NULL, 'r'},
		{"mode",	required_argument, NULL, 'm'},
	};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 'r':
			region = str2int(optarg);
			if (region < 0 || region > 7)  region=0;
			break;
		case 'm':
			mode = str2int(optarg);
			if (mode < 0 || mode >3)  mode=0;
			break;
		case 'u':
			usage(argv[0]);
			goto EXIT;
		case 'h':
			help();
			goto EXIT;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			goto EXIT;
		}
	}

	ge = mpp_ge_open();
	if (!ge) {
		ERR("open ge device error\n");
		goto EXIT;
	}

	if (fb_open()) {
		ERR("fb_open error\n");
		goto EXIT;
	}

	heap_fd = open(DMA_HEAP_DEV, O_RDWR);
	if (heap_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		goto EXIT;
	}

	src_fd = src_bmp_open(SCAN_ORDER_BMP);
	fsize = ALIGN_8B((XSIZE * YSIZE * 3));
	src_dmabuf_fd = dmabuf_request_one(heap_fd, fsize);
	if (src_dmabuf_fd < 0) {
		ERR("src_dmabuf_fd task failed\n");
		goto EXIT;
	}

	ret = draw_src_dmabuf(src_dmabuf_fd, src_fd, fsize);
	if (ret < 0) {
		ERR("draw_src_dmabuf task failed\n");
		goto EXIT;
	}

	mpp_ge_add_dmabuf(ge, src_dmabuf_fd);

	ret = ge_fill_background(ge, &blt, src_dmabuf_fd);
	if (ret) {
		ERR("fill_background failed:%d\n", ret);
		goto EXIT;
	}

	sleep(1);

	ret = ge_scan_order(ge, &blt, mode, region);
	if (ret) {
		ERR("ge_scan_order failed: %d\n", ret);
		goto EXIT;
	}

	ret = mpp_ge_sync(ge);
	if (ret) {
		ERR("sync task failed: %d\n", ret);
		goto EXIT;
	}

EXIT:
	if (src_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, src_dmabuf_fd);
		close(src_dmabuf_fd);
	}

	if (heap_fd > 0)
		close(heap_fd);

	if (src_fd > 0)
		src_bmp_close(src_fd);

	if (ge)
		mpp_ge_close(ge);

	fb_close();

	return 0;
}
