/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "base/lcd.h"
#include "aic_linux_mem.h"
#include "aic_dec_asset.h"

#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"
#define MAX_BUF_SIZE    200
#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))

extern int lcd_size_get(void);

static int g_heap_fd = -1;
static cma_buffer_hash_map *g_hash_map = NULL;

static int hash_delivery(void *buf, int size) {
  unsigned long long addr = (unsigned long long)buf;

  return addr % size;
}

static cma_buffer_hash_map *cma_buf_hash_create(int size) {
  cma_buffer_hash_map *map;

  map = (cma_buffer_hash_map *)malloc(sizeof(cma_buffer_hash_map));
  if (map != NULL) {
    map->size = size;
    map->cur_size = 0;
    map->cma_size = 0;
    map->buckets = (cma_buffer_hash **)calloc(size, sizeof(cma_buffer_hash *));

    if (map->buckets == NULL) {
      free(map);
      return NULL;
    }
  }

  return map;
}

static int cma_buf_hash_add(cma_buffer_hash_map *map, cma_buffer *data) {
  int hash_index = hash_delivery((unsigned int*)data->buf, map->size);
  cma_buffer_hash *cur_node = NULL;
  cma_buffer_hash *new_node = (cma_buffer_hash *)calloc(1, sizeof(cma_buffer_hash));

  if (map->cur_size > map->size - 1) {
    if (new_node != NULL)
      free(new_node);
    return -1;
  }

  if (new_node != NULL) {
    memcpy(&new_node->data, data, sizeof(cma_buffer));
    new_node->next = NULL;

    if (map->buckets[hash_index] == NULL) {
      map->buckets[hash_index] = new_node;
    } else {
      cur_node = map->buckets[hash_index];
      while (cur_node->next != NULL) {
        cur_node = cur_node->next;
      }
      cur_node->next = new_node;
    }
  } else {
    return -1;
  }

  map->cur_size++;
  return 0;
}

static int cma_buf_hash_find(cma_buffer_hash_map *map, void *buf, cma_buffer *data) {
  int hash_index = hash_delivery((unsigned int*)buf, map->size);
  cma_buffer_hash *cur_node = map->buckets[hash_index];

  while(cur_node != NULL) {
    if ((unsigned int*)cur_node->data.buf == (unsigned int*)buf) {
      memcpy(data, &cur_node->data, sizeof(cma_buffer));
      return 0;
    }
    cur_node = cur_node->next;
  }

  return -1;
}

static int cma_buf_hash_remove(cma_buffer_hash_map *map, void *buf) {
  int hash_index = hash_delivery(buf, map->size);
  cma_buffer_hash *cur_node = map->buckets[hash_index];
  cma_buffer_hash *prev_node = NULL;

  if (map->cur_size <= 0)
    return -1;

  while(cur_node != NULL) {
    if ((unsigned int*)cur_node->data.buf == (unsigned int*)buf) {
      if (prev_node == NULL) {
        map->buckets[hash_index] = cur_node->next;
      } else {
        prev_node->next = cur_node->next;
      }

      map->cur_size--;
      free(cur_node);
      return 0;
    }

    prev_node = cur_node;
    cur_node = cur_node->next;
  }

  return -1;
}

static void cma_buf_hash_destroy(cma_buffer_hash_map *map) {
  int i = 0;
  cma_buffer_hash *cur_node = NULL;
  cma_buffer_hash *tmp_node = NULL;

  if (map != NULL) {
    for (i = 0; i < map->size; i++) {
      cur_node = map->buckets[i];
      while(cur_node != NULL) {
        tmp_node = cur_node;
        cur_node = cur_node->next;
        free(tmp_node);
      }
    }

    free(map->buckets);
    free(map);
  }
}

int aic_cma_buf_malloc(cma_buffer *back, int size) {
  int ret = -1;
  int dma_fd = 0;
  void *dma_buf = NULL;
  struct dma_heap_allocation_data data = {0};

  if (g_heap_fd < 0) {
    log_info("Failed to aic_cma_buf_malloc  cma_fd = %d\n", g_heap_fd);
    return -1;
   } else {
     data.fd = 0;
     data.len = size;
     data.fd_flags = O_RDWR | O_CLOEXEC;
     data.heap_flags = 0;

     ret = ioctl(g_heap_fd, DMA_HEAP_IOCTL_ALLOC, &data);
     if (ret < 0) {
        log_info("in aic_cma_buf_malloc, ioctl() failed! errno, heap_fd = %d, size = %d\n", g_heap_fd,size);
	return -1;
     }

     dma_fd = data.fd;
     dma_buf = mmap(0, data.len, PROT_READ | PROT_WRITE, MAP_SHARED, dma_fd, 0);
     if (dma_buf == NULL) {
	log_info("mmap cma heap err\n");
        return -1;
     }

     memset(back, 0, sizeof(cma_buffer));
     back->type = FD_TYPE;
     back->fd = dma_fd;
     back->buf = dma_buf;
     back->buf_head = dma_buf;
     back->size = size;
  }

  return 0;
}

