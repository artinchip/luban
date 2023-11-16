/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */
#ifndef __CONFIG_PARSE_H__
#define __CONFIG_PARSE_H__

#include <linux/ctype.h>
#include <common.h>

#define IMG_NAME_MAX_SIZ 60
#define PROTECTION_PARTITION_LEN 128

int boot_cfg_parse_file(char *cfgtxt, u32 clen, char *key, char *fname,
			       u32 nsiz, ulong *offset, ulong *flen);
int boot_cfg_get_env(char *cfgtxt, u32 clen, char *name, u32 nsiz,
		     ulong *offset, ulong *envlen);
int boot_cfg_get_boot1(char *cfgtxt, u32 clen, char *name, u32 nsiz,
		       ulong *offset, ulong *boot1len);
int boot_cfg_get_image(char *cfgtxt, u32 clen, char *name, u32 nsiz);
int boot_cfg_get_protection(char *cfgtxt, u32 clen, char *name, u32 nsiz);
#endif/* __CONFIG_PARSE_H__ */
