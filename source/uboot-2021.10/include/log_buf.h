/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2020-2024 ArtInChip Technology Co., Ltd. All rights reserved.
 */

#ifndef _LOG_BUF_H_
#define _LOG_BUF_H_

int log_buf_init(void);
int log_buf_get_len(void);
int log_buf_write(char *in, int len);
int log_buf_read(char *out, int len);

#endif