void aic_cma_buf_free(cma_buffer *data) {
  if (data->type == FD_TYPE) {
    if (data->fd >= 0) {
      munmap(data->buf_head, data->size);
      close(data->fd);
      data->fd = -1;
    }
  } else {
    return;
  }
}

int aic_cma_buf_add(cma_buffer *data) {
  return cma_buf_hash_add(g_hash_map, data);
}

int aic_cma_buf_find(void *buf, cma_buffer *back) {
  return cma_buf_hash_find(g_hash_map, buf, back);
}

int aic_cma_buf_del(void *buf) {
  int ret = -1;
  cma_buffer node = { 0 };
  void *buf_node = NULL;

  ret = aic_cma_buf_find(buf, &node);
  if (ret < 0) {
    log_info("node is not exit, buf = 0x%08x\n", (unsigned int)buf);
    return -1;
  }

  buf_node = (void *)node.buf;

  aic_cma_buf_free(&node);
  return cma_buf_hash_remove(g_hash_map, buf_node);
}

static void aic_cma_buf_destroy() {
  int i = 0;
  cma_buffer_hash *cur_node = NULL;

  if (g_hash_map != NULL) {
    for (i = 0; i < g_hash_map->size; i++) {
      cur_node = g_hash_map->buckets[i];
      while(cur_node != NULL) {
        aic_cma_buf_free(&cur_node->data);
        cur_node = cur_node->next;
      }
    }
    cma_buf_hash_destroy(g_hash_map);
  } else {
    log_info("aic_cma_buf_destroy err\n");
  }
}

int aic_cma_buf_open(void) {
  if (g_heap_fd < 0) {
    g_heap_fd = open(DMA_HEAP_DEV, O_RDWR);
    if (g_heap_fd < 0) {
      log_error("Failed to open %s\n", DMA_HEAP_DEV);
      return -1;
    }
  }

  if (g_hash_map == NULL) {
    g_hash_map = cma_buf_hash_create(MAX_BUF_SIZE);
    if (g_hash_map == NULL) {
      log_info("Failed to create cma_buf_hash\n");
    }
  }

  return 0;
}

void aic_cma_buf_close(void) {
  if (g_heap_fd > 0)
    close(g_heap_fd);

  aic_cma_buf_destroy();
}

void aic_cma_buf_debug(int flag) {
  int i = 0;
  int sum_size = 0;
  cma_buffer_hash *cur_node = NULL;
  aic_dec_asset *ctx_data;

  if (g_hash_map == NULL) {
    log_info("hash need init\n");
    return;
  }

  if (flag & AIC_CMA_BUF_DEBUG_CONTEXT) {
    for (i = 0; i < g_hash_map->size; i++) {
      cur_node = g_hash_map->buckets[i];
      while(cur_node != NULL) {
        if (cur_node->data.type == FD_TYPE) {
          log_debug("fd = %d, buf = 0x%08x, size = %d\n",
                    cur_node->data.fd,
                    (unsigned int)cur_node->data.buf,
                    cur_node->data.size);
        } else if (cur_node->data.type == PHY_TYPE) {
          log_debug("phy = 0x%08x, buf = 0x%08x, size = %d\n",
                    (unsigned int)cur_node->data.phy_addr,
                    (unsigned int)cur_node->data.buf,
                    cur_node->data.size);
        }  else {
          ctx_data = cur_node->data.data;
          log_debug("name = %s, fd = %d, size = %d\n",
                    ctx_data->name, (unsigned int)cur_node->data.fd,
                    (unsigned int)cur_node->data.size);
        }
        sum_size += cur_node->data.size;
        cur_node = cur_node->next;
      }
    }
  }

  if (flag & AIC_CMA_BUF_DEBUG_SIZE) {
    log_debug("used_table_size = %d, used_mem_size = %d, sum_size = %d\n",
               (g_hash_map->size - g_hash_map->cur_size),
               g_hash_map->cma_size, (sum_size - lcd_size_get()));
  }
}

