/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef USERID_INTERNAL_H
#define USERID_INTERNAL_H

#include <unistd.h>
#include <stdint.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <stdint.h>
#include "list.h"

#define USERID_HEADER_MAGIC 0x48444955
#define USERID_ITEM_MAGIC   0x49444955

struct userid_storage_header {
	uint32_t magic;
	uint32_t crc32; /* CRC32 result from total_length to data end */
	uint32_t total_length; /* From this field to data end */
	uint32_t reserved[5];
};

struct userid_item_header {
	uint32_t magic;
	uint16_t name_len;
	uint16_t data_len;
};

struct userid_entity {
	struct list_head list;
	char *name;
	uint8_t *data;
	uint16_t name_len;
	uint16_t data_len;
};

int userid_part_load(char const *path, uint8_t **buf);
int userid_part_save(char const *path, uint8_t *buf, int len);

#endif
