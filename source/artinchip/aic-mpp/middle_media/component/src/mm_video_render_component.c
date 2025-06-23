/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media video render component
 */

#include "mm_video_render_component.h"

#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "mpp_decoder.h"
#include "mpp_dec_type.h"
#include "aic_render.h"
#include "dma_allocator.h"
#include "mpp_encoder.h"
#include "mpp_ge.h"


#define VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG 0x02
#define VIDEO_RENDER_WAIT_FRAME_INTERVAL (10 * 1000 * 1000)
#define VIDEO_RENDER_WAIT_FRAME_MAX_TIME (8 * 1000 * 1000)
#define MAX_VIDEO_DELAY_TIME_THRESHOLD (3 * 1000 * 1000)
#define MAX_VIDEO_SYNC_DIFF_TIME (10 * 1000)

typedef enum MM_VIDEO_SYNC_TYPE {
    MM_VIDEO_SYNC_INVAILD = -1,
    MM_VIDEO_SYNC_DROP = 0,
    MM_VIDEO_SYNC_DEALY,
    MM_VIDEO_SYNC_SHOW,
} MM_VIDEO_SYNC_TYPE;

typedef struct mm_video_render_dma_fd {
    int fd[3];
    struct mpp_list list;
} mm_video_render_dma_fd;

typedef struct mm_video_render_data {
    MM_STATE_TYPE state;
    pthread_mutex_t state_lock;
    mm_callback *p_callback;
    void *p_app_data;
    mm_handle h_self;
    mm_port_param port_param;

    mm_param_port_def in_port_def[2];
    mm_param_port_def out_port_def[2];

    mm_bind_info in_port_bind[2];
    mm_bind_info out_port_bind[2];

    pthread_t thread_id;
    struct aic_message_queue s_msg;

    struct aic_video_render *render;
    s32 layer_id;
    s32 dev_id;
    struct mpp_rect dis_rect;
    s32 dis_rect_change;

    //rotation
    s32 init_rotation_param; // -1-init fail,0-not init,1-init ok
    s32 rotation_angle_change;
    s32 rotation_angle;
    s32 dma_fd;
    struct mpp_frame rotation_frames[2];

    struct mpp_frame *p_cur_display_frame;
    struct mpp_ge *ge_handle;
    struct mpp_list dma_list;
    u32 rotation_index;

    struct mpp_frame disp_frames[2];
    s32 cur_disp_frame_id;
    s32 last_disp_frame_id;
    pthread_mutex_t in_frame_lock;

    MM_TIME_CLOCK_STATE clock_state;

    MM_BOOL frame_first_show_flag;
    MM_BOOL video_render_init_flag;
    MM_BOOL frame_end_flag;
    MM_BOOL flags;
    MM_BOOL wait_ready_frame_flag;

    u32 receive_frame_num;
    u32 show_frame_ok_num;
    u32 show_frame_fail_num;
    u32 giveback_frame_fail_num;
    u32 giveback_frame_ok_num;
    u32 drop_frame_num;
    u32 disp_frame_num;
    u32 calc_frame_rate;

    s64 first_show_pts; //us
    s64 wall_time_base;
    s64 pause_time_point;
    s64 pause_time_durtion;
    s64 pre_frame_pts;
    s32 dump_index;
} mm_video_render_data;

static void *mm_video_render_component_thread(void *p_thread_data);


static int mm_video_render_free_dma_buffer(struct mpp_frame* p_frame,int dma_fd)
{
    if (p_frame == NULL || dma_fd <= 0) {
        return -1;
    }
    if (p_frame->buf.fd[0] > 0) {
        dmabuf_free(p_frame->buf.fd[0]);
    }
    if (p_frame->buf.fd[0] > 0) {
        dmabuf_free(p_frame->buf.fd[0]);
    }
    if (p_frame->buf.fd[0] > 0) {
        dmabuf_free(p_frame->buf.fd[0]);
    }
    dmabuf_device_close(dma_fd);
    return 0;
}

static int mm_video_render_alloc_dma_buffer(struct mpp_frame *p_frame,
                                            mm_param_video_capture *p_capture)
{
    int ret = 0;
    int dma_fd;
    if (p_frame == NULL || p_capture == NULL) {
        return -1;
    }

    dma_fd = dmabuf_device_open();
    if (dma_fd < 0) {
        loge("dmabuf_device_open error dma_fd:%d \n", dma_fd);
        return -1;
    }

    p_frame->buf.fd[0] = dmabuf_alloc(dma_fd, p_capture->width * p_capture->height);
    if (p_frame->buf.fd[0] <= 0) {
        loge("p_dst_frame->buf.fd[0] alloc fail!!!\n");
        ret = -1;
        goto _exit0;
    }

    p_frame->buf.fd[1] = dmabuf_alloc(dma_fd, p_capture->width * p_capture->height / 4);
    if (p_frame->buf.fd[1] <= 0) {
        loge("p_dst_frame->buf.fd[1] alloc fail!!!\n");
        ret = -1;
        goto _exit1;
    }

    p_frame->buf.fd[2] = dmabuf_alloc(dma_fd, p_capture->width * p_capture->height / 4);
    if (p_frame->buf.fd[2] <= 0) {
        loge("p_dst_frame->buf.fd[2] alloc fail!!!\n");
        ret = -1;
        goto _exit2;
    }

    return dma_fd;

_exit2:
    if (p_frame->buf.fd[1] > 0)
        dmabuf_free(p_frame->buf.fd[1]);
_exit1:
    if (p_frame->buf.fd[0] > 0)
        dmabuf_free(p_frame->buf.fd[0]);
_exit0:
    if (dma_fd > 0)
        dmabuf_device_close(dma_fd);

    return ret;
}

static int mm_video_render_scale_pic(struct mpp_frame *p_src_frame,
                                     struct mpp_frame *p_dst_frame,
                                     mm_param_video_capture *p_capture)
{
    int ret = 0;
    struct ge_bitblt blt;
    struct mpp_ge *ge = NULL;

    if (p_src_frame == NULL || p_dst_frame == NULL) {
        loge("param error !!!\n");
        return -1;
    }

    ge = mpp_ge_open();
    if (!ge) {
        loge("open ge device\n");
        return -1;
    }

    memset(&blt, 0x00, sizeof(struct ge_bitblt));
    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    blt.src_buf.format = p_src_frame->buf.format;
    blt.src_buf.fd[0] = p_src_frame->buf.fd[0];
    blt.src_buf.fd[1] = p_src_frame->buf.fd[1];
    blt.src_buf.fd[2] = p_src_frame->buf.fd[2];
    blt.src_buf.stride[0] = p_src_frame->buf.stride[0];
    blt.src_buf.stride[1] = p_src_frame->buf.stride[1];
    blt.src_buf.stride[2] = p_src_frame->buf.stride[2];
    blt.src_buf.size.width = p_src_frame->buf.size.width;
    blt.src_buf.size.height = p_src_frame->buf.size.height;
    blt.src_buf.crop_en = 0;

    blt.dst_buf.buf_type = p_dst_frame->buf.buf_type = MPP_DMA_BUF_FD;
    blt.dst_buf.format = p_dst_frame->buf.format = MPP_FMT_YUV420P;
    blt.dst_buf.fd[0] = p_dst_frame->buf.fd[0];
    blt.dst_buf.fd[1] = p_dst_frame->buf.fd[1];
    blt.dst_buf.fd[2] = p_dst_frame->buf.fd[2];
    blt.dst_buf.stride[0] = p_dst_frame->buf.stride[0] = p_capture->width;
    blt.dst_buf.stride[1] = p_dst_frame->buf.stride[1] = p_capture->width / 2;
    blt.dst_buf.stride[2] = p_dst_frame->buf.stride[2] = p_capture->width / 2;
    blt.dst_buf.size.width = p_dst_frame->buf.size.width = p_capture->width;
    blt.dst_buf.size.height = p_dst_frame->buf.size.height = p_capture->height;
    blt.dst_buf.crop_en = 0;

    if (p_src_frame->buf.fd[0] > 0)
        mpp_ge_add_dmabuf(ge, p_src_frame->buf.fd[0]);
    if (p_src_frame->buf.fd[1] > 0)
        mpp_ge_add_dmabuf(ge, p_src_frame->buf.fd[1]);
    if (p_src_frame->buf.fd[2] > 0)
        mpp_ge_add_dmabuf(ge, p_src_frame->buf.fd[2]);

    if (p_dst_frame->buf.fd[0] > 0)
        mpp_ge_add_dmabuf(ge, p_dst_frame->buf.fd[0]);
    if (p_dst_frame->buf.fd[1] > 0)
        mpp_ge_add_dmabuf(ge, p_dst_frame->buf.fd[1]);
    if (p_dst_frame->buf.fd[2] > 0)
        mpp_ge_add_dmabuf(ge, p_dst_frame->buf.fd[2]);

    ret = mpp_ge_bitblt(ge, &blt);
    if (ret) {
        loge("mpp_ge_bitblt task failed: %d\n", ret);
        ret = -1;
        goto _exit0;
    }

    ret = mpp_ge_emit(ge);
    if (ret) {
        loge("mpp_ge_emit task failed: %d\n", ret);
        ret = -1;
        goto _exit0;
    }
    ret = mpp_ge_sync(ge);
    if (ret) {
        loge("mpp_ge_sync task failed: %d\n", ret);
        ret = -1;
        goto _exit0;
    }

_exit0:
    if (p_src_frame->buf.fd[0] > 0)
        mpp_ge_rm_dmabuf(ge, p_src_frame->buf.fd[0]);

    if (p_src_frame->buf.fd[1] > 0)
        mpp_ge_rm_dmabuf(ge, p_src_frame->buf.fd[1]);

    if (p_src_frame->buf.fd[2] > 0)
        mpp_ge_rm_dmabuf(ge, p_src_frame->buf.fd[2]);

    if (p_dst_frame->buf.fd[0] > 0)
        mpp_ge_rm_dmabuf(ge, p_dst_frame->buf.fd[0]);

    if (p_dst_frame->buf.fd[1] > 0)
        mpp_ge_rm_dmabuf(ge, p_dst_frame->buf.fd[1]);

    if (p_dst_frame->buf.fd[2] > 0)
        mpp_ge_rm_dmabuf(ge, p_dst_frame->buf.fd[2]);

    if (ge)
        mpp_ge_close(ge);

    return ret;
}

