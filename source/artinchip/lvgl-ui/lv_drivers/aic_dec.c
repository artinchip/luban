/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include "lvgl/lvgl.h"
#include "mpp_ge.h"
#include "mpp_decoder.h"
#include "frame_allocator.h"
#include "dma_allocator.h"
#include "aic_dec.h"
#include "aic_ui.h"

#define PNG_HEADER_SIZE (8 + 12 + 13) //png signature + IHDR chuck
#define PNGSIG 0x89504e470d0a1a0aull
#define MNGSIG 0x8a4d4e470d0a1a0aull
#define JPEG_SOI 0xFFD8
#define JPEG_SOF 0xFFC0

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

static int get_jpeg_format(uint8_t *buf, enum mpp_pixel_format *pix_fmt)
{
    int i;
    uint8_t h_count[3] = { 0 };
    uint8_t v_count[3]= { 0 };
    uint8_t nb_components = *buf++;

    for (i = 0; i < nb_components; i++) {
        uint8_t h_v_cnt;

        /* skip component id */
        buf++;
        h_v_cnt = *buf++;
        h_count[i] = h_v_cnt >> 4;
        v_count[i] = h_v_cnt & 0xf;

        /*skip quant_index*/
        buf++;
    }

    if (h_count[0] == 2 && v_count[0] == 2 && h_count[1] == 1 &&
        v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        *pix_fmt = MPP_FMT_YUV420P;
    } else if (h_count[0] == 4 && v_count[0] == 1 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        return -1;
    } else if (h_count[0] == 2 && v_count[0] == 1 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        *pix_fmt = MPP_FMT_YUV422P;
    } else if (h_count[0] == 1 && v_count[0] == 1 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count[0] == 1 && v_count[0] == 2 && h_count[1] == 1 &&
               v_count[1] == 2 && h_count[2] == 1 && v_count[2] == 2) {
        *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count[0] == 1 && v_count[0] == 2 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        return -1;
    } else if (h_count[1] == 0 && v_count[1] == 0 && h_count[2] == 0 &&
               v_count[2] == 0) {
        *pix_fmt = MPP_FMT_YUV400;
    } else {
        printf("Not support format! h_count: %d %d %d, v_count: %d %d %d\n",
            h_count[0], h_count[1], h_count[2],
            v_count[0], v_count[1], v_count[2]);
        return -1;
    }

    return 0;
}

static lv_fs_res_t jpeg_get_img_size(lv_fs_file_t *fp, int *w, int *h, enum mpp_pixel_format *pix_fmt)
{
    uint32_t read_num;
    uint8_t buf[128];
    lv_fs_res_t res = LV_RES_OK;

    // read JPEG SOI
    res = lv_fs_read(fp, buf, 2, &read_num);
    if (res != LV_FS_RES_OK || read_num != 2) {
        res = LV_RES_INV;
        goto read_err;
    }

    /* check SOI */
    if (stream_to_u16(buf) != JPEG_SOI) {
        res = LV_RES_INV;
        goto read_err;
    }

    /* find SOF */
    while (1) {
        int size;
        res = lv_fs_read(fp, buf, 4, &read_num);
        if (res != LV_FS_RES_OK || read_num != 4) {
            res = LV_RES_INV;
            goto read_err;
        }

        if (stream_to_u16(buf) == JPEG_SOF) {
            res = lv_fs_read(fp, buf, 15, &read_num);
            if (res != LV_FS_RES_OK) {
                res = LV_RES_INV;
                goto read_err;
            }

            *h = stream_to_u16(buf + 1);
            *w = stream_to_u16(buf + 3);

            get_jpeg_format(buf + 5, pix_fmt);
            break;
        } else {
            size = stream_to_u16(buf + 2);
            lv_fs_seek(fp, size - 2, SEEK_CUR);
            if (res != LV_FS_RES_OK) {
                res = LV_RES_INV;
                goto read_err;
            }
        }
    }

read_err:
    return res;
}

static lv_res_t jpeg_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    lv_fs_file_t f;
    lv_fs_res_t res;
    int width;
    int height;
    enum mpp_pixel_format fomat;

    res = lv_fs_open(&f, src, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK)
        return LV_RES_INV;

    res = jpeg_get_img_size(&f, &width, &height, &fomat);
    if (res != LV_FS_RES_OK)
        return LV_RES_INV;

    header->w = width;
    header->h = height;
    header->cf = LV_IMG_CF_TRUE_COLOR;
    lv_fs_close(&f);

    return LV_RES_OK;
}

