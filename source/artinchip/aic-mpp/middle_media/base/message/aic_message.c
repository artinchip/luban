/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_message
*/

#include "aic_message.h"
#include "mpp_log.h"
#include "mpp_mem.h"

s32  aic_msg_create(struct aic_message_queue* msg_que)
{
	int result = 0;
	int i = 0;
	struct aic_message   *msg;


	/*init msg_queue*/
	mpp_list_init(&msg_que->empty_msg_list);
	mpp_list_init(&msg_que->ready_msg_list);
	msg_que->msg_node_cnt = 0;
	msg_que->msg_cnt = 0;
	msg_que->need_signal = 0;
	if (pthread_mutex_init(&msg_que->mutex, NULL) != 0) {
		logd("pthread_mutex_init err!!!\n");
		return -1;
	}

	pthread_condattr_init(&msg_que->msg_cond_attr);
	pthread_condattr_setclock(&msg_que->msg_cond_attr,CLOCK_MONOTONIC);
	pthread_cond_init(&msg_que->msg_cond,&msg_que->msg_cond_attr);

	/*create empty msg node*/
	for (i=0;i<ONE_TIME_CREATE_MESSAGE_MAX_ITEMS;i++) {
		msg = (struct aic_message*)mpp_alloc(sizeof(struct aic_message));
		if(NULL == msg){
		    break;
		}
		memset(msg,0x00,sizeof(struct aic_message));
		mpp_list_add_tail(&msg->list, &msg_que->empty_msg_list);
		msg_que->msg_node_cnt++;
	}

	/*creat empty msg node failed*/
	if (0 == msg_que->msg_node_cnt) {
		logd("create empty msg node failed!!!\n");
		result = -1;
	}

	return result;

}

void aic_msg_destroy(struct aic_message_queue* msg_que)
{
	int cnt=0;
	struct aic_message   *msg, *msg1;
	aic_msg_clear(msg_que);
	pthread_mutex_lock(&msg_que->mutex);
	if (!mpp_list_empty(&msg_que->empty_msg_list)) {
		mpp_list_for_each_entry_safe(msg, msg1, &msg_que->empty_msg_list, list) {
			mpp_list_del(&msg->list);
			mpp_free(msg);
			cnt++;
		}
	}
	pthread_mutex_unlock(&msg_que->mutex);

	logd("free cnt:%d,free cnt :%d\n",cnt,msg_que->msg_node_cnt);

	if (cnt != msg_que->msg_node_cnt) {
		loge("why not eq,check code !!!,free cnt:%d,malloc cnt :%d\n",cnt,msg_que->msg_node_cnt);
	}
	mpp_list_init(&msg_que->empty_msg_list);
	mpp_list_init(&msg_que->ready_msg_list);

	pthread_condattr_destroy(&msg_que->msg_cond_attr);
	pthread_cond_destroy(&msg_que->msg_cond);
	pthread_mutex_destroy(&msg_que->mutex);
	logd("aic_msg_destroy\n");
}

void aic_msg_clear(struct aic_message_queue* msg_que)
{
	struct aic_message   *msg, *msg1;
	pthread_mutex_lock(&msg_que->mutex);
	if (!mpp_list_empty(&msg_que->ready_msg_list)) {
		mpp_list_for_each_entry_safe(msg, msg1, &msg_que->ready_msg_list, list) {
			if (msg->data) {
				mpp_free(msg->data);
				msg->data = NULL;
			}
			msg->data_size = 0;
			mpp_list_del(&msg->list);
			mpp_list_add_tail(&msg->list, &msg_que->empty_msg_list);
			msg_que->msg_cnt--;
		}
	}

	if(msg_que->msg_cnt != 0) {
		loge("why some msg not clear count[%d]!=0", msg_que->msg_cnt);
	}

	if (!mpp_list_empty(&msg_que->empty_msg_list)) {
		mpp_list_for_each_entry_safe(msg, msg1, &msg_que->empty_msg_list, list) {
			if (msg->data) {
				mpp_free(msg->data);
				msg->data = NULL;
			}
			msg->data_size = 0;
		}
	}
	pthread_mutex_unlock(&msg_que->mutex);

}

