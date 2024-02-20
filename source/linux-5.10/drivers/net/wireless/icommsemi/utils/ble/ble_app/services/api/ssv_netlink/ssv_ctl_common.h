/*
 * Copyright (c) 2020 iComm-semi Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file ssv_ctl_common.h
 * @brief Common defines and declarations for host driver for all platforms.
 */


#ifndef __SSV_CTL_COMMON_H__
#define __SSV_CTL_COMMON_H__
#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 *         Include Files
 ******************************************************************************/
#include <linux/genetlink.h>


/*******************************************************************************
 *         Defines
 ******************************************************************************/
/// SSV control netlink ID.
#define SSV_CTL_NL_ID                       (998)


#ifndef _SSV_TYPES_H_
//SSV PACK Definition
#define SSV_PACKED_STRUCT_BEGIN
#define SSV_PACKED_STRUCT               //__attribute__ ((packed))
#define SSV_PACKED_STRUCT_END           //__attribute__((packed))
#define SSV_PACKED_STRUCT_STRUCT        __attribute__ ((packed))
#define SSV_PACKED_STRUCT_FIELD(x)      x
#endif

/*******************************************************************************
 *         Enumerations
 ******************************************************************************/

enum {
    SSV_CTL_ATTR_UNSPEC,
    SSV_CTL_ATTR_ENABLE,
    SSV_CTL_ATTR_SUCCESS,
    SSV_CTL_ATTR_CHANNEL,
    SSV_CTL_ATTR_PROMISC,
    SSV_CTL_ATTR_RXFRAME,
    SSV_CTL_ATTR_SI_CMD,
    SSV_CTL_ATTR_SI_STATUS,
    SSV_CTL_ATTR_SI_SSID,
    SSV_CTL_ATTR_SI_PASS,
    SSV_CTL_ATTR_RAWDATA,
    SSV_CTL_ATTR_SSV_NIMBLE_ENABLE,
    SSV_CTL_ATTR_SSV_NIMBLE_DISABLE,
    SSV_CTL_ATTR_TO_SSV_NIMBLE,
    SSV_CTL_ATTR_FROM_SSV_NIMBLE,
    __SSV_CTL_ATTR_MAX,
};
#define SSV_CTL_ATTR_MAX (__SSV_CTL_ATTR_MAX - 1)
enum {
    SSV_CTL_CMD_UNSPEC,
    SSV_CTL_CMD_SMARTLINK,
    SSV_CTL_CMD_SET_CHANNEL,
    SSV_CTL_CMD_GET_CHANNEL,
    SSV_CTL_CMD_SET_PROMISC,
    SSV_CTL_CMD_GET_PROMISC,
    SSV_CTL_CMD_RX_FRAME,
    SSV_CTL_CMD_SMARTICOMM,
    SSV_CTL_CMD_SET_SI_CMD,
    SSV_CTL_CMD_GET_SI_STATUS,
    SSV_CTL_CMD_GET_SI_SSID,
    SSV_CTL_CMD_GET_SI_PASS,
    SSV_CTL_CMD_SEND_RAWDATA,
    SSV_CTL_CMD_SSV_NIMBLE_ENABLE,
    SSV_CTL_CMD_SSV_NIMBLE_DISABLE,
    SSV_CTL_CMD_TO_SSV_NIMBLE,
    SSV_CTL_CMD_FROM_SSV_NIMBLE,
    __SSV_CTL_CMD_MAX,
};
#define SSV_CTL_CMD_MAX (__SSV_CTL_CMD_MAX - 1)



#ifdef __cplusplus
}
#endif
#endif /* __SSV_CTL_COMMON_H__ */
