/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_audio_decoder interface
*/

#define LOG_TAG "audio_packet_manager"

#include <string.h>
#include <pthread.h>

#include "aic_audio_decoder.h"
#include "audio_packet_manager.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"

struct audio_packet_impl {
	struct mpp_packet pkt;
	size_t pos_offset;
	struct mpp_list list;
};

struct audio_packet_manager {
	int packet_count;
	pthread_mutex_t lock;
	struct mpp_list empty_list;
	struct mpp_list ready_list;
	int empty_num;
	int ready_num;
	struct audio_packet_impl *packet_node;

	unsigned char *buffer_start;
	unsigned char *buffer_end;
	int buffer_size;

	int write_offset;
	int read_offset;
	int available_size;
};

struct audio_packet_manager *audio_pm_create(struct aic_audio_decode_config *cfg)
{
	struct aic_audio_decode_config *init_cfg = cfg;
	struct aic_audio_decode_config default_cfg;
	struct audio_packet_manager *pm;
	struct audio_packet_impl *pkt_impl;
	int i;

	logi("create packet manager");

	if(init_cfg == NULL){
		init_cfg = &default_cfg;
		init_cfg->packet_buffer_size = 16*1024;
		init_cfg->packet_count = 8;
	}

	if (!init_cfg || init_cfg->packet_count <= 0 || init_cfg->packet_buffer_size <= 0)
		return NULL;
	pm = (struct audio_packet_manager *)mpp_alloc(sizeof(struct audio_packet_manager));
	if (!pm)
		return NULL;

	pm->empty_num = init_cfg->packet_count;
	pm->ready_num = 0;
	pm->packet_count = init_cfg->packet_count;
	pm->packet_node = (struct audio_packet_impl *)mpp_alloc(pm->packet_count * sizeof(struct audio_packet_impl));
	if (!pm->packet_node) {
		loge("alloc %d count packet failed!", pm->packet_count);
		mpp_free(pm);
		return NULL;
	}
	memset(pm->packet_node, 0, pm->packet_count * sizeof(struct audio_packet_impl));
	pm->buffer_size = init_cfg->packet_buffer_size;
	pm->buffer_start =mpp_alloc(pm->buffer_size);
	pm->buffer_end = pm->buffer_start + pm->buffer_size - 1;

	logi("packet manager create %d count packet, buffer size %d", pm->packet_count, pm->buffer_size);

	pm->read_offset = 0;
	pm->write_offset = 0;
	pm->available_size = pm->buffer_size;

	pthread_mutex_init(&pm->lock, NULL);
	mpp_list_init(&pm->empty_list);
	mpp_list_init(&pm->ready_list);

	pkt_impl = pm->packet_node;
	for (i = 0; i < pm->packet_count; i++) {
		mpp_list_init(&pkt_impl->list);
		mpp_list_add_tail(&pkt_impl->list, &pm->empty_list);
		pkt_impl++;
	}
	logi("create packet manager successful! (%p)", pm);

	return pm;
}

int audio_pm_destroy(struct audio_packet_manager *pm)
{

	if (!pm){
		loge("param error!!!\n");
		return -1;
	}
	pthread_mutex_destroy(&pm->lock);
	if (pm->packet_node)
		mpp_free(pm->packet_node);
	if (pm->buffer_start)
		mpp_free(pm->buffer_start);
	if (pm)
		mpp_free(pm);
	logi("destroy packet manager");

	return 0;
}

