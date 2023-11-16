/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __USERID_H
#define __USERID_H

int userid_init(void);
void userid_deinit(void);
int userid_get_count(void);
int userid_get_name(int idx, char *buf, int len);
int userid_get_data_length(const char *name);
int userid_read(const char *name, int offset, u8 *buf, int len);
int userid_write(const char *name, int offset, u8 *buf, int len);
int userid_remove(const char *name);
int userid_save(void);
int userid_import(u8 *buf);
int userid_export(u8 *buf);


#endif
