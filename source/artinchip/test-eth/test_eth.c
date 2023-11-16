// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Artinchip Technology Co., Ltd.
 * Authors:  wulv <lv.wu@artinchip.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <linux/mii.h>
#include <linux/types.h>
#include <linux/sockios.h>
#include <errno.h>

#define UDP_BUF_SIZE          256
#define DEFAULT_SLEEP_TIME    1000000
#define DEFAULT_TEST_COUNT    500 
#define FRAME_TYPE            0xFFFF

static char ethernet_recv_buf[UDP_BUF_SIZE];
static const char ethernet_send_msg[] =
{
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	'A','r','t','I','n','C','h','i','p',
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
	0xA5,0x5A,0xA5,0x5A,0xA5,0x5A,
};

#define USAGE \
	"Usage:              test_eth [-n count] [-d delay]\n" \
	"Options:\n" \
	"    -n count        Number of echo frame to test, default 500.\n" \
	"    -d delay        Delay time(S) between two loopback test,default 0.\n"

static struct sockaddr_ll sll;

int main(int argc, char **argv)
{
	int ret_val;
	int socket_fd;
	unsigned int count = 0;
	unsigned int max_count = DEFAULT_TEST_COUNT;
	unsigned int delay_s = 0;
	struct ifreq ethreq = {0};
	int c;
	char *end;

	if (argc > 5) {
		printf("Argument error\n");
		exit(-1);
	}
	while ((c = getopt(argc, argv, "n:d:h")) != -1) {
		switch (c) {
		case 'd':
			if (optarg)
			delay_s = strtoul(optarg, &end, 10);
			if (end == NULL) {
				printf("%s", USAGE);
				exit(-1);
			}
			break;

		case 'n':
			max_count = strtoul(optarg, &end, 10);
			if (end == NULL) {
				printf("%s", USAGE);
				exit(-1);
			}
			break;

		case 'h':
		default:
			printf("%s", USAGE);
			exit(-1);
		}
	}

	socket_fd = socket(AF_PACKET, SOCK_RAW, htons(FRAME_TYPE));
	if (socket_fd < 0) {
		printf("socket alloc error!\n");
		exit(-1);
	}

#if 0 /* RTL8201 loopback cause phy linked down */
	/* set phy loopback */
	struct mii_ioctl_data *mii;
	strncpy(ethreq.ifr_name, "eth0", IFNAMSIZ);
	if (ioctl(socket_fd, SIOCGMIIPHY, &ethreq) < 0) {
		printf("ioctl error\n");
		close(socket_fd);
		exit(-1);
	}
	mii = (struct mii_ioctl_data *)&ethreq.ifr_ifru;
	mii->reg_num = 0;
	mii->val_in = 0x4100;
	ioctl(socket_fd, SIOCSMIIREG, &ethreq);
#endif

	strncpy(ethreq.ifr_name, "eth0", IFNAMSIZ);
	if (ioctl(socket_fd, SIOCGIFINDEX, &ethreq) < 0) {
		printf("ioctl error\n");
		close(socket_fd);
		exit(-1);
	}
	sll.sll_ifindex = ethreq.ifr_ifindex;

	/* set socket send/recv timeout 1S */
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	ret_val = setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	if (ret_val != 0) {
		printf("SO_SNDTIMEO set error!\n");
		goto out;
	}

	ret_val = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (ret_val != 0) {
		printf("SO_RCVTIMEO set error!\n");
		goto out;
	}

	while (1) {
		ret_val = sendto(socket_fd, ethernet_send_msg, sizeof(ethernet_send_msg), 0, (void *)&sll, sizeof(sll));
		if (ret_val <= 0) {
			printf("send error! errno is: %d\n", errno);
			goto err_out;
		}

		ret_val = recvfrom(socket_fd, ethernet_recv_buf, UDP_BUF_SIZE, 0, NULL, NULL);
		if (ret_val <= 0) {
			printf("recv error! errno is:%d\n", errno);
			goto err_out;
		}

		if (memcmp(ethernet_send_msg, ethernet_recv_buf, sizeof(ethernet_send_msg)) != 0){
			printf("Test frame mismatch error\n");
			goto err_out;
		}

		printf("Ethernet circle test round :%d OK\n", count + 1);

		if (++count >= max_count) {
			printf("Ethernet circle test OK. totle: %d\n", count);
			goto out;
		}

		if (delay_s != 0) {
			sleep(delay_s);
		}
	}

err_out:
	printf("Ethernet circle test error, success count %d\n", count);

out:
	close(socket_fd);
	return 0;
}
