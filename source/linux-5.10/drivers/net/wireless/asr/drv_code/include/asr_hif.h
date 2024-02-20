/**
 ******************************************************************************
 *
 * @file asr_hif.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef ASR_HIF_H
#define ASR_HIF_H

#include "lmac_msg.h"

typedef enum {
	HIF_INVALID_TYPE = -1,
	HIF_RX_MSG = 0,
	HIF_RX_DATA,
	HIF_RX_LOG,
	HIF_TX_MSG,
	HIF_TX_DATA,
} HIF_TYPE_E;

typedef struct {
	struct co_list_hdr hdr;	//list header for chaining
	u16 type;
	u16 len;
	u32 addr;
} hif_elmt_t;

#define HIF_HDR_LEN sizeof(hif_elmt_t)

#endif //ASR_HIF_H
