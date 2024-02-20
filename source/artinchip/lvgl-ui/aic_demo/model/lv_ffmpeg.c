#if LV_USE_FFMPEG == 0

#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "lv_ffmpeg.h"
#include "aic_player.h"

#define MY_CLASS &lv_aic_player_class

struct ffmpeg_context_s {
    struct aic_player *aic_player;
    struct av_media_info media_info;
    int play_end;
};

#define FRAME_DEF_REFR_PERIOD   33  /*[ms]*/

static void lv_ffmpeg_player_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_ffmpeg_player_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);

static int aic_event_handle(void *app_data, int event, int data1, int data2)
{
    int ret = 0;
    struct ffmpeg_context_s *ffmpeg_ctx = (struct ffmpeg_context_s *)app_data;

    switch(event) {
        case AIC_PLAYER_EVENT_PLAY_END:
            ffmpeg_ctx->play_end = 1;
            LV_LOG_INFO("aic play end\n");
            break;
        case AIC_PLAYER_EVENT_PLAY_TIME:
            break;
        case AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED:
            break;
        case AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED:
            break;
        default:
            break;
    }
    return ret;
}

const lv_obj_class_t lv_aic_player_class = {
    .constructor_cb = lv_ffmpeg_player_constructor,
    .destructor_cb = lv_ffmpeg_player_destructor,
    .instance_size = sizeof(lv_ffmpeg_player_t),
    .base_class = &lv_img_class
};

void lv_ffmpeg_init(void)
{
    return;
}

int lv_ffmpeg_get_frame_num(const char * path)
{
    int ret = -1;

    return ret;
}

lv_obj_t * lv_ffmpeg_player_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

lv_res_t lv_ffmpeg_player_set_src(lv_obj_t * obj, const char *path)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_res_t res = LV_RES_INV;
    int aic_ret = 0;
    char fake_image_name[128] = {0};
    int width = 0;
    int height = 0;
    struct ffmpeg_context_s *ffmpeg_ctx;
    struct mpp_rect disp_rect;

    lv_ffmpeg_player_t *player = (lv_ffmpeg_player_t *)obj;

    if (!player->ffmpeg_ctx) {
        player->ffmpeg_ctx = calloc(1, sizeof(struct ffmpeg_context_s));
        if(player->ffmpeg_ctx == NULL) {
            LV_LOG_ERROR("ffmpeg_ctx malloc failed");
            goto failed;
        }
    }

    ffmpeg_ctx = player->ffmpeg_ctx;
    if (!ffmpeg_ctx->aic_player) {
        ffmpeg_ctx->aic_player = aic_player_create((char *)path);
        if (!ffmpeg_ctx->aic_player) {
            LV_LOG_ERROR("create aic player failed: %s", path);
            goto failed;
        }
        aic_player_set_event_callback(ffmpeg_ctx->aic_player, ffmpeg_ctx, aic_event_handle);
    }

    lv_timer_pause(player->timer);
    aic_player_set_uri(ffmpeg_ctx->aic_player, (char *)path);
    aic_ret = aic_player_prepare_sync(ffmpeg_ctx->aic_player);
    if (aic_ret) {
        LV_LOG_ERROR("aic_player_prepare failed");
        goto failed;
    }

    aic_ret = aic_player_get_media_info(ffmpeg_ctx->aic_player,
                                        &ffmpeg_ctx->media_info);
    if (aic_ret != 0) {
        LV_LOG_ERROR("aic_player_get_media_info failed");
        goto failed;
    }

    player->imgdsc.header.w = ffmpeg_ctx->media_info.video_stream.width;
    player->imgdsc.header.h = ffmpeg_ctx->media_info.video_stream.height;
    width = lv_obj_get_width(&player->img.obj);
    height = lv_obj_get_height(&player->img.obj);
    if (width == 0 && height == 0) {
        width = player->imgdsc.header.w;
        height = player->imgdsc.header.h;
    }

    snprintf(fake_image_name, 128, "L:/%dx%d_%d_%08x.fake",
             width, height, 0, 0x00000000);
    lv_img_set_src(&player->img.obj, fake_image_name);

    lv_area_t coords;
    lv_obj_get_coords(&player->img.obj, &coords);

    if (coords.x1 >= 0 && coords.y1 >= 0) {
        disp_rect.x = coords.x1;
        disp_rect.y = coords.y1;
    } else {
        disp_rect.x = 0;
        disp_rect.y = 0;
        LV_LOG_WARN("disp x: %x, y: %d", disp_rect.x, disp_rect.y);
    }

    disp_rect.width = width;
    disp_rect.height = height;
    if (player->imgdsc.header.w != 0 && player->imgdsc.header.h != 0) {
        int ret = aic_player_set_disp_rect(ffmpeg_ctx->aic_player, &disp_rect);
        if (ret != 0) {
            LV_LOG_ERROR("aic_player_set_disp_rect failed");
        }
    }
    LV_LOG_INFO("coords:%d, %d, %d, %d", coords.x1, coords.y1, coords.x2, coords.y2);
    LV_LOG_INFO("image:w:%d,h:%d", width, height);
    lv_timer_resume(player->timer);
    res = LV_RES_OK;

failed:
    return res;
}

