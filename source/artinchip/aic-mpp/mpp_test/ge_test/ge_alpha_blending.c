/*
 * Copyright (C) 2022-2023 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#include <signal.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "artinchip/sample_base.h"
#include "video/artinchip_fb.h"
#include "mpp_ge.h"

#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))

#define FB_DEV		"/dev/fb0"
#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"

#define ALPHA_SRC_PATH		DATADIR"/alpha_src.bmp"
#define ALPHA_DST_PATH		DATADIR"/alpha_dst.bmp"
#define ALPHA_BACK_GROUND_PATH	DATADIR"/alpha_back_ground.bmp"

#define PICTURE_SRC 		0
#define PICTURE_DST 		1
#define PICTURE_BACK_GROUND  	2
#define PICTURE_NUM		3
#define ALPHA_BACK_GROUND_NUM   14

/* bmp header format */
#pragma pack(push, 1)
struct bmp_header {
	unsigned short type;
	unsigned int size;
	unsigned short reserved1;
	unsigned short reserved2;
	unsigned int offset;

	unsigned int dib_size;
	int width;
	int height;
	unsigned short planes;
	unsigned short bit_count;
	unsigned int compression;
	unsigned int size_image;
	int x_meter;
	int y_meter;
	unsigned int clr_used;
	unsigned int clr_important;
};
#pragma pack(pop)

/* alpha test structure */
struct alpha_data {
	int dma_fd;
	char *bmp_path;
	int bmp_fd;
	int bmp_reverse;
	unsigned short file_type;
	unsigned int bmp_stride;
	unsigned int bmp_width;
	unsigned int bmp_height;
	unsigned int height;
	unsigned int stride;

	unsigned int src_alpha_mode;
	unsigned int src_global_alpha;
	unsigned int dst_alpha_mode;
	unsigned int dst_global_alpha;
};

/* screen information structure */
struct g_screen {
	int screen_w;
	int screen_h;
	int fb_fd;
	int fb_len;
	int fb_stride;
	unsigned int fb_format;
	unsigned int fb_phy;
	unsigned char *fb_buf;
};

static struct alpha_data alpha[PICTURE_NUM] ={0};
static struct g_screen screen = {0};

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n\n", app, __DATE__, __TIME__);
	printf(
	"\t-s, --src_alpha_mode,   Select src_alpha_mode (default 0), "
	"0: pixel alpha mode, 1: global alpha mode, 2: mixded alpha mode"
	"\n"
	"\t-g, --src_alpha_global, Set src_alpha_global value (default 0), "
	"source global alpha value (0~255)"
	"\n\n"
	"\t-d, --dst_alpha_mode,   Select dst_alpha_mode (default 0), "
	"0: pixel alpha mode, 1: global alpha mode, 2: mixded alpha mode"
	"\n"
	"\t-p, --dst_alpha_global, Select dst_alpha_global value (default 0), "
	"destination global alpha value (0~255)"
	"\n"

	"\t-u, --usage\n");
}