static int mm_video_render_encode_jpg(struct mpp_frame *p_frame,
                                      mm_param_video_capture *p_capture)
{
    int ret = 0;
    int quality = 90;
    int len, buf_len;
    int width, height;
    int jpeg_data_fd, dma_fd;
    FILE *fp_save = NULL;
    unsigned char *jpeg_vir_addr = NULL;

    dma_fd = dmabuf_device_open();
    if (dma_fd < 0) {
        loge("dmabuf_device_open error dma_fd:%d \n", dma_fd);
        return -1;
    }

    fp_save = fopen((char *)p_capture->p_file_path, "wb");
    if (fp_save == NULL) {
        loge("fopen %s error\n", p_capture->p_file_path);
        ret = -1;
        goto _exit0;
    }

    quality = p_capture->quality;
    width = p_frame->buf.size.width;
    height = p_frame->buf.size.height;
    buf_len = width * height * 4 / 5 * quality / 100;

    jpeg_data_fd = dmabuf_alloc(dma_fd, buf_len);
    if (jpeg_data_fd < 0) {
        loge("dmabuf_device_open error jpeg_data_fd:%d \n", jpeg_data_fd);
        ret = -1;
        goto _exit1;
    }

    jpeg_vir_addr = dmabuf_mmap(jpeg_data_fd, buf_len);
    if (!jpeg_vir_addr) {
        loge("dmabuf_mmap error\n");
        ret = -1;
        goto _exit2;
    }

    if (mpp_encode_jpeg(p_frame, quality, jpeg_data_fd, buf_len, &len) < 0) {
        loge("encode failed");
        ret = -1;
        goto _exit3;
    }

    fwrite(jpeg_vir_addr, 1, len, fp_save);

_exit3:
    if (jpeg_vir_addr)
        dmabuf_munmap(jpeg_vir_addr, buf_len);
_exit2:
    if (jpeg_data_fd > 0)
        dmabuf_free(jpeg_data_fd);
_exit1:
    if (fp_save)
        fclose(fp_save);
_exit0:
    if (dma_fd > 0)
        dmabuf_device_close(dma_fd);

    return ret;
}

static s32 mm_video_render_capture(mm_video_render_data *p_video_render_data,
                                   void *p_config)
{
    s32 error = MM_ERROR_NONE;
    s32 dma_fd = -1;
    s32 last_frame_id;
    mm_param_video_capture *p_capture = (mm_param_video_capture *)p_config;

    if (p_video_render_data->state != MM_STATE_PAUSE) {
        return MM_ERROR_INVALID_STATE;
    }

    struct mpp_frame dst_frame;

    memset(&dst_frame, 0x00, sizeof(struct mpp_frame));
    printf("p_capture->width:%d,p_capture->height:%d,p_capture->p_file_path:%s\n",
           p_capture->width, p_capture->height, p_capture->p_file_path);

    dma_fd = mm_video_render_alloc_dma_buffer(&dst_frame, p_capture);
    if (dma_fd <= 0) {
        loge("mm_video_render_alloc_dma_buffer fail \n");
        return MM_ERROR_UNDEFINED;
    }

    last_frame_id = p_video_render_data->last_disp_frame_id;
    if (mm_video_render_scale_pic(&p_video_render_data->disp_frames[last_frame_id],
                                  &dst_frame, p_capture) != 0) {
        loge("mm_video_render_scale_pic fail \n");
        mm_video_render_free_dma_buffer(&dst_frame, dma_fd);
        return MM_ERROR_UNDEFINED;
    }

    if (mm_video_render_encode_jpg(&dst_frame, p_capture) != 0) {
        loge("mm_video_render_encode_jpg fail \n");
        error = MM_ERROR_UNDEFINED;
    }

    mm_video_render_free_dma_buffer(&dst_frame, dma_fd);

    return error;
}

static s32 mm_video_render_get_component_num(enum mpp_pixel_format format)
{
    int component_num = 0;
    if (format == MPP_FMT_ARGB_8888) {
        component_num = 1;
    } else if (format == MPP_FMT_RGBA_8888) {
        component_num = 1;
    } else if (format == MPP_FMT_RGB_888) {
        component_num = 1;
    } else if (format == MPP_FMT_YUV420P) {
        component_num = 3;
    } else if (format == MPP_FMT_NV12 || format == MPP_FMT_NV21) {
        component_num = 2;
    } else if (format == MPP_FMT_YUV444P) {
        component_num = 3;
    } else if (format == MPP_FMT_YUV422P) {
        component_num = 3;
    } else if (format == MPP_FMT_YUV400) {
        component_num = 1;
    } else {
        loge("no sup_port picture foramt %d, default argb8888", format);
    }
    return component_num;
}


static int mm_video_render_is_exist_dma_fd(mm_video_render_data * p_video_render_data,struct mpp_frame* p_frame)
{
    int comp = 0;
    int find = 0;
    if (mpp_list_empty(&p_video_render_data->dma_list)) {
        return find;
    }
    comp = mm_video_render_get_component_num(p_frame->buf.format);
    if (!mpp_list_empty(&p_video_render_data->dma_list)) {
        mm_video_render_dma_fd *dma_fd_node = NULL, *dma_fd_node1 = NULL;
        mpp_list_for_each_entry_safe(dma_fd_node, dma_fd_node1,
                                     &p_video_render_data->dma_list, list)
        {
            if (comp == 1) {
                if (dma_fd_node->fd[0] == p_frame->buf.fd[0]) {
                    find = 1;
                    break;
                }
            } else if (comp == 2) {
                if (dma_fd_node->fd[0] == p_frame->buf.fd[0] &&
                    dma_fd_node->fd[1] == p_frame->buf.fd[1]) {
                    find = 1;
                    break;
                }
            } else if (comp == 3) {
                if (dma_fd_node->fd[0] == p_frame->buf.fd[0] &&
                    dma_fd_node->fd[1] == p_frame->buf.fd[1] &&
                    dma_fd_node->fd[2] == p_frame->buf.fd[2]) {
                    find = 1;
                    break;
                }
            }
        }
    }
    return find;
}

static int mm_video_render_add_dma_fd(mm_video_render_data *p_video_render_data,
                                      struct mpp_frame *p_frame)
{
    int comp = 0, i = 0;
    mm_video_render_dma_fd *dma_fd_node = NULL;

    comp = mm_video_render_get_component_num(p_frame->buf.format);
    dma_fd_node = (mm_video_render_dma_fd *)mpp_alloc(sizeof(mm_video_render_dma_fd));
    memset(dma_fd_node, 0x00, sizeof(mm_video_render_dma_fd));
    for (i = 0; i < comp; i++) {
        dma_fd_node->fd[i] = p_frame->buf.fd[i];
    }
    mpp_list_add_tail(&dma_fd_node->list, &p_video_render_data->dma_list);
    return 0;
}

static int mm_video_render_remove_dma_fd(mm_video_render_data *p_video_render_data,
                                         struct mpp_frame *p_frame)
{
    int comp = 0;
    int find = -1;
    if (mpp_list_empty(&p_video_render_data->dma_list)) {
        return find;
    }
    comp = mm_video_render_get_component_num(p_frame->buf.format);
    if (!mpp_list_empty(&p_video_render_data->dma_list)) {
        mm_video_render_dma_fd *dma_fd_node = NULL, *dma_fd_node1 = NULL;
        mpp_list_for_each_entry_safe(dma_fd_node, dma_fd_node1,
                                     &p_video_render_data->dma_list, list)
        {
            if (comp == 1) {
                if (dma_fd_node->fd[0] == p_frame->buf.fd[0]) {
                    mpp_list_del(&dma_fd_node->list);
                    mpp_free(dma_fd_node);
                    find = 0;
                    break;
                }
            } else if (comp == 2) {
                if (dma_fd_node->fd[0] == p_frame->buf.fd[0] &&
                    dma_fd_node->fd[1] == p_frame->buf.fd[1]) {
                    mpp_list_del(&dma_fd_node->list);
                    mpp_free(dma_fd_node);
                    find = 0;
                    break;
                }
            } else if (comp == 3) {
                if (dma_fd_node->fd[0] == p_frame->buf.fd[0] &&
                    dma_fd_node->fd[1] == p_frame->buf.fd[1] &&
                    dma_fd_node->fd[2] == p_frame->buf.fd[2]) {
                    mpp_list_del(&dma_fd_node->list);
                    mpp_free(dma_fd_node);
                    find = 0;
                    break;
                }
            }
        }
    }
    return find;
}


static int
mm_video_render_free_frame_buffer(mm_video_render_data *p_video_render_data,
                                  struct mpp_frame *p_frame)
{
    int i = 0;
    int comp = 0;

    if (!p_video_render_data || !p_frame) {
        return -1;
    }

    comp = mm_video_render_get_component_num(p_frame->buf.format);

    // remove dmafd in the GE
    if (mm_video_render_is_exist_dma_fd(p_video_render_data, p_frame)) {
        mm_video_render_remove_dma_fd(p_video_render_data, p_frame);
        for (i = 0; i < comp; i++) {
            mpp_ge_rm_dmabuf(p_video_render_data->ge_handle, p_frame->buf.fd[i]);
            printf("[%s:%d]:%d\n", __FUNCTION__, __LINE__, p_frame->buf.fd[i]);
        }
    }

    for (i = 0; i < comp; i++) {
        if (p_frame->buf.fd[i]) {
            dmabuf_free(p_frame->buf.fd[i]);
            p_frame->buf.fd[i] = 0;
        }
    }

    return 0;
}

