/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc:  packet (video bitstream container) manager
*/

#ifndef PACKET_MANAGER_H
#define PACKET_MANAGER_H

#include "ve_buffer.h"

struct packet_manager;

struct packet {
	unsigned char *data;	// source data virtual address
	size_t size;		// source data size
	int64_t pts;		// source data pts
	unsigned int flag;	// source data eos flag
	size_t phy_offset;	// video bytestream offset
	size_t phy_size;	// video bytestream size
	unsigned int phy_base;	// vidoe bytestream physic address
};

struct packet_manager_init_cfg {
	struct ve_buffer_allocator *ve_buf_handle;	// mpp buffer handle
	size_t buffer_size;				// video bytestream size
	int packet_count;				// packet buffer count
};

/*
	create packet manager
*/
struct packet_manager *pm_create(struct packet_manager_init_cfg *cfg);

/*
	destroy packet manager
*/
int pm_destroy(struct packet_manager *pm);

/*
	get a empty packet for external caller
*/
int pm_dequeue_empty_packet(struct packet_manager *pm, struct mpp_packet *packet, size_t size);

/*
	external caller put the packet to ready list, which is ready for decoding
*/
int pm_enqueue_ready_packet(struct packet_manager *pm, struct mpp_packet *packet);

/*
	get a ready packet for codec.
*/
struct packet *pm_dequeue_ready_packet(struct packet_manager *pm);

/*
	codec put the packet to empty list
*/
int pm_enqueue_empty_packet(struct packet_manager *pm, struct packet *packet);

/*
	get the packet number of empty list
*/
int pm_get_empty_packet_num(struct packet_manager *pm);

/*
	get the packet number of ready list
*/
int pm_get_ready_packet_num(struct packet_manager *pm);

/*
	reclaim the packet to ready list, if the packet not decoded
*/
int pm_reclaim_ready_packet(struct packet_manager *pm, struct packet *packet);

int pm_reset(struct packet_manager *pm);

#endif /* PACKET_MANAGER_H */
