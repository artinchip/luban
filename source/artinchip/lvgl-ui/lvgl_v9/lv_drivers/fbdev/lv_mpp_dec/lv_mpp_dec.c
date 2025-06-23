/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "lvgl.h"
#include "mpp_ge.h"
#include "mpp_decoder.h"
#include "frame_allocator.h"
#include "lv_mpp_dec.h"
#include "aic_ui.h"
#include "../lv_ge2d/lv_draw_ge2d_utils.h"
#include "dma_allocator.h"

#define PNG_HEADER_SIZE (8 + 12 + 13) //png signature + IHDR chuck
#define PNGSIG 0x89504e470d0a1a0aull
#define MNGSIG 0x8a4d4e470d0a1a0aull
#define JPEG_SOI 0xFFD8
#define JPEG_SOF 0xFFC0
#define ALIGN_16B(x) (((x) + (15)) & ~(15))

#define CHECK_RET(func, ret)  do {\
                            if (func != ret) {\
                                LV_LOG_ERROR("Invalid status");\
                                res = LV_RESULT_INVALID;\
                                goto out;\
                            }\
                         } while (0)

#define CHECK_PTR(ptr)  do {\
                            if (ptr == NULL) {\
                                LV_LOG_ERROR("ptr == NULL");\
                                res = LV_RESULT_INVALID;\
                                goto out;\
                            }\
                        } while (0)

typedef struct {
    lv_mutex_t lock;
    int32_t cache_num;
    int32_t max_num;
} mpp_cache_t;

static lv_color_format_t mpp_fmt_to_lv_fmt(enum mpp_pixel_format cf)
{
    lv_color_format_t fmt = LV_COLOR_FORMAT_ARGB8888;

    switch(cf) {
        case MPP_FMT_RGB_565:
            fmt = LV_COLOR_FORMAT_RGB565;
            break;
        case MPP_FMT_RGB_888:
            fmt = LV_COLOR_FORMAT_RGB888;
            break;
        case MPP_FMT_ARGB_8888:
            fmt = LV_COLOR_FORMAT_ARGB8888;
            break;
        case MPP_FMT_XRGB_8888:
            fmt = LV_COLOR_FORMAT_XRGB8888;
            break;
        case MPP_FMT_YUV420P:
            fmt = LV_COLOR_FORMAT_I420;
            break;
        case MPP_FMT_YUV422P:
            fmt = LV_COLOR_FORMAT_I422;
            break;
        case MPP_FMT_YUV444P:
            fmt = LV_COLOR_FORMAT_I444;
            break;
        case MPP_FMT_YUV400:
            fmt = LV_COLOR_FORMAT_I400;
            break;
        default:
            LV_LOG_ERROR("unsupported format:%d", cf);
            break;
    }

    return fmt;
}

static int mpp_get_rgb_stride(int width, enum mpp_pixel_format fmt)
{
    int stride;

    switch(fmt) {
        case MPP_FMT_RGB_565:
            stride = ALIGN_16B(width) * 2;
            break;
        case MPP_FMT_RGB_888:
            stride = ALIGN_16B(width) * 3;
            break;
        case MPP_FMT_ARGB_8888:
            stride = ALIGN_16B(width) * 4;
            break;
        case MPP_FMT_XRGB_8888:
            stride = ALIGN_16B(width) * 4;
            break;
        default:
            stride = ALIGN_16B(width) * 4;
            LV_LOG_ERROR("unsupported format:%d", fmt);
            break;
    }
    return stride;
}

static inline bool image_suffix_is_jpg(char *ptr)
{
    return ((!strcmp(ptr, ".jpg")) || (!strcmp(ptr, ".jpeg"))
            || (!strcmp(ptr, ".JPG")) || (!strcmp(ptr, ".JPEG")));
}

struct lv_stream_t
{
    lv_fs_file_t f;
    int cur_index;
    const uint8_t *src;
    int is_file;
    int data_size;
};

