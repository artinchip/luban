/*
 * Copyright (C) 2022-2023 Artinchip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <signal.h>
#include <math.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include "mpp_ge.h"

#define PI 3.14159265

#define SIN(x) (sin((x) * PI / 180.0))
#define COS(x) (cos((x) * PI / 180.0))

#define BUF_NUM		2
#define FB_DEV		"/dev/fb0"
#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"

#define CLOCK_IMAGE		DATADIR"/clock.bmp"
#define SECOND_IMAGE		DATADIR"/second.bmp"

#define CLOCK_IMAGE_WIDTH	590
#define CLOCK_IMAGE_HEIGHT	600

#define SECOND_IMAGE_WIDTH	48
#define SECOND_IMAGE_HEIGHT	220

#define ROT_SRC_CENTER_X	24
#define ROT_SRC_CENTER_Y	194

#define ROT_DST_CENTER_X	294
#define ROT_DST_CENTER_Y	297

static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
static unsigned int g_fb_phy2 = 0;
unsigned char *g_fb_buf = NULL;

static const char sopts[] = "u";
static const struct option lopts[] = {
	{"usage",	no_argument,       NULL, 'u'},
};

static const char *bmps[BUF_NUM] = {
	CLOCK_IMAGE,
	SECOND_IMAGE
};

static void show_buf_info()
{
	printf(
	"\n"
	"\t Framebuffer just uses single buffer, \n"
	"\t FBIOPAN_DISPLAY can't work and the screen maybe tear effect.\n"
	"\n"
	"\t If you want to display better, please double the size of fb0 in dts\n"
	"\t to make sure ioctl FBIOPAN_DISPLAY works.\n"
	"\n");
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
			g_screen_w * g_screen_h * (var.bits_per_pixel / 8);
	} else {
		g_fb_phy2 = g_fb_phy;
		show_buf_info();
	}

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

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-u, --usage\n\n");
	printf("Example: ./%s\n", app);
}

static int src_bmp_open(int *fd, int *size)
{
	int i = 0;

	for (i = 0; i < BUF_NUM; i++) {
		fd[i] = open(bmps[i], O_RDWR);
		if (fd[i] < 0) {
			ERR("Failed to open %s, errno: %d[%s]\n",
				bmps[i], errno, strerror(errno));
			return -1;
		}
		size[i] = lseek(fd[i], 0, SEEK_END);
		lseek(fd[i], 54, SEEK_SET);
	}
	return 0;
}

static void src_bmp_close(int *fd)
{
	int i = 0;

	for (i = 0; i < BUF_NUM; i++) {
		if (fd[i] > 0)
			close(fd[i]);
	}
}

static void dmabuf_close(int *dmabuf_fd)
{
	int i;

	for (i = 0; i < BUF_NUM; i++) {
		if (dmabuf_fd[i] > 0)
			close(dmabuf_fd[i]);
	}
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

static int dmabuf_request(int *fsize, int *dmabuf_fd)
{
	int i = 0;
	int heap_fd = -1;

	heap_fd = open(DMA_HEAP_DEV, O_RDWR);
	if (heap_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		return -1;
	}

	for (i = 0; i < BUF_NUM; i++) {
		dmabuf_fd[i] = dmabuf_request_one(heap_fd, fsize[i]);
		if (dmabuf_fd[i] < 0)
			return -1;
	}

	close(heap_fd);
	return 0;
}

static int dmabuf_draw_one(int dmabuf_fd, int fd, int len)
{
	unsigned char * buf = NULL;
	int ret = 0;

	buf = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED,	dmabuf_fd, 0);
	if (buf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	ret = read(fd, buf, len);
	if (ret != len) {
		ERR("read(%d) return %d. errno: %d[%s]\n", len,
			ret, errno, strerror(errno));
		return -1;
	}

	munmap(buf, len);
	return 0;
}

static int dmabuf_draw(int *dmabuf_fd, int *fd, int *len)
{
	int i = 0;

	for (i = 0; i < BUF_NUM; i++)
		dmabuf_draw_one(dmabuf_fd[i], fd[i], len[i] - 54);

	return 0;
}

static void dmabuf_add(struct mpp_ge *ge, int *dmabuf_fd)
{
	int i = 0;

	for (i = 0; i < BUF_NUM; i++)
		mpp_ge_add_dmabuf(ge, dmabuf_fd[i]);
}

static void dmabuf_remove(struct mpp_ge *ge, int *dmabuf_fd)
{
	int i = 0;

	for (i = 0; i < BUF_NUM; i++)
		mpp_ge_rm_dmabuf(ge, dmabuf_fd[i]);
}

static void draw_clock(struct ge_bitblt *blt, int src_dmabuf_fd, int index)
{
	/* source buffer */
	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.fd[0] = src_dmabuf_fd;
	blt->src_buf.stride[0] = CLOCK_IMAGE_WIDTH * 4;
	blt->src_buf.size.width = CLOCK_IMAGE_WIDTH;
	blt->src_buf.size.height = CLOCK_IMAGE_HEIGHT;
	blt->src_buf.format = MPP_FMT_ARGB_8888;

	blt->src_buf.crop_en = 1;
	blt->src_buf.crop.x = 0;
	blt->src_buf.crop.y = 0;
	blt->src_buf.crop.width = CLOCK_IMAGE_WIDTH;
	blt->src_buf.crop.height = CLOCK_IMAGE_HEIGHT;

	/* dstination buffer */
	blt->dst_buf.buf_type = MPP_PHY_ADDR;

	if (index)
		blt->dst_buf.phy_addr[0] = g_fb_phy2;
	else
		blt->dst_buf.phy_addr[0] = g_fb_phy;

	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;

	blt->ctrl.flags = 0;

	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = 0;
	blt->dst_buf.crop.y = 0;
	blt->dst_buf.crop.width = CLOCK_IMAGE_WIDTH;
	blt->dst_buf.crop.height = CLOCK_IMAGE_HEIGHT;
}

static void move_second_hand(struct mpp_ge *ge, struct ge_rotation *rot,
				int second, int src_dmabuf_fd, int index)
{
	double degree = 0.0;

	/* source buffer */
	rot->src_buf.buf_type = MPP_DMA_BUF_FD;
	rot->src_buf.fd[0] = src_dmabuf_fd;
	rot->src_buf.stride[0] = SECOND_IMAGE_WIDTH * 4;
	rot->src_buf.size.width = SECOND_IMAGE_WIDTH;
	rot->src_buf.size.height = SECOND_IMAGE_HEIGHT;
	rot->src_buf.format = MPP_FMT_ARGB_8888;
	rot->src_buf.crop_en = 0;

	rot->src_rot_center.x = ROT_SRC_CENTER_X;
	rot->src_rot_center.y = ROT_SRC_CENTER_Y;

	/* destination buffer */
	rot->dst_buf.buf_type = MPP_PHY_ADDR;

	if (index)
		rot->dst_buf.phy_addr[0] = g_fb_phy2;
	else
		rot->dst_buf.phy_addr[0] = g_fb_phy;

	rot->dst_buf.stride[0] = g_fb_stride;
	rot->dst_buf.size.width = g_screen_w;
	rot->dst_buf.size.height = g_screen_h;
	rot->dst_buf.format = g_fb_format;
	rot->dst_buf.crop_en = 0;

	rot->dst_rot_center.x = ROT_DST_CENTER_X;
	rot->dst_rot_center.y = ROT_DST_CENTER_Y;

	rot->ctrl.alpha_en = 1;

	/*
	 * The range of degrees is [0, 360), and the step size is 0.1
	 */
	degree = second * 6.0;
	rot->angle_sin = (int)(SIN(degree) * 4096);
	rot->angle_cos = (int)(COS(degree) * 4096);
}

static int aic_fb_filp(int index)
{
	struct fb_var_screeninfo var = {0};
	int zero = 0;

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

int main(int argc, char **argv)
{
	struct mpp_ge *ge = NULL;
	int ret = -1;
	int index = 0;
	int i;

	int src_fd[BUF_NUM] = {-1};
	int src_dmabuf_fd[BUF_NUM] = {-1};
	int fsize[BUF_NUM] = {0};
	struct ge_bitblt blt = {0};
	struct ge_rotation rot = {0};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 'u':
			usage(argv[0]);
			return 0;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			return -1;
		}
	}

	ge = mpp_ge_open();
	if (!ge) {
		ERR("open ge device\n");
		exit(1);
	}

	if (fb_open()) {
		ERR("fb_open\n");
		goto err_fb;
	}

	if (src_bmp_open(src_fd, fsize) < 0) {
		ERR("Failed to open bmp file, errno: %d[%s]\n",
			errno, strerror(errno));
		goto err_fb;
	}

	if (dmabuf_request(fsize, src_dmabuf_fd) < 0) {
		ERR("Failed to request dmabuf\n");
		goto err_src_bmp;
	}

	dmabuf_draw(src_dmabuf_fd, src_fd, fsize);
	dmabuf_add(ge, src_dmabuf_fd);

	for (i = 0; i < 60; i++) {
		draw_clock(&blt, src_dmabuf_fd[0], index);
		ret = mpp_ge_bitblt(ge, &blt);
		if (ret) {
			ERR("bitblt task failed:%d\n", ret);
			goto err_dmabuf;
		}

		move_second_hand(ge, &rot, i,
			src_dmabuf_fd[1], index);
		ret = mpp_ge_rotate(ge, &rot);
		if (ret) {
			ERR("rotate task failed:%d\n", ret);
			goto err_dmabuf;
		}

		ret = mpp_ge_emit(ge);
		if (ret) {
			ERR("emit task failed:%d\n", ret);
			goto err_dmabuf;
		}

		ret = mpp_ge_sync(ge);
		if (ret) {
			ERR("sync task failed:%d\n", ret);
			goto err_dmabuf;
		}

		aic_fb_filp(index);

		index = !index;

		sleep(1);
	}

err_dmabuf:
	dmabuf_remove(ge, src_dmabuf_fd);
	dmabuf_close(src_dmabuf_fd);
err_src_bmp:
	src_bmp_close(src_fd);
err_fb:
	fb_close();
	if (ge)
		mpp_ge_close(ge);

	return ret;
}
