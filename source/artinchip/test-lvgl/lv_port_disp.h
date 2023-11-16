/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

//#define TRIPLE_FRAME_BUF_EN

void lv_port_disp_init(void);

void lv_port_disp_exit(void);

int fbdev_draw_fps(void);

int disp_is_swap(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PORT_DISP_H*/