static inline uint64_t stream_to_u64(uint8_t *ptr)
{
    return ((uint64_t)ptr[0] << 56) | ((uint64_t)ptr[1] << 48) |
           ((uint64_t)ptr[2] << 40) | ((uint64_t)ptr[3] << 32) |
           ((uint64_t)ptr[4] << 24) | ((uint64_t)ptr[5] << 16) |
           ((uint64_t)ptr[6] << 8) | ((uint64_t)ptr[7]);
}

static inline unsigned int stream_to_u32(uint8_t *ptr)
{
    return ((unsigned int)ptr[0] << 24) |
           ((unsigned int)ptr[1] << 16) |
           ((unsigned int)ptr[2] << 8) |
           (unsigned int)ptr[3];
}

static inline unsigned short stream_to_u16(uint8_t *ptr)
{
    return  ((unsigned int)ptr[0] << 8) | (unsigned int)ptr[1];
}

static lv_fs_res_t lv_aic_stream_open(struct lv_stream_t *stream, const void *src)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    stream->src = ((lv_img_dsc_t *)src)->data;
    stream->cur_index = 0;
    if (lv_image_src_get_type(src) == LV_IMAGE_SRC_FILE) {
        stream->is_file = 1;
        res = lv_fs_open(&stream->f, src, LV_FS_MODE_RD);
    } else {
        stream->is_file = 0;
        stream->data_size = ((lv_img_dsc_t *)src)->data_size;
    }

    return res;
}

static lv_fs_res_t lv_aic_stream_close(struct lv_stream_t *stream)
{
   if (stream->is_file)
        lv_fs_close(&stream->f);

    return LV_FS_RES_OK;
}

static lv_fs_res_t lv_aic_stream_seek(struct lv_stream_t *stream, uint32_t pos, lv_fs_whence_t whence)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    if (stream->is_file) {
        res = lv_fs_seek(&stream->f, pos, whence);
    } else {
        // only support SEEK_CUR
        stream->cur_index += pos;
    }

    return res;
}

static lv_fs_res_t lv_aic_stream_read(struct lv_stream_t *stream, void *buf, uint32_t btr, uint32_t *br)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    if (stream->is_file) {
        res = lv_fs_read(&stream->f, buf, btr, br);
    } else {
        if (stream->cur_index + btr <= stream->data_size) {
            memcpy(buf, &stream->src[stream->cur_index], btr);
            stream->cur_index += btr;
            *br = btr;
        } else {
            LV_LOG_WARN("data_size:%d, cur_index:%d", stream->data_size, stream->cur_index);
        }
    }

    return res;
}

static void lv_aic_stream_reset(struct lv_stream_t *stream)
{
    stream->cur_index = 0;
    return;
}

static lv_result_t lv_aic_stream_get_size(struct lv_stream_t *stream, uint32_t *file_size)
{
    lv_result_t res = LV_RESULT_OK;

    if (stream->is_file) {
        lv_fs_seek(&stream->f, 0, SEEK_END);
        lv_fs_tell(&stream->f, file_size);
        lv_fs_seek(&stream->f, 0, SEEK_SET);
    } else {
        *file_size = stream->data_size;
    }

    return res;
}

static int get_jpeg_format(uint8_t *buf, enum mpp_pixel_format *pix_fmt)
{
    int i;
    uint32_t h_count_flag;
    uint32_t v_count_flag;
    uint8_t h_count[3] = { 0 };
    uint8_t v_count[3] = { 0 };
    uint8_t nb_components = *buf++;

    for (i = 0; i < nb_components; i++) {
        uint8_t h_v_cnt;

        // skip component id
        buf++;
        h_v_cnt = *buf++;
        h_count[i] = h_v_cnt >> 4;
        v_count[i] = h_v_cnt & 0xf;

        // skip quant_index
        buf++;
    }

    h_count_flag =  h_count[2] | (h_count[1] << 4) | (h_count[0] << 8);
    v_count_flag =  v_count[2] | (v_count[1] << 4) | (v_count[0] << 8);

    if (h_count_flag == 0x211 && v_count_flag == 0x211) {
        *pix_fmt = MPP_FMT_YUV420P;
    } else if (h_count_flag == 0x211 && v_count_flag == 0x111) {
        *pix_fmt = MPP_FMT_YUV422P;
    } else if (h_count_flag == 0x111 && v_count_flag == 0x111) {
        *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count_flag == 0x111 && v_count_flag == 0x222) {
        *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count[1] == 0 && v_count[1] == 0 && h_count[2] == 0 &&
               v_count[2] == 0) {
        *pix_fmt = MPP_FMT_YUV400;
    } else {
        LV_LOG_ERROR("Not support format! h_count: %d %d %d, v_count: %d %d %d",
            h_count[0], h_count[1], h_count[2],
            v_count[0], v_count[1], v_count[2]);
        return -1;
    }

    return 0;
}

