/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#ifndef _ARTINCHIP_SPI_MEM_ENC_H_
#define _ARTINCHIP_SPI_MEM_ENC_H_
#include <spi-mem.h>

void *spi_mem_enc_init(struct spi_slave *ss);
int spi_mem_enc_xfer_cfg(void *handle, u32 addr, u32 clen, int mode);
int spi_mem_enc_read(void *handle, struct spi_mem_op *op);
int spi_mem_enc_write(void *handle, struct spi_mem_op *op);

#endif /* _ARTINCHIP_SPI_MEM_ENC_H_ */
