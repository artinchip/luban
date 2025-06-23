// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:
 *	Matteo <duanmt@artinchip.com>
 *	Huahui Mai <huahui.mai@artinchip.com>
 */

#include <artinchip/sample_base.h>
#include <stdbool.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <video/artinchip_fb.h>

/* Global macro and variables */

#define ALIGN_8B(x) (((x) + (7)) & ~(7))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#define ALIGN_32B(x) (((x) + (31)) & ~(31))
#define ALIGN_64B(x) (((x) + (63)) & ~(63))
#define ALIGN_128B(x) (((x) + (127)) & ~(127))

#define FORMAT_INVALID		16
#define AICFB_VID_BUF_NUM	2
#define AIC_CMA_BUF_MAX		(8 * 1024 * 1024)
#define DMA_HEAP_DEV		"/dev/dma_heap/reserved"
#define FB_DEV			"/dev/fb0"

static const char sopts[] = "w:h:s:f:i:lu";
static const struct option lopts[] = {
	{"width",	  required_argument, NULL, 'w'},
	{"height",	  required_argument, NULL, 'h'},
	{"stride",	  required_argument, NULL, 's'},
	{"format",	  required_argument, NULL, 'f'},
	{"input",	  required_argument, NULL, 'i'},
	{"list",		no_argument, NULL, 'l'},
	{"usage",		no_argument, NULL, 'u'},
	{0, 0, 0, 0}
};

struct video_data_format {
	enum mpp_pixel_format format;
	char f_str[15];
	int plane_num;
	union
	{
		int y_shift;
		int pixel_bytes;
	};
	int u_shift;
	int v_shift;
};

struct video_data_format g_vformat[] = {
	{MPP_FMT_YUV420P, "yuv420p", 3, {0}, 2, 2},
	{MPP_FMT_YUV422P, "yuv422p", 3, {0}, 1, 1},

	{MPP_FMT_NV12, "nv12", 2, {0}, 1, 0},
	{MPP_FMT_NV21, "nv21", 2, {0}, 1, 0},
	{MPP_FMT_NV16, "nv16", 2, {0}, 0, 0},
	{MPP_FMT_NV61, "nv61", 2, {0}, 0, 0},

	{MPP_FMT_YUYV, "yuyv", 1, {1}, 0, 0},
	{MPP_FMT_YVYU, "yvyu", 1, {1}, 0, 0},
	{MPP_FMT_UYVY, "uyvy", 1, {1}, 0, 0},
	{MPP_FMT_VYUY, "vyuy", 1, {1}, 0, 0},

	{MPP_FMT_YUV400, "yuv400", 1, {0}, 0, 0},

	{MPP_FMT_YUV420_128x16_TILE, "yuv420_128x16", 2, {0}, 1, 0},
	{MPP_FMT_YUV420_64x32_TILE,  "yuv420_64x32",  2, {0}, 1, 0},
	{MPP_FMT_YUV422_128x16_TILE, "yuv422_128x16", 2, {0}, 0, 0},
	{MPP_FMT_YUV422_64x32_TILE,  "yuv422_64x32",  2, {0}, 0, 0},

	{MPP_FMT_XRGB_8888, "xrgb8888" ,1, {4}, 0, 0},
	{MPP_FMT_ARGB_8888, "argb8888", 1, {4}, 0, 0},
	{MPP_FMT_ARGB_4444, "argb4444", 1, {2}, 0, 0},
	{MPP_FMT_ARGB_1555, "argb1555", 1, {2}, 0, 0},
	{MPP_FMT_RGB_565,   "rgb565"  , 1, {2}, 0, 0},
	{MPP_FMT_RGB_888,   "rgb888"  , 1, {3}, 0, 0},

	{MPP_FMT_MAX, "", 0, {0}, 0, 0}
};

struct video_plane {
	int fd;
	char *buf;
	int len;
};

struct video_buf {
	struct video_plane y;
	struct video_plane u;
	struct video_plane v;
};

struct aicfb_video_layer {
	int w;
	int h;
	int s;
	struct video_data_format *f;
	struct video_buf vbuf[AICFB_VID_BUF_NUM];
};

static int g_fb_fd = -1;
static struct aicfb_video_layer g_vlayer = {0};

