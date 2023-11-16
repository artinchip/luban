/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Definitions for the ArtinChip Video Engine driver
 *
 * Copyright (C) 2020-2021 ArtinChip Technology Co., Ltd.
 * Authors:  Jun <lijun.li@artinchip.com>
 */

#ifndef _UAPI__ARTINCHIP_VE_H_
#define _UAPI__ARTINCHIP_VE_H_

#include <linux/ioctl.h>
#include "mpp_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct ve_info {
	int reg_size;
};

struct wait_info {
	int wait_time;
	unsigned int reg_status;
};

#define IOC_TYPE_VE			'V'

#define IOC_VE_GET_CLIENT		_IO(IOC_TYPE_VE, 0x00)

#define IOC_VE_PUT_CLIENT		_IO(IOC_TYPE_VE, 0x01)

#define IOC_VE_WAIT			_IOWR(IOC_TYPE_VE, 0x02, \
						struct wait_info)

#define IOC_VE_GET_INFO			_IOR(IOC_TYPE_VE, 0x03, \
						struct ve_info)

#define IOC_VE_SET_INFO			_IOW(IOC_TYPE_VE, 0x04, \
						struct ve_info)

#define IOC_VE_RESET			_IO(IOC_TYPE_VE, 0x05)

#define IOC_VE_ADD_DMA_BUF		_IOWR(IOC_TYPE_VE, 0x06, \
						struct dma_buf_info)

#define IOC_VE_RM_DMA_BUF		_IOW(IOC_TYPE_VE, 0x07, \
						struct dma_buf_info)

#if defined(__cplusplus)
}
#endif

#endif /* _UAPI__ARTINCHIP_VE_H_ */