/*
 Frame, which is got from deocoder, has aligned 16 byte width and height.
so frame buf has same size  and only alloc mem one time is ok, but we
need to calc crop.x/crop.y/crop.width/crop.height

--------------               ------------------
|          | |               | |              |
|          | |  rot 90       | |              |
|          | | ========>     | |              |
|          | |               | |--------------|
|----------| |               ------------------
--------------

--------------                 --------------
|          | |                 | -----------|
|          | |    rot 180      | |          |
|          | |   ======>       | |          |
|          | |                 | |          |
|----------| |                 | |          |
--------------                 --------------

--------------
|          | |               ---------------------
|          | |   rot 270     |-----------------| |
|          | |   =======>    |                 | |
|          | |               |                 | |
|----------| |               |                 | |
--------------               |-----------------| |
*/
static int mm_video_render_set_rotation_frame_info(
    mm_video_render_data *p_video_render_data, struct mpp_frame *p_frame)
{
    int i = 0;
    int cnt;
	int buf_size;

	int dma_fd = p_video_render_data->dma_fd;
	if (dma_fd < 0) {
		return -1;
	}

    cnt = sizeof(p_video_render_data->rotation_frames) /
          sizeof(p_video_render_data->rotation_frames[0]);

    for (i = 0; i < cnt; i++) {
        p_video_render_data->rotation_frames[i].id = p_video_render_data->rotation_index++;
        p_video_render_data->rotation_frames[i].buf.buf_type = p_frame->buf.buf_type;
        p_video_render_data->rotation_frames[i].buf.format = p_frame->buf.format;
        if (p_video_render_data->rotation_angle == MPP_ROTATION_90) {
            p_video_render_data->rotation_frames[i].buf.size.width = p_frame->buf.size.height;
            p_video_render_data->rotation_frames[i].buf.size.height = p_frame->buf.size.width;
            p_video_render_data->rotation_frames[i].buf.crop_en = 1;
            p_video_render_data->rotation_frames[i].buf.crop.x = p_frame->buf.size.height - p_frame->buf.crop.height;
            p_video_render_data->rotation_frames[i].buf.crop.y = 0;
            p_video_render_data->rotation_frames[i].buf.crop.width = p_frame->buf.crop.height;
            p_video_render_data->rotation_frames[i].buf.crop.height = p_frame->buf.crop.width;
        } else if (p_video_render_data->rotation_angle == MPP_ROTATION_180) {
            p_video_render_data->rotation_frames[i].buf.size.width = p_frame->buf.size.width;
            p_video_render_data->rotation_frames[i].buf.size.height = p_frame->buf.size.height;
            p_video_render_data->rotation_frames[i].buf.crop_en = 1;
            p_video_render_data->rotation_frames[i].buf.crop.x = p_frame->buf.size.width - p_frame->buf.crop.width;
            p_video_render_data->rotation_frames[i].buf.crop.y = p_frame->buf.size.height - p_frame->buf.crop.height;
            p_video_render_data->rotation_frames[i].buf.crop.width = p_frame->buf.crop.width;
            p_video_render_data->rotation_frames[i].buf.crop.height = p_frame->buf.crop.height;
        } else if (p_video_render_data->rotation_angle == MPP_ROTATION_270) {
            p_video_render_data->rotation_frames[i].buf.size.width = p_frame->buf.size.height;
            p_video_render_data->rotation_frames[i].buf.size.height = p_frame->buf.size.width;
            p_video_render_data->rotation_frames[i].buf.crop_en = 1;
            p_video_render_data->rotation_frames[i].buf.crop.x = 0;
            p_video_render_data->rotation_frames[i].buf.crop.y = p_frame->buf.size.width - p_frame->buf.crop.width;
            p_video_render_data->rotation_frames[i].buf.crop.width = p_frame->buf.crop.height;
            p_video_render_data->rotation_frames[i].buf.crop.height = p_frame->buf.crop.width;
        } else { // MPP_ROTATION_0
            return 0;
        }
    }

    buf_size = p_video_render_data->rotation_frames[0].buf.size.width *
               p_video_render_data->rotation_frames[0].buf.size.height;

    for (i = 0; i < cnt; i++) {
        switch (p_frame->buf.format) {
            case MPP_FMT_YUV420P:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    (p_video_render_data->rotation_frames[i].buf.size.width + 15) / 16 * 16;
                p_video_render_data->rotation_frames[i].buf.stride[1] =
                    p_video_render_data->rotation_frames[i].buf.stride[0] >> 1;
                p_video_render_data->rotation_frames[i].buf.stride[2] =
                    p_video_render_data->rotation_frames[i].buf.stride[0] >> 1;

                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[2] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
                    p_video_render_data->rotation_frames[i].buf.fd[1] = dmabuf_alloc(dma_fd,(buf_size >> 2));
                    p_video_render_data->rotation_frames[i].buf.fd[2] = dmabuf_alloc(dma_fd,(buf_size >> 2));
			    }

                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 ||
				    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0 ||
				    p_video_render_data->rotation_frames[i].buf.fd[2] <= 0) {
				    loge("%d-%d-%d\n",p_video_render_data->rotation_frames[i].buf.fd[0],
					    p_video_render_data->rotation_frames[i].buf.fd[1],
					    p_video_render_data->rotation_frames[i].buf.fd[2]);
				    return -1;
			    }
                break;
            case MPP_FMT_YUV444P:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    (p_video_render_data->rotation_frames[i].buf.size.width + 15) / 16 * 16;
                p_video_render_data->rotation_frames[i].buf.stride[1] =
                    p_video_render_data->rotation_frames[i].buf.stride[0];
                p_video_render_data->rotation_frames[i].buf.stride[2] =
                    p_video_render_data->rotation_frames[i].buf.stride[0];
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[2] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
                    p_video_render_data->rotation_frames[i].buf.fd[1] = dmabuf_alloc(dma_fd,buf_size);
                    p_video_render_data->rotation_frames[i].buf.fd[2] = dmabuf_alloc(dma_fd,buf_size);
                }
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 ||
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0 ||
                    p_video_render_data->rotation_frames[i].buf.fd[2] <= 0) {
                    loge("%d-%d-%d\n",p_video_render_data->rotation_frames[i].buf.fd[0],
                        p_video_render_data->rotation_frames[i].buf.fd[1],
                        p_video_render_data->rotation_frames[i].buf.fd[2]);
                    return -1;
                }
                break;
            case MPP_FMT_YUV422P:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    (p_video_render_data->rotation_frames[i].buf.size.width + 15) / 16 * 16;
                p_video_render_data->rotation_frames[i].buf.stride[1] =
                    p_video_render_data->rotation_frames[i].buf.stride[0] >> 1;
                p_video_render_data->rotation_frames[i].buf.stride[2] =
                    p_video_render_data->rotation_frames[i].buf.stride[0] >> 1;
                buf_size = p_video_render_data->rotation_frames[i].buf.stride[0] *
                    p_video_render_data->rotation_frames[i].buf.size.height;
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[2] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
                    p_video_render_data->rotation_frames[i].buf.fd[1] = dmabuf_alloc(dma_fd,buf_size >> 1);
                    p_video_render_data->rotation_frames[i].buf.fd[2] = dmabuf_alloc(dma_fd,buf_size >> 1);
                }
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 ||
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0 ||
                    p_video_render_data->rotation_frames[i].buf.fd[2] <= 0) {
                    loge("%d-%d-%d\n",p_video_render_data->rotation_frames[i].buf.fd[0],
                        p_video_render_data->rotation_frames[i].buf.fd[1],
                        p_video_render_data->rotation_frames[i].buf.fd[2]);
                    return -1;
                }
                break;
            case MPP_FMT_NV12:
            case MPP_FMT_NV21:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    (p_video_render_data->rotation_frames[i].buf.size.width +
                     15) /
                    16 * 16;
                p_video_render_data->rotation_frames[i].buf.stride[1] =
                    p_video_render_data->rotation_frames[i].buf.stride[0] >> 1;
                p_video_render_data->rotation_frames[i].buf.stride[2] = 0;

                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 &&
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
                    p_video_render_data->rotation_frames[i].buf.fd[1] = dmabuf_alloc(dma_fd,buf_size >> 1);
                    p_video_render_data->rotation_frames[i].buf.fd[2] = 0;
                }
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0 ||
                    p_video_render_data->rotation_frames[i].buf.fd[1] <= 0) {
                    loge("%d-%d-%d\n",p_video_render_data->rotation_frames[i].buf.fd[0],
                        p_video_render_data->rotation_frames[i].buf.fd[1],
                        p_video_render_data->rotation_frames[i].buf.fd[2]);
                    return -1;
                }
                break;
            case MPP_FMT_YUV400:
            case MPP_FMT_ABGR_8888:
            case MPP_FMT_ARGB_8888:
            case MPP_FMT_RGBA_8888:
            case MPP_FMT_BGRA_8888:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    (p_video_render_data->rotation_frames[i].buf.size.width +
                     15) /
                    16 * 16;
                p_video_render_data->rotation_frames[i].buf.stride[1] = 0;
                p_video_render_data->rotation_frames[i].buf.stride[2] = 0;
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size*4);
                }
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0) {
                    loge("%d\n",p_video_render_data->rotation_frames[i].buf.fd[0]);
                    return -1;
                }
                break;

            case MPP_FMT_BGR_888:
            case MPP_FMT_RGB_888:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    p_video_render_data->rotation_frames[i].buf.size.width;
                p_video_render_data->rotation_frames[i].buf.stride[1] = 0;
                p_video_render_data->rotation_frames[i].buf.stride[2] = 0;
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size*3);
                }
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0) {
                    loge("%d\n",p_video_render_data->rotation_frames[i].buf.fd[0]);
                    return -1;
                }
                break;
            case MPP_FMT_BGR_565:
            case MPP_FMT_RGB_565:
                p_video_render_data->rotation_frames[i].buf.stride[0] =
                    p_video_render_data->rotation_frames[i].buf.size.width;
                p_video_render_data->rotation_frames[i].buf.stride[1] = 0;
                p_video_render_data->rotation_frames[i].buf.stride[2] = 0;
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0) {
                    p_video_render_data->rotation_frames[i].buf.fd[0] = dmabuf_alloc(dma_fd,buf_size*2);
                }
                if (p_video_render_data->rotation_frames[i].buf.fd[0] <= 0) {
                    loge("%d\n",p_video_render_data->rotation_frames[i].buf.fd[0]);
                    return -1;
                }
                break;

            default:
                loge("unsup_port format");
                return -1;
        }
    }
    return 0;
}


static int
mm_video_render_init_rotation_param(mm_video_render_data *p_video_render_data,
                                    struct mpp_frame *p_frame)
{
    int i = 0;
    int cnt = 0;

    p_video_render_data->init_rotation_param = 0;
    if (p_video_render_data->rotation_angle == MPP_ROTATION_0) {
        p_video_render_data->init_rotation_param = 1;
        return 0;
    }

    //open GE
    if (p_video_render_data->ge_handle == NULL) {
        p_video_render_data->ge_handle = mpp_ge_open();
        if (p_video_render_data->ge_handle == NULL) {
            loge("open ge device error\n");
            p_video_render_data->init_rotation_param = -1;
            return -1;
        }
    }

    // open DMA  device
    if (p_video_render_data->dma_fd <= 0) {
        p_video_render_data->dma_fd = dmabuf_device_open();
        if (p_video_render_data->dma_fd < 0) {
            loge("dmabuf_device_open error\n");
            goto _exit;
        }
    }

    //set rotation frame info
    if (mm_video_render_set_rotation_frame_info(p_video_render_data, p_frame) != 0) {
        loge("mm_video_render_set_rotation_frame_info\n");
        p_video_render_data->init_rotation_param = -1;
        goto _exit;
    }

    p_video_render_data->init_rotation_param = 1;
    return 0;

_exit:
    cnt = sizeof(p_video_render_data->rotation_frames) / sizeof(p_video_render_data->rotation_frames[0]);
    for (i = 0; i < cnt; i++) {
        mm_video_render_free_frame_buffer(
            p_video_render_data, &p_video_render_data->rotation_frames[i]);
    }

    if (p_video_render_data->dma_fd > 0) {
        dmabuf_device_close(p_video_render_data->dma_fd);
        p_video_render_data->dma_fd = -1;
    }

    if (p_video_render_data->ge_handle) {
        mpp_ge_close(p_video_render_data->ge_handle);
        p_video_render_data->ge_handle = NULL;
    }
    p_video_render_data->init_rotation_param = -1;
    return -1;
}

