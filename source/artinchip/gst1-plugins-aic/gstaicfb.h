/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  <qi.xu@artinchip.com>
 */

struct gst_aicfb;

struct gst_aicfb* gst_aicfb_open();
void gst_aicfb_close(struct gst_aicfb* aicfb);
int gst_aicfb_render(struct gst_aicfb* aicfb, struct mpp_buf* buf, int buf_id);
