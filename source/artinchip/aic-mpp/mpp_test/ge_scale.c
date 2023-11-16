/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
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
#define SCALE_IMAGE	DATADIR"/scale.bmp"

#define SCALE_WIDTH  0
#define SCALE_HEIGHT 1
#define SCALE_W_H    2

#define ONE_PIXEL 1.0

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
static unsigned int g_fb_phy2 = 0;
unsigned char *g_fb_buf = NULL;

struct BmpHeader bmp_header = {0};

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf(
	"\t    --Framebuffer uses double buffer in this test, "
	"ensure that fb0 is properly configure"
	"\n\n"
	"\t-t, --type, Select scale type (default 2)\n"
	"\t    --type, 0: wide stretch, 1: height stretch, 2: width and height stretch"
	"\n\n"
	"\t-u, --usage\n");
}

static void show_buf_info()
{
	printf(
	"\n"
	"\t Framebuffer must uses double buffer, "
	"can't uses single buffer\n"
	"\t Otherwise, GE will write to unauthorized memory, "
	"which may affect system operation."
	"\n\n"
	"\t Before running this demo, "
	"be sure to confirm Framebuffer is configured as double buffer"
	"\n\n");
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

	if (var.yres * 2 <= var.yres_virtual) {
		g_fb_phy2 = g_fb_phy +
			g_fb_stride * g_screen_h;
	} else {
		g_fb_phy2 = g_fb_phy;
		show_buf_info();
	}

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
	if (!g_fb_buf)
		munmap(g_fb_buf, g_fb_len);

	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static int src_bmp_open(int *src_fd)
{
	*src_fd = open(SCALE_IMAGE, O_RDWR);
	if (*src_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			SCALE_IMAGE, errno, strerror(errno));
		return -1;
	}

	if (read(*src_fd, &bmp_header, sizeof(struct BmpHeader)) !=
		sizeof(struct BmpHeader))
		ERR("read bmp file error \n");

	return 0;
}

static void src_bmp_close(int src_fd)
{
	if (src_fd > 0)
		close(src_fd);
}

static int format_to_stride(int format, int width)
{
	int stride = -1;

	if (format != MPP_FMT_RGB_888) {
		ERR("Set stride error, format = %d\n", format);
		return stride;
	}

	stride = BYTE_ALIGN((width * 3), 8);
	return stride;
}

static int format_to_height(int format, int height)
{
	int t_height = -1;

	if (format != MPP_FMT_RGB_888) {
		ERR("Set height error, format = %d\n", format);
		return t_height;
	}

	t_height = height;
	return t_height;
}

static int format_to_bmp_data_length(int format, int width)
{
	int data_length = -1;

	if (format != MPP_FMT_RGB_888) {
		ERR("Set data length error, format = %d\n", format);
		return data_length;
	}

	data_length = BYTE_ALIGN((width * 3), 4);

	return data_length;
}