static lv_fs_res_t png_get_img_size(lv_fs_file_t *fp, int *w, int *h, enum mpp_pixel_format *fomat)
{
    uint32_t read_num;
    unsigned char buf[64];
    int color_type;

    lv_fs_read(fp, buf, PNG_HEADER_SIZE, &read_num);

    *w = stream_to_u32(buf + 8 + 8);
    *h = stream_to_u32(buf + 8 + 8 + 4);

    color_type = buf[8 + 8 + 8 + 1];
    if (color_type == 2)
        *fomat = MPP_FMT_XRGB_8888;
    else
        *fomat = MPP_FMT_ARGB_8888;

    return LV_RES_OK;
}

static lv_fs_res_t get_file_size(lv_fs_file_t *fp, unsigned int *file_size)
{
    lv_fs_seek(fp, 0, SEEK_END);
    lv_fs_tell(fp, file_size);
    lv_fs_seek(fp, 0, SEEK_SET);

    return LV_RES_OK;
}

static lv_res_t fake_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    int width;
    int height;
    int blend;
    unsigned int color;
    int ret;

    FAKE_IMAGE_PARSE(src, ret, width, height, blend, color);
    header->w = width;
    header->h = height;
    header->cf = LV_IMG_CF_TRUE_COLOR;
    ret = LV_RES_OK;

    return ret;
}

static lv_res_t png_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    lv_fs_file_t f;
    lv_fs_res_t res;
    uint32_t read_num;
    uint8_t buf[64];

    res = lv_fs_open(&f, src, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK)
        return LV_RES_INV;

    // read png sig + IHDR chuck
    res = lv_fs_read(&f, buf, PNG_HEADER_SIZE, &read_num);
    if (res != LV_FS_RES_OK || read_num != PNG_HEADER_SIZE) {
        lv_fs_close(&f);
        return LV_RES_INV;
    }

    /* check signature */
    uint64_t sig = stream_to_u64(buf);
    if (sig != PNGSIG && sig != MNGSIG) {
        LV_LOG_WARN("Invalid PNG signature 0x%08llx.", (unsigned long long)sig);
        lv_fs_close(&f);
        return LV_RES_INV;
    }

    header->w = stream_to_u32(buf + 8 + 8);
    header->h = stream_to_u32(buf + 8 + 8 + 4);
    header->cf = LV_IMG_CF_RAW;

    lv_fs_close(&f);
    return LV_RES_OK;
}

static lv_res_t aic_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    char* ptr = NULL;

    if (lv_img_src_get_type(src) != LV_IMG_SRC_FILE) {
        return LV_RES_INV;
    }

    ptr = strrchr(src, '.');
    if (!strcmp(ptr, ".png")) {
        return png_decoder_info(decoder, src, header);
    } else if ((!strcmp(ptr, ".jpg")) || (!strcmp(ptr, ".jpeg"))) {
        return jpeg_decoder_info(decoder, src, header);
    } else if (!strcmp(ptr, ".fake")) {
        return fake_decoder_info(decoder, src, header);
    } else {
        return LV_RES_INV;
    }

    return LV_RES_OK;
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

