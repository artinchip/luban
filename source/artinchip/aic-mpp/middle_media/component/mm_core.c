/*
 * Copyright (C) 2020-2023 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media core
 */

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include "mpp_log.h"
#include "mm_core.h"
#include "mm_demuxer_component.h"
#include "mm_vdec_component.h"
#include "mm_video_render_component.h"
#include "mm_adec_component.h"
#include "mm_audio_render_component.h"
#include "mm_clock_component.h"
#include "mm_muxer_component.h"
#include "mm_venc_component.h"

mm_component_register g_cmponent_registered[] = {
    {MM_COMPONENT_DEMUXER_NAME, mm_demuxer_component_init},
    {MM_COMPONENT_VDEC_NAME, mm_vdec_component_init},
    {MM_COMPONENT_VIDEO_RENDER_NAME, mm_video_render_component_init},
    {MM_COMPONENT_ADEC_NAME, mm_adec_component_init},
    {MM_COMPONENT_AUDIO_RENDER_NAME, mm_audio_render_component_init},
    {MM_COMPONENT_CLOCK_NAME, mm_clock_component_init},
    {MM_COMPONENT_MUXER_NAME, mm_muxer_component_init},
    {MM_COMPONENT_VENC_NAME, mm_venc_component_init}
};

s32 mm_init(void)
{
    s32 ret = MM_ERROR_NONE;

    return ret;
}

s32 mm_deinit(void)
{
    s32 ret = MM_ERROR_NONE;

    return ret;
}

s32 mm_get_handle(mm_handle *p_handle, char *component_name, void *p_app_data,
                  mm_callback *p_cb)
{
    s32 error = MM_ERROR_NONE;
    MM_BOOL b_find = MM_FALSE;

    s32 i, index;
    s32 comp_num =
        sizeof(g_cmponent_registered) / sizeof(mm_component_register);

    for (i = 0; i < comp_num; i++) {
        if (!strcmp(component_name, g_cmponent_registered[i].p_name)) {
            b_find = MM_TRUE;
            index = i;
            break;
        }
    }

    if (b_find == MM_TRUE) {
        *p_handle = (mm_handle)malloc(sizeof(mm_component));
        error = g_cmponent_registered[index].p_init(*p_handle);
        if (error == MM_ERROR_NONE) {
            ((mm_component *)(*p_handle))
                ->set_callback(*p_handle, p_cb, p_app_data);
            logd("get handle ok,component name:%s\n",
                 g_cmponent_registered[index].p_name);
        } else {
            loge("find compoent but init fail:0x%x!!\n", error);
            free(*p_handle);
            *p_handle = NULL;
        }
    } else {
        error = MM_ERROR_COMPONENT_NOT_FOUND;
    }

    return error;
}

s32 mm_free_handle(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    error = ((mm_component *)h_component)->deinit(h_component);
    free(h_component);
    return error;
}

s32 mm_set_bind(mm_handle h_out, u32 out_port, mm_handle h_in, u32 in_port)
{
    s32 ret = MM_ERROR_NONE;
    mm_component *comp_in;
    mm_component *comp_out;

    comp_in = (mm_component *)h_in;
    comp_out = (mm_component *)h_out;

    if (NULL != h_out && NULL != h_in) { // setup
        //step1 output
        ret = comp_out->bind_request(h_out, out_port, h_in, in_port);
        //step2 input
        if (MM_ERROR_NONE == ret) {
            ret = comp_in->bind_request(h_in, in_port, h_out, out_port);
            if (MM_ERROR_NONE != ret) {
                logd("unable to setup bind on input component.\n");
                comp_out->bind_request(h_out, out_port, NULL, 0);
            }
        } else {
            logd("unable to setup bind on output component.\n");
        }
    } else if (NULL == h_out && NULL != h_in) { //cancel input
        comp_in->bind_request(h_in, in_port, NULL, 0);
    } else if (NULL == h_in && NULL != h_out) { //cancel output
        comp_out->bind_request(h_out, out_port, NULL, 0);
    } else {
        return MM_ERROR_BAD_PARAMETER;
    }

    return ret;
}