static int
mm_video_render_deinit_rotation_param(mm_video_render_data *p_video_render_data)
{
    int i = 0;
    int cnt = 0;

    // init fail ,so do not need to deinit
    if (p_video_render_data->init_rotation_param != 1) {
        return 0;
    }
    cnt = sizeof(p_video_render_data->rotation_frames) /
          sizeof(p_video_render_data->rotation_frames[0]);

    for (i = 0; i < cnt; i++) {
        mm_video_render_free_frame_buffer(
            p_video_render_data, &p_video_render_data->rotation_frames[i]);
    }

    if (p_video_render_data->dma_fd > 0) {
        dmabuf_device_close(p_video_render_data->dma_fd);
        p_video_render_data->dma_fd = -1;
    }

    if (!mpp_list_empty(&p_video_render_data->dma_list) && p_video_render_data->ge_handle) {
        mm_video_render_dma_fd *dma_fd_node = NULL, *dma_fd_node1 = NULL;
        mpp_list_for_each_entry_safe(dma_fd_node, dma_fd_node1,
                                     &p_video_render_data->dma_list, list)
        {
            for (i = 0; i < 3; i++) {
                if (dma_fd_node->fd[i] > 0) {
                    mpp_ge_rm_dmabuf(p_video_render_data->ge_handle, dma_fd_node->fd[i]);
                    printf("[%s:%d]:%d\n", __FUNCTION__, __LINE__, dma_fd_node->fd[i]);
                    dma_fd_node->fd[i] = -1;
                }
            }
            mpp_list_del(&dma_fd_node->list);
            mpp_free(dma_fd_node);
        }
    }

    if (p_video_render_data->ge_handle) {
        mpp_ge_close(p_video_render_data->ge_handle);
        p_video_render_data->ge_handle = NULL;
    }

    return 0;
}

static int
mm_video_render_rotate_frame(mm_video_render_data *p_video_render_data,
                             struct mpp_frame *p_frame)
{
    int ret = 0;
    struct ge_bitblt blt;
    struct mpp_ge *ge = NULL;
    int find = 0;
    int comp = 0;
    int i = 0;

    if (p_video_render_data == NULL || p_frame == NULL) {
        loge("param error !!!\n");
        return -1;
    }

    if (p_video_render_data->rotation_angle == MPP_ROTATION_0) {
        p_video_render_data->p_cur_display_frame = p_frame;
        return 0;
    }

    if (p_video_render_data->init_rotation_param != 1) {
        p_video_render_data->p_cur_display_frame = p_frame;
        loge("RotationParam do not init ok !!!\n");
        return -1;
    }

    memset(&blt, 0x00, sizeof(struct ge_bitblt));
    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    blt.src_buf.format = p_frame->buf.format;
    blt.src_buf.fd[0] = p_frame->buf.fd[0];
    blt.src_buf.fd[1] = p_frame->buf.fd[1];
    blt.src_buf.fd[2] = p_frame->buf.fd[2];
    blt.src_buf.stride[0] = p_frame->buf.stride[0];
    blt.src_buf.stride[1] = p_frame->buf.stride[1];
    blt.src_buf.stride[2] = p_frame->buf.stride[2];
    blt.src_buf.size.width = p_frame->buf.size.width;
    blt.src_buf.size.height = p_frame->buf.size.height;

    if (p_video_render_data->p_cur_display_frame ==
        &p_video_render_data->rotation_frames[0]) {
        p_video_render_data->p_cur_display_frame =
            &p_video_render_data->rotation_frames[1];
    } else {
        p_video_render_data->p_cur_display_frame =
            &p_video_render_data->rotation_frames[0];
    }

    blt.dst_buf.buf_type =
        p_video_render_data->p_cur_display_frame->buf.buf_type;
    blt.dst_buf.format = p_video_render_data->p_cur_display_frame->buf.format;
    blt.dst_buf.fd[0] =
        p_video_render_data->p_cur_display_frame->buf.fd[0];
    blt.dst_buf.fd[1] =
        p_video_render_data->p_cur_display_frame->buf.fd[1];
    blt.dst_buf.fd[2] =
        p_video_render_data->p_cur_display_frame->buf.fd[2];
    blt.dst_buf.stride[0] =
        p_video_render_data->p_cur_display_frame->buf.stride[0];
    blt.dst_buf.stride[1] =
        p_video_render_data->p_cur_display_frame->buf.stride[1];
    blt.dst_buf.stride[2] =
        p_video_render_data->p_cur_display_frame->buf.stride[2];
    blt.dst_buf.size.width =
        p_video_render_data->p_cur_display_frame->buf.size.width;
    blt.dst_buf.size.height =
        p_video_render_data->p_cur_display_frame->buf.size.height;
    blt.dst_buf.crop_en = 0;
    blt.ctrl.flags = p_video_render_data->rotation_angle;

    p_video_render_data->p_cur_display_frame->pts = p_frame->pts;
    p_video_render_data->p_cur_display_frame->flags = p_frame->flags;

    ge = p_video_render_data->ge_handle;

    comp = mm_video_render_get_component_num(p_frame->buf.format);
    find = mm_video_render_is_exist_dma_fd(p_video_render_data, p_frame);
    if (!find) {
        for (i = 0; i < comp; i++) {
            mpp_ge_add_dmabuf(ge, p_frame->buf.fd[i]);
        }
        mm_video_render_add_dma_fd(p_video_render_data, p_frame);
    }

    comp = mm_video_render_get_component_num(
        p_video_render_data->p_cur_display_frame->buf.format);
    find = mm_video_render_is_exist_dma_fd(p_video_render_data,
                                           p_video_render_data->p_cur_display_frame);
    if (!find) {
        for (i = 0; i < comp; i++) {
            mpp_ge_add_dmabuf(ge, p_video_render_data->p_cur_display_frame->buf.fd[i]);
            printf("[%s:%d]:%d\n", __FUNCTION__, __LINE__,
                   p_video_render_data->p_cur_display_frame->buf.fd[i]);
        }
        mm_video_render_add_dma_fd(p_video_render_data,
                                   p_video_render_data->p_cur_display_frame);
    }

    ret = mpp_ge_bitblt(ge, &blt);
    if (ret) {
        loge("mpp_ge_bitblt task failed: %d\n", ret);
        ret = -1;
        goto _exit0;
    }
    ret = mpp_ge_emit(ge);
    if (ret) {
        loge("mpp_ge_emit task failed: %d\n", ret);
        ret = -1;
        goto _exit0;
    }
    ret = mpp_ge_sync(ge);
    if (ret) {
        loge("mpp_ge_sync task failed: %d\n", ret);
        ret = -1;
        goto _exit0;
    }

_exit0:
    return ret;
}

static s32 mm_video_render_get_frame(mm_handle h_vdec_comp,
                                     struct mpp_frame *p_frame)
{
    s32 ret = DEC_OK;
    struct mpp_decoder *p_decoder = NULL;
    mm_get_parameter(h_vdec_comp, MM_INDEX_PARAM_VIDEO_DECODER_HANDLE,
                     (void *)&p_decoder);
    if (p_decoder == NULL) {
        return DEC_ERR_NULL_PTR;
    }

    ret = mpp_decoder_get_frame(p_decoder, p_frame);
    if (ret != DEC_OK) {
        return ret;
    }
    return ret;
}

static void mm_video_render_wait_frame_timeout(mm_video_render_data *p_video_render_data, int retval)
{
    struct timespec before = { 0 }, after = { 0 };
    mm_bind_info *p_bind_vdec;

    if (p_video_render_data->frame_end_flag) {
        printf("[%s:%d]:receive video frame end flag\n", __FUNCTION__, __LINE__);
        p_video_render_data->flags |= VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
        return;
    }
    /*avoid dec thread caching too many packet then entering sleep and timeout exit*/
    p_bind_vdec = &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];
    mm_send_command(p_bind_vdec->p_bind_comp, MM_COMMAND_NOPS, 0, NULL);

    clock_gettime(CLOCK_REALTIME, &before);

    pthread_mutex_lock(&p_video_render_data->in_frame_lock);
    p_video_render_data->wait_ready_frame_flag = MM_TRUE;
    pthread_mutex_unlock(&p_video_render_data->in_frame_lock);

    /*if no empty frame then goto sleep, wait wkup by vdec*/
    aic_msg_wait_new_msg(&p_video_render_data->s_msg,
                         VIDEO_RENDER_WAIT_FRAME_INTERVAL);
    clock_gettime(CLOCK_REALTIME, &after);
    long diff = (after.tv_sec - before.tv_sec) * 1000 * 1000 +
                (after.tv_nsec - before.tv_nsec) / 1000;

    /*if the get frame diff time overange max wait time, then indicate fame end*/
    if (diff > VIDEO_RENDER_WAIT_FRAME_MAX_TIME) {
        printf("[%s:%d]:%ld, Get frame timeout reason %d\n", __FUNCTION__, __LINE__, diff, retval);
        p_video_render_data->flags |= VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
    }
}


static s32 mm_video_render_put_frame(mm_handle h_vdec_comp,
                                     struct mpp_frame *p_frame)
{
    s32 ret = DEC_OK;
    struct mpp_decoder *p_decoder = NULL;
    mm_get_parameter(h_vdec_comp, MM_INDEX_PARAM_VIDEO_DECODER_HANDLE,
                     (void *)&p_decoder);
    if (p_decoder == NULL) {
        return DEC_ERR_NULL_PTR;
    }

    ret = mpp_decoder_put_frame(p_decoder, p_frame);
    if (ret != DEC_OK) {
        return ret;
    }
    return ret;
}

static int
mm_video_render_giveback_all_frames(mm_video_render_data *p_video_render_data)
{
    int ret = MM_ERROR_NONE;

    mm_bind_info *p_bind_vdec =
        &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];

    if (p_video_render_data->disp_frame_num > 0) {
        ret = mm_video_render_put_frame(
            p_bind_vdec->p_bind_comp,
            &p_video_render_data
                 ->disp_frames[p_video_render_data->last_disp_frame_id]);
        if (DEC_OK == ret) {
            p_video_render_data->giveback_frame_ok_num++;
        } else {
            p_video_render_data->giveback_frame_fail_num++;
        }
        p_video_render_data->disp_frame_num = 0;
    }

    return ret;
}

static s32 mm_video_render_send_command(mm_handle h_component,
                                        MM_COMMAND_TYPE cmd, u32 param1,
                                        void *p_cmd_data)
{
    mm_video_render_data *p_video_render_data;
    s32 error = MM_ERROR_NONE;
    struct aic_message s_msg;
    p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);
    s_msg.message_id = cmd;
    s_msg.param = param1;
    s_msg.data_size = 0;

    //now not use always NULL
    if (p_cmd_data != NULL) {
        s_msg.data = p_cmd_data;
        s_msg.data_size = strlen((char *)p_cmd_data);
    }
    if (MM_COMMAND_WKUP == (s32)cmd) {
        pthread_mutex_lock(&p_video_render_data->in_frame_lock);
        if (p_video_render_data->wait_ready_frame_flag == MM_TRUE) {
            aic_msg_put(&p_video_render_data->s_msg, &s_msg);
            p_video_render_data->wait_ready_frame_flag = MM_FALSE;
        }
        pthread_mutex_unlock(&p_video_render_data->in_frame_lock);
    } else {
        if (MM_COMMAND_EOS == (s32)cmd) {
            p_video_render_data->frame_end_flag = MM_TRUE;
        }
        aic_msg_put(&p_video_render_data->s_msg, &s_msg);
    }

    return error;
}

