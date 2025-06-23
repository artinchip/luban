/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <che.jiang@artinchip.com>
 *  Desc: middle media component desc
 */

#ifndef MM_COMPONENT_H
#define MM_COMPONENT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "mpp_dec_type.h"
#include <inttypes.h>
#include <mm_index.h>

#define MM_CLOCK_PORT0 0x00000001
#define MM_CLOCK_PORT1 0x00000002
#define MM_CLOCK_PORT2 0x00000004

typedef void *mm_handle;

typedef enum MM_COMMAND_TYPE {
    MM_COMMAND_UNKNOWN,
    MM_COMMAND_STATE_SET, /* Change the component state */
    MM_COMMAND_FLUSH,     /* Flush the data queue(s) of a component */
    MM_COMMAND_STOP,
    MM_COMMAND_NOPS,
    MM_COMMAND_WKUP,
    MM_COMMAND_EOS,
} MM_COMMAND_TYPE;

typedef enum MM_STATE_TYPE {
    MM_STATE_INVALID,

    /* component has been loaded but has not completed initialization. */
    MM_STATE_LOADED,

    /* component initialization has been completed successfully
         and the component is ready to start. */
    MM_STATE_IDLE,

    /**< component has accepted the start command and
         is processing data (if data is available) */
    MM_STATE_EXECUTING,

    /**< component has received pause command */
    MM_STATE_PAUSE,
} MM_STATE_TYPE;

typedef enum MM_AUDIO_CODING_TYPE {
    MM_AUDIO_CODING_UNUSED = 0, /* Placeholder value when coding is N/A  */
    MM_AUDIO_CODING_AUTODETECT, /* auto detection of audio format */
    MM_AUDIO_CODING_PCM,        /* Any variant of PCM coding */
    MM_AUDIO_CODING_AAC,        /* Any variant of AAC encoded data */
    MM_AUDIO_CODING_MP3,        /* Any variant of MP3 encoded data */
    MM_AUDIO_CODING_MAX = 0x7FFFFFFF
} MM_AUDIO_CODING_TYPE;

typedef enum MM_VIDEO_CODING_TYPE {
    MM_VIDEO_CODING_UNUSED,     /* Value when coding is N/A */
    MM_VIDEO_CODING_AUTODETECT, /* Autodetection of coding type */
    MM_VIDEO_CODING_MPEG2,      /* AKA: H.262 */
    MM_VIDEO_CODING_H263,       /* H.263 */
    MM_VIDEO_CODING_MPEG4,      /* MPEG-4 */
    MM_VIDEO_CODING_AVC,        /* H.264/AVC */
    MM_VIDEO_CODING_MJPEG,      /* Motion JPEG */
    MM_VIDEO_CODING_MAX = 0x7FFFFFFF
} MM_VIDEO_CODING_TYPE;

typedef enum MM_COLOR_FORMAT_TYPE {
    MM_COLOR_FORMAT_UNUSED,
    MM_COLOR_FORMAT_YUV420P,
    MM_COLOR_FORMAT_NV12,
    MM_COLOR_FORMAT_NV21,
    MM_COLOR_FORMAT_RGB565,
    MM_COLOR_FORMAT_ARGB8888,
    MM_COLOR_FORMAT_RGB888,
    MM_COLOR_FORMAT_ARGB1555,
    MM_COLOR_FORMAT_MAX = 0x7FFFFFFF
} MM_COLOR_FORMAT_TYPE;

typedef enum MM_TIME_CLOCK_STATE {
    MM_TIME_CLOCK_STATE_RUNNING, /* Clock running. */

    /* Clock waiting until the prescribed clients emit their start time. */
    MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME,
    MM_TIME_CLOCK_STATE_STOPPED, /**< Clock stopped. */
    MM_TIME_CLOCK_STATE_MAX = 0x7FFFFFFF
} MM_TIME_CLOCK_STATE;

typedef enum MM_TIME_REF_CLOCK_TYPE {
    MM_TIME_REF_CLOCK_NONE, /* Use no references. */
    MM_TIME_REF_CLOCK_AUDIO,
    MM_TIME_REF_CLOCK_VIDEO,
    MM_TIME_REF_CLOCK_MAX = 0x7FFFFFFF
} MM_TIME_REF_CLOCK_TYPE;

typedef struct mm_param_content_uri {
    /* size of the structure in bytes, including actual URI name */
    u32 size;
    u8 content_uri[1]; /* The URI name */
} mm_param_content_uri;

typedef struct mm_param_u32 {
    u32 port_index; /* port that this structure applies to */
    u32 u32;        /* U32 value */
} mm_param_u32;

typedef struct mm_audio_param_port_format {
    u32 port_index; /* Indicates which port to set */
    u32 index;      /* Indicates the enumeration index for the format from 0x0 to N-1 */

    /* Type of data expected for this port (e.g. PCM, AMR, MP3, etc) */
    MM_AUDIO_CODING_TYPE encoding;
} mm_audio_param_port_format;

typedef struct ms_audio_port_def {
    /* Type of data expected for this port (e.g. PCM, AMR, MP3, etc) */
    MM_AUDIO_CODING_TYPE encoding;
    u32 channels;
    u32 bitrate;
    u32 sample_rate;
} ms_audio_port_def;

