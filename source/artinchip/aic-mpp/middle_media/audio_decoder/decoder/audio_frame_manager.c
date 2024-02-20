/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_audio_decoder interface
*/

#define LOG_TAG "audio_frame_manager"

#include <string.h>
#include <pthread.h>

#include "audio_frame_manager.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"

struct audio_frame_impl {
	struct aic_audio_frame frame;
	int 	samples_per_frame;
	int 	bits_per_sample;
	struct mpp_list list;
};

/*
	1 decoder get a empty frame from empty_list and  move this frame to using_list
	2 decoder porcess completely, move the frame in using_list to ready_list
	3 render get a filled frame from reay_list and move this frame to used_list
	4 when render return the frame in used_list,move the frame to empty_list
*/
struct audio_frame_manager {
	int frame_count;
	pthread_mutex_t lock;
	struct mpp_list empty_list;//idle
	struct mpp_list ready_list;// decoded completely
	struct mpp_list using_list;//decoding completely
	struct mpp_list used_list;//rending
	int empty_num;
	int ready_num;
	int using_num;
	int used_num;
	struct audio_frame_impl *frame_node;
};

#define MAX_FRAME_COUNT 128

struct audio_frame_manager *audio_fm_create(struct audio_frame_manager_cfg *cfg)
{
	struct audio_frame_manager_cfg *init_cfg = cfg;
	struct audio_frame_manager *fm;
	struct audio_frame_impl *frame_impl;
	int i;
	logi("create frame manager");
	if (!init_cfg || init_cfg->samples_per_frame <= 0) {
		loge("para error!!!\n");
		return NULL;
	}

	fm = (struct audio_frame_manager *)mpp_alloc(sizeof(struct audio_frame_manager));
	if (!fm) {
		loge("mpp_alloc audio_frame_manager error!!!\n");
		return NULL;
	}
	if (init_cfg->frame_count <= 0) {
		init_cfg->frame_count = 16;
	} else if (init_cfg->frame_count < MAX_FRAME_COUNT) {

	} else {
		init_cfg->frame_count = MAX_FRAME_COUNT;
	}
	fm->empty_num = init_cfg->frame_count;
	fm->ready_num = 0;
	fm->using_num = 0;
	fm->frame_count = init_cfg->frame_count;
	logi("frame manager frame_count:%d,per frame data len:%d \n", fm->frame_count,init_cfg->bits_per_sample*init_cfg->samples_per_frame/8);
	fm->frame_node = (struct audio_frame_impl *)mpp_alloc(fm->frame_count * sizeof(struct audio_frame_impl));
	if (!fm->frame_node) {
		loge("alloc %d count frame failed!", fm->frame_count);
		mpp_free(fm);
		return NULL;
	}
	memset(fm->frame_node, 0, fm->frame_count * sizeof(struct audio_frame_impl));
	pthread_mutex_init(&fm->lock, NULL);
	mpp_list_init(&fm->empty_list);
	mpp_list_init(&fm->ready_list);
	mpp_list_init(&fm->using_list);
	mpp_list_init(&fm->used_list);

	frame_impl = fm->frame_node;
	for (i = 0; i < fm->frame_count; i++) {
		frame_impl->bits_per_sample = init_cfg->bits_per_sample;
		frame_impl->samples_per_frame = init_cfg->samples_per_frame;
		frame_impl->frame.size = frame_impl->bits_per_sample * init_cfg->samples_per_frame/8;
		frame_impl->frame.data = mpp_alloc(frame_impl->frame.size);
		if (frame_impl->frame.data == NULL) {
			loge("mpp_alloc frame_impl error,%d!!\n",i);
		}
		frame_impl++;
	}
	if (i < fm->frame_count) {
		loge("alloc %d frame buffer failed, only %d frame buffer allocated", fm->frame_count, i);
		for (; i >= 0; i--) {
			mpp_free(fm->frame_node[i].frame.data);
			fm->frame_node[i].frame.data = NULL;
		}
		mpp_free(fm->frame_node);
		mpp_free(fm);
		return NULL;
	}
	frame_impl = fm->frame_node;
	for (i = 0; i < fm->frame_count; i++) {
		mpp_list_init(&frame_impl->list);
		mpp_list_add_tail(&frame_impl->list, &fm->empty_list);
		frame_impl++;
	}
	logi("create frame manager successful! (%p)", fm);