static s32 mm_video_render_get_parameter(mm_handle h_component,
                                         MM_INDEX_TYPE index, void *p_param)
{
    mm_video_render_data *p_video_render_data;
    s32 error = MM_ERROR_NONE;
    p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_CONFIG_COMMON_OUTPUT_CROP:
            ((mm_config_rect *)p_param)->left = p_video_render_data->dis_rect.x;
            ((mm_config_rect *)p_param)->top = p_video_render_data->dis_rect.y;
            ((mm_config_rect *)p_param)->width =
                p_video_render_data->dis_rect.width;
            ((mm_config_rect *)p_param)->height =
                p_video_render_data->dis_rect.height;
            break;
        case MM_INDEX_VENDOR_VIDEO_RENDER_SCREEN_SIZE: {
            struct mpp_size size;
            if (p_video_render_data->video_render_init_flag != 1) {
                loge("p_video_render_data->render not init!!!\n");
                error = MM_ERROR_UNDEFINED;
                break;
            }
            p_video_render_data->render->get_screen_size(
                p_video_render_data->render, &size);
            ((mm_param_screen_size *)p_param)->width = size.width;
            ((mm_param_screen_size *)p_param)->height = size.height;
            break;
        }
        default:
            error = MM_ERROR_UNSUPPORT;
            break;
    }

    return error;
}

static s32 mm_video_render_set_parameter(mm_handle h_component,
                                         MM_INDEX_TYPE index, void *p_param)
{
    mm_video_render_data *p_video_render_data;
    s32 error = MM_ERROR_NONE;
    p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);
    if (p_param == NULL) {
        loge("param error!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    switch (index) {
        case MM_INDEX_CONFIG_COMMON_OUTPUT_CROP:
            p_video_render_data->dis_rect.x = ((mm_config_rect *)p_param)->left;
            p_video_render_data->dis_rect.y = ((mm_config_rect *)p_param)->top;
            p_video_render_data->dis_rect.width =
                ((mm_config_rect *)p_param)->width;
            p_video_render_data->dis_rect.height =
                ((mm_config_rect *)p_param)->height;
            p_video_render_data->dis_rect_change = MM_TRUE;
            break;

        default:
            break;
    }
    return error;
}

static s32 mm_video_render_get_config(mm_handle h_component,
                                      MM_INDEX_TYPE index, void *p_config)
{
    s32 error = MM_ERROR_NONE;
    mm_video_render_data *p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_CONFIG_COMMON_ROTATE: {
            mm_config_rotation *p_rotation = (mm_config_rotation *)p_config;
            p_rotation->rotation = p_video_render_data->rotation_angle;
            break;
        }
        default:
            break;
    }
    return error;
}

static s32 mm_video_render_set_config(mm_handle h_component,
                                      MM_INDEX_TYPE index, void *p_config)
{
    int ret = MM_ERROR_NONE;
    s32 error = MM_ERROR_NONE;
    mm_video_render_data *p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_CONFIG_TIME_POSITION:
            //1 clear input buffer list
            mm_video_render_giveback_all_frames(p_video_render_data);
            //2 reset flag
            p_video_render_data->frame_first_show_flag = MM_TRUE;
            p_video_render_data->flags = 0;
            p_video_render_data->clock_state =
                MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME;
            if ((p_video_render_data->dis_rect.width != 0) &&
                (p_video_render_data->dis_rect.width != 0)) {
                p_video_render_data->dis_rect_change = MM_TRUE;
            }
            break;

        case MM_INDEX_CONFIG_TIME_CLOCK_STATE: {
            mm_time_config_clock_state *p_state =
                (mm_time_config_clock_state *)p_config;
            p_video_render_data->clock_state = p_state->state;
            printf("[%s:%d]p_video_render_data->clock_state:%d\n", __FUNCTION__,
                   __LINE__, p_video_render_data->clock_state);
            break;
        }

        case MM_INDEX_VENDOR_VIDEO_RENDER_INIT:
            if (!p_video_render_data->render) {
                ret = aic_video_render_create(&p_video_render_data->render);
                if (ret) {
                    error = MM_ERROR_INSUFFICIENT_RESOURCES;
                    loge("aic_video_render_create error!!!!\n");
                    break;
                }
                ret = p_video_render_data->render->init(
                    p_video_render_data->render, p_video_render_data->layer_id,
                    p_video_render_data->dev_id);
                if (!ret) {
                    printf("[%s:%d]p_video_render_data->render->init ok\n",
                           __FUNCTION__, __LINE__);
                    p_video_render_data->video_render_init_flag = 1;
                } else {
                    loge("p_video_render_data->render->init fail\n");
                    error = MM_ERROR_INSUFFICIENT_RESOURCES;
                }
            } else {
                loge("p_video_render_data->render has been created!!1!\n");
            }
            break;

        case MM_INDEX_VENDOR_VIDEO_RENDER_CAPTURE:
            mm_video_render_capture(p_video_render_data, p_config);
            break;

        case MM_INDEX_CONFIG_COMMON_ROTATE: {
            mm_config_rotation *p_rotation = (mm_config_rotation *)p_config;
            if (p_video_render_data->rotation_angle !=
                p_rotation
                    ->rotation) { //MPP_ROTATION_0 MPP_ROTATION_90 MPP_ROTATION_180 MPP_ROTATION_270
                p_video_render_data->rotation_angle = p_rotation->rotation;
                p_video_render_data->rotation_angle_change = 1;
            }
            break;
        }

        default:
            break;
    }
    return error;
}

static s32 mm_video_render_get_state(mm_handle h_component,
                                     MM_STATE_TYPE *p_state)
{
    mm_video_render_data *p_video_render_data;
    s32 error = MM_ERROR_NONE;
    p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);

    pthread_mutex_lock(&p_video_render_data->state_lock);
    *p_state = p_video_render_data->state;
    pthread_mutex_unlock(&p_video_render_data->state_lock);

    return error;
}

static s32 mm_video_render_bind_request(mm_handle h_comp, u32 port,
                                        mm_handle h_bind_comp, u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;
    mm_video_render_data *p_video_render_data;
    p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_video_render_data->state != MM_STATE_LOADED) {
        loge(
            "Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
            p_video_render_data->state);
        return MM_ERROR_INVALID_STATE;
    }
    if (port == VIDEO_RENDER_PORT_IN_VIDEO_INDEX) {
        p_port =
            &p_video_render_data->in_port_def[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];
        p_bind_info =
            &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];
    } else if (port == VIDEO_RENDER_PORT_IN_CLOCK_INDEX) {
        p_port =
            &p_video_render_data->in_port_def[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
        p_bind_info =
            &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
    } else {
        loge("component can not find port :%u,"
             "VIDEO_RENDER_PORT_IN_VIDEO_INDEX:%d\n",
             port, VIDEO_RENDER_PORT_IN_VIDEO_INDEX);
        return MM_ERROR_BAD_PARAMETER;
    }

    if (NULL == h_bind_comp && 0 == bind_port) {
        p_bind_info->flag = MM_FALSE;
        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        return MM_ERROR_NONE;
    }

    if (p_port->dir == MM_DIR_OUTPUT) {
        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        p_bind_info->flag = MM_TRUE;
    } else if (p_port->dir == MM_DIR_INPUT) {
        mm_param_port_def bind_port_param;
        bind_port_param.port_index = bind_port;
        mm_get_parameter(h_bind_comp, MM_INDEX_PARAM_PORT_DEFINITION,
                         &bind_port_param);

        if (bind_port_param.dir != MM_DIR_OUTPUT) {
            loge("both ports are input.\n");
            return MM_ERROR_PORT_NOT_COMPATIBLE;
        }

        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        p_bind_info->flag = MM_TRUE;
    } else {
        loge("port is neither output nor input.\n");
        return MM_ERROR_PORT_NOT_COMPATIBLE;
    }
    return error;
}

static s32 mm_video_render_set_callback(mm_handle h_component,
                                        mm_callback *p_cb, void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_video_render_data *p_video_render_data;
    p_video_render_data =
        (mm_video_render_data *)(((mm_component *)h_component)->p_comp_private);
    p_video_render_data->p_callback = p_cb;
    p_video_render_data->p_app_data = p_app_data;
    return error;
}

s32 mm_video_render_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_video_render_data *p_video_render_data;

    p_comp = (mm_component *)h_component;
    struct aic_message s_msg;
    p_video_render_data = (mm_video_render_data *)p_comp->p_comp_private;

    pthread_mutex_lock(&p_video_render_data->state_lock);
    if (p_video_render_data->state != MM_STATE_LOADED) {
        logw(
            "compoent is in %d,but not in MM_STATE_LOADED(1),can ont FreeHandle.\n",
            p_video_render_data->state);
        pthread_mutex_unlock(&p_video_render_data->state_lock);
        return MM_ERROR_UNSUPPORT;
    }
    pthread_mutex_unlock(&p_video_render_data->state_lock);

    s_msg.message_id = MM_COMMAND_STOP;
    s_msg.data_size = 0;
    aic_msg_put(&p_video_render_data->s_msg, &s_msg);
    pthread_join(p_video_render_data->thread_id, (void *)&error);

    pthread_mutex_destroy(&p_video_render_data->in_frame_lock);
    pthread_mutex_destroy(&p_video_render_data->state_lock);

    aic_msg_destroy(&p_video_render_data->s_msg);

    mm_video_render_deinit_rotation_param(p_video_render_data);

    if (p_video_render_data->render) {
        p_video_render_data->render->set_on_off(p_video_render_data->render, 0);
        aic_video_render_destroy(p_video_render_data->render);
        p_video_render_data->render = NULL;
    }

    mpp_free(p_video_render_data);
    p_video_render_data = NULL;

    logd("mm_video_render_component_deinit\n");
    return error;
}