static int dmabuf_request_one(int fd, int format, int d_width, int d_height)
{
	int ret = -1;
	int stride = 0;
	int height = 0;
	struct dma_heap_allocation_data data = {0};

	stride = format_to_stride(format, d_width);
	if (stride < 0) {
		ERR(" format_to_stride execution error\n");
		return -1;
	}

	height = format_to_height(format, d_height);
	if (height < 0) {
		ERR("format_to_height execution error\n");
		return -1;
	}

	data.fd = 0;
	data.len = stride * height;
	data.fd_flags = O_RDWR | O_CLOEXEC;
	data.heap_flags = 0;

	ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &data);
	if (ret < 0) {
		ERR("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return data.fd;
}

static int draw_src_dmabuf(int dmabuf_fd, int src_fd)
{
	int i = 0;
	int len = 0;
	int height = 0;
	int stride = 0;
	int ori_width = 0;
	int ori_height = 0;
	int data_length = 0;
	unsigned char *buf = NULL;
	unsigned char *dmabuf = NULL;

	if (bmp_header.type != 0x4D42) {
		ERR("not a Bmp file\n");
		return -1;
	}

	ori_width = abs(bmp_header.width);
	ori_height = abs(bmp_header.height);

	stride = format_to_stride(MPP_FMT_RGB_888, ori_width);
	if (stride < 0) {
		ERR("format_to_stride execution error\n");
		return -1;
	}

	height = format_to_height(MPP_FMT_RGB_888, ori_height);
	if (height < 0) {
		ERR("format_to_height execution error\n");
		return -1;
	}

	data_length = format_to_bmp_data_length(MPP_FMT_RGB_888, ori_width);
	if (data_length < 0) {
		ERR("data_length execution error\n");
		return -1;
	}

	len = stride * height;
	dmabuf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
	if (dmabuf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	/* read from bottom to top */
	if (bmp_header.height < 0){
		buf = dmabuf;
		for (i = 0; i < ori_height; i++) {
			if ((read(src_fd, buf, data_length) != data_length)) {
				ERR("read(%d) . errno: %d[%s]\n", data_length,
					errno, strerror(errno));
				return -1;
			}
			buf += stride;
		}
	/* read from top to bottom */
	} else {
		for (i = ori_height -1; i >= 0; i--) {
			buf = dmabuf + (i * stride);
			if ((read(src_fd, buf, data_length) != data_length)) {
				ERR("read(%d) . errno: %d[%s]\n", data_length,
					errno, strerror(errno));
				return -1;
			}
		}
	}

	munmap(dmabuf, len);

	return 0;
}

static void ge_scale_set(struct ge_bitblt *blt, int src_dma_fd, int dst_dma_fd,
			 int width, int height)
{
	int src_width = 0;
	int src_height = 0;
	int src_stride = 0;

	int dst_width = 0;
	int dst_height = 0;
	int dst_stride = 0;

	memset(blt, 0, sizeof(struct ge_bitblt));

	/* src layer settings  */
	src_width = abs(bmp_header.width);
	src_height = abs(bmp_header.height);

	src_stride = format_to_stride(MPP_FMT_RGB_888, src_width);
	if(src_stride < 0)
		ERR("format_to_stride execution error\n");

	src_height = format_to_height(MPP_FMT_RGB_888, src_height);
	if (src_height < 0)
		ERR("format_to_height execution error\n");

	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = src_dma_fd;
	blt->src_buf.stride[0] = src_stride;
	blt->src_buf.size.width = src_width;
	blt->src_buf.size.height = src_height;
	blt->src_buf.format = MPP_FMT_RGB_888;
	blt->src_buf.crop_en = 0;

	/* dst layer settings */
	dst_width = width;
	dst_height = height;

	dst_stride = format_to_stride(MPP_FMT_RGB_888, dst_width);
	if(dst_stride < 0)
		ERR("format_to_stride execution error\n");

	dst_height = format_to_height(MPP_FMT_RGB_888, dst_height);
	if (dst_height < 0)
		ERR("format_to_height execution error\n");

	blt->dst_buf.buf_type = MPP_DMA_BUF_FD;
	blt->dst_buf.fd[0] = dst_dma_fd;
	blt->dst_buf.stride[0] = dst_stride;
	blt->dst_buf.size.width = dst_width;
	blt->dst_buf.size.height = dst_height;
	blt->dst_buf.format = MPP_FMT_RGB_888;
	blt->dst_buf.crop_en = 0;
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
		ERR("mpp_ge_bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_sync(ge);
	if (ret) {
		ERR("mpp_ge_sync task failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void clear_screen(int index)
{
	unsigned char *clear_buf = NULL;
	unsigned int len = 0;

	len = g_fb_len / 2;
	if (index)
		clear_buf = g_fb_buf + len;
	else
		clear_buf = g_fb_buf;

	memset(clear_buf, 0, len);
}

static int aic_fb_filp(int index)
{
	int zero = 0;
	struct fb_var_screeninfo var = {0};

	if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		return -1;
	}

	if (g_screen_h + var.yres > var.yres_virtual)
		return 0;

	if (index) {
		var.xoffset = 0;
		var.yoffset = g_screen_h;
	} else {
		var.xoffset = 0;
		var.yoffset = 0;
	}

	if (ioctl(g_fb_fd, FBIOPAN_DISPLAY, &var) == 0) {
		if (ioctl(g_fb_fd, AICFB_WAIT_FOR_VSYNC, &zero) < 0) {
			ERR("ioctl AICFB_WAIT_FOR_VSYNC\n");
			return -1;
		}
	} else {
		ERR("ioctl FBIOPAN_DISPLAY\n");
		return -1;
	}

	return 0;
}

static int set_display_pos(int width, int height, int *pos_x, int *pos_y)
{
	if (width < 4 || height < 4)
		return -1;

	if (width > g_screen_w)
		width = g_screen_w;

	if (height > g_screen_h)
		height = g_screen_h;

	*pos_x = (g_screen_w / 2 - width / 2);
	*pos_y = (g_screen_h / 2 - height / 2);

	return 0;
}

static void ge_display_set(struct ge_bitblt *blt, int dma_fd,
			   int width, int height, int index)
{
	int x = 0;
	int y = 0;
	int ret = 0;
	int dsp_stride = 0;
	int dsp_height = 0;

	memset(blt, 0, sizeof(struct ge_bitblt));

	/* src layer settings  */
	dsp_stride = format_to_stride(MPP_FMT_RGB_888, width);
	if(dsp_stride < 0)
		ERR("format_to_stride execution error\n");

	dsp_height = format_to_height(MPP_FMT_RGB_888, height);
	if (dsp_height < 0)
		ERR("format_to_height execution error\n");

	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = dma_fd;
	blt->src_buf.stride[0] = dsp_stride;
	blt->src_buf.size.width = width;
	blt->src_buf.size.height = dsp_height;
	blt->src_buf.format =  MPP_FMT_RGB_888;
	blt->src_buf.crop_en = 0;

	/* dst layer settings */
	ret = set_display_pos(width, height, &x, &y);
	if (ret < 0)
		ERR("ge_display_pos failed.\n");

	if (index)
		blt->dst_buf.phy_addr[0] = g_fb_phy2;
	else
		blt->dst_buf.phy_addr[0] = g_fb_phy;

	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;
	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = x;
	blt->dst_buf.crop.y = y;
	blt->dst_buf.crop.width = width;
	blt->dst_buf.crop.height = height;
}

static int resize_image(double *width, double *height, int scale_type, double num)
{
	double aspect_ration = 0;
	double new_height = 0;
	double new_width = 0;
	float width_scale = 0;
	float height_scale = 0;

	aspect_ration = (*width) / (*height);
	new_height = *height;
	new_width = *width;

	if (scale_type == SCALE_W_H) {
		if (*width > *height) {
			new_width = *width + num;
			new_height = new_width / aspect_ration;
		} else {
			new_height = *height + num;
			new_width = new_height * aspect_ration;
		}
	} else if (scale_type == SCALE_WIDTH) {
		new_width += num;
	} else {
		new_height += num;
	}

	width_scale = (int)new_width / (float)abs(bmp_header.width);
	height_scale = (int)new_height / (float)abs(bmp_header.height);

	if (width_scale < (float)1.0 / 16 || height_scale < (float)1.0 / 16)
		return 1;

	if (width_scale > (float)16 || height_scale > (float)16)
		return 1;

	new_width = new_width >= g_screen_w ? g_screen_w : new_width;
	new_height = new_height >= g_screen_h ? g_screen_h : new_height;

	new_width = new_width < 4 ? 4 : new_width;
	new_height = new_height < 4 ? 4 : new_height;

	*width = new_width;
	*height = new_height;

	if ((int)*width == 4 || (int)*height == 4 ||
		(int)*width == g_screen_w || (int)*height == g_screen_h)
		return 1;

	return 0;
}

static int scale_test(struct mpp_ge *ge, struct ge_bitblt *blt, int heap_fd,
		      int src_dma_fd, int dst_dma_fd, int scale_type)
{
	int ret = 0;
	int index = 0;
	int shrink_en = 1;
	double width = abs(bmp_header.width);
	double height = abs(bmp_header.height);

	while(1)
	{
		ge_scale_set(blt, src_dma_fd, dst_dma_fd,
			     width, height);
		ret = run_ge_bitblt(ge, blt);
		if (ret < 0) {
			ERR("run_ge_bitblt error\n");
			return -1;
		}
		/* cross clear framebuffer */
		clear_screen(index);

		ge_display_set(blt, dst_dma_fd, width,
			       height, index);
		ret = run_ge_bitblt(ge, blt);
		if (ret < 0) {
			ERR("run_ge_bitblt error\n");
			return -1;
		}

		/* cross display framebuffer */
		aic_fb_filp(index);
		index = !index;

		if (shrink_en == 1)
		{
			ret = resize_image(&width, &height, scale_type, ONE_PIXEL);
			if (ret < 0) {
				ERR("resize_image task failed\n");
				return -1;
			}

			if (ret == 1)
				shrink_en = 0;
		} else {
			ret = resize_image(&width, &height, scale_type, - ONE_PIXEL);
			if (ret < 0) {
				ERR("resize_image task failed\n");
				return -1;
			}

			if (ret == 1)
				return 0;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int src_dmabuf_fd = -1;
	int dst_dmabuf_fd = -1;
	float scale_type = 2;
	int heap_fd = -1;
	int src_fd = -1;
	struct mpp_ge *ge = NULL;
	struct ge_bitblt blt = {0};

	const char sopts[] = "ut:";
	const struct option lopts[] = {
		{"usage",	no_argument,       NULL, 'u'},
		{"type",	required_argument, NULL, 't'},
	};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 't':
			scale_type = str2int(optarg);
			if ((scale_type > 2) || (scale_type < 0)) {
				printf("scale_type invalid, please set against\n");
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

	ret = src_bmp_open(&src_fd);
	if (ret < 0) {
		ERR("src_bmp_open error \n");
		goto EXIT;
	}

	src_dmabuf_fd = dmabuf_request_one(heap_fd, MPP_FMT_RGB_888,
					   abs(bmp_header.width), abs(bmp_header.height));
	if (src_dmabuf_fd < 0) {
		ERR("src_dmabuf_fd failed to request dmabuf\n");
		goto EXIT;
	}

	dst_dmabuf_fd = dmabuf_request_one(heap_fd, MPP_FMT_RGB_888,
					   g_screen_w, g_screen_h);
	if (dst_dmabuf_fd < 0) {
		ERR("dst_dmabuf_fd failed to request dmabuf\n");
		goto EXIT;
	}

	ret = draw_src_dmabuf(src_dmabuf_fd, src_fd);
	if (ret < 0) {
		ERR("draw_src_dmabuf task failed\n");
		goto EXIT;
	}

	mpp_ge_add_dmabuf(ge, src_dmabuf_fd);
	mpp_ge_add_dmabuf(ge, dst_dmabuf_fd);

	ret = scale_test(ge, &blt, heap_fd, src_dmabuf_fd,
			 dst_dmabuf_fd, scale_type);
	if (ret < 0) {
		ERR("scale_test task failed: %d\n", ret);
		goto EXIT;
	}

EXIT:
	if (src_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, src_dmabuf_fd);
		close(src_dmabuf_fd);
	}

	if (dst_dmabuf_fd > 0) {
		mpp_ge_rm_dmabuf(ge, dst_dmabuf_fd);
		close(dst_dmabuf_fd);
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
