/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for the ArtInChip DDMA
 *
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Authors: matteo <duanmt@artinchip.com>
 */

#ifndef __ARTINCHIP_DMA_H
#define __ARTINCHIP_DMA_H

struct dma_chan *aic_ddma_request_chan(struct device *dev, u32 port);
int aic_ddma_transfer(struct dma_chan *chan);
void aic_ddma_release_chan(struct dma_chan *chan);

#endif /* __ARTINCHIP_DMA_H */