s32 mm_video_render_component_init(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    u32 err = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_video_render_data *p_video_render_data;
    s32 port_index = 0;
    s8 msg_create = 0;
    s8 state_lock_init = 0;
    s8 in_frame_lock_init = 0;

    logw("mm_video_render_component_init....");

    p_comp = (mm_component *)h_component;

    p_video_render_data =
        (mm_video_render_data *)mpp_alloc(sizeof(mm_video_render_data));

    if (NULL == p_video_render_data) {
        loge("mpp_alloc(sizeof(mm_video_render_data) fail!");
        return MM_ERROR_INSUFFICIENT_RESOURCES;
    }

    memset(p_video_render_data, 0x0, sizeof(mm_video_render_data));
    p_comp->p_comp_private = (void *)p_video_render_data;
    p_video_render_data->frame_first_show_flag = MM_TRUE;
    p_video_render_data->state = MM_STATE_LOADED;
    p_video_render_data->h_self = p_comp;

    p_comp->set_callback = mm_video_render_set_callback;
    p_comp->send_command = mm_video_render_send_command;
    p_comp->get_state = mm_video_render_get_state;
    p_comp->get_parameter = mm_video_render_get_parameter;
    p_comp->set_parameter = mm_video_render_set_parameter;
    p_comp->get_config = mm_video_render_get_config;
    p_comp->set_config = mm_video_render_set_config;
    p_comp->bind_request = mm_video_render_bind_request;
    p_comp->deinit = mm_video_render_component_deinit;

    port_index = VIDEO_RENDER_PORT_IN_VIDEO_INDEX;
    p_video_render_data->in_port_def[port_index].port_index = port_index;
    p_video_render_data->in_port_def[port_index].enable = MM_TRUE;
    p_video_render_data->in_port_def[port_index].dir = MM_DIR_INPUT;
    port_index = VIDEO_RENDER_PORT_IN_CLOCK_INDEX;
    p_video_render_data->in_port_def[port_index].port_index = port_index;
    p_video_render_data->in_port_def[port_index].enable = MM_TRUE;
    p_video_render_data->in_port_def[port_index].dir = MM_DIR_INPUT;

    if (pthread_mutex_init(&p_video_render_data->in_frame_lock, NULL)) {
        loge("pthread_mutex_init in frame lockfail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    in_frame_lock_init = 1;

    if (aic_msg_create(&p_video_render_data->s_msg) < 0) {
        loge("aic_msg_create fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    msg_create = 1;

    p_video_render_data->clock_state = MM_TIME_CLOCK_STATE_STOPPED;
    p_video_render_data->rotation_angle = MPP_ROTATION_0;
    p_video_render_data->rotation_angle_change = 0;
    p_video_render_data->init_rotation_param = 0;
    p_video_render_data->cur_disp_frame_id = 0;
    p_video_render_data->last_disp_frame_id = 1;
    p_video_render_data->disp_frame_num = 0;
    p_video_render_data->rotation_index = 1;
    p_video_render_data->frame_end_flag = MM_FALSE;

    mpp_list_init(&p_video_render_data->dma_list);

    if (pthread_mutex_init(&p_video_render_data->state_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    state_lock_init = 1;

    // Create the component thread
    err = pthread_create(&p_video_render_data->thread_id, NULL,
                         mm_video_render_component_thread, p_video_render_data);
    if (err) {
        loge("pthread_create fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }

    return error;

_EXIT:

    if (in_frame_lock_init) {
        pthread_mutex_destroy(&p_video_render_data->in_frame_lock);
    }

    if (state_lock_init) {
        pthread_mutex_destroy(&p_video_render_data->state_lock);
    }

    if (msg_create) {
        aic_msg_destroy(&p_video_render_data->s_msg);
    }

    if (p_video_render_data) {
        mpp_free(p_video_render_data);
        p_video_render_data = NULL;
    }

    return error;
}

static s64 mm_video_render_clock_get_sys_time()
{
    struct timespec ts = { 0, 0 };
    s64 tick = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    tick = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return tick;
}

static void
mm_video_render_event_notify(mm_video_render_data *p_video_render_data,
                             MM_EVENT_TYPE event, u32 data1, u32 data2,
                             void *p_event_data)
{
    if (p_video_render_data && p_video_render_data->p_callback &&
        p_video_render_data->p_callback->event_handler) {
        p_video_render_data->p_callback->event_handler(
            p_video_render_data->h_self, p_video_render_data->p_app_data, event,
            data1, data2, p_event_data);
    }
}

static void mm_video_render_state_change_to_invalid(
    mm_video_render_data *p_video_render_data)
{
    p_video_render_data->state = MM_STATE_INVALID;
    mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                 MM_ERROR_INVALID_STATE, 0, NULL);
    mm_video_render_event_notify(p_video_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_video_render_data->state, NULL);
}

static void mm_video_render_state_change_to_loaded(
    mm_video_render_data *p_video_render_data)
{
    if (p_video_render_data->state == MM_STATE_IDLE) {
        mm_video_render_giveback_all_frames(p_video_render_data);
    } else if (p_video_render_data->state == MM_STATE_EXECUTING) {
    } else if (p_video_render_data->state == MM_STATE_PAUSE) {
    } else {
        mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_video_render_data->state, NULL);
        return;
    }
    p_video_render_data->state = MM_STATE_LOADED;
    mm_video_render_event_notify(p_video_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_video_render_data->state, NULL);
}

static void
mm_video_render_state_change_to_idle(mm_video_render_data *p_video_render_data)
{
    int ret = 0;
    if (p_video_render_data->state == MM_STATE_LOADED) {
        //create video_handle
        if (!p_video_render_data->render) {
            ret = aic_video_render_create(&p_video_render_data->render);
        }
        if (ret != 0) {
            loge("aic_video_render_create fail\n");
            mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                         MM_ERROR_INCORRECT_STATE_TRANSITION,
                                         p_video_render_data->state, NULL);
            return;
        }
    } else if (p_video_render_data->state == MM_STATE_PAUSE) {
    } else if (p_video_render_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_video_render_data->state, NULL);

        return;
    }

    p_video_render_data->state = MM_STATE_IDLE;
    mm_video_render_event_notify(p_video_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_video_render_data->state, NULL);
}

static void
mm_video_render_state_change_to_pause(mm_video_render_data *p_video_render_data)
{
    mm_bind_info *p_bind_clock =
        &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];

    if (p_video_render_data->state == MM_STATE_LOADED) {
    } else if (p_video_render_data->state == MM_STATE_IDLE) {
    } else if (p_video_render_data->state == MM_STATE_EXECUTING) {
        mm_time_config_timestamp timestamp;
        if (p_bind_clock->flag) {
            mm_get_config(p_bind_clock->p_bind_comp,
                          MM_INDEX_CONFIG_TIME_CUR_MEDIA_TIME, &timestamp);
            printf("[%s:%d]Excuting--->Pause,timestamp:" FMT_d64 "\n",
                   __FUNCTION__, __LINE__, timestamp.timestamp);
        } else {
            p_video_render_data->pause_time_point =
                mm_video_render_clock_get_sys_time();
            printf("[%s:%d]Excuting--->Pause,pause_time_point:" FMT_d64 "\n",
                   __FUNCTION__, __LINE__,
                   p_video_render_data->pause_time_point);
        }
    } else {
        mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_video_render_data->state, NULL);
        return;
    }
    p_video_render_data->state = MM_STATE_PAUSE;
    mm_video_render_event_notify(p_video_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_video_render_data->state, NULL);
}

static void mm_video_render_state_change_to_executing(
    mm_video_render_data *p_video_render_data)
{
    if (p_video_render_data->state == MM_STATE_LOADED) {
        mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_video_render_data->state, NULL);

        return;

    } else if (p_video_render_data->state == MM_STATE_IDLE) {
    } else if (p_video_render_data->state == MM_STATE_PAUSE) {
        mm_time_config_timestamp timestamp;
        mm_bind_info *p_bind_clock =
            &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
        if (p_bind_clock->flag) {
            mm_get_config(p_bind_clock->p_bind_comp,
                          MM_INDEX_CONFIG_TIME_CUR_MEDIA_TIME, &timestamp);
            printf("[%s:%d]Pause--->Excuting,timestamp:" FMT_d64 "\n",
                   __FUNCTION__, __LINE__, timestamp.timestamp);
        } else {
            p_video_render_data->pause_time_durtion +=
                (mm_video_render_clock_get_sys_time() -
                 p_video_render_data->pause_time_point);
            printf("[%s:%d]Pause--->Excuting,pause_time_point:" FMT_d64
                   ",curTime:" FMT_d64 ",pauseDura:" FMT_d64 "\n",
                   __FUNCTION__, __LINE__,
                   p_video_render_data->pause_time_point,
                   mm_video_render_clock_get_sys_time(),
                   p_video_render_data->pause_time_durtion);
        }
    } else {
        mm_video_render_event_notify(p_video_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_video_render_data->state, NULL);

        return;
    }
    p_video_render_data->state = MM_STATE_EXECUTING;
    mm_video_render_event_notify(p_video_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_video_render_data->state, NULL);
}


static s64
mm_vdieo_render_get_media_time(mm_video_render_data *p_video_render_data)
{
    s64 media_time;
    media_time = mm_video_render_clock_get_sys_time() -
                 p_video_render_data->wall_time_base -
                 p_video_render_data->pause_time_durtion +
                 p_video_render_data->first_show_pts;
    return media_time;
}

static void
mm_vdieo_render_set_media_clock(mm_video_render_data *p_video_render_data,
                                struct mpp_frame *p_frame_info)
{
    p_video_render_data->wall_time_base = mm_video_render_clock_get_sys_time();
    p_video_render_data->pause_time_durtion = 0;
    p_video_render_data->first_show_pts = p_frame_info->pts;
    p_video_render_data->pre_frame_pts = p_frame_info->pts;
    logw("p_video_render_data->first_show_pts:" FMT_d64
         ",p_video_render_data->wall_time_base:" FMT_d64 "\n",
         p_video_render_data->first_show_pts,
         p_video_render_data->wall_time_base);
}

static s32
mm_vdieo_render_process_video_sync(mm_video_render_data *p_video_render_data,
                                   struct mpp_frame *p_frame_info, s64 *delay)
{
    s64 delay_time = 0;
    mm_time_config_timestamp timestamp = { 0 };
    MM_VIDEO_SYNC_TYPE sync_type = MM_VIDEO_SYNC_INVAILD;
    mm_bind_info *p_bind_clock =
        &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
    if (p_video_render_data == NULL || p_frame_info == NULL || delay == NULL) {
        return MM_VIDEO_SYNC_INVAILD;
    }

    if (p_bind_clock->flag) {
        timestamp.port_index = p_bind_clock->bind_port_index;
        mm_get_config(p_bind_clock->p_bind_comp,
                      MM_INDEX_CONFIG_TIME_CUR_MEDIA_TIME, &timestamp);
        if (timestamp.timestamp == -1) {
            delay_time = p_frame_info->pts -
                         mm_vdieo_render_get_media_time(p_video_render_data);
            if (delay_time > 10 * 1000 * 1000 ||
                delay_time < -10 * 1000 * 1000) {
                loge(
                    "tunneld clock ,but audio do not come ,vidoe pts jump event!!!\n");
                delay_time = 40 * 1000;
                mm_vdieo_render_set_media_clock(p_video_render_data,
                                                p_frame_info);
            }
        } else {
            delay_time = p_frame_info->pts - timestamp.timestamp;
            if (delay_time > 10 * 1000 * 1000 ||
                delay_time < -10 * 1000 * 1000) {
                if (p_frame_info->pts - p_video_render_data->pre_frame_pts >
                        10 * 1000 * 1000 ||
                    p_frame_info->pts - p_video_render_data->pre_frame_pts <
                        -10 * 1000 * 1000) { // case by video pts jump event

                    loge("video pts jump event!!!\n");
                } else {
                    loge("audio pts jump event!!!\n");
                }
            }
        }
    } else {
        delay_time = p_frame_info->pts -
                     mm_vdieo_render_get_media_time(p_video_render_data);
        if (delay_time > 10 * 1000 * 1000 ||
            delay_time < -10 * 1000 * 1000) { //pts jump event
            loge("not tunneled clock ,vidoe pts jump event!!!\n");
            delay_time = 40 * 1000; //this should  arccording to fame rate
            mm_vdieo_render_set_media_clock(p_video_render_data, p_frame_info);
        }
    }

    if (p_frame_info->flags & FRAME_FLAG_EOS) {
        printf("[%s:%d]pts:%lld,delay_time:" FMT_d64 "\n", __FUNCTION__,
               __LINE__, p_frame_info->pts, delay_time);
    }

    if (delay_time > 2 * MAX_VIDEO_SYNC_DIFF_TIME) {
        sync_type = MM_VIDEO_SYNC_DEALY;
    } else if (delay_time > (-2) * MAX_VIDEO_SYNC_DIFF_TIME) {
        sync_type = MM_VIDEO_SYNC_SHOW;
    } else {
        sync_type = MM_VIDEO_SYNC_DROP;
    }
    *delay = delay_time;

    return (s32)sync_type;
}

//#define MM_VIDEO_RENDER_ENABLE_DUMP_PIC

#ifdef MM_VIDEO_RENDER_ENABLE_DUMP_PIC
static int mm_video_render_dump_pic(struct mpp_buf *video, int index)
{
    int i;
    int data_size[3] = { 0, 0, 0 };
    unsigned char* hw_data[3] =  {0};
    int comp = 3;
    FILE *fp_save = NULL;
    char fileName[255] = { 0 };
    if (video->format == MPP_FMT_YUV420P) {
        comp = 3;
        data_size[0] = video->size.height * video->stride[0];
        data_size[1] = data_size[2] = data_size[0] / 4;
    } else if (video->format == MPP_FMT_NV12 || video->format == MPP_FMT_NV21) {
        comp = 2;
        data_size[0] = video->size.height * video->stride[0];
        data_size[1] = data_size[0] / 2;
    } else if (video->format == MPP_FMT_YUV444P) {
        comp = 3;
        data_size[0] = video->size.height * video->stride[0];
        data_size[1] = data_size[2] = data_size[0];
    } else if (video->format == MPP_FMT_YUV422P) {
        comp = 3;
        data_size[0] = video->size.height * video->stride[0];
        data_size[1] = data_size[2] = data_size[0] / 2;
    } else if (video->format == MPP_FMT_RGBA_8888 ||
               video->format == MPP_FMT_BGRA_8888 ||
               video->format == MPP_FMT_ARGB_8888 ||
               video->format == MPP_FMT_ABGR_8888) {
        comp = 1;
        data_size[0] = video->size.height * video->stride[0];
    } else if (video->format == MPP_FMT_RGB_888 ||
               video->format == MPP_FMT_BGR_888) {
        comp = 1;
        data_size[0] = video->size.height * video->stride[0];
    } else if (video->format == MPP_FMT_RGB_565 ||
               video->format == MPP_FMT_BGR_565) {
        comp = 1;
        data_size[0] = video->size.height * video->stride[0];
    }
    loge("data_size: %d %d %d, height: %d, stride: %d, format: %d",
         data_size[0], data_size[1], data_size[2], video->size.height,
         video->stride[0], video->format);
    snprintf(fileName, sizeof(fileName), "/mnt/sdcard/video/pics/pic%d.yuv", index);
    fp_save = fopen(fileName, "wb");
    if (fp_save == NULL) {
        loge("fopen %s error\n", fileName);
        return -1;
    }
    logd("fopen %s ok\n", fileName);
    for (i = 0; i < comp; i++) {
        hw_data[i] = mmap(NULL, data_size[i], PROT_READ, MAP_SHARED, video->fd[i], 0);
        if (hw_data[i] == MAP_FAILED) {
            loge("dmabuf alloc mmap failed!");
            return -1;
	}
        if (fp_save)
            fwrite(hw_data[i], 1, data_size[i], fp_save);
    }
    fclose(fp_save);
    for (i = 0; i < comp; i++) {
        munmap(hw_data[i], data_size[i]);
    }
    return 0;
}
#endif

static void mm_video_render_frame_swap(s32 *a, s32 *b)
{
    s32 tmp = *a;
    *a = *b;
    *b = tmp;
}

static int
mm_video_render_component_process_cmd(mm_video_render_data *p_video_render_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    s32 cmd_data;
    struct aic_message message;

    if (aic_msg_get(&p_video_render_data->s_msg, &message) == 0) {
        cmd = message.message_id;
        cmd_data = message.param;
        logd("cmd:%d, cmd_data:%d\n", cmd, cmd_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            pthread_mutex_lock(&p_video_render_data->state_lock);
            if (p_video_render_data->state == (MM_STATE_TYPE)(cmd_data)) {
                mm_video_render_event_notify(p_video_render_data,
                                             MM_EVENT_ERROR,
                                             MM_ERROR_SAME_STATE, 0, NULL);
                pthread_mutex_unlock(&p_video_render_data->state_lock);
                goto CMD_EXIT;
            }
            switch (cmd_data) {
                case MM_STATE_INVALID:
                    mm_video_render_state_change_to_invalid(
                        p_video_render_data);
                    break;
                case MM_STATE_LOADED:
                    mm_video_render_state_change_to_loaded(p_video_render_data);
                    break;
                case MM_STATE_IDLE:
                    mm_video_render_state_change_to_idle(p_video_render_data);
                    break;
                case MM_STATE_EXECUTING:
                    mm_video_render_state_change_to_executing(
                        p_video_render_data);
                    break;
                case MM_STATE_PAUSE:
                    mm_video_render_state_change_to_pause(p_video_render_data);
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&p_video_render_data->state_lock);
        } else if (MM_COMMAND_STOP == cmd) {
            logi("mm_video_render_component_thread ready to exit!!!\n");
            goto CMD_EXIT;
        }
    }

CMD_EXIT:
    return cmd;
}

void mm_video_render_print_frame_count(mm_video_render_data *p_video_render_data)
{
    printf("[%s:%d]receive_frame_num:%u,"
           "show_frame_ok_num:%u,"
           "show_frame_fail_num:%u,"
           "giveback_frame_ok_num:%u,"
           "giveback_frame_fail_num:%u,"
           "drop_frame_num:%u\n",
           __FUNCTION__, __LINE__, p_video_render_data->receive_frame_num,
           p_video_render_data->show_frame_ok_num,
           p_video_render_data->show_frame_fail_num,
           p_video_render_data->giveback_frame_ok_num,
           p_video_render_data->giveback_frame_fail_num,
           p_video_render_data->drop_frame_num);
}

void mm_video_render_print_frame(struct mpp_frame *p_frame)
{
    printf(
        "[%s:%d]stride[0]:%d,stride[1]:%d,stride[2]:%d,format:%d,width:%d,height:%d,"
        "crop_en:%d,crop.x:%d,crop.y:%d,crop.width:%d,crop.height:%d\n",
        __FUNCTION__, __LINE__, p_frame->buf.stride[0], p_frame->buf.stride[1],
        p_frame->buf.stride[2], p_frame->buf.format, p_frame->buf.size.width,
        p_frame->buf.size.height, p_frame->buf.crop_en, p_frame->buf.crop.x,
        p_frame->buf.crop.y, p_frame->buf.crop.width, p_frame->buf.crop.height);
}

void mm_video_render_calc_fps(mm_video_render_data *p_video_render_data)
{
    static struct timespec pev = { 0 }, cur = { 0 };

    if (pev.tv_sec == 0) {
        clock_gettime(CLOCK_REALTIME, &pev);
    } else {
        long diff = 0;
        clock_gettime(CLOCK_REALTIME, &cur);
        diff = (cur.tv_sec - pev.tv_sec) * 1000 * 1000 +
               (cur.tv_nsec - pev.tv_nsec) / 1000;

        if (diff > 1000 * 1000) {
            logd("frame_rate:%d\n", p_video_render_data->calc_frame_rate);
            p_video_render_data->calc_frame_rate = 0;
            pev = cur;
        }
    }
}

static void mm_video_render_set_dis_rect(mm_video_render_data *p_video_render_data)
{
    struct mpp_size size;
    struct mpp_rect dis_rect;

    p_video_render_data->render->get_screen_size(p_video_render_data->render,
                                                 &size);
    dis_rect.x = 0;
    dis_rect.y = 0;
    dis_rect.width = size.width;
    dis_rect.height = size.height;
    if (p_video_render_data->dis_rect_change) {
        p_video_render_data->render->set_dis_rect(
            p_video_render_data->render, &p_video_render_data->dis_rect);
        p_video_render_data->dis_rect_change = MM_FALSE;
    } else {
        p_video_render_data->render->set_dis_rect(p_video_render_data->render,
                                                  &dis_rect);
        printf("[%s:%d]init dis rect:[%d,%d,%d,%d]!!!\n", __FUNCTION__,
               __LINE__, dis_rect.x, dis_rect.y, dis_rect.width,
               dis_rect.height);
    }
}


static s32 mm_video_render_rend_frame(mm_video_render_data *p_video_render_data,
                                     struct mpp_frame *p_frame)
{
    s32 ret = MM_ERROR_NONE;
    if (p_video_render_data->rotation_angle_change) {
        mm_video_render_init_rotation_param(
            p_video_render_data, p_frame);
        p_video_render_data->rotation_angle_change = 0;
    }
    mm_video_render_rotate_frame(p_video_render_data, p_frame);

#ifdef MM_VIDEO_RENDER_ENABLE_DUMP_PIC
    mm_video_render_dump_pic(&p_video_render_data->p_cur_display_frame->buf,
                             p_video_render_data->dump_index++);
#endif
    ret = p_video_render_data->render->rend(
        p_video_render_data->render, p_video_render_data->p_cur_display_frame);

    if (ret == 0) {
        p_video_render_data->show_frame_ok_num++;
    } else {
        // how to do ,now deal with  same success
        loge("render error");
        p_video_render_data->show_frame_fail_num++;
    }

    return ret;
}

static s32
mm_video_render_process_sync_show(mm_video_render_data *p_video_render_data,
                                  MM_VIDEO_SYNC_TYPE sync_type, s64 delay_time)
{
    s32 ret = MM_ERROR_NONE;
    s32 data1, data2;
    s32 cur_frame_id, last_frame_id;
    mm_bind_info *p_bind_vdec;

    cur_frame_id = p_video_render_data->cur_disp_frame_id;
    last_frame_id = p_video_render_data->last_disp_frame_id;
    p_bind_vdec =
        &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];

    if (sync_type == MM_VIDEO_SYNC_SHOW) {
_AIC_SHOW_DIRECT_:
        ret = mm_video_render_rend_frame(p_video_render_data,
            &p_video_render_data->disp_frames[cur_frame_id]);
        if (ret == 0) {
            p_video_render_data->calc_frame_rate++;
            if (p_video_render_data->disp_frames[cur_frame_id].flags &
                FRAME_FLAG_EOS) {
                p_video_render_data->flags |=
                    VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
                printf("[%s:%d]receive frame_end_flag\n", __FUNCTION__,
                       __LINE__);
            }
        } else {
            // how to do ,now deal with  same success
            loge("render error");
            p_video_render_data->show_frame_fail_num++;
            if (p_video_render_data->disp_frame_num) {
                ret = mm_video_render_put_frame(
                    p_bind_vdec->p_bind_comp,
                    &p_video_render_data->disp_frames[last_frame_id]);
                if (DEC_OK == ret) {
                    p_video_render_data->giveback_frame_ok_num++;
                } else {
                    p_video_render_data->giveback_frame_fail_num++;
                }
            }
            mm_video_render_frame_swap(
                &p_video_render_data->cur_disp_frame_id,
                &p_video_render_data->last_disp_frame_id);
            p_video_render_data->disp_frame_num++;
            mm_send_command(p_bind_vdec->p_bind_comp, MM_COMMAND_WKUP, 0, NULL);
            return ret;
        }

        data1 = (p_video_render_data->disp_frames[cur_frame_id].pts >> 32) &
                0x00000000ffffffff;
        data2 = p_video_render_data->disp_frames[cur_frame_id].pts &
                0x00000000ffffffff;
        mm_video_render_event_notify(
            p_video_render_data, MM_EVENT_VIDEO_RENDER_PTS, data1, data2, NULL);

    } else if (sync_type == MM_VIDEO_SYNC_DROP) {
        static int drop_num = 0;
        if (p_video_render_data->disp_frames[cur_frame_id].flags &
            FRAME_FLAG_EOS) {
            p_video_render_data->flags |=
                VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
            printf("[%s:%d]receive frame_end_flag\n", __FUNCTION__, __LINE__);
        }

        p_video_render_data->drop_frame_num++;
        drop_num++;
        if (drop_num > 50) {
            drop_num = 0;
            printf("MM_VIDEO_SYNC_DROP:drop_frame_num:%u"
                   ",delay_time:" FMT_d64 "\n",
                   p_video_render_data->drop_frame_num, delay_time);
        }
    } else if (sync_type == MM_VIDEO_SYNC_DEALY) {
        struct timespec delay_before = {0}, delay_after = {0};
        long  delay = 0;
        logd("video sync delay:%ld,%ld\n", delay_time, delay_time-MAX_VIDEO_SYNC_DIFF_TIME);
        pthread_mutex_lock(&p_video_render_data->in_frame_lock);
        p_video_render_data->wait_ready_frame_flag = MM_FALSE;
        pthread_mutex_unlock(&p_video_render_data->in_frame_lock);
        clock_gettime(CLOCK_REALTIME, &delay_before);
        if (delay_time - MAX_VIDEO_SYNC_DIFF_TIME < MAX_VIDEO_DELAY_TIME_THRESHOLD)
            aic_msg_wait_new_msg(&p_video_render_data->s_msg, delay_time - MAX_VIDEO_SYNC_DIFF_TIME);
        else
            aic_msg_wait_new_msg(&p_video_render_data->s_msg, MAX_VIDEO_SYNC_DIFF_TIME);
        clock_gettime(CLOCK_REALTIME, &delay_after);
        delay = (delay_after.tv_sec - delay_before.tv_sec) * 1000 * 1000 +
                (delay_after.tv_nsec - delay_before.tv_nsec) / 1000;
        logd("[%s:%d]:true sleep time %ld, predict sleep time:%ld\n", __FUNCTION__, __LINE__,
             delay, delay_time - MAX_VIDEO_SYNC_DIFF_TIME);
        goto _AIC_SHOW_DIRECT_;
    }

    return ret;
}


