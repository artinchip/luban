/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 ArtInChip Technology Co.,Ltd
 */
#ifndef __USB_DETECT_H__
#define __USB_DETECT_H__

int usb_host_udisk_connection_check(void);
void usb_dev_connection_check_start(int id);
void usb_dev_connection_check_end(int id);
int usb_dev_connection_check_status(int id);

#endif