void lv_ffmpeg_player_set_cmd(lv_obj_t * obj, lv_ffmpeg_player_cmd_t cmd)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;
    struct ffmpeg_context_s *ffmpeg_ctx;
    int ret;

    if(!player->ffmpeg_ctx) {
        LV_LOG_ERROR("ffmpeg_ctx is NULL");
        return;
    }

    ffmpeg_ctx = player->ffmpeg_ctx;
    if (!ffmpeg_ctx->aic_player) {
        LV_LOG_ERROR("aic_player is NULL ");
        return;
    }

    switch(cmd) {
        case LV_FFMPEG_PLAYER_CMD_START:
            aic_player_start(ffmpeg_ctx->aic_player);
            ret = aic_player_play(ffmpeg_ctx->aic_player);
            if (ret != 0) {
                LV_LOG_ERROR("aic_player_play failed");
                break;
            }
            LV_LOG_INFO("aic player start");
            break;
        case LV_FFMPEG_PLAYER_CMD_STOP:
            ret = aic_player_stop(ffmpeg_ctx->aic_player);
            if (ret != 0) {
                LV_LOG_ERROR("aic_player_stop failed");
                break;
            }
            LV_LOG_INFO("aic player stop");
            break;
        case LV_FFMPEG_PLAYER_CMD_PAUSE:
            ret = aic_player_pause(ffmpeg_ctx->aic_player);
            if (ret != 0) {
                LV_LOG_ERROR("aic_player_pause failed");
                break;
            }
            LV_LOG_INFO("aic player pause");
            break;
        case LV_FFMPEG_PLAYER_CMD_RESUME:
            ret = aic_player_start(ffmpeg_ctx->aic_player);
            if (ret != 0) {
                LV_LOG_ERROR("aic_player_start failed");
                break;
            }
            ret = aic_player_play(ffmpeg_ctx->aic_player);
            if (ret != 0) {
                LV_LOG_ERROR("aic_player_play failed");
                break;
            }
            LV_LOG_INFO("aic player resume");
            break;
        default:
            LV_LOG_ERROR("Error cmd: %d", cmd);
            break;
    }
}

void lv_ffmpeg_player_set_auto_restart(lv_obj_t * obj, bool en)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;
    player->auto_restart = en;
}

static void lv_ffmpeg_player_frame_update_cb(lv_timer_t * timer)
{
    lv_obj_t * obj = (lv_obj_t *)timer->user_data;
    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;
    char fake_image_name[128] = {0};
    int width;
    int height;
    static int last_width = 0;
    static int last_height = 0;

    struct mpp_rect disp_rect;
    struct ffmpeg_context_s *ffmpeg_ctx;

    ffmpeg_ctx = player->ffmpeg_ctx;
    if(!ffmpeg_ctx) {
        return;
    }

    if (!ffmpeg_ctx->aic_player) {
        return;
    }

    /* player->imgdsc.header.w == 0 && player->imgdsc.header.h == 0, it's audio */
    if (player->imgdsc.header.w != 0 && player->imgdsc.header.h != 0) {
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        width = lv_obj_get_width(obj);
        height = lv_obj_get_height(obj);

        LV_LOG_INFO("cb coords:%d, %d, %d, %d", coords.x1, coords.y1, coords.x2, coords.y2);
        LV_LOG_INFO("cb image:w:%d,h:%d", width, height);

        if (coords.x1 >= 0 && coords.y1 >= 0) {
            disp_rect.x = coords.x1;
            disp_rect.y = coords.y1;
        } else {
            disp_rect.x = 0;
            disp_rect.y = 0;
            LV_LOG_WARN("disp x: %x, y: %d", disp_rect.x, disp_rect.y);
        }

        if (last_width != width || last_height != height) {
            if (width == 0 && height == 0) {
                width = player->imgdsc.header.w;
                height = player->imgdsc.header.h;
            }
            snprintf(fake_image_name, 128, "L:/%dx%d_%d_%08x.fake",
                     width, height, 0, 0x00000000);
            lv_img_set_src(&player->img.obj, fake_image_name);

            last_width = width;
            last_height = height;
        }

        disp_rect.width = width;
        disp_rect.height = height;
        int ret = aic_player_set_disp_rect(ffmpeg_ctx->aic_player, &disp_rect);
        if (ret != 0) {
            LV_LOG_ERROR("aic_player_set_disp_rect failed");
        }
    }

    if (player->auto_restart && ffmpeg_ctx->play_end) {
        ffmpeg_ctx->play_end = 0;
        aic_player_seek(ffmpeg_ctx->aic_player, 0);
        LV_LOG_INFO("auto_restart");
    }
    //lv_img_cache_invalidate_src(lv_img_get_src(obj));
    //lv_obj_invalidate(obj);
}

static void lv_ffmpeg_player_constructor(const lv_obj_class_t * class_p,
                                         lv_obj_t * obj)
{
    LV_TRACE_OBJ_CREATE("begin");

    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;

    player->auto_restart = false;
    player->ffmpeg_ctx = NULL;
    player->timer = lv_timer_create(lv_ffmpeg_player_frame_update_cb,
                                    FRAME_DEF_REFR_PERIOD, obj);
    lv_timer_pause(player->timer);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_ffmpeg_player_destructor(const lv_obj_class_t * class_p,
                                        lv_obj_t * obj)
{
    LV_TRACE_OBJ_CREATE("begin");

    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;

    if(player->timer) {
        lv_timer_del(player->timer);
        player->timer = NULL;
    }

    if(player->ffmpeg_ctx) {
        struct ffmpeg_context_s *ffmpeg_ctx;
        ffmpeg_ctx = player->ffmpeg_ctx;
        if (ffmpeg_ctx->aic_player) {
            aic_player_destroy(ffmpeg_ctx->aic_player);
            ffmpeg_ctx->aic_player = NULL;
        }
        free(player->ffmpeg_ctx);
        player->ffmpeg_ctx = NULL;
    }

    LV_TRACE_OBJ_CREATE("finished");
}

#endif /*NO LV_USE_FFMPEG*/