static void *mm_video_render_component_thread(void *p_thread_data)
{
    s32 ret = MM_ERROR_NONE;
    s32 cmd = MM_COMMAND_UNKNOWN;
    mm_video_render_data *p_video_render_data =
        (mm_video_render_data *)p_thread_data;
    MM_BOOL b_notify_frame_end = 0;

    mm_bind_info *p_bind_clock;
    mm_bind_info *p_bind_vdec;
    s32 cur_frame_id, last_frame_id;

    p_video_render_data->wait_ready_frame_flag = 1;

    p_bind_clock =
        &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
    p_bind_vdec =
        &p_video_render_data->in_port_bind[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];

    while (1) {
    _AIC_MSG_GET_:

        /* process cmd and change state*/
        cmd = mm_video_render_component_process_cmd(p_video_render_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            continue;
        } else if (MM_COMMAND_STOP == cmd) {
            goto _EXIT;
        }

        if (p_video_render_data->state != MM_STATE_EXECUTING) {
            aic_msg_wait_new_msg(&p_video_render_data->s_msg, 0);
            continue;
        }

        if (p_video_render_data->flags &
            VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG) {
            if (!b_notify_frame_end) {
                mm_video_render_event_notify(p_video_render_data,
                                             MM_EVENT_BUFFER_FLAG, 0, 0, NULL);
                b_notify_frame_end = 1;
            }
            aic_msg_wait_new_msg(&p_video_render_data->s_msg, 0);
            continue;
        }
        b_notify_frame_end = 0;

        /* get frame from video decoder*/
        cur_frame_id = p_video_render_data->cur_disp_frame_id;
        last_frame_id = p_video_render_data->last_disp_frame_id;
        ret = mm_video_render_get_frame(
            p_bind_vdec->p_bind_comp,
            &p_video_render_data->disp_frames[cur_frame_id]);
        if (ret != DEC_OK) {
            mm_video_render_wait_frame_timeout(p_video_render_data, ret);
            continue;
        }
        p_video_render_data->receive_frame_num++;

        if (p_video_render_data->frame_first_show_flag) {
            if (!p_video_render_data->video_render_init_flag) {
                ret = p_video_render_data->render->init(
                    p_video_render_data->render, p_video_render_data->layer_id,
                    p_video_render_data->dev_id);
                if (!ret) {
                    p_video_render_data->video_render_init_flag = 1;
                } else {
                    loge("p_video_render_data->render->init fail\n");
                }
            }

            if (p_bind_clock->flag) {
                mm_time_config_timestamp timestamp;
                timestamp.port_index = p_bind_clock->bind_port_index;
                timestamp.timestamp =
                    p_video_render_data->disp_frames[last_frame_id].pts;
                mm_set_config(p_bind_clock->p_bind_comp,
                              MM_INDEX_CONFIG_TIME_CLIENT_START_TIME,
                              &timestamp);
                // whether need to wait????
                if (p_video_render_data->clock_state !=
                    MM_TIME_CLOCK_STATE_RUNNING) {
                    if (p_video_render_data->disp_frame_num) {
                        ret = mm_video_render_put_frame(
                            p_bind_vdec->p_bind_comp,
                            &p_video_render_data->disp_frames[last_frame_id]);
                        if (DEC_OK == ret) {
                            p_video_render_data->giveback_frame_ok_num++;
                        } else {
                            p_video_render_data->giveback_frame_fail_num++;
                        }
                        mm_send_command(p_bind_vdec->p_bind_comp,
                                        MM_COMMAND_WKUP, 0, NULL);
                    }
                    mm_video_render_frame_swap(
                        &p_video_render_data->cur_disp_frame_id,
                        &p_video_render_data->last_disp_frame_id);
                    p_video_render_data->disp_frame_num++;
                    aic_msg_wait_new_msg(&p_video_render_data->s_msg,
                                         10 * 1000);
                    goto _AIC_MSG_GET_;
                }
                printf("[%s:%d]audio start time arrive\n", __FUNCTION__,
                       __LINE__);
            } else { //if it does not tunneld with clock ,it need calcuaute media time by self for control frame rate
                mm_vdieo_render_set_media_clock(
                    p_video_render_data,
                    &p_video_render_data->disp_frames[cur_frame_id]);
            }

            mm_video_render_set_dis_rect(p_video_render_data);

            mm_video_render_print_frame(
                &p_video_render_data->disp_frames[cur_frame_id]);

            /* do render one frame*/
            ret = mm_video_render_rend_frame(p_video_render_data,
                &p_video_render_data->disp_frames[cur_frame_id]);
            if (ret == 0) {
                p_video_render_data->frame_first_show_flag = MM_FALSE;
                mm_video_render_event_notify(p_video_render_data,
                                             MM_EVENT_VIDEO_RENDER_FIRST_FRAME,
                                             0, 0, NULL);
                if (p_video_render_data->disp_frames[cur_frame_id].flags &
                    FRAME_FLAG_EOS) {
                    p_video_render_data->flags |=
                        VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
                    printf("[%s:%d]receive frame_end_flag\n", __FUNCTION__,
                           __LINE__);
                }
            } else {
                // how to do ,now deal with  same success
                loge("render error");
            }
            if (p_video_render_data->disp_frame_num) {
                ret = mm_video_render_put_frame(
                    p_bind_vdec->p_bind_comp,
                    &p_video_render_data->disp_frames[last_frame_id]);
                if (DEC_OK == ret) {
                    p_video_render_data->giveback_frame_ok_num++;
                } else {
                    p_video_render_data->giveback_frame_fail_num++;
                }
                mm_send_command(p_bind_vdec->p_bind_comp, MM_COMMAND_WKUP, 0,
                                NULL);
            }
            mm_video_render_frame_swap(
                &p_video_render_data->cur_disp_frame_id,
                &p_video_render_data->last_disp_frame_id);
            p_video_render_data->disp_frame_num++;
        } else { // not fisrt show
            s64 delay_time = 0;
            MM_VIDEO_SYNC_TYPE sync_type;

            /* process video sync*/
            sync_type = mm_vdieo_render_process_video_sync(
                p_video_render_data,
                &p_video_render_data->disp_frames[cur_frame_id], &delay_time);

            mm_video_render_calc_fps(p_video_render_data);

            /* process render show diffrent strategy*/
            ret = mm_video_render_process_sync_show(p_video_render_data,
                                                    sync_type, delay_time);
            if (ret != 0) {
                goto _AIC_MSG_GET_;
            }

            if (p_video_render_data->disp_frame_num) {
                ret = mm_video_render_put_frame(
                    p_bind_vdec->p_bind_comp,
                    &p_video_render_data->disp_frames[last_frame_id]);
                if (DEC_OK == ret) {
                    p_video_render_data->giveback_frame_ok_num++;
                } else {
                    p_video_render_data->giveback_frame_fail_num++;
                }
                mm_send_command(p_bind_vdec->p_bind_comp, MM_COMMAND_WKUP, 0,
                                NULL);
            }
            mm_video_render_frame_swap(
                &p_video_render_data->cur_disp_frame_id,
                &p_video_render_data->last_disp_frame_id);
            p_video_render_data->disp_frame_num++;
        }
        if (p_video_render_data->dis_rect_change) {
            p_video_render_data->render->set_dis_rect(
                p_video_render_data->render, &p_video_render_data->dis_rect);
            p_video_render_data->dis_rect_change = MM_FALSE;
        }
    }

_EXIT:
    mm_video_render_print_frame_count(p_video_render_data);
    printf("[%s:%d]mm_video_render_component_thread exit\n", __FUNCTION__,
           __LINE__);
    return (void *)MM_ERROR_NONE;
}
