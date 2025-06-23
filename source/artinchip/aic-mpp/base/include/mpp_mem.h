/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: virtual memory allocator
 */

#ifndef MPP_MEM_H
#define MPP_MEM_H

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "mpp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

void *mpp_alloc(size_t size);

void *mpp_realloc(void *ptr,size_t size);

void mpp_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MEM_H */
