/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <che.jiang@artinchip.com>
 *  Desc: middle media core desc
 */

#ifndef MM_CORE_h
#define MM_CORE_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <mm_component.h>

#define MM_MAX_STRINGNAME_SIZE 128

#define MM_COMPONENT_DEMUXER_NAME                 "MM.AIC.DEMUXER.ALL"
#define MM_COMPONENT_VDEC_NAME                    "MM.AIC.VDEC.ALL"
#define MM_COMPONENT_VIDEO_RENDER_NAME            "MM.AIC.VIDEORENDER.ALL"
#define MM_COMPONENT_ADEC_NAME                    "MM.AIC.ADEC.ALL"
#define MM_COMPONENT_AUDIO_RENDER_NAME            "MM.AIC.AUDIORENDER.ALL"
#define MM_COMPONENT_CLOCK_NAME                   "MM.AIC.CLOCK.ALL"
#define MM_COMPONENT_MUXER_NAME                   "MM.AIC.MUXER.ALL"
#define MM_COMPONENT_VENC_NAME                    "MM.AIC.VENC.ALL"

#define MM_COMPONENT_DEMUXER_THREAD_PRIORITY      (23)
#define MM_COMPONENT_VDEC_THREAD_PRIORITY         (22)
#define MM_COMPONENT_ADEC_THREAD_PRIORITY         (22)
#define MM_COMPONENT_VIDEO_RENDER_THREAD_PRIORITY (21)
#define MM_COMPONENT_AUDIO_RENDER_THREAD_PRIORITY (22)

#define DEMUX_PORT_AUDIO_INDEX 0
#define DEMUX_PORT_VIDEO_INDEX 1
#define DEMUX_PORT_CLOCK_INDEX 2

#define MUX_PORT_AUDIO_INDEX 0
#define MUX_PORT_VIDEO_INDEX 1
#define MUX_PORT_CLOCK_INDEX 2

#define VDEC_PORT_IN_INDEX  0
#define VDEC_PORT_OUT_INDEX 1

#define VENC_PORT_IN_INDEX  0
#define VENC_PORT_OUT_INDEX 1

#define VIDEO_RENDER_PORT_IN_VIDEO_INDEX 0
#define VIDEO_RENDER_PORT_IN_CLOCK_INDEX 1

#define ADEC_PORT_IN_INDEX  0
#define ADEC_PORT_OUT_INDEX 1

#define AUDIO_RENDER_PORT_IN_AUDIO_INDEX 0
#define AUDIO_RENDER_PORT_IN_CLOCK_INDEX 1

#define CLOCK_PORT_OUT_VIDEO 0
#define CLOCK_PORT_OUT_AUDIO 1

typedef enum MM_ERROR_TYPE {
    MM_ERROR_NONE = 0,
    MM_ERROR_UNDEFINED = 1,
    MM_ERROR_UNSUPPORT,
    MM_ERROR_NULL_POINTER,
    MM_ERROR_EMPTY_DATA,
    MM_ERROR_READ_FAILED,
    MM_ERROR_SAME_STATE,
    MM_ERROR_INVALID_STATE,
    MM_ERROR_BAD_PARAMETER,
    MM_ERROR_MB_ERRORS_IN_FRAME,
    MM_ERROR_FORMAT_NOT_DETECTED,
    MM_ERROR_COMPONENT_NOT_FOUND,
    MM_ERROR_PORT_NOT_COMPATIBLE,
    MM_ERROR_INSUFFICIENT_RESOURCES,
    MM_ERROR_INCORRECT_STATE_TRANSITION,
} MM_ERROR_TYPE;

typedef enum MM_EVENT_TYPE {
    MM_EVENT_CMD_COMPLETE, /**< component has sucessfully completed a command */
    MM_EVENT_ERROR,        /**< component has detected an error condition */
    MM_EVENT_BUFFER_FLAG,  /**< component has detected an EOS */
    MM_EVENT_PORT_FORMAT_DETECTED, /**< Component has detected a supported format. */

    MM_EVENT_VIDEO_RENDER_PTS,
    MM_EVENT_VIDEO_RENDER_FIRST_FRAME,

    MM_EVENT_AUDIO_RENDER_PTS,
    MM_EVENT_AUDIO_RENDER_FIRST_FRAME,

    MM_EVENT_MUXER_NEED_NEXT_FILE,
    MM_EVENT_MAX = 0x7FFFFFFF
} MM_EVENT_TYPE;

typedef s32 (*mm_component_init)(mm_handle h_component);

typedef struct mm_component_register {
    const char *p_name;
    mm_component_init p_init;
} mm_component_register;

typedef struct mm_bind_info {
    u32 flag;
    u32 port_index; /** port index. */
    mm_handle p_self_comp;
    u32 bind_port_index;   /** bind port index. */
    mm_handle p_bind_comp; /** bind component handle. */
} mm_bind_info;

typedef struct mm_port_param {
    u32 size;           /**< size of the structure in bytes */
    u32 ports;          /**< The number of ports for this component */
    u32 start_port_num; /** first port number for this type of port */
} mm_port_param;

#define mm_send_command(h_component, cmd, param, p_cmd_data) \
    ((mm_component *)h_component)                            \
        ->send_command(h_component, cmd, param, p_cmd_data) /* Macro End */

#define mm_get_parameter(h_component, index, p_param) \
    ((mm_component *)h_component)                     \
        ->get_parameter(h_component, index, p_param) /* Macro End */

#define mm_set_parameter(h_component, index, p_param) \
    ((mm_component *)h_component)                     \
        ->set_parameter(h_component, index, p_param) /* Macro End */

#define mm_get_config(h_component, index, p_config) \
    ((mm_component *)h_component)                   \
        ->get_config(h_component, index, p_config) /* Macro End */

#define mm_set_config(h_component, index, p_config) \
    ((mm_component *)h_component)                   \
        ->set_config(h_component, index, p_config) /* Macro End */

#define mm_get_state(h_component, p_state) \
    ((mm_component *)h_component)          \
        ->get_state(h_component, p_state) /* Macro End */

#define mm_send_buffer(h_component, p_buffer) \
    ((mm_component *)h_component)          \
        ->send_buffer(h_component, p_buffer) /* Macro End */

#define mm_giveback_buffer(h_component, p_buffer) \
    ((mm_component *)h_component)          \
        ->giveback_buffer(h_component, p_buffer) /* Macro End */

s32 mm_init();

s32 mm_deinit();

s32 mm_get_handle(mm_handle *p_handle, char *component_name, void *p_app_data,
                  mm_callback *p_cb);

s32 mm_free_handle(mm_handle h_component);

s32 mm_set_bind(mm_handle h_out, u32 out_port, mm_handle h_in, u32 in_port);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/* File EOF */