s32  aic_msg_put(struct aic_message_queue* msg_que, struct aic_message *msg)
{
    struct aic_message *message;

	int i = 0;

	pthread_mutex_lock(&msg_que->mutex);

	if (mpp_list_empty(&msg_que->empty_msg_list)) {
		logd(" no empty node,need to extend more!");
		for(i=0;i<ONE_TIME_CREATE_MESSAGE_MAX_ITEMS;i++) {
			message = (struct aic_message*)mpp_alloc(sizeof(struct aic_message));
			if(NULL == message){
				break;
			}
			memset(message,0x00,sizeof(struct aic_message));
			mpp_list_add_tail(&message->list, &msg_que->empty_msg_list);
			msg_que->msg_node_cnt++;
		}

		if (0 == i) {
			logd("create empty msg node failed!!!\n");
			pthread_mutex_unlock(&msg_que->mutex);
			return  -1;
		}
	}

	message = mpp_list_first_entry(&msg_que->empty_msg_list, struct aic_message, list);
	message->message_id = msg->message_id;
	message->param = msg->param;

	if (msg->data && msg->data_size>0) {
		message->data = mpp_alloc(msg->data_size);
		if (message->data) {
			message->data_size = msg->data_size;
			memcpy(message->data, msg->data, msg->data_size);
		}
		else{
			logd(" malloc msg data fail!");
			pthread_mutex_unlock(&msg_que->mutex);
			return -1;
		}
	}


	mpp_list_del(&message->list);
	mpp_list_add_tail(&message->list, &msg_que->ready_msg_list);
	msg_que->msg_cnt++;
	if(msg_que->need_signal){
		pthread_cond_signal(&msg_que->msg_cond);
	}

	logv("queue node:[%d],"\
		 "queue msg cnt:[%d],"\
		 "msg_id:[%d],"\
		 "para0:[%d],"\
		 "data_size:[%d],"\
		 "data:[%p]\n"\
		 ,msg_que->msg_node_cnt
		 ,msg_que->msg_cnt
		 ,message->message_id
		 ,message->param
		 ,message->data_size
		 ,message->data);

	pthread_mutex_unlock(&msg_que->mutex);

	return 0;

}

s32  aic_msg_get(struct aic_message_queue* msg_que, struct aic_message *msg)
{
	struct aic_message *message;
	pthread_mutex_lock(&msg_que->mutex);
	if(mpp_list_empty(&msg_que->ready_msg_list)){
		pthread_mutex_unlock(&msg_que->mutex);
		return -1;
	}
	message = mpp_list_first_entry(&msg_que->ready_msg_list, struct aic_message, list);
	msg->message_id = message->message_id;
	msg->param = message->param;
	if (message->data && message->data_size>0) {
		msg->data = mpp_alloc(message->data_size);
		if (msg->data) {
			msg->data_size = message->data_size;
			memcpy(msg->data, message->data, message->data_size);
			mpp_free(message->data);
			message->data = NULL;
			message->data_size = 0;
		}
		else {
			logd(" malloc msg data fail!");
			pthread_mutex_unlock(&msg_que->mutex);
			return -1;
		}
	}

	mpp_list_del(&message->list);
	mpp_list_add_tail(&message->list, &msg_que->empty_msg_list);
	msg_que->msg_cnt--;

	logv("queue node:[%d],"\
		"queue msg cnt:[%d],"\
		"msg_id:[%d],"\
		"para0:[%d],"\
		"data_size:[%d],"\
		"data:[%p]\n"\
		,msg_que->msg_node_cnt
		,msg_que->msg_cnt
		,message->message_id
		,message->param
		,message->data_size
		,message->data);

	pthread_mutex_unlock(&msg_que->mutex);
	return 0;

}

s32  aic_msg_cnt(struct aic_message_queue* msg_que)
{
	int cnt;
	pthread_mutex_lock(&msg_que->mutex);
	cnt = msg_que->msg_cnt;
	pthread_mutex_unlock(&msg_que->mutex);
	return cnt;
}

s32  aic_msg_wait_new_msg(struct aic_message_queue* msg_que,u64 us)
{
	int ret;
	struct timespec out_time;
	u64  tmp;

	pthread_mutex_lock(&msg_que->mutex);
	if(msg_que->msg_cnt != 0){
		pthread_mutex_unlock(&msg_que->mutex);
		return 0;
	}
	if(us == 0){
		msg_que->need_signal = 1;
		ret = pthread_cond_wait(&msg_que->msg_cond,&msg_que->mutex);
		msg_que->need_signal = 0;
	}else{
		clock_gettime(CLOCK_MONOTONIC,&out_time);
		out_time.tv_sec += us/(1*1000*1000);
		tmp = out_time.tv_nsec/1000 + us%(1*1000*1000);
		//tmp may larger than (1*1000*1000) = 1s
		out_time.tv_sec += tmp/(1*1000*1000);
		tmp = tmp%(1*1000*1000);
		out_time.tv_nsec = tmp*1000;
		msg_que->need_signal = 1;
		ret = pthread_cond_timedwait(&msg_que->msg_cond,&msg_que->mutex,&out_time);
		msg_que->need_signal = 0;
	}
	pthread_mutex_unlock(&msg_que->mutex);

	return ret;
}



