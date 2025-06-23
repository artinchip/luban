/*
 * Copyright (C) 2022-2023 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <huahui.mai@artinchip.com>
 *  Desc: mpp heap test
 */

#include <artinchip/sample_base.h>
#include <signal.h>
#include <sys/time.h>
#include <video/artinchip_fb.h>
#include <linux/fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "ve_buffer.h"
#include "mpp_log.h"

#define FB_DEV		"/dev/fb0"
#define HEAP_DEV	"/dev/dma_heap/mpp"

static int g_heap_fd = -1;
static int g_fb_fd = -1;
static int g_fb_len = 0;
static int g_screen_w = 0;
static int g_screen_h = 0;
unsigned char *g_fb_buf = NULL;

static const char sopts[] = "h";
static const struct option lopts[] = {
	{"help",	no_argument,       NULL, 'h'},
};

static void print_help(void)
{
	printf("Usage: mpp_heap_fb_test [options]:\n"
		"   -h                             help\n\n"
		"Example: mpp_heap_fb_test\n");
}

static int fb_device_open(void)
{
	int fd;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;

	fd = open(FB_DEV, O_RDWR);
	if (fd < 0) {
		loge("open %s failed!", FB_DEV);
		return -1;
	}
	g_fb_fd = fd;

	if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		loge("ioctl FBIOGET_FSCREENINFO failed");
		close(g_fb_fd);
		return -1;
	}
	if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		loge("ioctl FBIOGET_VSCREENINFO failed");
		close(g_fb_fd);
		return -1;
	}

	g_fb_len = fix.smem_len;
	g_screen_w = var.xres;
	g_screen_h = var.yres;

	g_fb_buf = mmap(NULL, g_fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        g_fb_fd, 0);
	if (g_fb_buf == (unsigned char *)-1) {
		loge("mmap framebuffer failed");
		g_fb_fd = -1;
		g_fb_buf = NULL;
		return -1;
	}
	return 0;
}

static void fb_device_close(void)
{
	if (!g_fb_buf) {
		munmap(g_fb_buf, g_fb_len);
	}

	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static int mpp_heap_open(void)
{
	int heap_fd;

	heap_fd = open(HEAP_DEV, O_RDWR);
	if (heap_fd < 0) {
		loge("open %s failed!", HEAP_DEV);
		return -1;
	}
	g_heap_fd = heap_fd;
	return 0;

}

static int mpp_heap_alloc(int size)
{
	struct dma_heap_allocation_data data = {0};
	int ret;

	if (g_heap_fd < 0)
		return -1;

	g_heap_fd = open(HEAP_DEV, O_RDWR);
	if (g_heap_fd < 0) {
		loge("open %s failed!", HEAP_DEV);
		return -1;
	}

	data.fd = 0;
	data.len = size;
	data.fd_flags = O_RDWR | O_CLOEXEC;
	data.heap_flags = 0;

	ret = ioctl(g_heap_fd, DMA_HEAP_IOCTL_ALLOC, &data);
	if (ret < 0) {
		loge("dmabuf alloc failed, ret %d", ret);
		return ret;
	}
	return data.fd;
}

static void mpp_heap_close(void)
{
	if (g_heap_fd > 0)
		close(g_heap_fd);
}

static void dmabuf_cpu_begin(int dmabuf_fd)
{
	struct dma_buf_sync flag;

	flag.flags = DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_START;
	if (ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &flag) < 0)
		loge("ioctl() failed! err %d[%s]\n",
				errno, strerror(errno));
}

static void dmabuf_cpu_end(int dmabuf_fd)
{
	struct dma_buf_sync flag;

	flag.flags = DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_END;
	if (ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &flag) < 0)
		loge("ioctl() failed! err %d[%s]\n",
				errno, strerror(errno));
}

static void fb_color_block(void *addr, u32 width, u32 height, u32 color)
{
    u32 i, j;
    u32 *pos = (u32 *)addr;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            pos[j] = color;
        }
        pos += width;
    }
}

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned char *buf = NULL;
	int size = 0;
	int zero = 0;
	int index = 0;
	int i = 0;

	struct fb_var_screeninfo var = {0};
	struct dma_buf_info dmabuf_info = {0};
	int dmabuf_fd = 0;

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {

		switch (ret) {
		case 'h':
			print_help();
			return 0;
		default:
			break;
		}
	}

	if (mpp_heap_open() < 0)
		return -1;

	if (fb_device_open() < 0)
		return -1;

	size = g_fb_len;

	dmabuf_fd = mpp_heap_alloc(size);
	if (dmabuf_fd < 0) {
		loge("alloc dmabuf failed");
		goto out;
	}

	logi("alloc dmabuf id: %d", dmabuf_fd);

	buf = dmabuf_mmap(dmabuf_fd, size);
	if (!buf) {
		loge("mmap failed");
		goto out;
	}

	ret = ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var);
	if (ret) {
		loge("ioctl FBIOGET_VSCREENINFO");
		goto out;
	}

	for (i = 0; i < 4; i++)
		logi("var reserved[%d]: %x", i, var.reserved[i]);

	dmabuf_info.fd = dmabuf_fd;

	ret = ioctl(g_fb_fd, AICFB_ADD_DMABUF, &dmabuf_info);
	if (ret) {
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		goto out;
	}

	/*
	 * Standard DMA-BUF is a cached buffer.
	 *
	 * After cpu writes, flush cache is necessary.
	 *
	 * If the cpu don't need to read memory,
	 * dmabuf_cpu_begin() can be removed.
	 */
	logi("dmabuf memory set 0x00FF0000");
	dmabuf_cpu_begin(dmabuf_fd);

	time_start(dmabuf_cache);
	fb_color_block(buf, g_screen_w, g_screen_h, 0x00FF0000);
	dmabuf_cpu_end(dmabuf_fd);
	time_end(dmabuf_cache);

	/*
	 * /dev/fb0 is a uncache buffer.
	 */
	logi("/dev/fb0 memory set 0x00FF0000");
	time_start(fb_uncache);
	fb_color_block(g_fb_buf, g_screen_w, g_screen_h, 0x0000FF00);
	time_end(fb_uncache);

	do {
		if (index)
			var.reserved[0] = dmabuf_fd;
		else
			var.reserved[0] = 0;

		if (ioctl(g_fb_fd, FBIOPAN_DISPLAY, &var) == 0) {
			if (ioctl(g_fb_fd, AICFB_WAIT_FOR_VSYNC, &zero) < 0) {
				loge("ioctl AICFB_WAIT_FOR_VSYNC\n");
				goto out;
			}
		} else {
			loge("ioctl FBIOPAN_DISPLAY\n");
			goto out;
		}

		index = !index;
		usleep(400 * 1000);
		i++;
	} while (i < 10);

out:
	if (buf)
		dmabuf_munmap(buf, size);

	if(dmabuf_fd) {
		ioctl(g_fb_fd, AICFB_RM_DMABUF, &dmabuf_info);
		close(dmabuf_fd);
	}

	mpp_heap_close();
	fb_device_close();

	return ret;
}

