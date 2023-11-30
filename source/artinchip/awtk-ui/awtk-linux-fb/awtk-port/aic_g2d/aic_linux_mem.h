/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_LINUX_MEM_H_
#define _AIC_LINUX_MEM_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define FD_TYPE        1
#define PHY_TYPE       2
#define CTX_ASSET_TYPE 3

#define AIC_CMA_BUF_DEBUG_SIZE    (1<<0)
#define AIC_CMA_BUF_DEBUG_CONTEXT (1<<1)

struct _cma_buffer {
  int fd; /* linux only */
  int type;
  unsigned int size;
  void *buf; /* addressing address */
  void *data;
  void *buf_head;
  unsigned int phy_addr;
};

struct _cma_buffer_hash {
  struct _cma_buffer data;
  struct _cma_buffer_hash *next;
};

struct _cma_buffer_hash_map {
  struct _cma_buffer_hash **buckets;
  unsigned int size;
  unsigned int cur_size;
  unsigned int cma_size;
};

typedef struct _cma_buffer cma_buffer;
typedef struct _cma_buffer_hash cma_buffer_hash;
typedef struct _cma_buffer_hash_map cma_buffer_hash_map;

int aic_cma_buf_open(void);
void aic_cma_buf_close(void);
int aic_cma_buf_malloc(cma_buffer *back, int size);
void aic_cma_buf_free(cma_buffer * buf);

/* add cma_buf in manager, use in bitmap */
int aic_cma_buf_add(cma_buffer *data);
int aic_cma_buf_find(void *buf, cma_buffer *back);
int aic_cma_buf_del(void *buf);

void aic_cma_buf_debug(int flag);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
