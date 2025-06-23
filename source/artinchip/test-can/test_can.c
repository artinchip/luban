// SPDX-License-Identifier: Apache-2.0
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
#define MAX_SIZE	60

int test_can_loopback(char *dev_name)
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
	strcpy(ifr.ifr_name, dev_name);
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

void usage(char *program)
{
	printf("Usage: %s DEV_NAME\n", program);
	printf("\tExample:\n");
	printf("\t\t%s can0\n", program);
	printf("\t\t%s can1\n", program);
}

int main(int argc, char *argv[])
{
	int ret;
	char buf[MAX_SIZE];

	if (argc != 2 || (strcmp(argv[1], "can0") && strcmp(argv[1], "can1"))) {
		usage(argv[0]);
		return -1;
	}

	snprintf(buf, MAX_SIZE,
		 "ip link set %s type can bitrate 1000000 loopback on",
		 argv[1]);
	system(buf);
	snprintf(buf, MAX_SIZE, "ifconfig %s up", argv[1]);
	system(buf);

	ret = test_can_loopback(argv[1]);
	snprintf(buf, MAX_SIZE, "ifconfig %s down", argv[1]);
	system(buf);
	if (!ret) {
		printf("%s loopback test success\n", argv[1]);
		return 0;
	} else {
		printf("%s loopback test error\n", argv[1]);
		return -1;
	}
}