int audio_pm_dequeue_empty_packet(struct audio_packet_manager *pm, struct mpp_packet *packet, size_t size)
{
	struct audio_packet_impl *pkt_impl;
	int left_size;		// remain size in end of stream buffer

	if (!pm || !packet || size <= 0){
		loge("param error!!!\n");
		return -1;
	}

	//logd("packet manager dequeue empty mpp packet, pm: %p, packet: %p, size: %d", pm, packet, size);

	pthread_mutex_lock(&pm->lock);
	left_size = pm->buffer_size - pm->write_offset;

	if (pm->available_size < size) {
		logd("packet manager dequeue mpp packet size %zu > available size %d", size, pm->available_size);
		pthread_mutex_unlock(&pm->lock);
		return -1;
	}

	if (pm->write_offset >= pm->read_offset) {
		if (left_size < size && pm->read_offset < size) {
			logd("packet manager dequeue mpp packet size failed,size:%zu, read offset %d write offset %d",size,
			     pm->read_offset, pm->write_offset);
			pthread_mutex_unlock(&pm->lock);
			return -1;
		}
	}

	pkt_impl = mpp_list_first_entry_or_null(&pm->empty_list, struct audio_packet_impl, list);
	if(!pkt_impl) {
		logd("packet manager dequeue mpp packet failed!");
		pthread_mutex_unlock(&pm->lock);
		return -1;
	}

	pkt_impl->pkt.size = size;
	pkt_impl->pkt.data = pm->buffer_start + pm->write_offset;
	pkt_impl->pos_offset = 0;

	if (pm->write_offset >= pm->read_offset) {
		if (left_size >= size) {
			//* if left_size is enough to store this packet
			pm->write_offset += size;
			pm->available_size -= size;

			if (pm->write_offset == pm->buffer_size)
				pm->write_offset = 0;
		} else if (pm->read_offset >= size) {
			logd("left_size: %d, size: %zu", left_size, size);
			//* if left_size is not enough to store this packet,
			//*  we store this packet from the start of stream buffer
			pm->write_offset = size;
			pm->available_size -= (size + left_size);
			pkt_impl->pos_offset = left_size;
			pkt_impl->pkt.data = pm->buffer_start;
		} else {
			// left_size is not enough to store this packet,
			// and there is no buffer in start of stream buffer
			logd("packet manager dequeue mpp packet failed!");
			pthread_mutex_unlock(&pm->lock);
			return -1;
		}
	} else {
		pm->write_offset += size;
		pm->available_size -= size;
	}

	//logd("get empty packet : data%p, size: %d", pkt_impl->pkt.data, size);
	packet->data = pkt_impl->pkt.data;
	packet->size = pkt_impl->pkt.size;
	pm->empty_num --;
	logd("empty_num:%d,ready_num:%d\n",pm->empty_num,pm->ready_num);

	pthread_mutex_unlock(&pm->lock);

	return 0;
}

int audio_pm_enqueue_ready_packet(struct audio_packet_manager *pm, struct mpp_packet *packet)
{
	struct audio_packet_impl *pkt_impl;
	size_t left_offset;

	if (!pm || !packet){
		loge("param error!!!\n");
		return -1;
	}

	pthread_mutex_lock(&pm->lock);

	pkt_impl = mpp_list_first_entry_or_null(&pm->empty_list, struct audio_packet_impl, list);
	if(!pkt_impl) {
		logd("packet manager enqueue ready mpp packet failed!");
		pthread_mutex_unlock(&pm->lock);
		return -1;
	}

	if (pkt_impl->pkt.data != packet->data || pkt_impl->pkt.size < packet->size) {
		logw("packet manager enqueue ready mpp packet check failed!");
		pthread_mutex_unlock(&pm->lock);
		return -1;
	}

	if (pkt_impl->pkt.size > packet->size) {
		left_offset = pkt_impl->pkt.size - packet->size;
		if (pm->write_offset == 0) {
			pm->write_offset = pm->buffer_size - left_offset;
			pm->available_size += left_offset;
		} else {
			pm->write_offset -= left_offset;
			pm->available_size += left_offset;
		}
	}

	pkt_impl->pkt.size = packet->size;
	pkt_impl->pkt.pts = packet->pts;
	pkt_impl->pkt.flag = packet->flag;

	//logd("enqueue ready packet : data%p, size: %d", pkt_impl->pkt.data, pkt_impl->pkt.size);

	mpp_list_del_init(&pkt_impl->list);
	mpp_list_add_tail(&pkt_impl->list, &pm->ready_list);
	pm->ready_num++;
	logd("empty_num:%d,ready_num:%d\n",pm->empty_num,pm->ready_num);

	pthread_mutex_unlock(&pm->lock);

	return 0;
}

