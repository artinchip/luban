/**
 ****************************************************************************************
 *
 * @file asr_strs.h
 *
 * @brief Miscellaneous debug strings
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _ASR_STRS_H_
#define _ASR_STRS_H_

#include "lmac_msg.h"

#define ASR_ID2STR(tag) (((MSG_T(tag) < ARRAY_SIZE(asr_id2str)) &&        \
                           (asr_id2str[MSG_T(tag)]) &&          \
                           ((asr_id2str[MSG_T(tag)])[MSG_I(tag)])) ?   \
                          (asr_id2str[MSG_T(tag)])[MSG_I(tag)] : "unknown")

extern const char *const *asr_id2str[TASK_LAST_EMB + 1];

#endif /* _ASR_STRS_H_ */
