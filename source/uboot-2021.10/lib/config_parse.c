// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Jianfeng Li <jianfeng.li@artinchip.com>
 */

#include <config_parse.h>

static u32 boot_cfg_get_key_val(char *cfgtxt, u32 len, const char *key,
				u32 klen, char *val, u32 vlen)
{
	u32 idx = 0, cnt = 0;

	while (idx < len) {
		/* Begin of one string line */

		/* Skip blank */
		if (isspace(cfgtxt[idx])) {
			idx++;
			continue;
		}

		/* Skip comment line */
		if (cfgtxt[idx] == '#') {
			while ((idx < len) && (cfgtxt[idx] != '\n'))
				idx++;
			continue;
		}

		if (strncmp(&cfgtxt[idx], key, klen)) {
			/* Skip line if key is not matched */
			while ((idx < len) && (cfgtxt[idx] != '\n'))
				idx++;
			continue;
		}

		/* Key is matched, but need to skip blank and '=' */
		idx += klen;
		while ((cfgtxt[idx] == ' ') || (cfgtxt[idx] == '\t') ||
		    (cfgtxt[idx] == '=')) {
			idx++;
			if (idx >= len)
				break;
		}

		/* Now it is clean, get value */
		cnt = 0;
		while ((idx < len) && (cfgtxt[idx] != '\n')) {
			val[cnt] = cfgtxt[idx];
			cnt++;
			idx++;
			if (cnt == vlen)
				break;
		}
		break;
	}

	val[cnt] = '\0';
	return cnt;
}

/*
 * Check key value format:
 * - boot0=size@offset
 * - boot0=example.bin
 */
static u32 boot_cfg_is_offset_format(char *val, u32 len)
{
	u32 i = 0, sign_idx;

	sign_idx = 0;
	for (i = 0; i < len; i++) {
		if (isspace(val[i]))
			continue;
		if ((val[i] == 'x') || (val[i] == 'X'))
			continue;
		if (val[i] == '@') {
			sign_idx = i;
			continue;
		}
		if (!isxdigit(val[i])) {
			sign_idx = 0;
			break;
		}
	}

	return sign_idx;
}

/*
 * Get the given key's data file name and valid data length
 * - cfgtxt: The bootcfg.txt file data content
 * - clen: The bootcfg.txt file data length
 * - key: Key name
 * - fname: The output data file name
 * - nsiz: The fname buffer size
 * - offset: The firmware data's start offset in the file data(fname)
 * - flen: The firmware data's valid length
 */
int boot_cfg_parse_file(char *cfgtxt, u32 clen, char *key, char *fname,
			       u32 nsiz, ulong *offset, ulong *flen)
{
	char val[IMG_NAME_MAX_SIZ];
	u32 vlen, sign_idx;

	vlen = boot_cfg_get_key_val(cfgtxt, clen, key, strlen(key), val,
				    IMG_NAME_MAX_SIZ);
	if (vlen == 0)
		return -1;

	sign_idx = boot_cfg_is_offset_format(val, vlen);
	if (sign_idx) {
		*flen = (ulong)simple_strtoul(val, NULL, 16);
		*offset = (ulong)simple_strtoul(&val[sign_idx + 1], NULL, 16);
		vlen = boot_cfg_get_key_val(cfgtxt, clen, "image", 5, val,
					    IMG_NAME_MAX_SIZ);
		memcpy(fname, val, min(vlen, nsiz));
		fname[vlen] = 0;
	} else {
		*flen = 0;
		*offset = 0;
		memcpy(fname, val, min(vlen, nsiz));
		fname[vlen] = 0;
	}

	printf("0x%x @ %s\n", (u32)(*offset), fname);
	return vlen;
}

int boot_cfg_get_env(char *cfgtxt, u32 clen, char *name, u32 nsiz,
		     ulong *offset, ulong *envlen)
{
	return boot_cfg_parse_file(cfgtxt, clen, "env", name, nsiz, offset,
				   envlen);
}

int boot_cfg_get_boot1(char *cfgtxt, u32 clen, char *name, u32 nsiz,
		       ulong *offset, ulong *boot1len)
{
	return boot_cfg_parse_file(cfgtxt, clen, "boot1", name, nsiz,
					offset, boot1len);
}

int boot_cfg_get_image(char *cfgtxt, u32 clen, char *name, u32 nsiz)
{
	u32 ret;

	ret = boot_cfg_get_key_val(cfgtxt, clen, "image", 5, name, nsiz);
	if (ret <= 0)
		return -1;

	return 0;
}

int boot_cfg_get_protection(char *cfgtxt, u32 clen, char *name, u32 nsiz)
{
	u32 ret;

	ret =  boot_cfg_get_key_val(cfgtxt, clen, "protection", 10, name, nsiz);
	if (ret <= 0)
		return -1;

	return 0;
}
