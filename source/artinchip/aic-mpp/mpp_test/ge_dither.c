/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#include <string.h>

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
#define SINGER_32BIT_IMAGE	DATADIR"/singer_alpha.bmp"

#define SRC_DISP_REGION 0
#define DST_DISP_REGION 1

/* str to format table struct */
struct StrToFormat {
	char *str;
	int format;
};

/* bmp header format */
#pragma pack(push, 1)
struct BmpHeader {
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

static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
static unsigned char *g_fb_buf = NULL;
struct BmpHeader bmp_header = {0};

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-o, --dither_on,  Select open dither (default 0), 0 :close  1 :open\n");
	printf("\t-s, --src_format, Select src format  (default argb8888)\n");
	printf("\t-d, --dst_format, Select dst format  (default argb4444)\n");

	printf("\t-u, --usage\n");
	printf("\t-h, --help, list the supported format and the test instructions\n\n");
}

static void help(void)
{
	printf("\r--This is ge bitblt operation using dither function.\n");
	printf("\r--Dither supports input argb8888 and argb888 formats, and output argb4444, argb1555 and argb565 formats.\n");
	printf("\r--In this example, the input layer is src, output layer is dst.\n");
	printf("\r--In addition, the uppercase and lowercase letters of the src and dst parameters does not affect.\n");

	printf("\r--Please use parameter -u or --usage to list other parameters.\n\n");
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

	return 0;
}

static void fb_close(void)
{
	if (!g_fb_buf) {
		munmap(g_fb_buf, g_fb_len);
	}

	if (g_fb_fd > 0) {
		close(g_fb_fd);
	}
}

static int ori_bmp_open(int *ori_fd)
{
	*ori_fd = open(SINGER_32BIT_IMAGE, O_RDWR);
	if (*ori_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			SINGER_32BIT_IMAGE, errno, strerror(errno));
		return -1;
	}

	if (read(*ori_fd, &bmp_header, sizeof(struct BmpHeader)) !=
		sizeof(struct BmpHeader)) {
			ERR("read bmp file error \n");
	}

	return 0;
}

static void ori_bmp_close(int ori_fd)
{
	if (ori_fd > 0) {
		close(ori_fd);
	}
}

static int str_to_format(char *str)
{
	int i = 0;
	static struct StrToFormat table[] = {
		{"argb8888", MPP_FMT_ARGB_8888},
		{"argb888", MPP_FMT_RGB_888},
		{"argb4444", MPP_FMT_ARGB_4444},
		{"argb1555", MPP_FMT_ARGB_1555},
		{"argb565", MPP_FMT_RGB_565},
	};

	const int table_size = sizeof(table) / sizeof(table[0]);
	for (i = 0; i < table_size ; i++) {
		if(!strncasecmp(str, table[i].str, strlen(table[i].str))) {
			return table[i].format;
		}
	}

	return -1;
}

static int format_to_stride(int format, int width)
{
	int stride = -1;

	switch (format) {
	case MPP_FMT_ARGB_8888:
		stride = BYTE_ALIGN((width * 4), 8);
		break;
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_RGB_565:
		stride = BYTE_ALIGN((width * 2), 8);
		break;
	case MPP_FMT_RGB_888:
		stride = BYTE_ALIGN((width * 3), 8);
		break;
	default:
		ERR("Set stride error, format = %d\n", format);
		break;
	}

	return stride;
}

static int format_to_hight(int format, int hight)
{
	int t_hight = -1;

	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_RGB_888:
	case MPP_FMT_RGB_565:
		t_hight = hight;
		break;
	default:
		ERR("Set hight error, format = %d\n", format);
		break;
	}

	return t_hight;
}