static inline lv_result_t check_jpeg_soi(struct lv_stream_t *stream)
{
    uint32_t read_num;
    uint8_t buf[128];
    lv_fs_res_t fs_res;
    lv_result_t res = LV_RESULT_OK;

    // read JPEG SOI
    fs_res = lv_aic_stream_read(stream, buf, 2, &read_num);
    if (fs_res != LV_FS_RES_OK || read_num != 2) {
        res = LV_RESULT_INVALID;
        goto read_err;
    }

    // check SOI
    if (buf[0] != 0xff || buf[1] != 0xd8) {
        res = LV_RESULT_INVALID;
        goto read_err;
    }

    // check SOI
    if (stream_to_u16(buf) != JPEG_SOI) {
        res = LV_RESULT_INVALID;
        goto read_err;
    }

read_err:
    return res;
}

static lv_result_t jpeg_get_img_size(struct lv_stream_t *stream, int *w, int *h, enum mpp_pixel_format *pix_fmt)
{
    uint32_t read_num;
    uint8_t buf[128];
    lv_fs_res_t fs_res;
    lv_result_t res = LV_RESULT_OK;

    if (check_jpeg_soi(stream) != LV_RESULT_OK) {
        res = LV_RESULT_INVALID;
        goto read_err;
    }

    // find SOF
    while (1) {
        int size;
        fs_res = lv_aic_stream_read(stream, buf, 4, &read_num);
        if (fs_res != LV_FS_RES_OK || read_num != 4) {
            res = LV_RESULT_INVALID;
            goto read_err;
        }

        if (stream_to_u16(buf) == JPEG_SOF) {
            fs_res = lv_aic_stream_read(stream, buf, 15, &read_num);
            if (fs_res != LV_FS_RES_OK) {
                res = LV_RESULT_INVALID;
                goto read_err;
            }

            *h = stream_to_u16(buf + 1);
            *w = stream_to_u16(buf + 3);

            get_jpeg_format(buf + 5, pix_fmt);
            break;
        } else {
            size = stream_to_u16(buf + 2);
             fs_res = lv_aic_stream_seek(stream, size - 2, SEEK_CUR);
            if (fs_res != LV_FS_RES_OK) {
                res = LV_RESULT_INVALID;
                goto read_err;
            }
        }
    }
read_err:
    return res;
}