static int fb_open(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct aicfb_layer_data layer;

	screen.fb_fd = open(FB_DEV, O_RDWR);
	if (screen.fb_fd == -1) {
		ERR("open %s", FB_DEV);
		return -1;
	}

	if (ioctl(screen.fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		ERR("ioctl FBIOGET_FSCREENINFO");
		close(screen.fb_fd);
		return -1;
	}

	if (ioctl(screen.fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(screen.fb_fd);
		return -1;
	}

	if(ioctl(screen.fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(screen.fb_fd);
		return -1;
	}

	screen.screen_w = var.xres;
	screen.screen_h = var.yres;
	screen.fb_len = fix.smem_len;
	screen.fb_phy = fix.smem_start;
	screen.fb_stride = layer.buf.stride[0];
	screen.fb_format = layer.buf.format;

	screen.fb_buf = mmap(NULL, screen.fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        screen.fb_fd, 0);

	if (screen.fb_buf == MAP_FAILED) {
		ERR("mmap framebuffer");
		close(screen.fb_fd);

		screen.fb_fd = -1;
		screen.fb_buf = NULL;
		return -1;
	}

	return 0;
}

static void fb_close(void)
{
	if (!screen.fb_buf)
		munmap(screen.fb_buf, screen.fb_len);

	if (screen.fb_fd > 0)
		close(screen.fb_fd);
}

static int bmp_open(void)
{
	int i = 0;
	struct bmp_header bmp_header = {0};

	/* set bmp file path */
	alpha[0].bmp_path = ALPHA_SRC_PATH;
	alpha[1].bmp_path = ALPHA_DST_PATH;
	alpha[2].bmp_path = ALPHA_BACK_GROUND_PATH;

	for(i = 0; i < PICTURE_NUM; i++) {
		alpha[i].bmp_fd = open(alpha[i].bmp_path, O_RDWR);
		if (alpha[i].bmp_fd < 0) {
			ERR("Failed to open %s, errno: %d[%s]\n",
				alpha[i].bmp_path, errno, strerror(errno));
			return -1;
		}

		if (read(alpha[i].bmp_fd, &bmp_header, sizeof(struct bmp_header)) !=
			sizeof(struct bmp_header)) {
			ERR("read bmp file error \n");
			return -1;
		}

		alpha[i].file_type = bmp_header.type;
		alpha[i].bmp_width = bmp_header.width;
		alpha[i].bmp_height = abs(bmp_header.height);

		if (bmp_header.height < 0)
			alpha[i].bmp_reverse = 1;
	}

	return 0;
}

static void bmp_close(void)
{
	int i = 0;

	for(i = 0; i < PICTURE_NUM; i++) {
		if (alpha[i].bmp_fd > 0)
			close(alpha[i].bmp_fd);
	}
}

static void set_alpha(int src_alpha_mode, int dst_alpha_mode,
			   int src_alpha_global, int dst_alpha_global)
{
	int i = 0;

	alpha[0].src_alpha_mode = src_alpha_mode;
	alpha[0].src_global_alpha = src_alpha_global;

	alpha[1].dst_alpha_mode = dst_alpha_mode;
	alpha[1].dst_global_alpha = dst_alpha_global;

	/* Set stride, height, bmp_stride, when bmp file format is argb8888 */
	for(i = 0; i < PICTURE_NUM; i++) {
		alpha[i].stride = BYTE_ALIGN((alpha[i].bmp_width * 4), 8);
		alpha[i].height = alpha[i].bmp_height;
		alpha[i].bmp_stride = BYTE_ALIGN((alpha[i].bmp_width * 4), 4);
	}
}

static int dmabuf_heap_open()
{
	int fd = 0;

	fd = open(DMA_HEAP_DEV, O_RDWR);
	if (fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		return -1;
	}

	return fd;
}

static void dmabuf_heap_close(int heap_fd)
{
	if (heap_fd > 0)
		close(heap_fd);
}

static int dmabuf_request(int heap_fd)
{
	int i = 0;
	struct dma_heap_allocation_data data = {0};

	for(i = 0; i < PICTURE_NUM; i++) {
		data.fd = 0;
		data.len = alpha[i].stride * alpha[i].height;
		data.fd_flags = O_RDWR | O_CLOEXEC;
		data.heap_flags = 0;

		if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
			ERR("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));
			return -1;
		}

		alpha[i].dma_fd = data.fd;
	}

	return 0;
}

static void dmabuf_release()
{
	int i = 0;

	for(i = 0; i < PICTURE_NUM; i++) {
		if (alpha[i].dma_fd > 0)
			close (alpha[i].dma_fd);
	}
}

static int draw_bmp_dmabuf(int pic_num)
{
	int i = 0;
	int ret = 0;
	int len = 0;
	unsigned char *buf = NULL;
	unsigned char *dmabuf = NULL;

	if (alpha[pic_num].file_type != 0x4D42) {
		ERR("not a Bmp file\n");
		return -1;
	}

	len = alpha[pic_num].stride * alpha[pic_num].height;
	dmabuf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, alpha[pic_num].dma_fd, 0);
	if (dmabuf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	/* read from bottom to top */
	if (alpha[i].bmp_reverse == 1) {
		buf = dmabuf;
		for (i = 0; i < alpha[pic_num].bmp_height; i++) {
			ret = read(alpha[pic_num].bmp_fd, buf,alpha[pic_num].bmp_stride);
			if (ret != alpha[pic_num].bmp_stride) {
				ERR("read(%d) . errno: %d[%s]\n", alpha[pic_num].bmp_stride,
					errno, strerror(errno));
				return -1;
			}

			buf += alpha[pic_num].stride;
		}
	/* read from top to bottom */
	} else {
		for (i = alpha[pic_num].bmp_height -1; i >= 0; i--) {
			buf = dmabuf + (i * alpha[pic_num].stride);
			ret = read(alpha[pic_num].bmp_fd, buf,alpha[pic_num].bmp_stride);
			if (ret != alpha[pic_num].bmp_stride) {
				ERR("read(%d) . errno: %d[%s]\n", alpha[pic_num].bmp_stride,
					errno, strerror(errno));
				return -1;
			}
		}
	}

	munmap(dmabuf, len);
	lseek(alpha[pic_num].bmp_fd, sizeof(struct bmp_header), SEEK_SET);

	return 0;
}

static void ge_add_pic_dmabuf(struct mpp_ge *ge)
{
	int i = 0;

	for (i = 0; i < PICTURE_NUM; i++)
	{
		if (alpha[i].dma_fd > 0)
			mpp_ge_add_dmabuf(ge, alpha[i].dma_fd);
	}
}

static void ge_rm_pic_dmabuf(struct mpp_ge *ge)
{
	int i = 0;

	for (i = 0; i < PICTURE_NUM; i++)
	{
		if (alpha[i].dma_fd > 0)
			mpp_ge_rm_dmabuf(ge, alpha[i].dma_fd);
	}
}

static int set_display_pos(int *pos_x, int *pos_y, int alpha_rules)
{
	static int pos[][2] = {
		{450, 420}, {640, 420}, {70, 60}, {450, 60}, {640, 60},
		{830, 60}, {70, 240}, {260, 240}, {450, 240}, {640, 240},
		{830, 240}, {260, 420}, {70, 420}, {260, 60}, {0, 0}
	};

	if (alpha_rules > ALPHA_BACK_GROUND_NUM || alpha_rules < 0) {
		ERR("set_display_pos error, alpha_rules = %d\n", alpha_rules);
		return -1;
	}

	*pos_x = pos[alpha_rules][0];
	*pos_y = pos[alpha_rules][1];

	return 0;
}

static void ge_display_set(struct ge_bitblt *blt, int pic_num, int alpha_rules)
{
	int x = 0;
	int y = 0;

	memset(blt, 0, sizeof(struct ge_bitblt));

	/* src layer settings */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = alpha[pic_num].dma_fd;
	blt->src_buf.stride[0] = alpha[pic_num].stride;
	blt->src_buf.size.width = alpha[pic_num].bmp_width;
	blt->src_buf.size.height = alpha[pic_num].bmp_height;
	blt->src_buf.format =  MPP_FMT_ARGB_8888;
	blt->src_buf.crop_en = 0;

	/* dst layer settings */
	if (set_display_pos(&x, &y, alpha_rules) < 0)
		ERR("ge_display_pos error.\n");

	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.stride[0] = screen.fb_stride;
	blt->dst_buf.phy_addr[0] = screen.fb_phy;
	blt->dst_buf.size.width = screen.screen_w;
	blt->dst_buf.size.height = screen.screen_h;
	blt->dst_buf.format = screen.fb_format;
	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = x;
	blt->dst_buf.crop.y = y;
	blt->dst_buf.crop.width = alpha[pic_num].bmp_width;
	blt->dst_buf.crop.height = alpha[pic_num].bmp_height;
}

static void ge_porter_duff_set(struct ge_bitblt *blt, int src_num, int dst_num, int alpha_rule)
{
	memset(blt, 0, sizeof(struct ge_bitblt));

	/* src layer settings  */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = alpha[src_num].dma_fd;
	blt->src_buf.stride[0] = alpha[src_num].stride;
	blt->src_buf.size.width = alpha[src_num].bmp_width;
	blt->src_buf.size.height = alpha[src_num].bmp_height;
	blt->src_buf.format = MPP_FMT_ARGB_8888;
	blt->ctrl.src_alpha_mode = alpha[src_num].src_alpha_mode;
	blt->ctrl.src_global_alpha = alpha[src_num].src_global_alpha;

	/* dst layer settings */
	blt->dst_buf.buf_type = MPP_DMA_BUF_FD;
	blt->dst_buf.fd[0] =  alpha[dst_num].dma_fd;
	blt->dst_buf.stride[0] = alpha[dst_num].stride;
	blt->dst_buf.size.width = alpha[dst_num].bmp_width;
	blt->dst_buf.size.height = alpha[dst_num].bmp_height;
	blt->dst_buf.format = MPP_FMT_ARGB_8888;
	blt->ctrl.dst_alpha_mode = alpha[dst_num].dst_alpha_mode;
	blt->ctrl.dst_global_alpha = alpha[dst_num].dst_global_alpha;

	blt->ctrl.alpha_en = 1;
	blt->ctrl.alpha_rules = alpha_rule;
}

static int run_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *blt)
{
	int ret = -1;

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("mpp_ge_bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("mpp_ge_emit task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_sync(ge);
	if (ret) {
		ERR("mpp_ge_sync task failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ge_porter_duff_test(struct mpp_ge *ge, struct ge_bitblt *blt)
{
	int i = 0;
	int ret = -1;

	for (i = 0; i < PICTURE_NUM; i++)
	{
		/* draw all images */
		ret = draw_bmp_dmabuf(i);
		if (ret < 0) {
			ERR("draw_bmp_dmabuf error\n");
			return -1;
		}
	}

	/* display background */
	ge_display_set(blt, PICTURE_BACK_GROUND, ALPHA_BACK_GROUND_NUM);
	ret = run_ge_bitblt(ge, blt);
	if (ret < 0) {
		ERR("run_ge_bitblt error\n");
		return -1;
	}

	for (i = 0; i < ALPHA_BACK_GROUND_NUM; i++)
	{
		/* set according to different porter duff rules */
		ge_porter_duff_set(blt, PICTURE_SRC, PICTURE_DST, i);
		ret = run_ge_bitblt(ge, blt);
		if (ret < 0) {
			ERR("run_ge_bitblt error\n");
			return -1;
		}

		ge_display_set(blt, PICTURE_DST, i);
		ret = run_ge_bitblt(ge, blt);
		if (ret < 0) {
			ERR("run_ge_bitblt error\n");
			return -1;
		}

		/* redraw dst image */
		ret = draw_bmp_dmabuf(PICTURE_DST);
		if (ret < 0) {
			ERR("draw_bmp_dmabuf error\n");
			return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = -1;
	int heap_fd = -1;
	int src_alpha_mode = 0;
	int dst_alpha_mode = 0;
	int src_alpha_global = 0;
	int dst_alpha_global = 0;
	struct mpp_ge *ge = NULL;
	struct ge_bitblt blt = {0};

	const char sopts[] = "us:d:g:p:";
	const struct option lopts[] = {
		{"usage",		no_argument,       NULL, 'u'},
		{"src_alpha_mode" ,	required_argument, NULL, 's'},
		{"dst_alpha_mode",	required_argument, NULL, 'd'},
		{"src_alpha_global",	required_argument, NULL, 'g'},
		{"dst_alpha_global",	required_argument, NULL, 'p'},
	};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 's':
			src_alpha_mode = str2int(optarg);
			if ((src_alpha_mode > 3) || (src_alpha_mode < 0)) {
				printf("src_alpha_mode invalid, please input against\n");
				goto EXIT;
			}
			break;
		case 'd':
			dst_alpha_mode = str2int(optarg);
			if ((dst_alpha_mode > 3) || (dst_alpha_mode < 0)) {
				printf("dst_alpha_mode invalid, please input against\n");
				goto EXIT;
			}
			break;
		case 'g':
			src_alpha_global = str2int(optarg);
			if ((src_alpha_global > 255) || (src_alpha_global < 0)) {
				printf("src_alpha_global invalid, please input against\n");
				goto EXIT;
			}
			break;
		case 'p':
			dst_alpha_global = str2int(optarg);
			if ((dst_alpha_global > 255) || (dst_alpha_global < 0)) {
				printf("dst_alpha_global invalid, please input against\n");
				goto EXIT;
			}
			break;
		case 'u':
			usage(argv[0]);
			goto EXIT;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			goto EXIT;
		}
	}

	ge = mpp_ge_open();
	if (!ge) {
		ERR("mpp_ge_open error\n");
		goto EXIT;
	}

	ret = fb_open();
	if (ret < 0) {
		ERR("fb_open error\n");
		goto EXIT;
	}

	ret = bmp_open();
	if (ret < 0) {
		ERR("bmp_open error\n");
		goto EXIT;
	}

	heap_fd = dmabuf_heap_open();
	if (heap_fd < 0) {
		ERR("dmabuf_heap_open error\n");
		goto EXIT;
	}

	set_alpha(src_alpha_mode, dst_alpha_mode, src_alpha_global, dst_alpha_global);

	ret = dmabuf_request(heap_fd);
	if (ret < 0) {
		ERR("dmabuf_request error\n");
		goto EXIT;
	}

	ge_add_pic_dmabuf(ge);

	ret = ge_porter_duff_test(ge, &blt);
	if (ret < 0) {
		ERR("ge_porter_duff_test error\n");
		goto EXIT;
	}

EXIT:
	ge_rm_pic_dmabuf(ge);

	dmabuf_release();

	dmabuf_heap_close(heap_fd);

	bmp_close();

	if (ge)
		mpp_ge_close(ge);

	fb_close();

	return 0;
}