	return fm;
}

int audio_fm_destroy(struct audio_frame_manager *fm)
{
	int i = 0;
	struct audio_frame_impl *frame = NULL,*frame1 = NULL;

	if (!fm)
		return -1;

	pthread_mutex_lock(&fm->lock);
	if (!mpp_list_empty(&fm->empty_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->empty_list, list) {
			mpp_list_del_init(&frame->list);
		}
	}

	if (!mpp_list_empty(&fm->ready_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->ready_list, list) {
			mpp_list_del_init(&frame->list);
		}
	}

	if (!mpp_list_empty(&fm->using_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->using_list, list) {
			mpp_list_del_init(&frame->list);
		}
	}
	if (!mpp_list_empty(&fm->used_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->used_list, list) {
			mpp_list_del_init(&frame->list);
		}
	}
	mpp_list_del_init(&fm->empty_list);
	mpp_list_del_init(&fm->ready_list);
	mpp_list_del_init(&fm->using_list);
	mpp_list_del_init(&fm->used_list);
	pthread_mutex_unlock(&fm->lock);
	pthread_mutex_destroy(&fm->lock);

	frame = fm->frame_node;
	if (frame != NULL) {
		for (i = 0; i < fm->frame_count; i++) {
			if (frame->frame.data != NULL) {
				mpp_free(frame->frame.data);
				frame->frame.data = NULL;
			}
			frame++;
		}
	}
	if (fm->frame_node)
		mpp_free(fm->frame_node);
	if (fm)
		mpp_free(fm);
	logi("destroy frame manager");

	return 0;
}

struct aic_audio_frame * audio_fm_decoder_get_frame(struct audio_frame_manager *fm)
{
	struct audio_frame_impl *frm_impl;

	if (!fm)
		return NULL;
	pthread_mutex_lock(&fm->lock);
	frm_impl = mpp_list_first_entry_or_null(&fm->empty_list, struct audio_frame_impl, list);
	if(!frm_impl) {
		pthread_mutex_unlock(&fm->lock);
		return NULL;
	}
	mpp_list_del_init(&frm_impl->list);
	mpp_list_add_tail(&frm_impl->list,  &fm->using_list);
	fm->empty_num--;
	fm->using_num++;
	pthread_mutex_unlock(&fm->lock);
	logd("empty_num: %d,ready_num:%d,decoding_num:%d,rending_num:%d\n"
			,fm->empty_num,fm->ready_num,fm->using_num,fm->used_num);
	return (struct aic_audio_frame *)frm_impl;

}

int audio_fm_decoder_put_frame(struct audio_frame_manager *fm, struct aic_audio_frame *frame)
{
	struct audio_frame_impl *frm_impl = (struct audio_frame_impl *)frame;
	int match = 0;

	if (!fm || !frm_impl) {
		loge("para error:fm or frame!\n");
		return -1;
	}

	pthread_mutex_lock(&fm->lock);
	if (!mpp_list_empty(&fm->using_list)) {
		struct audio_frame_impl *frm=NULL,*frm1=NULL;
		mpp_list_for_each_entry_safe(frm, frm1, &fm->using_list, list) {
			if (frm_impl == frm) {
				match = 1;
				break;
			}
		}
	} else {
		pthread_mutex_unlock(&fm->lock);
		loge("frame addr not match!\n");
		return -1;
	}

	if (!match) {
		pthread_mutex_unlock(&fm->lock);
		loge("frame addr not match!\n");
		return -1;
	}
	mpp_list_del_init(&frm_impl->list);
	mpp_list_add_tail(&frm_impl->list, &fm->ready_list);
	fm->ready_num++;
	fm->using_num--;
	logd("empty_num: %d,ready_num:%d,decoding_num:%d,rending_num:%d\n"
			,fm->empty_num,fm->ready_num,fm->using_num,fm->used_num);

	pthread_mutex_unlock(&fm->lock);

	return 0;
}