static lv_res_t aic_decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    int ret;
    lv_fs_file_t image_file;
    struct mpp_packet packet;
    struct mpp_frame frame;
    int width, height;
    int dma_fd;
    struct mpp_frame *alloc_frame;
    struct mpp_decoder *dec;
    struct frame_allocator *allocator;
    uint32_t file_len = 0;
    uint32_t read_size = 0;
    enum mpp_codec_type type = MPP_CODEC_VIDEO_DECODER_PNG;
    char *ptr = NULL;
    struct decode_config config = { 0 };

    ptr = strrchr(dsc->src, '.');
    if ((!strcmp(ptr, ".jpg")) || (!strcmp(ptr, ".jpeg")))
       type = MPP_CODEC_VIDEO_DECODER_MJPEG;

    lv_fs_res_t res = lv_fs_open(&image_file, dsc->src, LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK)
        goto err_fs_open;

    if (type == MPP_CODEC_VIDEO_DECODER_PNG) {
        png_get_img_size(&image_file, &width, &height, &config.pix_fmt);
        if (config.pix_fmt == MPP_FMT_ARGB_8888)
            dsc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        else
            dsc->header.cf = LV_IMG_CF_TRUE_COLOR;

        config.pix_fmt = MPP_FMT_ARGB_8888;
    } else {
        dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
        jpeg_get_img_size(&image_file, &width, &height, &config.pix_fmt);
        printf("jpeg w: %d, h:%d format:%d\n", width, height, config.pix_fmt);
    }
    get_file_size(&image_file, &file_len);
    dec = mpp_decoder_create(type);
    if (!dec) {
        LV_LOG_ERROR("mpp_decoder_create failed\n");
        goto err_dec_create;
    }

    config.bitstream_buffer_size = (file_len + 1023) & (~1023);
    config.extra_frame_num = 0;
    config.packet_count = 1;
    dma_fd = dmabuf_device_open();
    if (dma_fd < 0) {
        LV_LOG_ERROR("dmabuf_device_open failed:%d\n", dma_fd);
        goto err_dev_open;
    }

    alloc_frame = (struct mpp_frame *)malloc(sizeof(struct mpp_frame));
    if (!alloc_frame) {
        LV_LOG_ERROR("malloc frame failed\n");
        goto err_alloc_frame;
    }

    alloc_frame->id = 0;
    alloc_frame->buf.size.width = width;
    alloc_frame->buf.size.height = height;
    alloc_frame->buf.format = config.pix_fmt;
    if (type == MPP_CODEC_VIDEO_DECODER_PNG) {
        alloc_frame->buf.stride[0] =  (width * 4 + 15) & (~15);
    } else {
        alloc_frame->buf.size.height = (height + 15) & (~15);
        if (config.pix_fmt == MPP_FMT_YUV420P || config.pix_fmt == MPP_FMT_YUV422P) {
            alloc_frame->buf.stride[0] =  (width + 15) & (~15);
            alloc_frame->buf.stride[1] =  alloc_frame->buf.stride[0] >> 1;
            alloc_frame->buf.stride[2] =  alloc_frame->buf.stride[0] >> 1;
        } else if (config.pix_fmt == MPP_FMT_YUV444P) {
            alloc_frame->buf.stride[0] =  (width + 15) & (~15);
            alloc_frame->buf.stride[1] =  alloc_frame->buf.stride[0];
            alloc_frame->buf.stride[2] =  alloc_frame->buf.stride[0];
        }
    }

    if (mpp_buf_alloc(dma_fd, &alloc_frame->buf) < 0) {
        LV_LOG_ERROR("mpp_buf_alloc failed\n");
        goto err_buf_alloc;
    }

    /* allocator will be free inside decoder */
    allocator = open_allocator(alloc_frame);
    if (!allocator) {
        LV_LOG_ERROR("open_allocator failed\n");
        goto err_allocator;
    }

    mpp_decoder_control(dec, MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, (void*)allocator);
    mpp_decoder_init(dec, &config);
    memset(&packet, 0, sizeof(struct mpp_packet));
    mpp_decoder_get_packet(dec, &packet, file_len);

    lv_fs_read(&image_file, packet.data, file_len, &read_size);
    packet.size = file_len;
    packet.flag = PACKET_FLAG_EOS;

    mpp_decoder_put_packet(dec, &packet);
    ret = mpp_decoder_decode(dec);
    if(ret < 0) {
        LV_LOG_ERROR("mpp dec failed\n");
        goto err_dec;
    }

    memset(&frame, 0, sizeof(struct mpp_frame));
    mpp_decoder_get_frame(dec, &frame);
    mpp_decoder_put_frame(dec, &frame);
    mpp_decoder_destory(dec);
    dmabuf_device_close(dma_fd);
    lv_fs_close(&image_file);

    if (type == MPP_CODEC_VIDEO_DECODER_PNG)
        dsc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    else
        dsc->header.cf = LV_IMG_CF_TRUE_COLOR;

    dsc->img_data = (unsigned char *)alloc_frame;

    return LV_RES_OK;

err_dec:
err_allocator:
    mpp_buf_free(&alloc_frame->buf);
err_buf_alloc:
    free(alloc_frame);
err_alloc_frame:
    dmabuf_device_close(dma_fd);
err_dev_open:
    mpp_decoder_destory(dec);
err_dec_create:
    lv_fs_close(&image_file);
err_fs_open:
    dsc->img_data = NULL;
    return LV_RES_INV;
}

static void aic_decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    if (dsc->img_data) {
        struct mpp_frame *alloc_frame = (struct mpp_frame *)dsc->img_data;
        mpp_buf_free(&alloc_frame->buf);
        free(alloc_frame);
        dsc->img_data = NULL;
    }

    return;
}

void aic_dec_create()
{
    lv_img_decoder_t *aic_dec = lv_img_decoder_create();

    lv_img_decoder_set_info_cb(aic_dec, aic_decoder_info);
    lv_img_decoder_set_open_cb(aic_dec, aic_decoder_open);
    lv_img_decoder_set_close_cb(aic_dec, aic_decoder_close);
}
