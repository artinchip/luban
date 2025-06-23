/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef _MIPI_DSI_REG_H_
#define _MIPI_DSI_REG_H_

#include "../aic_fb.h"

#include "dsi_reg_v1.0.h"

/* Processor to Peripheral Direction (Processor-Sourced) Packet Data Types */
#define DSI_DT_VSS		0x01
#define DSI_DT_VSE		0x11
#define DSI_DT_HSS		0x21
#define DSI_DT_HSE		0x31
#define DSI_DT_EOT		0x08
#define DSI_DT_CM_OFF		0x02
#define DSI_DT_CM_ON		0x12
#define DSI_DT_SHUT_DOWN	0x22
#define DSI_DT_TURN_ON		0x32
#define DSI_DT_GEN_WR_P0	0x03
#define DSI_DT_GEN_WR_P1	0x13
#define DSI_DT_GEN_WR_P2	0x23
#define DSI_DT_GEN_RD_P0	0x04
#define DSI_DT_GEN_RD_P1	0x14
#define DSI_DT_GEN_RD_P2	0x24
#define DSI_DT_DCS_WR_P0	0x05
#define DSI_DT_DCS_WR_P1	0x15
#define DSI_DT_DCS_RD_P0	0x06
#define DSI_DT_MAX_RET_SIZE	0x37
#define DSI_DT_NULL		0x09
#define DSI_DT_BLK		0x19
#define DSI_DT_GEN_LONG_WR	0x29
#define DSI_DT_DCS_LONG_WR	0x39
#define DSI_DT_PIXEL_RGB565	0x0E
#define DSI_DT_PIXEL_RGB666P	0x1E
#define DSI_DT_PIXEL_RGB666	0x2E
#define DSI_DT_PIXEL_RGB888	0x3E
/* Data Types for Peripheral-sourced Packets */
#define DSI_DT_ACK_ERR		0x02
#define DSI_DT_EOT_PERI		0x08
#define DSI_DT_GEN_RD_R1	0x11
#define DSI_DT_GEN_RD_R2	0x12
#define DSI_DT_GEN_LONG_RD_R	0x1A
#define DSI_DT_DCS_LONG_RD_R	0x1C
#define DSI_DT_DCS_RD_R1	0x21
#define DSI_DT_DCS_RD_R2	0x22

#define DSI_DCS_ENTER_IDLE_MODE		0x39
#define DSI_DCS_ENTER_INVERT_MODE	0x21
#define DSI_DCS_ENTER_NORMAL_MODE	0x13
#define DSI_DCS_ENTER_PARTIAL_MODE	0x12
#define DSI_DCS_ENTER_SLEEP_MODE	0x10
#define DSI_DCS_EXIT_IDLE_MODE		0x38
#define DSI_DCS_EXIT_INVERT_MODE	0x20
#define DSI_DCS_EXIT_SLEEP_MODE		0x11
#define DSI_DCS_GET_ADDRESS_MODE	0x0b
#define DSI_DCS_GET_BLUE_CHANNEL	0x08
#define DSI_DCS_GET_DIAGNOSTIC_RESULT	0x0f
#define DSI_DCS_GET_DISPLAY_MODE	0x0d
#define DSI_DCS_GET_GREEN_CHANNEL	0x07
#define DSI_DCS_GET_PIXEL_FORMAT	0x0c
#define DSI_DCS_GET_POWER_MODE		0x0a
#define DSI_DCS_GET_RED_CHANNEL		0x06
#define DSI_DCS_GET_SCANLINE		0x45
#define DSI_DCS_GET_SIGNAL_MODE		0x0e
#define DSI_DCS_NOP			0x00
#define DSI_DCS_READ_DDB_CONTINUE	0xa8
#define DSI_DCS_READ_DDB_START		0xa1
#define DSI_DCS_READ_MEMORY_CONTINUE	0x3e
#define DSI_DCS_READ_MEMORY_START	0x2e
#define DSI_DCS_SET_ADDRESS_MODE	0x36
#define DSI_DCS_SET_COLUMN_ADDRESS	0x2a
#define DSI_DCS_SET_DISPLAY_OFF		0x28
#define DSI_DCS_SET_DISPLAY_ON		0x29
#define DSI_DCS_SET_GAMMA_CURVE		0x26
#define DSI_DCS_SET_PAGE_ADDRESS	0x2b
#define DSI_DCS_SET_PARTIAL_AREA	0x30
#define DSI_DCS_SET_PIXEL_FORMAT	0x3a
#define DSI_DCS_SET_SCROLL_AREA		0x33
#define DSI_DCS_SET_SCROLL_START	0x37
#define DSI_DCS_SET_TEAR_OFF		0x34
#define DSI_DCS_SET_TEAR_ON		0x35
#define DSI_DCS_SET_TEAR_SCANLINE	0x44
#define DSI_DCS_SOFT_RESET		0x01
#define DSI_DCS_WRITE_LUT		0x2d
#define DSI_DCS_WRITE_MEMORY_CONTINUE	0x3c
#define DSI_DCS_WRITE_MEMORY_START	0x2c

#endif // end of _MIPI_DSI_REG_H_
