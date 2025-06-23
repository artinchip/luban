/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0e
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_message
 */


#ifndef __AIC_MESSAGE_H__
#define __AIC_MESSAGE_H__

#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <semaphore.h>
#include <sys/time.h>

#include "mpp_dec_type.h"
#include "mpp_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */




#define ONE_TIME_CREATE_MESSAGE_MAX_ITEMS (10)

struct aic_message {
	s32 				message_id;
	s32 				param;
	void				*data;
	s32					data_size;
	struct mpp_list			list;
};

struct aic_message_queue {
	s32  				msg_cnt;
	s32 				msg_node_cnt;
	struct mpp_list     empty_msg_list;
	struct mpp_list     ready_msg_list;
	pthread_mutex_t     mutex;

	pthread_cond_t      msg_cond;
	pthread_condattr_t  msg_cond_attr;
	s8 					need_signal;
};

s32  aic_msg_create(struct aic_message_queue* msg_que);
void aic_msg_destroy(struct aic_message_queue* msg_que);
void aic_msg_clear(struct aic_message_queue* msg_que);
s32  aic_msg_put(struct aic_message_queue* msg_que, struct aic_message *msg);
s32  aic_msg_get(struct aic_message_queue* msg_que, struct aic_message *msg);
s32  aic_msg_cnt(struct aic_message_queue* msg_que);

s32  aic_msg_wait_new_msg(struct aic_message_queue* msg_que,u64 us);//unit us

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif


