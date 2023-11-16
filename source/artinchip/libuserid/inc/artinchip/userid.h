/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#ifndef ARTINCHIP_USERID_H
#define ARTINCHIP_USERID_H

int userid_init(const char *path);
void userid_deinit(void);
int userid_get_count(void);
int userid_get_name(int idx, char *buf, int len);
int userid_get_data_length(const char *name);
int userid_read(const char *name, int offset, uint8_t *buf, int len);
int userid_save(const char *path);
int userid_remove(const char *name);
int userid_write(const char *name, int offset, uint8_t *buf, int len);
#endif