static int format_to_bmp_data_length(int format, int width)
{
	int data_length = -1;

	switch (format) {
	case MPP_FMT_ARGB_8888:
		data_length = BYTE_ALIGN((width * 4), 4);
		break;
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_RGB_565:
		data_length = BYTE_ALIGN((width * 2), 4);
		break;
	case MPP_FMT_RGB_888:
		data_length = BYTE_ALIGN((width * 3), 4);
		break;
	default:
		ERR("Set data length error, format = %d\n",format);
		break;
	}

	return data_length;
}

static int dmabuf_request_one(int fd, int format)
{
	int ret = -1;
	int stride = 0;
	int hight = 0;
	unsigned int ori_width = 0;
	unsigned int ori_height = 0;
	struct dma_heap_allocation_data data = {0};

	ori_width = abs(bmp_header.width);
	ori_height = abs(bmp_header.height);

	stride = format_to_stride(format, ori_width);
	if (stride < 0) {
		ERR("format_to_stride execution error\n");
		return -1;
	}

	hight = format_to_hight(format, ori_height);
	if (hight < 0) {
		ERR("format_to_hight execution error\n");
		return -1;
	}

	data.fd = 0;
	data.len = stride * hight;
	data.fd_flags = O_RDWR | O_CLOEXEC;
	data.heap_flags = 0;

	ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &data);
	if (ret < 0) {
		ERR("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return data.fd;
}

static int draw_ori_dmabuf(int dmabuf_fd, int ori_fd, int format)
{
	int i = 0;
	int len = 0;
	int hight = 0;
	int stride = 0;
	unsigned char *dmabuf = NULL;
	unsigned char *buf = NULL;
	unsigned int ori_width = 0;
	unsigned int ori_height = 0;
	unsigned int line_length = 0;
	unsigned int data_length = 0;

	if (bmp_header.type != 0x4D42) {
		ERR("not a Bmp file\n");
		return -1;
	}

	ori_width = abs(bmp_header.width);
	ori_height = abs(bmp_header.height);

	stride = format_to_stride(format, ori_width);
	if (stride < 0) {
		ERR("format_to_stride execution error\n");
		return -1;
	}

	hight = format_to_hight(format, ori_height);
	if (hight < 0) {
		ERR("format_to_hight execution error\n");
		return -1;
	}

	len = stride * hight;
	dmabuf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
	if (dmabuf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	data_length = format_to_bmp_data_length(format, ori_width);
	line_length = format_to_stride(format, ori_width);

	/* read from bottom to top */
	if (bmp_header.height < 0){
		buf = dmabuf;
		for (i = 0; i < ori_height; i++) {
			if ((read(ori_fd, buf, data_length) != data_length)) {
				ERR("read(%d) . errno: %d[%s]\n", data_length,
					errno, strerror(errno));
				return -1;
			}
			buf += line_length;
		}
	/* read from top to bottom */
	} else {
		for (i = ori_height -1; i >= 0; i--) {
			buf = dmabuf + (i * line_length);
			if ((read(ori_fd, buf, data_length) != data_length)) {
				ERR("read(%d) . errno: %d[%s]\n", data_length,
					errno, strerror(errno));
				return -1;
			}
		}
	}

	munmap(dmabuf, len);

	return 0;
}

/* just do a format conversion */
static int ge_dither_set(struct ge_bitblt *blt, int src_dma_fd, int dst_dma_fd, int src_format, int dst_format, int dith_en)
{
	int ori_width = 0;
	int ori_height = 0;
	int src_stride = 0;
	int dst_stride = 0;

	ori_width = abs(bmp_header.width);
	ori_height = abs(bmp_header.height);

	src_stride = format_to_stride(src_format, ori_width);
	if(src_stride < 0) {
		ERR("format_to_stride execution error\n");
		return -1;
	}

	dst_stride = format_to_stride(dst_format, ori_width);
	if(dst_stride < 0) {
		ERR("format_to_stride execution error\n");
		return -1;
	}

	/* src layer settings  */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = src_dma_fd;
	blt->src_buf.stride[0] = src_stride;
	blt->src_buf.size.width = ori_width;
	blt->src_buf.size.height = ori_height;
	blt->src_buf.format = src_format;
	blt->src_buf.crop_en = 0;

	/* dst layer settings */
	blt->dst_buf.buf_type = MPP_DMA_BUF_FD;
	blt->dst_buf.fd[0] = dst_dma_fd;
	blt->dst_buf.stride[0] = dst_stride;
	blt->dst_buf.size.width = ori_width;
	blt->dst_buf.size.height = ori_height;
	blt->dst_buf.format = dst_format;
	blt->dst_buf.crop_en = 0;

	blt->ctrl.dither_en = dith_en;

	return 0;
}

static int ge_dither(struct mpp_ge *ge, struct ge_bitblt *blt, int src_dma_fd, int dst_dma_fd, int src_format, int dst_format, int dith_en)
{
	int ret = -1;

	ret = ge_dither_set(blt, src_dma_fd, dst_dma_fd, src_format, dst_format, dith_en);
	if (ret < 0) {
		ERR("ge_dither dither_set task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("ge_dither bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("ge_dither emit task failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ge_display_set(struct ge_bitblt *blt, int dma_fd, int format, int region)
{
	int dstx = 0;
	int dsty = 0;
	int ori_width = 0;
	int ori_height = 0;
	int stride = 0;

	ori_width = abs(bmp_header.width);
	ori_height = abs(bmp_header.height);

	stride = format_to_stride(format, ori_width);
	if(stride < 0) {
		ERR("format_to_stride execution error\n");
		return -1;
	}

	if (region == SRC_DISP_REGION) {
		dstx = 0;
		dsty = 0;
	} else {
		dstx = g_screen_w / 2;
		dsty = 0;
	}

	/* src layer settings  */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = dma_fd;
	blt->src_buf.stride[0] = stride;
	blt->src_buf.size.width = ori_width;
	blt->src_buf.size.height = ori_height;
	blt->src_buf.format =  format;

	/* dst layer settings */
	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.phy_addr[0] = g_fb_phy;
	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;
	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = dstx;
	blt->dst_buf.crop.y = dsty;
	blt->dst_buf.crop.width = g_screen_w / 2;
	blt->dst_buf.crop.height = g_screen_h;

	blt->ctrl.dither_en = 0;

	return 0;
}

static int  ge_display(struct mpp_ge *ge, struct ge_bitblt *blt, int dma_fd, int format, int region)
{
	int ret = -1;

	ret = ge_display_set(blt, dma_fd, format, region);
	if (ret < 0) {
		ERR("ge_display set task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("ge_display bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("ge_display emit task failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ori_to_src(struct mpp_ge *ge, struct ge_bitblt *blt, int ori_dma_fd , int src_dma_fd, int src_format)
{
	int ret = -1;

	/* Run an bitlet operation without opening dither */
	ret = ge_dither(ge, blt, ori_dma_fd, src_dma_fd, MPP_FMT_ARGB_8888, src_format, 0);
	if (ret < 0) {
		ERR("ori_to_src bitlet task failed\n");
		return ret;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int ori_dmabuf_fd = -1;
	int src_dmabuf_fd = -1;
	int dst_dmabuf_fd = -1;
	int heap_fd = -1;
	int ori_fd = -1;
	int src_format = 0;
	int dst_format = 0;
	int dither_on = 0;
	struct mpp_ge *ge = NULL;
	struct ge_bitblt blt = {0};

	const char sopts[] = "uhs:d:o:";
	const struct option lopts[] = {
		{"usage",		no_argument,       NULL, 'u'},
		{"help",		no_argument,       NULL, 'h'},
		{"src_format",	required_argument, NULL, 's'},
		{"dst_format",	required_argument, NULL, 'd'},
		{"dither_on",	required_argument, NULL, 'o'},
	};

	src_format = str_to_format("argb8888");
	dst_format = str_to_format("argb4444");
	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 's':
			src_format = str_to_format(optarg);
			if (src_format < 0) {
				printf("src format set error, please set against\n");
				goto EXIT;
			}

			break;
		case 'd':
			dst_format = str_to_format(optarg);
			if (dst_format < 0) {
				printf("dst format set error, please set against\n");
				goto EXIT;
			}
			break;
		case 'o':
			dither_on = str2int(optarg);
			if ((dither_on > 1) || (dither_on < 0)) {
				printf("dither switch set error, please set against\n");
				goto EXIT;
			}
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
		ERR("open ge device\n");
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

	ret = ori_bmp_open(&ori_fd);
	if (ret < 0) {
		ERR("ori_bmp_open error \n");
		goto EXIT;
	}

	ori_dmabuf_fd = dmabuf_request_one(heap_fd, MPP_FMT_ARGB_8888);
	if (ori_dmabuf_fd < 0) {
		ERR("ori_dmabuf_fd failed to request dmabuf\n");
		goto EXIT;
	}

	src_dmabuf_fd = dmabuf_request_one(heap_fd, src_format);
	if (src_dmabuf_fd < 0) {
		ERR("src_dmabuf_fd failed to request dmabuf\n");
		goto EXIT;
	}

	dst_dmabuf_fd = dmabuf_request_one(heap_fd, dst_format);
	if (dst_dmabuf_fd < 0) {
		ERR("dst_dmabuf_fd failed to request dmabuf\n");
		goto EXIT;
	}

	/* read the original image data */
	ret = draw_ori_dmabuf(ori_dmabuf_fd, ori_fd, MPP_FMT_ARGB_8888);
	if (ret < 0) {
		ERR("draw_src_dmabuf task failed\n");
		goto EXIT;
	}

	mpp_ge_add_dmabuf(ge, ori_dmabuf_fd);
	mpp_ge_add_dmabuf(ge, src_dmabuf_fd);
	mpp_ge_add_dmabuf(ge, dst_dmabuf_fd);

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("emit task failed:%d\n", ret);
		goto EXIT;
	}

	/* convert the original image format to input format */
	ret = ori_to_src(ge, &blt, ori_dmabuf_fd, src_dmabuf_fd, src_format);
	if (ret) {
		ERR("ori_to_src task failed:%d\n", ret);
		goto EXIT;
	}
	/* Display SRC layer data on the left side of the screen */
	ret = ge_display(ge, &blt, src_dmabuf_fd, src_format, SRC_DISP_REGION);
	if (ret) {
		ERR("ge_display task failed:%d\n", ret);
		goto EXIT;
	}

	/* Dither format conversion */
	ret = ge_dither(ge, &blt, src_dmabuf_fd, dst_dmabuf_fd, src_format, dst_format, dither_on);
	if (ret < 0) {
		ERR("ge_dither task failed\n");
		goto EXIT;
	}

	/* Display SRC layer data on the right side of the screen */
	ret = ge_display(ge, &blt, dst_dmabuf_fd, dst_format, DST_DISP_REGION);
	if (ret < 0) {
		ERR("ge_display task failed\n");
		goto EXIT;
	}

	ret = mpp_ge_sync(ge);
	if (ret) {
		ERR("sync task failed: %d\n", ret);
		goto EXIT;
	}

EXIT:
	if (ori_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, ori_dmabuf_fd);
		close(ori_dmabuf_fd);
	}

	if (src_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, src_dmabuf_fd);
		close(src_dmabuf_fd);
	}

	if (dst_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, dst_dmabuf_fd);
		close(dst_dmabuf_fd);
	}

	if (heap_fd > 0) {
		close(heap_fd);
	}

	if (ori_fd > 0) {
		ori_bmp_close(ori_fd);
	}

	if (ge) {
		mpp_ge_close(ge);
	}

	fb_close();

	return 0;
}