/* Functions */
void usage(char *program)
{
	printf("Usage: %s [options]: \n", program);
	printf("\t -w, --width\t\tneed an integer argument\n");
	printf("\t -h, --height\t\tneed an integer argument\n");
	printf("\t -s, --stride\t\tvideo stride, just tile format need\n");
	printf("\t -f, --format\t\tvideo format, yuv420p etc\n");
	printf("\t -i, --input\t\tneed a file name \n");
	printf("\t -l, --list\t\tlist the supported formats\n");
	printf("\t -u, --usage \n");
	printf("\n");
	printf("Example: %s -w 176 -h 144 -f yuv420p/rgb888 -i my.yuv/my.rgb\n", program);
}

int format_list(char *program)
{
	printf("%s support the following formats:\n", program);
	printf("\t yuv420p\n");
	printf("\t yuv422p\n");
	printf("\t yuv400\n");
	printf("\t nv12\n");
	printf("\t nv21\n");
	printf("\t nv16\n");
	printf("\t nv61\n");
	printf("\t yuyv\n");
	printf("\t yvyu\n");
	printf("\t uyvy\n");
	printf("\t vyuy\n");
	printf("\t yuv420_128x16\n");
	printf("\t yuv420_64x32\n");
	printf("\t yuv422_128x16\n");
	printf("\t yuv422_64x32\n");
	printf("\t rgb565\n");
	printf("\t rgb888\n");
	printf("\t xrgb888\n");
	printf("\t argb4444\n");
	printf("\t argb8888\n");
	printf("\t argb1555\n");
	printf("\n");
	return 0;
}

static inline bool format_invalid(enum mpp_pixel_format format)
{
	if (format == MPP_FMT_MAX)
		return true;

	return false;
}

static inline bool is_packed_format(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		return true;
	default:
		break;
	}
	return false;
}

static inline bool is_tile_format(enum mpp_pixel_format format)
{
	switch (format) {
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

static inline bool is_plane_format(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_YUV420P:
	case MPP_FMT_YUV422P:
	case MPP_FMT_YUV400:
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		return true;
	default:
		break;
	}
	return false;
}

static inline bool is_2plane(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		return true;
	default:
		break;
	}
	return false;
}

static inline bool is_tile_16_align(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_YUV420_128x16_TILE:
	case MPP_FMT_YUV422_128x16_TILE:
		return true;
	default:
		break;
	}
	return false;
}

static inline bool is_tile_32_align(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_YUV420_64x32_TILE:
	case MPP_FMT_YUV422_64x32_TILE:
		return true;
	default:
		break;
	}
	return false;
}

