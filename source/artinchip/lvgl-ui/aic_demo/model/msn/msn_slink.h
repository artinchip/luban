/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  artinchip
 */

#ifndef _AIC_DEMO_ZHI_JIAN_SLINK_H
#define _AIC_DEMO_ZHI_JIAN_SLINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "linksupport.h"

#define SLINK_UNCONNECTED  0
#define SLINK_CONNECTING   1
#define SLINK_CONNECTED    2

struct msn_slink {
    link_player_sink sink;
    int link_status;
    int connect_type;
    int link_type;
};

struct msn_slink* msn_slink_create(LinkType connect_type);
int msn_slink_modify_connect_type(struct msn_slink* link, LinkType connect_type);
int msn_slink_get_status(struct msn_slink* link);
int msn_slink_start(struct msn_slink* link);
int msn_slink_stop(struct msn_slink* link);
int msn_slink_delete(struct msn_slink* link);

void msn_slink_set_gbtc_connect_type(bool btc_connect_type);
void msn_slink_set_gusb_is_connect_type(bool usb_is_connect_type);
void msn_slink_set_gconnect_type(int connect_type);
void msn_slink_set_gstart_link_type(int link_type);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