typedef struct mm_video_param_port_format {
    u32 port_index;
    u32 index;
    MM_VIDEO_CODING_TYPE compression_format;
    MM_COLOR_FORMAT_TYPE color_format;
    u32 framerate;
} mm_video_param_port_format;

typedef struct mm_image_param_qfactor {
    u32 port_index;
    u32 q_factor;
} mm_image_param_qfactor;

typedef struct ms_video_port_def {
    u32 frame_width;
    u32 frame_height;
    s32 stride;
    u32 slice_height;
    u32 bitrate;
    u32 framerate;
    MM_VIDEO_CODING_TYPE compression_format;
    MM_COLOR_FORMAT_TYPE color_format;
} ms_video_port_def;

typedef struct mm_param_port_def {
    u32 port_index; /* Port number the structure applies to */
    u32 dir;        /* Direction (input or output) of this port */
    MM_BOOL enable; /* Ports default to enabled and are enabled/disabled */
    union {
        ms_audio_port_def audio;
        ms_video_port_def video;
    } format;
} mm_param_port_def;

typedef struct mm_param_skip_track {
    u32 port_index;
} mm_param_skip_track;

typedef struct mm_param_screen_size {
    u32 port_index;
    s32 width;
    s32 height;
} mm_param_screen_size;

typedef struct mm_param_audio_volume {
    u32 port_index;
    s32 volume;
} mm_param_audio_volume;

typedef struct mm_param_frame_end {
    u32 port_index;      /* port that this structure applies to */
    MM_BOOL b_frame_end; /* 0-clear   1- set */
} mm_param_frame_end;

typedef struct mm_param_record_file_info {
    u32 port_index;
    s32 file_num;
    s64 duration;
    s32 muxer_type;
} mm_param_record_file_info;

typedef struct mm_param_video_capture {
    u32 port_index;
    s8 *p_file_path;
    s32 width;
    s32 height;
    s32 quality;
} mm_param_video_capture;

typedef struct mm_config_rect {
    u32 port_index;
    s32 left;
    s32 top;
    u32 width;
    u32 height;
} mm_config_rect;

typedef struct mm_config_rotation {
    u32 port_index;
    u32 rotation;
} mm_config_rotation;

typedef struct mm_time_config_timestamp {
    u32 port_index; /* port that this structure applies to */
    s64 timestamp;  /* timestamp .*/
} mm_time_config_timestamp;


typedef struct mm_time_config_clock_state {
    MM_TIME_CLOCK_STATE state; /* state of the media time. */
    s64 start_time;            /* start time of the media time. */

    /* Time to offset the media time by * (e.g. preroll). Media time will be
       * reported to be nOffset ticks earlier.*/
    s64 offset;
    u32 wait_mask; /* mask values. */
} mm_time_config_clock_state;

typedef struct mm_time_config_active_ref_clock {
    MM_TIME_REF_CLOCK_TYPE clock; /* Reference clock used to compute media time */
} mm_time_config_active_ref_clock;


typedef struct mm_buffer
{
    u8* p_buffer;
    u32 buffer_size;
    s64 time_stamp;
    u32 flags;
    u32 output_port_index;
    u32 input_port_index;
} mm_buffer;

typedef struct mm_callback {
    /* The event_handler method is used to notify the application when an
        event of interest occurs.*/
    s32 (*event_handler)(mm_handle h_component, void *p_app_data, u32 event,
                         u32 data1, u32 data2, void *p_event_data);

    s32 (*giveback_buffer)(mm_handle h_component, void* p_app_data,
                          mm_buffer* p_buffer);

} mm_callback;


typedef struct mm_component {
    /* p_comp_private is a pointer to the component private data area.
        The application should not access this data area. */
    void *p_comp_private;

    /* p_app_private is application private data*/
    void *p_app_private;

    s32 (*send_command)(mm_handle h_component, MM_COMMAND_TYPE cmd, u32 param,
                        void *p_cmd_data);

    s32 (*get_parameter)(mm_handle h_component, MM_INDEX_TYPE index,
                         void *p_param);

    s32 (*set_parameter)(mm_handle h_component, MM_INDEX_TYPE index,
                         void *p_param);

    s32 (*get_config)(mm_handle h_component, MM_INDEX_TYPE index,
                      void *p_config);

    s32 (*set_config)(mm_handle h_component, MM_INDEX_TYPE index,
                      void *p_config);

    s32 (*get_state)(mm_handle h_component, MM_STATE_TYPE *p_state);

    s32 (*bind_request)(mm_handle h_comp, u32 n_port, mm_handle h_bind_comp,
                        u32 n_port_bind);

    s32 (*set_callback)(mm_handle h_component, mm_callback *p_cb,
                        void *p_app_data);

    s32 (*send_buffer)(mm_handle h_component, mm_buffer *p_buffer);

    s32 (*giveback_buffer)(mm_handle h_component, mm_buffer *p_buffer);

    s32 (*deinit)(mm_handle h_component);
} mm_component;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/* File EOF */