static int tile_format_size(struct aicfb_video_layer *vlayer, int shift)
{
	int size = -1;

	if (is_tile_16_align(vlayer->f->format))
		size = vlayer->s * ALIGN_16B(vlayer->h >> shift);

	if (is_tile_32_align(vlayer->f->format))
		size = vlayer->s * ALIGN_32B(vlayer->h >> shift);

	return size;
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

int vidbuf_request_one(struct video_plane *plane, int len, int fd)
{
	int ret;
	char *buf = NULL;
	struct dma_heap_allocation_data data = {0};

	if ((len < 0) || (len > AIC_CMA_BUF_MAX)) {
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
	plane->fd = data.fd;

	buf = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED,	data.fd, 0);
	if (buf == MAP_FAILED) {
		ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
		return -1;
	}
	DBG("Get virtual address: %p\n", buf);
	plane->buf = buf;
	plane->len = len;
	return 0;
}

void aic_fb_open(void)
{
	if (g_fb_fd > 0)
		return;

	g_fb_fd = device_open(FB_DEV, O_RDWR);
	if (g_fb_fd < 0)
		ERR("Failed to open %s. errno: %d[%s]\n",
			FB_DEV, errno, strerror(errno));
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

int vidbuf_request(struct aicfb_video_layer *vlayer)
{
	int i, j;
	int heap_fd = -1;
	int y_frame = vlayer->w * vlayer->h;

	heap_fd = device_open(DMA_HEAP_DEV, O_RDWR);
	if (heap_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		return -1;
	}
	/* Prepare two group buffer for video player*/
	for (i = 0; i < AICFB_VID_BUF_NUM; i++) {
		struct video_plane *p = (struct video_plane *)&vlayer->vbuf[i];
		int *shift = &vlayer->f->y_shift;
		for (j = 0; j < vlayer->f->plane_num; j++, p++) {

			if (is_tile_format(vlayer->f->format))
				vidbuf_request_one(p, tile_format_size(vlayer, shift[j]), heap_fd);

			if (is_packed_format(vlayer->f->format))
				vidbuf_request_one(p, y_frame << shift[j], heap_fd);

			if (is_plane_format(vlayer->f->format))
				vidbuf_request_one(p, y_frame >> shift[j], heap_fd);

			if (is_rgb_format(vlayer->f->format))
				vidbuf_request_one(p, y_frame * shift[j], heap_fd);
		}
	}
	close(heap_fd);
	return 0;
}

void vidbuf_release(struct aicfb_video_layer *vlayer)
{
	int i, j;

	for (i = 0; i < AICFB_VID_BUF_NUM; i++) {
		struct video_plane *p = (struct video_plane *)&vlayer->vbuf[i];
		for (j = 0; j < vlayer->f->plane_num; j++, p++) {
			if (munmap(p->buf, p->len) < 0)
				ERR("munmap() failed! errno: %d[%s]\n",
					errno, strerror(errno));

			close(p->fd);
		}
	}
}

void vidbuf_dmabuf_begin(struct aicfb_video_layer *vlayer)
{
	int i, j;
	struct dma_buf_info fds = {0};

	for (i = 0; i < AICFB_VID_BUF_NUM; i++) {
		struct video_plane *p = (struct video_plane *)&vlayer->vbuf[i];
		for (j = 0; j < vlayer->f->plane_num; j++, p++) {
			fds.fd = p->fd;
			if (ioctl(g_fb_fd, AICFB_ADD_DMABUF, &fds) < 0)
				ERR("ioctl() failed! err %d[%s]\n",
					errno, strerror(errno));
		}
	}
}

void vidbuf_dmabuf_end(struct aicfb_video_layer *vlayer)
{
	int i, j;
	struct dma_buf_info fds = {0};

	for (i = 0; i < AICFB_VID_BUF_NUM; i++) {
		struct video_plane *p = (struct video_plane *)&vlayer->vbuf[i];
		for (j = 0; j < vlayer->f->plane_num; j++, p++) {
			fds.fd = p->fd;
			if (ioctl(g_fb_fd, AICFB_RM_DMABUF, &fds) < 0)
				ERR("ioctl() failed! err %d[%s]\n",
					errno, strerror(errno));

			close(fds.fd);
		}
	}
}

void video_layer_set(struct aicfb_video_layer *vlayer, int index)
{
	struct aicfb_layer_data layer = {0};
	struct video_buf *vbuf = &vlayer->vbuf[index];

	layer.layer_id = 0;
	layer.enable = 1;
	layer.scale_size.width = vlayer->w;
	layer.scale_size.height = vlayer->h;
	layer.pos.x = 10;
	layer.pos.y = 10;
	layer.buf.size.width = vlayer->w;
	layer.buf.size.height = vlayer->h;
	layer.buf.format = vlayer->f->format;
	layer.buf.buf_type = MPP_DMA_BUF_FD;

	layer.buf.fd[0] = vbuf->y.fd;
	layer.buf.fd[1] = vbuf->u.fd;
	layer.buf.fd[2] = vbuf->v.fd;

	if (is_packed_format(vlayer->f->format))
		layer.buf.stride[0] = vlayer->w << 1;

	if (is_tile_format(vlayer->f->format)) {
		layer.buf.stride[0] = vlayer->s;
		layer.buf.stride[1] = vlayer->s;
	}

	if (is_plane_format(vlayer->f->format)) {
		layer.buf.stride[0] = vlayer->w;
		layer.buf.stride[1] = is_2plane(vlayer->f->format) ?
					vlayer->w : vlayer->w >> 1;
		layer.buf.stride[2] = vlayer->w >> 1;
	}

	if(is_rgb_format(vlayer->f->format))
		layer.buf.stride[0] = vlayer->w * vlayer->f->pixel_bytes;

	if (ioctl(g_fb_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		ERR("ioctl() failed! err %d[%s]\n", errno, strerror(errno));
}

void vidbuf_cpu_begin(struct video_buf *vbuf, int plane_num)
{
	int i;
	struct dma_buf_sync flag;
	struct video_plane *p = (struct video_plane *)vbuf;

	for (i = 0; i < plane_num; i++, p++) {
		flag.flags = DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_START;
		if (ioctl(p->fd, DMA_BUF_IOCTL_SYNC, &flag) < 0)
			ERR("ioctl() failed! err %d[%s]\n",
				errno, strerror(errno));
	}
}

void vidbuf_cpu_end(struct video_buf *vbuf, int plane_num)
{
	int i;
	struct dma_buf_sync flag;
	struct video_plane *p = (struct video_plane *)vbuf;

	for (i = 0; i < plane_num; i++, p++) {
		flag.flags = DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_END;
		if (ioctl(p->fd, DMA_BUF_IOCTL_SYNC, &flag) < 0)
			ERR("ioctl() failed! err %d[%s]\n",
				errno, strerror(errno));
	}
}

int vidbuf_read(struct aicfb_video_layer *vlayer, int index, int fd, int plane_num)
{
	int i, ret = 0;
	static int frame_cnt = 0;
	struct video_plane *p = (struct video_plane *)&vlayer->vbuf[index];

	if (frame_cnt == 0)
		lseek(fd, 0, SEEK_SET);

	for (i = 0; i < plane_num; i++, p++) {
		ret = read(fd, p->buf, p->len);
		if (ret != p->len) {
			ERR("read(%d) return %d. errno: %d[%s] "
				"frame %d - %d, offset %ld\n",
				p->len, ret, errno, strerror(errno),
				frame_cnt, i, lseek(fd, 0, SEEK_CUR));
			return -1;
		}
	}
	frame_cnt++;
	return ret;
}

int format_parse(char *str)
{
	int i;

	for (i = 0; g_vformat[i].format != MPP_FMT_MAX; i++) {
		if (strncasecmp(g_vformat[i].f_str, str, strlen(str)) == 0)
			return i;
	}

	ERR("Invalid format: %s\n", str);
	return FORMAT_INVALID;
}

int main(int argc, char **argv)
{
	int vid_fd = -1;
	int ret = 0;
	int c = 0;
	int fsize = 0;
	int index = 0;

	DBG("Compile time: %s\n", __TIME__);
	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'w':
			g_vlayer.w = str2int(optarg);
			continue;
		case 'h':
			g_vlayer.h = str2int(optarg);
			continue;
		case 's':
			g_vlayer.s = str2int(optarg);
			break;
		case 'f':
			g_vlayer.f = &g_vformat[format_parse(optarg)];
			if (format_invalid(g_vlayer.f->format))
				return format_list(argv[0]);
			break;
		case 'i':
			vid_fd = device_open(optarg, O_RDONLY);
			if (vid_fd < 0) {
				ERR("Failed to open %s. errno: %d[%s]\n",
					optarg, errno, strerror(errno));
				return -1;
			}
			fsize = lseek(vid_fd, 0, SEEK_END);
			DBG("open(%s) %d, size %d\n", optarg, vid_fd, fsize);
			break;
		case 'l':
			format_list(argv[0]);
			return 0;
		case 'u':
			usage(argv[0]);
			return 0;
		default:
			break;
		}
	}

	if (is_tile_format(g_vlayer.f->format) && (g_vlayer.s == 0)) {
		ERR("YUV tile format need a stride\n");
		return 0;
	}

	aic_fb_open();
	set_ui_layer_alpha(128);
	vidbuf_request(&g_vlayer);
	vidbuf_dmabuf_begin(&g_vlayer);

	do {
		struct video_buf *vbuf = &g_vlayer.vbuf[index];
		int plane_num = g_vlayer.f->plane_num;

		vidbuf_cpu_begin(vbuf, plane_num);
		ret = vidbuf_read(&g_vlayer, index, vid_fd, plane_num);
		vidbuf_cpu_end(vbuf, plane_num);
		if (ret < 0)
			break;

		video_layer_set(&g_vlayer, index);
		index = !index;
		usleep(40000);
		if (lseek(vid_fd, 0, SEEK_CUR) == fsize)
			break;
	} while (1);

	vidbuf_dmabuf_end(&g_vlayer);
	vidbuf_release(&g_vlayer);

	if (vid_fd > 0)
		close(vid_fd);
	if (g_fb_fd > 0)
		close (g_fb_fd);
	return ret;
}