int audio_fm_render_get_frame(struct audio_frame_manager *fm,struct aic_audio_frame *frame)
{
	struct audio_frame_impl *frm_impl = NULL;

	if (!fm)
		return -1;
	pthread_mutex_lock(&fm->lock);
	frm_impl = mpp_list_first_entry_or_null(&fm->ready_list, struct audio_frame_impl, list);
	if(!frm_impl) {
		pthread_mutex_unlock(&fm->lock);
		return -1;
	}
	mpp_list_del_init(&frm_impl->list);
	mpp_list_add_tail(&frm_impl->list,  &fm->used_list);
	fm->ready_num--;
	fm->used_num++;
	logd("empty_num: %d,ready_num:%d,decoding_num:%d,rending_num:%d\n"
			,fm->empty_num,fm->ready_num,fm->using_num,fm->used_num);

	memcpy(frame, &frm_impl->frame, sizeof(struct aic_audio_frame));
	pthread_mutex_unlock(&fm->lock);
	return 0;

}

int audio_fm_render_put_frame(struct audio_frame_manager *fm, struct aic_audio_frame *frame)
{
	struct audio_frame_impl *frm=NULL,*frm1=NULL;
	int match = 0;

	if (!fm || !frame) {
		loge("para error:fm or frame!\n");
		return -1;
	}

	pthread_mutex_lock(&fm->lock);
	if (!mpp_list_empty(&fm->used_list)) {
		mpp_list_for_each_entry_safe(frm, frm1, &fm->used_list, list) {
			if (frame->data == frm->frame.data) {
				match = 1;
				break;
			}
		}
	} else {
		pthread_mutex_unlock(&fm->lock);
		loge("frame addr not match!\n");
		return -1;
	}
	if (!match) {
		pthread_mutex_unlock(&fm->lock);
		loge("frame addr not match!\n");
		return -1;
	}
	mpp_list_del_init(&frm->list);
	mpp_list_add_tail(&frm->list, &fm->empty_list);
	fm->empty_num++;
	fm->used_num--;
	logd("empty_num: %d,ready_num:%d,decoding_num:%d,rending_num:%d\n"
			,fm->empty_num,fm->ready_num,fm->using_num,fm->used_num);


	pthread_mutex_unlock(&fm->lock);
	return 0;
}

int audio_fm_get_empty_frame_num(struct audio_frame_manager *fm)
{
	return fm->empty_num;
}

int audio_fm_get_render_frame_num(struct audio_frame_manager *fm)
{
	return fm->ready_num;
}


int audio_fm_reset(struct audio_frame_manager *fm)
{
	struct audio_frame_impl *frame = NULL,*frame1 = NULL;
	int i = 0;

	if (!fm) {
		loge("audio_fm_reset fail:fm=NULL\n");
		return -1;
	}

	pthread_mutex_lock(&fm->lock);

	if (!mpp_list_empty(&fm->used_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->used_list, list) {
			mpp_list_del_init(&frame->list);
			mpp_list_add_tail(&frame->list,  &fm->empty_list);
			logd("pts:%ld \n",frame->frame.pts);
		}
	}

	if (!mpp_list_empty(&fm->using_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->using_list, list) {
			mpp_list_del_init(&frame->list);
			mpp_list_add_tail(&frame->list,  &fm->empty_list);
			logd("pts:%ld \n",frame->frame.pts);
		}
	}

	if (!mpp_list_empty(&fm->ready_list)) {
		mpp_list_for_each_entry_safe(frame, frame1, &fm->ready_list, list) {
			mpp_list_del_init(&frame->list);
			mpp_list_add_tail(&frame->list,  &fm->empty_list);
			logd("pts:%ld \n",frame->frame.pts);
		}
	}
	//clear flags
	frame = fm->frame_node;
	for (i = 0; i < fm->frame_count; i++) {
		frame->frame.flag = 0;
		frame++;
	}

	logd("empty_num:%d,ready_num:%d,using_num:%d,used_num:%d\n",fm->empty_num,fm->ready_num,fm->using_num,fm->used_num);
	fm->empty_num = fm->frame_count;
	fm->ready_num = 0;
	fm->using_num = 0;
	fm->used_num = 0;

	pthread_mutex_unlock(&fm->lock);

	return 0;
}