static lv_result_t jpeg_decoder_info(lv_image_decoder_t *decoder, const void *src, lv_image_header_t *header)
{
    lv_fs_res_t res;
    int width;
    int height;
    enum mpp_pixel_format format = MPP_FMT_ARGB_8888;
    struct lv_stream_t stream;

    res = lv_aic_stream_open(&stream, src);
    if (res != LV_FS_RES_OK)
        return LV_RESULT_INVALID;

    res = jpeg_get_img_size(&stream, &width, &height, &format);
    if (res != LV_RESULT_OK) {
        lv_aic_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

#if defined(MPP_JPEG_DEC_OUT_SIZE_LIMIT_ENABLE)
    int size_shift = jpeg_size_limit(width, height);
    width = width >> size_shift;
    height = height >> size_shift;
    header->reserved_2 = size_shift;
#endif

    header->w = width;
    header->h = height;
    header->cf = mpp_fmt_to_lv_fmt(format);

    // means aic mpp buffer
    header->flags |= LV_IMAGE_FLAGS_USER8;

    if (!lv_fmt_is_yuv(header->cf))
        header->stride = mpp_get_rgb_stride(width, format);

    LV_LOG_INFO("w:%d, h:%d, cf:%d", header->w, header->h, header->cf);
	lv_aic_stream_close(&stream);

    return LV_RESULT_OK;
}

static inline lv_result_t check_png_sig(struct lv_stream_t *stream)
{
    uint32_t read_num;
    unsigned char buf[64];
    uint64_t sig;

    lv_aic_stream_read(stream, buf, 8, &read_num);
    sig = stream_to_u64(buf);
    if (sig != PNGSIG && sig != MNGSIG) {
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

static lv_result_t png_decoder_info(lv_image_decoder_t *decoder, const void *src, lv_image_header_t *header)
{
    lv_fs_res_t res;
    uint32_t read_num;
    uint8_t buf[64];
    struct lv_stream_t stream;
    int color_type;

    res = lv_aic_stream_open(&stream, src);
    if (res != LV_FS_RES_OK) {
        return LV_RESULT_INVALID;
    }

    // read png sig + IHDR chuck
    res = lv_aic_stream_read(&stream, buf, PNG_HEADER_SIZE, &read_num);
    if (res != LV_FS_RES_OK || read_num != PNG_HEADER_SIZE) {
        LV_LOG_ERROR("lv_aic_stream_read failed");
        lv_aic_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

    // check signature
    uint64_t sig = stream_to_u64(buf);
    if (sig != PNGSIG && sig != MNGSIG) {
        LV_LOG_WARN("Invalid PNG signature 0x%08llx.", (unsigned long long)sig);
        lv_aic_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

    header->w = stream_to_u32(buf + 8 + 8);
    header->h = stream_to_u32(buf + 8 + 8 + 4);
    
    // means aic mpp buffer
    header->flags |= LV_IMAGE_FLAGS_USER8;

    color_type = buf[8 + 8 + 8 + 1];
    if (color_type == 2) {
        header->cf = mpp_fmt_to_lv_fmt(MPP_FMT_RGB_888);
        header->stride = mpp_get_rgb_stride(header->w, MPP_FMT_RGB_888);
    } else {
        header->cf = mpp_fmt_to_lv_fmt(MPP_FMT_ARGB_8888);
        header->stride = mpp_get_rgb_stride(header->w, MPP_FMT_ARGB_8888);
    }

    LV_LOG_INFO("header->w:%d, header->h:%d, header->cf:%d", header->w, header->h, header->cf);
    lv_aic_stream_close(&stream);

    return LV_RESULT_OK;
}

static lv_res_t fake_decoder_info(lv_image_decoder_t *decoder, const void *src, lv_image_header_t *header)
{
    int width;
    int height;
    int alpha_en;
    unsigned int color;
    int ret;

    FAKE_IMAGE_PARSE((char *)src, ret, width, height, alpha_en, color);
    header->w = width;
    header->h = height;
    header->cf = LV_COLOR_FORMAT_RAW;

    // means aic mpp buffer
    header->flags |= LV_IMAGE_FLAGS_USER8;

    if (ret != 4) {
        LV_LOG_WARN("w:%d, h:%d, alpha_en:%d, color:%08x", width, height, alpha_en, color);
        return LV_RESULT_INVALID;
    } else {
        return LV_RES_OK;
    }
}

static lv_result_t lv_mpp_dec_info(lv_image_decoder_t *decoder, const void *src, lv_image_header_t *header)
{
    char* ptr = NULL;
    lv_result_t res = LV_RESULT_INVALID;

    if (lv_image_src_get_type(src) == LV_IMAGE_SRC_FILE) {
        ptr = strrchr(src, '.');
        if (!strcmp(ptr, ".png")) {
            return png_decoder_info(decoder, src, header);
        } else if (image_suffix_is_jpg(ptr)) {
            return jpeg_decoder_info(decoder, src, header);
        } else if (!strcmp(ptr, ".fake")) {
            return fake_decoder_info(decoder, src, header);
        } else {
            return LV_RESULT_INVALID;
        }
    } else if (lv_image_src_get_type(src) == LV_IMAGE_SRC_VARIABLE) {
        lv_color_format_t cf = ((lv_img_dsc_t *)src)->header.cf;
        if (cf == LV_COLOR_FORMAT_RAW || cf == LV_COLOR_FORMAT_RAW) {
            res = jpeg_decoder_info(decoder, src, header);
            if (res != LV_RESULT_OK)
                res = png_decoder_info(decoder, src, header);
        }
    }

    return res;
}

struct ext_frame_allocator {
    struct frame_allocator base;
    struct mpp_frame* frame;
};

static int alloc_frame_buffer(struct frame_allocator *p, struct mpp_frame* frame,
                              int width, int height, enum mpp_pixel_format format)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

    memcpy(frame, impl->frame, sizeof(struct mpp_frame));
    return 0;
}

static int free_frame_buffer(struct frame_allocator *p, struct mpp_frame *frame)
{
    return 0;
}

static int close_allocator(struct frame_allocator *p)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

    free(impl);

    return 0;
}

static struct alloc_ops def_ops = {
    .alloc_frame_buffer = alloc_frame_buffer,
    .free_frame_buffer = free_frame_buffer,
    .close_allocator = close_allocator,
};

static struct frame_allocator* open_allocator(struct mpp_frame* frame)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)malloc(sizeof(struct ext_frame_allocator));

    if(impl == NULL) {
        return NULL;
    }

    memset(impl, 0, sizeof(struct ext_frame_allocator));

    impl->frame = frame;
    impl->base.ops = &def_ops;
    return &impl->base;
}

static void frame_buf_free(mpp_decoder_data_t *mpp_data)
{
    int i;
    for (i = 0; i < 3; i++) {
        if (mpp_data->data[i] != MAP_FAILED && mpp_data->data[i]) {
            dmabuf_munmap((unsigned char*)mpp_data->data[i], mpp_data->size[i]);
            mpp_data->data[i] = NULL;
            dmabuf_free(mpp_data->dec_buf.fd[i]);
        }
    }
}

static lv_result_t frame_buf_alloc(mpp_decoder_data_t *mpp_data, struct mpp_buf *alloc_buf,
                                   int *size, uint32_t cf)
{
    int i;
    int data_size = 0;
    int dma_fd = dmabuf_device_open();
    for (i = 0; i < 3; i++) {
        if (size[i]) {
            alloc_buf->fd[i] = dmabuf_alloc(dma_fd, size[i]);
            if (alloc_buf->fd[i] < 0) {
                goto alloc_error;
            } else {
                mpp_data->dec_buf.fd[i] = alloc_buf->fd[i];
                mpp_data->data[i] = dmabuf_mmap(alloc_buf->fd[i], size[i]);
                if (mpp_data->data[i] == MAP_FAILED) {
                    goto alloc_error;
                }
                mpp_data->size[i] = size[i];
                data_size += size[i];
            }
        }
    }

    mpp_data->decoded.header.w = alloc_buf->size.width;
    mpp_data->decoded.header.h = alloc_buf->size.height;
    mpp_data->decoded.header.cf = cf;
    mpp_data->decoded.header.magic = LV_IMAGE_HEADER_MAGIC;

    // means aic mpp buffer
    mpp_data->decoded.header.flags = LV_IMAGE_FLAGS_USER8;

    if (lv_fmt_is_yuv(cf)) {
        mpp_data->decoded.data =(uint8_t *)(&mpp_data->dec_buf);
        mpp_data->decoded.data_size = data_size;
        mpp_data->decoded.unaligned_data = (uint8_t *)(&mpp_data->dec_buf);
    } else {
        mpp_data->decoded.header.stride = alloc_buf->stride[0];
        mpp_data->decoded.data =(uint8_t *)mpp_data->data[0];
        mpp_data->decoded.data_size = size[0];
        mpp_data->decoded.unaligned_data = mpp_data->data[0];
    }

    dmabuf_device_close(dma_fd);
    return LV_RESULT_OK;
alloc_error:
    dmabuf_device_close(dma_fd);
    frame_buf_free(mpp_data);
    return LV_RESULT_INVALID;
}

static void set_frame_buf_size(struct mpp_frame *frame, int *buf_size, int size_shift)
{
    int height_align;
    int width = frame->buf.size.width;
    int height = frame->buf.size.height;

    if (size_shift > 0) {
        height_align = ALIGN_16B(ALIGN_16B(height) >> size_shift);
        frame->buf.size.width =  width >> size_shift;;
        frame->buf.size.height = height >> size_shift;
    } else {
        height_align = ALIGN_16B(height);
    }

    switch (frame->buf.format) {
    case MPP_FMT_YUV420P:
        if (size_shift > 0)
            frame->buf.stride[0] =  ALIGN_16B(ALIGN_16B(width) >> size_shift);
        else
            frame->buf.stride[0] =  ALIGN_16B(width);

        frame->buf.stride[1] =  frame->buf.stride[0] >> 1;
        frame->buf.stride[2] =  frame->buf.stride[0] >> 1;
        buf_size[0] = frame->buf.stride[0] * height_align;
        buf_size[1] = frame->buf.stride[1] * (height_align >> 1);
        buf_size[2] = frame->buf.stride[2] * (height_align >> 1);
        break;
    case MPP_FMT_YUV422P:
        if (size_shift > 0)
            frame->buf.stride[0] =  ALIGN_16B(ALIGN_16B(width) >> size_shift);
        else
            frame->buf.stride[0] =  ALIGN_16B(width);

        frame->buf.stride[1] =  frame->buf.stride[0] >> 1;
        frame->buf.stride[2] =  frame->buf.stride[0] >> 1;
        buf_size[0] = frame->buf.stride[0] * height_align;
        buf_size[1] = frame->buf.stride[1] * height_align;
        buf_size[2] = frame->buf.stride[2] * height_align;
        break;
    case MPP_FMT_YUV444P:
        if (size_shift > 0)
            frame->buf.stride[0] =  ALIGN_16B(ALIGN_16B(width) >> size_shift);
        else
            frame->buf.stride[0] =  ALIGN_16B(width);

        frame->buf.stride[1] =  frame->buf.stride[0];
        frame->buf.stride[2] =  frame->buf.stride[0];
        buf_size[0] = frame->buf.stride[0] * height_align;
        buf_size[1] = frame->buf.stride[1] * height_align;
        buf_size[2] = frame->buf.stride[2] * height_align;
        break;
    case MPP_FMT_RGB_565:
        if (size_shift > 0)
            frame->buf.stride[0] =  ALIGN_16B(ALIGN_16B(width) >> size_shift) * 2;
        else
            frame->buf.stride[0] =  ALIGN_16B(width) * 2;

        buf_size[0] = frame->buf.stride[0] * height_align;
    case MPP_FMT_RGB_888:
        if (size_shift > 0)
            frame->buf.stride[0] =  ALIGN_16B(ALIGN_16B(width) >> size_shift) * 3;
        else
            frame->buf.stride[0] =  ALIGN_16B(width) * 3;

        buf_size[0] = frame->buf.stride[0] * height_align;
        break;
    case MPP_FMT_ARGB_8888:
        if (size_shift > 0) {
            frame->buf.stride[0] =  ALIGN_16B(ALIGN_16B(width) >> size_shift) * 4;
            buf_size[0] = frame->buf.stride[0] * height_align;
        } else {
            frame->buf.stride[0] =  ALIGN_16B(width * 4);
            buf_size[0] = frame->buf.stride[0] * height;
        }
        break;
    default:
        LV_LOG_ERROR("unsupported format:%d", frame->buf.format);
        break;
    }
}

static inline bool image_cache_check(mpp_cache_t *mpp_cache)
{
#if LV_CACHE_IMG_NUM_LIMIT == 0
    return true;
#else
    if (mpp_cache->cache_num < mpp_cache->max_num) {
        return true;
    } else {
        if (lv_drop_one_cached_image())
            return true;
        else
            return false;
    }
#endif
}

static lv_result_t lv_mpp_dec_open(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc)
{
    lv_result_t res = LV_RESULT_OK;
    uint32_t file_len = 0;
    struct lv_stream_t stream;
    struct mpp_packet packet;
    int width = 0;
    int height = 0;
    enum mpp_codec_type type = MPP_CODEC_VIDEO_DECODER_PNG;
    char* ptr = NULL;
    struct decode_config config = { 0 };
    int buf_size[3] = { 0 };
    struct mpp_decoder *dec = NULL;
    struct frame_allocator *allocator = NULL;

    if (lv_aic_stream_open(&stream, dsc->src) != LV_FS_RES_OK)
        return LV_RESULT_INVALID;

    mpp_decoder_data_t *mpp_data = (mpp_decoder_data_t *)lv_malloc_zeroed(sizeof(mpp_decoder_data_t));
    CHECK_PTR(mpp_data);

    width = dsc->header.w;
    height = dsc->header.h;
    config.pix_fmt = lv_fmt_to_mpp_fmt(dsc->header.cf);

    if (lv_image_src_get_type(dsc->src) == LV_IMAGE_SRC_FILE) {
        ptr = strrchr(dsc->src, '.');
        if (image_suffix_is_jpg(ptr))
            type = MPP_CODEC_VIDEO_DECODER_MJPEG;

    } else if (lv_image_src_get_type(dsc->src) == LV_IMAGE_SRC_VARIABLE) {
        res = check_jpeg_soi(&stream);
        if (res == LV_RESULT_OK) {
            type = MPP_CODEC_VIDEO_DECODER_MJPEG;
        } else {
            lv_aic_stream_reset(&stream);
            res = check_png_sig(&stream);
            CHECK_RET(res, LV_RESULT_OK);
        }
        lv_aic_stream_reset(&stream);
    }

    lv_aic_stream_get_size(&stream, &file_len);
    dec = mpp_decoder_create(type);
    CHECK_PTR(dec);

    config.bitstream_buffer_size = (file_len + 1023) & (~1023);
    config.extra_frame_num = 0;
    config.packet_count = 1;

    struct mpp_frame dec_frame;
    memset(&dec_frame, 0, sizeof(struct mpp_frame));
    dec_frame.buf.size.width = width;
    dec_frame.buf.size.height = height;
    dec_frame.buf.format = config.pix_fmt;
    dec_frame.buf.buf_type = MPP_DMA_BUF_FD;

    int size_shift = 0;
#if defined(MPP_JPEG_DEC_OUT_SIZE_LIMIT_ENABLE)
    if (type == MPP_CODEC_VIDEO_DECODER_MJPEG)
        size_shift = dsc->header.reserved_2;
#endif
    set_frame_buf_size(&dec_frame, buf_size, 0);
    if (size_shift > 0) {
        struct mpp_scale_ratio scale;
        scale.hor_scale = size_shift;
        scale.ver_scale = size_shift;
        mpp_decoder_control(dec, MPP_DEC_INIT_CMD_SET_SCALE, &scale);
    }

    CHECK_RET(frame_buf_alloc(mpp_data, &dec_frame.buf, buf_size, dsc->header.cf), LV_RESULT_OK);

    // allocator will be released in decoder
    allocator = open_allocator(&dec_frame);
    CHECK_PTR(allocator);

    mpp_decoder_control(dec, MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, (void*)allocator);
    CHECK_RET(mpp_decoder_init(dec, &config), 0);
    memset(&packet, 0, sizeof(struct mpp_packet));
    mpp_decoder_get_packet(dec, &packet, file_len);

    uint32_t read_size = 0;
    lv_aic_stream_read(&stream, packet.data, file_len, &read_size);
    packet.size = file_len;
    packet.flag = PACKET_FLAG_EOS;

    mpp_decoder_put_packet(dec, &packet);
    CHECK_RET(mpp_decoder_decode(dec), 0);

    struct mpp_frame frame;
    memset(&frame, 0, sizeof(struct mpp_frame));
    mpp_decoder_get_frame(dec, &frame);
    mpp_decoder_put_frame(dec, &frame);

    memcpy(&mpp_data->dec_buf, &frame.buf, sizeof(struct mpp_buf));
    dsc->decoded = &mpp_data->decoded;

#if LV_CACHE_DEF_SIZE > 0
    if (!dsc->args.no_cache) {
        mpp_cache_t *mpp_cache = (mpp_cache_t *)decoder->user_data;
        if (image_cache_check(mpp_cache)) {
            // Add it to cache
            lv_image_cache_data_t search_key;
            search_key.src_type = dsc->src_type;
            search_key.src = dsc->src;
            search_key.slot.size = dsc->decoded->data_size;
            mpp_data->cached = true;
            lv_cache_entry_t *cache_entry = lv_image_decoder_add_to_cache(decoder, &search_key, dsc->decoded, mpp_data);
            CHECK_PTR(cache_entry);
            dsc->cache_entry = cache_entry;
            lv_mutex_lock(&mpp_cache->lock);
            mpp_cache->cache_num++;
            lv_mutex_unlock(&mpp_cache->lock);
        }
    }
#endif

out:
    if (dec)
        mpp_decoder_destory(dec);

    if (res == LV_RESULT_INVALID) {
        dsc->decoded = NULL;
        if (mpp_data) {
            frame_buf_free(mpp_data);
            lv_free(mpp_data);
        }
    }

    lv_aic_stream_close(&stream);
    return res;
}

static void mpp_dec_data_release(mpp_decoder_data_t *decoder_data)
{
    if (decoder_data == NULL)
        return;

    if (decoder_data) {
        frame_buf_free(decoder_data);
        lv_free(decoder_data);
    }
}

static void lv_mpp_dec_close(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc)
{
    mpp_decoder_data_t *mpp_data =  (mpp_decoder_data_t *)dsc->decoded;
    LV_UNUSED(decoder);

    if (!mpp_data->cached) {
        mpp_dec_data_release(mpp_data);
    } else {
        lv_cache_release(dsc->cache, dsc->cache_entry, NULL);
    }

    return;
}

static void mpp_dec_cache_free_cb(void *node, void *user_data)
{
    lv_image_cache_data_t *entry = (lv_image_cache_data_t *)node;
    mpp_cache_t *mpp_cache = (mpp_cache_t *)entry->decoder->user_data;

    mpp_dec_data_release((mpp_decoder_data_t *)entry->user_data);
    lv_mutex_lock(&mpp_cache->lock);
    mpp_cache->cache_num--;
    lv_mutex_unlock(&mpp_cache->lock);

    // Free the duplicated file name
    if (entry->src_type == LV_IMAGE_SRC_FILE)
        lv_free((void *)entry->src);
}

static lv_image_decoder_t *mpp_dec = NULL;
void lv_mpp_dec_init(void)
{
    lv_image_decoder_t *dec = lv_image_decoder_create();
    mpp_cache_t *mpp_cache = (mpp_cache_t *)lv_malloc_zeroed(sizeof(mpp_cache_t));

    if (mpp_cache == NULL)
        return;

    mpp_cache->max_num = LV_CACHE_IMG_NUM;
    lv_mutex_init(&mpp_cache->lock);
    dec->user_data = mpp_cache;
    lv_image_decoder_set_info_cb(dec, lv_mpp_dec_info);
    lv_image_decoder_set_open_cb(dec, lv_mpp_dec_open);
    lv_image_decoder_set_close_cb(dec, lv_mpp_dec_close);

     // use custom cache free method
    lv_image_decoder_set_cache_free_cb(dec, mpp_dec_cache_free_cb);
    mpp_dec = dec;
}

void lv_mpp_dec_deinit(void)
{
    lv_image_decoder_t * dec = NULL;
    while ((dec = lv_image_decoder_get_next(dec)) != NULL) {
        if (dec->info_cb == lv_mpp_dec_info) {
            mpp_cache_t *mpp_cache = (mpp_cache_t *)dec->user_data;
            lv_mutex_delete(&mpp_cache->lock);
            lv_free(dec->user_data);
            lv_image_decoder_delete(dec);
            break;
        }
    }
}

#if LV_CACHE_DEF_SIZE > 0
bool lv_drop_one_cached_image()
{
    #define img_cache_p (LV_GLOBAL_DEFAULT()->img_cache)
    #define lv_initialized  (LV_GLOBAL_DEFAULT()->inited)
    if (lv_initialized)
        return lv_cache_evict_one(img_cache_p, NULL);
    else
        return false;
}
#endif

void lv_img_cache_set_size(uint16_t max_num)
{
    lv_image_cache_drop(NULL);
    if (mpp_dec) {
        mpp_cache_t *mpp_cache = (mpp_cache_t *)mpp_dec->user_data;
        mpp_cache->max_num = max_num;
    }
}
