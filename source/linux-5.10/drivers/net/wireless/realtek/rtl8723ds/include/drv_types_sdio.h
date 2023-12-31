/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#ifndef __DRV_TYPES_SDIO_H__
#define __DRV_TYPES_SDIO_H__

/* SDIO Header Files */
	#include <linux/mmc/sdio_func.h>
	#include <linux/mmc/sdio_ids.h>
	#include <linux/mmc/host.h>
	#include <linux/mmc/card.h>

	#ifdef CONFIG_PLATFORM_SPRD
		#include <linux/gpio.h>
		#include <custom_gpio.h>
	#endif /* CONFIG_PLATFORM_SPRD */

typedef struct sdio_data {
	u8  func_number;

	u8  tx_block_mode;
	u8  rx_block_mode;
	u32 block_transfer_len;

	struct sdio_func	*func;
	_thread_hdl_ sys_sdio_irq_thd;
	unsigned int clock;
} SDIO_DATA, *PSDIO_DATA;

#define dvobj_to_sdio_func(d)	((d)->intf_data.func)

#define RTW_SDIO_ADDR_CMD52_BIT		(1<<17)
#define RTW_SDIO_ADDR_CMD52_GEN(a)	(a | RTW_SDIO_ADDR_CMD52_BIT)
#define RTW_SDIO_ADDR_CMD52_CLR(a)	(a&~RTW_SDIO_ADDR_CMD52_BIT)
#define RTW_SDIO_ADDR_CMD52_CHK(a)	(a&RTW_SDIO_ADDR_CMD52_BIT ? 1 : 0)

#define RTW_SDIO_ADDR_F0_BIT		(1<<18)
#define RTW_SDIO_ADDR_F0_GEN(a)		(a | RTW_SDIO_ADDR_F0_BIT)
#define RTW_SDIO_ADDR_F0_CLR(a)		(a&~RTW_SDIO_ADDR_F0_BIT)
#define RTW_SDIO_ADDR_F0_CHK(a)		(a&RTW_SDIO_ADDR_F0_BIT ? 1 : 0)

#define RTW_SDIO_CLK_40M	40000000

#endif