int audio_pm_reclaim_ready_packet(struct audio_packet_manager *pm, struct mpp_packet *packet)
{

	logd("packet manager return ready mpp packet");

	if (!pm || !packet)
		return -1;

	pthread_mutex_lock(&pm->lock);

	pm->ready_num++;

	pthread_mutex_unlock(&pm->lock);

	return 0;
}

struct mpp_packet *audio_pm_dequeue_ready_packet(struct audio_packet_manager *pm)
{
	struct audio_packet_impl *pkt_impl;

	//logd("packet manager dequeue ready packet");

	if (!pm){
		loge("param error!!!\n");
		return NULL;
	}
	pthread_mutex_lock(&pm->lock);

	pkt_impl = mpp_list_first_entry_or_null(&pm->ready_list, struct audio_packet_impl, list);
	if(!pkt_impl) {
		pthread_mutex_unlock(&pm->lock);
		return NULL;
	}
	pm->ready_num--;
	logd("empty_num:%d,ready_num:%d\n",pm->empty_num,pm->ready_num);

	pthread_mutex_unlock(&pm->lock);

	return (struct mpp_packet *)pkt_impl;
}

int audio_pm_enqueue_empty_packet(struct audio_packet_manager *pm, struct mpp_packet *packet)
{
	struct audio_packet_impl *pkt_impl = (struct audio_packet_impl *)packet;
	size_t read_offset;

	//logd("packet manager enqueue empty packet");

	if (!pm || !pkt_impl){
		loge("param error!!!\n");
		return -1;
	}
	pthread_mutex_lock(&pm->lock);

	mpp_list_del_init(&pkt_impl->list);
	mpp_list_add_tail(&pkt_impl->list, &pm->empty_list);

	read_offset = pm->read_offset + pkt_impl->pos_offset + pkt_impl->pkt.size;
	if (read_offset >= pm->buffer_size)
		read_offset -= pm->buffer_size;

	pm->read_offset = read_offset;
	pm->available_size += pkt_impl->pos_offset + pkt_impl->pkt.size;
	pm->empty_num++;
	logd("empty_num:%d,ready_num:%d\n",pm->empty_num,pm->ready_num);

	pthread_mutex_unlock(&pm->lock);

	return 0;
}

int audio_pm_get_empty_packet_num(struct audio_packet_manager *pm)
{
	return pm->empty_num;
}

int audio_pm_get_ready_packet_num(struct audio_packet_manager *pm)
{
	return pm->ready_num;
}

int audio_pm_reset(struct audio_packet_manager *pm)
{
	if (!pm){
		loge("audio_pm_reset fail:fm=NULL\n");
		return -1;
	}
	pthread_mutex_lock(&pm->lock);
	if(!mpp_list_empty(&pm->ready_list)){
		struct audio_packet_impl *pkt1=NULL,*pkt2=NULL;
		mpp_list_for_each_entry_safe(pkt1,pkt2,&pm->ready_list,list){
			mpp_list_del_init(&pkt1->list);
			mpp_list_add_tail(&pkt1->list, &pm->empty_list);
		}
	}
	logd("read_offset:%d,write_offset:%d,empty_num:%d,ready_num:%d,available_size:%d\n"
	,pm->read_offset
	,pm->write_offset
	,pm->empty_num
	,pm->ready_num
	,pm->available_size);

	pm->read_offset = 0;
	pm->write_offset = 0;
	pm->empty_num =pm->packet_count;
	pm->ready_num = 0;
	pm->available_size = pm->buffer_size;

	pthread_mutex_unlock(&pm->lock);
	return 0;
}