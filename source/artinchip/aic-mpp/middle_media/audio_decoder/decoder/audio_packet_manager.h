/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_audio_decoder interface
*/


#ifndef _AUDIO_PACKET_MANAGER_H_
#define _AUDIO_PACKET_MANAGER_H_


#include "aic_audio_decoder.h"

struct audio_packet_manager;
/*
	create packet manager
*/
struct audio_packet_manager *audio_pm_create(struct aic_audio_decode_config *cfg);

/*
	destroy packet manager
*/
int audio_pm_destroy(struct audio_packet_manager *pm);

/*
	get a empty packet for external caller
*/
int audio_pm_dequeue_empty_packet(struct audio_packet_manager *pm, struct mpp_packet *packet, size_t size);

/*
	external caller put the packet to ready list, which is ready for decoding
*/
int audio_pm_enqueue_ready_packet(struct audio_packet_manager *pm, struct mpp_packet *packet);

/*
	get a ready packet for codec.
*/
struct mpp_packet *audio_pm_dequeue_ready_packet(struct audio_packet_manager *pm);

/*
	codec put the packet to empty list
*/
int audio_pm_enqueue_empty_packet(struct audio_packet_manager *pm, struct mpp_packet *packet);

/*
	get the packet number of empty list
*/
int audio_pm_get_empty_packet_num(struct audio_packet_manager *pm);

/*
	get the packet number of ready list
*/
int audio_pm_get_ready_packet_num(struct audio_packet_manager *pm);

/*
	reclaim the packet to ready list, if the packet not decoded
*/
int audio_pm_reclaim_ready_packet(struct audio_packet_manager *pm, struct mpp_packet *packet);

int audio_pm_reset(struct audio_packet_manager *pm);

#endif /* PACKET_MANAGER_H */
