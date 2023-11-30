// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <linux/fb.h>
#include <sys/time.h>

#include <video/artinchip_fb.h>
#include <artinchip/sample_base.h>

/* Global macro and variables */

#define VID_BUF_NUM		3
#define DVP_PLANE_NUM		2
#define CMA_BUF_MAX		(8 * 1024 * 1024)
#define DMA_HEAP_DEV		"/dev/dma_heap/reserved"
#define FB_DEV			"/dev/fb0"
#define VIDEO_DEV		"/dev/video0"
#define SENSOR_DEV		"/dev/v4l-subdev0"
#define DVP_SUBDEV_DEV		"/dev/v4l-subdev1"

static const char sopts[] = "f:c:w:h:r:uv";
static const struct option lopts[] = {
	{"format",	  required_argument, NULL, 'f'},
	{"capture",	  required_argument, NULL, 'c'},
	{"width",	  required_argument, NULL, 'w'},
	{"height",	  required_argument, NULL, 'h'},
	{"framerate",	  required_argument, NULL, 'r'},
	{"usage",		no_argument, NULL, 'u'},
	{"verbose",	  required_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

struct video_plane {
	int fd;
	int buf;
	int len;
};

struct video_buf_info {
	char *vaddr;
	u32 len;
	u32 offset;
	struct video_plane planes[DVP_PLANE_NUM];
};

struct aic_video_data {
	int w;
	int h;
	int frame_size;
	int frame_cnt;
	int fmt;  // output format
	struct v4l2_subdev_format src_fmt;
	struct video_buf_info binfo[VID_BUF_NUM];
};

static int g_verbose = 0;
static int g_fb_fd = -1;
static int g_fb_xres = 0;
static int g_fb_yres = 0;
static int g_video_fd = -1;
static int g_sensor_fd = -1;
static int g_sensor_width = 640;
static int g_sensor_height = 480;
static int g_sensor_fr = 30;
static int g_dvp_subdev_fd = -1;
static struct aic_video_data g_vdata = {0};

/* Functions */

void usage(char *program)
{
	printf("Usage: %s [options]: \n", program);
	printf("\t -f, --format\t\tformat of input video, NV12/NV16 etc\n");
	printf("\t -c, --count\t\tthe number of capture frame \n");
	printf("\t -w, --width\t\tthe width of sensor \n");
	printf("\t -h, --height\t\tthe height of sensor \n");
	printf("\t -r, --framerate\tthe framerate of sensor \n");
	printf("\t -u, --usage \n");
	printf("\t -v, --verbose \n");
	printf("\n");
	printf("Example: %s -f nv16 -c 1\n", program);
}

/* Open a device file to be needed. */
int device_open(char *_fname, int _flag)
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

int get_fb_size(void)
{
	struct fb_var_screeninfo var;

	if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}
	g_fb_xres = var.xres;
	g_fb_yres = var.yres;
	printf("Framebuf size: width %d, height %d\n", g_fb_xres, g_fb_yres);
	return 0;
}

int set_ui_layer_alpha(int val)
{
	int ret = 0;
	struct aicfb_alpha_config alpha = {0};

	alpha.layer_id = 1;
	alpha.enable = 1;
	alpha.mode = 1;
	alpha.value = val;
	ret = ioctl(g_fb_fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);
	if (ret < 0)
		ERR("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));

	return ret;
}

void vidbuf_dmabuf_begin(struct aic_video_data *vdata)
{
	int i, j;
	struct dma_buf_info fds = {0};

	for (i = 0; i < VID_BUF_NUM; i++) {
		struct video_plane *plane = vdata->binfo[i].planes;
		for (j = 0; j < DVP_PLANE_NUM; j++, plane++) {
			fds.fd = plane->fd;
			if (ioctl(g_fb_fd, AICFB_ADD_DMABUF, &fds) < 0)
				ERR("Failed to add DMABUF for %d! %d[%s]\n",
					plane->fd, errno, strerror(errno));
		}
	}
}

void vidbuf_dmabuf_end(struct aic_video_data *vdata)
{
	int i, j;
	struct dma_buf_info fds = {0};

	for (i = 0; i < VID_BUF_NUM; i++) {
		struct video_plane *plane = vdata->binfo[i].planes;
		for (j = 0; j < DVP_PLANE_NUM; j++, plane++) {
			fds.fd = plane->fd;
			if (ioctl(g_fb_fd, AICFB_RM_DMABUF, &fds) < 0)
				ERR("Failed to rm DMABUF for %d! err %d[%s]\n",
					plane->fd, errno, strerror(errno));
		}
	}
}

int sensor_set_fmt(void)
{
	int ret = 0;
	struct v4l2_subdev_format f = {0};
	struct v4l2_subdev_frame_interval fr = {0};

	g_sensor_fd = device_open(SENSOR_DEV, O_RDWR);
	if (g_sensor_fd < 0)
		return -1;

	/* Set resolution */

	f.pad = 0;
	f.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	if (ioctl(g_sensor_fd, VIDIOC_SUBDEV_G_FMT, &f) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	if (f.format.width != g_sensor_width ||
	    f.format.height != g_sensor_height) {
		printf("Set sensor resolution: %dx%d -> %dx%d\n",
			f.format.width, f.format.height,
			g_sensor_width, g_sensor_height);
		f.format.width = g_sensor_width;
		f.format.height = g_sensor_height;
		if (ioctl(g_sensor_fd, VIDIOC_SUBDEV_S_FMT, &f) < 0) {
			ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
			return -1;
		}
	}

	/* Confirm the current resolution */
	if (ioctl(g_sensor_fd, VIDIOC_SUBDEV_G_FMT, &f) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	/* Set framerate */

	ret = ioctl(g_sensor_fd, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fr);
	if ((ret == 0) && (fr.interval.denominator != g_sensor_fr)) {
		printf("Set sensor framerate: %d -> %d\n",
			fr.interval.denominator, g_sensor_fr);
		fr.interval.denominator = g_sensor_fr;
		ret = ioctl(g_sensor_fd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fr);
		if (ret < 0) {
			ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
			return -1;
		}

		/* Confirm the current framerate */
		ioctl(g_sensor_fd, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fr);
	}

	g_vdata.src_fmt = f;
	g_vdata.w = g_vdata.src_fmt.format.width;
	g_vdata.h = g_vdata.src_fmt.format.height;
	printf("Sensor format: w %d h %d, code %#x, colorspace %#x, fr %d\n",
	    f.format.width, f.format.height, f.format.code,
	    f.format.colorspace, fr.interval.denominator);
	return 0;
}

int dvp_subdev_set_fmt(void)
{
	struct v4l2_subdev_format f = g_vdata.src_fmt;

	g_dvp_subdev_fd = device_open(DVP_SUBDEV_DEV, O_RDWR);
	if (g_dvp_subdev_fd < 0)
		return -1;

	f.pad = 0;
	f.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	if (ioctl(g_dvp_subdev_fd, VIDIOC_SUBDEV_S_FMT, &f) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int dvp_cfg(int width, int height, int format)
{
	struct v4l2_format f = {0};

	f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	f.fmt.pix_mp.width = g_vdata.src_fmt.format.width;
	f.fmt.pix_mp.height = g_vdata.src_fmt.format.height;
	f.fmt.pix_mp.pixelformat = g_vdata.fmt;
	f.fmt.pix_mp.num_planes = DVP_PLANE_NUM;
	if (ioctl(g_video_fd, VIDIOC_S_FMT, &f) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int dvp_expbuf(int index)
{
	int i;
	struct video_buf_info *binfo = &g_vdata.binfo[index];
	struct v4l2_exportbuffer expbuf = {0};

	for (i = 0; i < DVP_PLANE_NUM; i++) {
		memset(&expbuf, 0, sizeof(struct v4l2_exportbuffer));
		expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		expbuf.index = index;
		expbuf.plane = i;
		if (ioctl(g_video_fd, VIDIOC_EXPBUF, &expbuf) < 0) {
			ERR("ioctl() failed! err %d[%s]\n",
				errno, strerror(errno));
			return -1;
		}
		binfo->planes[i].fd = expbuf.fd;
		if (g_verbose)
			DBG("%d-%d Export DMABUF fd %d\n", index, i, expbuf.fd);
	}

	return 0;
}

int dvp_request_buf(int num)
{
	int i;
	struct v4l2_buffer buf = {0};
	struct v4l2_requestbuffers req = {0};
	struct v4l2_plane planes[DVP_PLANE_NUM];

	req.count  = num;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP; // Only MMAP will do alloc memory
	if (ioctl(g_video_fd, VIDIOC_REQBUFS, &req) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	for (i = 0; i < num; i++) {
	        if (dvp_expbuf(i) < 0)
			return -1;

	        memset(&buf, 0, sizeof(struct v4l2_buffer));
	        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	        buf.index = i;
	        buf.length = DVP_PLANE_NUM;
	        buf.memory = V4L2_MEMORY_DMABUF;
	        buf.m.planes = planes;
	        if (ioctl(g_video_fd, VIDIOC_QUERYBUF, &buf) < 0) {
			ERR("ioctl() failed! err %d[%s]\n",
				errno, strerror(errno));
			return -1;
	        }
	}

	return 0;
}

void dvp_release_buf(int num)
{
	int i;
	struct video_buf_info *binfo = NULL;

	for (i = 0; i < num; i++) {
		binfo = &g_vdata.binfo[i];
		if (binfo->vaddr) {
			munmap(binfo->vaddr, binfo->len);
			binfo->vaddr = NULL;
		}
	}
}

int dvp_queue_buf(int index)
{
	struct v4l2_buffer buf = {0};
	struct v4l2_plane planes[DVP_PLANE_NUM] = {0};

	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index  = index;
	buf.length = DVP_PLANE_NUM;
	buf.m.planes = planes;
	if (ioctl(g_video_fd, VIDIOC_QBUF, &buf) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int dvp_dequeue_buf(int *index)
{
	struct v4l2_buffer buf = {0};
	struct v4l2_plane planes[DVP_PLANE_NUM] = {0};

	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.length = DVP_PLANE_NUM;
	buf.m.planes = planes;
	if (ioctl(g_video_fd, VIDIOC_DQBUF, &buf) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	*index = buf.index;
	return 0;
}

int dvp_start(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (ioctl(g_video_fd, VIDIOC_STREAMON, &type) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int dvp_stop(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (ioctl(g_video_fd, VIDIOC_STREAMOFF, &type) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

#define DVP_SCALE_OFFSET	10

int video_layer_set(struct aic_video_data *vdata, int index)
{
	struct aicfb_layer_data layer = {0};
	struct video_buf_info *binfo = &vdata->binfo[index];

	layer.layer_id = 0;
	layer.enable = 1;

	if ((vdata->w < g_fb_xres - 2 * DVP_SCALE_OFFSET) &&
		(vdata->h < g_fb_yres - 2 * DVP_SCALE_OFFSET)) {
		layer.scale_size.width = vdata->w;
		layer.scale_size.height = vdata->h;
	} else {
		layer.scale_size.width = g_fb_xres - 2 * DVP_SCALE_OFFSET;
		layer.scale_size.height = g_fb_yres - 2 * DVP_SCALE_OFFSET;
	}

	layer.pos.x = DVP_SCALE_OFFSET;
	layer.pos.y = DVP_SCALE_OFFSET;
	layer.buf.size.width = vdata->w;
	layer.buf.size.height = vdata->h;
	if (g_vdata.fmt == V4L2_PIX_FMT_NV16)
		layer.buf.format = MPP_FMT_NV16;
	else
		layer.buf.format = MPP_FMT_NV12;

	layer.buf.buf_type = MPP_DMA_BUF_FD;
	layer.buf.fd[0] = binfo->planes[0].fd;
	layer.buf.fd[1] = binfo->planes[1].fd;
	layer.buf.stride[0] = vdata->w;
	layer.buf.stride[1] = vdata->w;

	if (ioctl(g_fb_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int video_layer_disable(void)
{
	struct aicfb_layer_data layer = {0};

	layer.layer_id = 0;
	if (ioctl(g_fb_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0) {
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
		return -1;
	}
	return 0;
}

#define US_PER_SEC      1000000

static void show_fps(struct timeval *start, struct timeval *end, int cnt)
{
    double diff;

    if (end->tv_usec < start->tv_usec) {
        diff = (double)(US_PER_SEC + end->tv_usec - start->tv_usec)/US_PER_SEC;
        diff += end->tv_sec - 1 - start->tv_sec;
    } else {
        diff = (double)(end->tv_usec - start->tv_usec)/US_PER_SEC;
        diff += end->tv_sec - start->tv_sec;
    }

    printf("\nDVP frame rate: %.1f, frame %d / %.1f seconds\n",
           (double)cnt/diff, cnt, diff);
}

int main(int argc, char **argv)
{
	int c, frame_cnt = 1;
	int i, index = 0;
	struct timeval start, end;

	printf("Compile time: %s %s\n", __DATE__, __TIME__);
	g_vdata.fmt = V4L2_PIX_FMT_NV12;
	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'f':
			if (strncasecmp("nv16", optarg, strlen(optarg)) == 0)
				g_vdata.fmt = V4L2_PIX_FMT_NV16;
			continue;
		case 'c':
			frame_cnt = str2int(optarg);
			continue;
		case 'w':
			g_sensor_width = str2int(optarg);
			continue;
		case 'h':
			g_sensor_height = str2int(optarg);
			continue;
		case 'r':
			g_sensor_fr = str2int(optarg);
			continue;
		case 'u':
			usage(argv[0]);
			return 0;
		case 'v':
			g_verbose = 1;
			continue;
		default:
			break;
		}
	}

	printf("Capture %d frames from camera\n", frame_cnt);
	printf("DVP out format: %s\n",
		g_vdata.fmt == V4L2_PIX_FMT_NV16 ? "NV16" : "NV12");

	if (sensor_set_fmt() < 0)
		return -1;
	if (dvp_subdev_set_fmt() < 0)
		return -1;

	if (g_vdata.fmt == V4L2_PIX_FMT_NV16)
		g_vdata.frame_size = g_vdata.w * g_vdata.h * 2;
	if (g_vdata.fmt == V4L2_PIX_FMT_NV12)
		g_vdata.frame_size = (g_vdata.w * g_vdata.h * 3) >> 1;

	g_fb_fd = device_open(FB_DEV, O_RDWR);
	if (g_fb_fd < 0)
		return -1;
	if (get_fb_size())
		goto end;
	if (set_ui_layer_alpha(15) < 0)
		goto end;

	g_video_fd = device_open(VIDEO_DEV, O_RDWR);
	if (g_video_fd < 0)
		goto end;
	if (dvp_cfg(g_vdata.w, g_vdata.h, g_vdata.fmt) < 0)
		goto end;
	if (dvp_request_buf(VID_BUF_NUM) < 0)
		goto end;

	vidbuf_dmabuf_begin(&g_vdata);
	for (i = 0; i < VID_BUF_NUM; i++)
		if (dvp_queue_buf(i) < 0)
			goto end;

	if (dvp_start() < 0)
		goto end;

	gettimeofday(&start, NULL);
	for (i = 0; i < frame_cnt; i++) {
		if (dvp_dequeue_buf(&index) < 0)
			break;
		if (g_verbose)
			DBG("Set the buf %d to video layer\n", index);
		if (video_layer_set(&g_vdata, index) < 0)
			break;
		dvp_queue_buf(index);

		if (i && (i % 1000 == 0)) {
			gettimeofday(&end, NULL);
			show_fps(&start, &end, i);
		}
	}
	gettimeofday(&end, NULL);
	show_fps(&start, &end, i);

	dvp_stop();
	vidbuf_dmabuf_end(&g_vdata);
	dvp_release_buf(VID_BUF_NUM);

end:
	video_layer_disable();
	if (g_fb_fd > 0)
		close(g_fb_fd);
	if (g_video_fd > 0)
		close(g_video_fd);
	if (g_sensor_fd > 0)
		close(g_sensor_fd);
	if (g_dvp_subdev_fd > 0)
		close(g_dvp_subdev_fd);

	return 0;
}
