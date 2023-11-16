// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Author: weijie.ding <weijie.ding@artinchip.com>
 */

/*
 * This program is used to test CAN0 loopback. You
 * should connect the TX and RX of CAN0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <linux/can.h>
#include <linux/if.h>

#define CAN_DEBUG	0

int test_can_loopback()
{
	int sock_fd, epfd, err;
	struct can_frame tx_frame, rx_frame;
	struct epoll_event event, ev;
	socklen_t len;
	struct sockaddr_can addr;
	struct ifreq ifr;

	memset(&tx_frame, 0, sizeof(struct can_frame));
	memset(&rx_frame, 0, sizeof(struct can_frame));

	sock_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	strcpy(ifr.ifr_name, "can0");
	ioctl(sock_fd, SIOCGIFINDEX, &ifr);

	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));

	tx_frame.can_id = 0x5A1;
	strcpy((char *)tx_frame.data, "hello");
	tx_frame.can_dlc = strlen((const char *)tx_frame.data);

	epfd = epoll_create(1);
	if (epfd < 0) {
		perror("epoll_create error!\n");
		return -1;
	}

	ev.events = EPOLLIN;
	err = epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &ev);
	if (err < 0) {
		perror("epoll_ctl()");
		return -1;
	}

	sendto(sock_fd, &tx_frame, sizeof(struct can_frame), 0,
	       (struct sockaddr *)&addr, sizeof(addr));

	err = epoll_wait(epfd, &event, 1, -1);

	recvfrom(sock_fd, &rx_frame, sizeof(struct can_frame), 0,
		 (struct sockaddr *)&addr, &len);

#if CAN_DEBUG
	printf("CAN frame:\n\t ID: %x\n\t DLC: %x\n\t DATA: ",
	       rx_frame.can_id, rx_frame.can_dlc);

	for (int i = 0; i < rx_frame.can_dlc; i++)
		printf("%02x ", rx_frame.data[i]);
	printf("\n");
#endif

	err = memcmp((void *)&tx_frame, (void *)&rx_frame,
		     sizeof(struct can_frame));
	if (err)
		return -1;
	else
		return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	system("ip link set can0 type can bitrate 1000000 loopback on");
	system("ifconfig can0 up");

	ret = test_can_loopback();
	system("ifconfig can0 down");
	if (!ret) {
		printf("CAN loopback test success\n");
		return 0;
	} else {
		printf("CAN loopback test error\n");
		return -1;
	}
}

