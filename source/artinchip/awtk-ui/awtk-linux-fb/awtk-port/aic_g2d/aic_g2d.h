/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _TK_AIC_G2D_H
#define _TK_AIC_G2D_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "aic_linux_mem.h"
#include "base/image_loader.h"

void tk_aic_g2d_open(void);
void tk_aic_g2d_close(void);

/* add to cma manager and add to ge */
int aic_cma_buf_add_ge(cma_buffer *data);
int aic_cma_buf_find_ge(void *buf, cma_buffer *back);
int aic_cma_buf_del_ge(void *buf);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _TK_AIC_G2D_H */
