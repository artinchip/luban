// SPDX-License-Identifier: Apache-2.0
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
#define CMD_SIZE              IFNAMSIZ + 15 /* eth max name + strlen("ifconfig %s down") */

#define USAGE \
	"Usage:              test_eth [-n count] [-d delay] [-i eth0]\n" \
	"example:            test_eth -n 100 -d 0 -i eth0\n" \
	"Options:\n" \
	"    -n count        Number of echo frame to test, default 500.\n" \
	"    -d delay        Delay time(S) between two loopback test,default 0.\n" \
	"    -i eth_name     Select the name of the network card, such as eth0 or eth1 ..., default eth0.\n"

static int phy_link_poll(int socket_fd, struct ifreq *ethreq, int timeout_s)
{
	do {
		if (ioctl(socket_fd, SIOCGIFFLAGS, ethreq) < 0) {
			printf("Failed to get %s flags\n", ethreq->ifr_name);
			return -1;
		}

		if ((ethreq->ifr_flags & IFF_RUNNING) != 0)
			break;
		sleep(1);
		printf("Wait for phy connected...\n");
	}while(timeout_s-- > 0);

	if (timeout_s <= 0)
		return -1;

	return 0;
}

static int sock_timeout_set_1s(int socket_fd)
{
	struct timeval timeout;
	int ret_val;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	ret_val = setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	if (ret_val != 0) {
		printf("SO_SNDTIMEO set error!\n");
		return -1;
	}

	ret_val = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (ret_val != 0) {
		printf("SO_RCVTIMEO set error!\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret_val;
	int socket_fd;
	unsigned int count = 0;
	unsigned int max_count = DEFAULT_TEST_COUNT;
	unsigned int delay_s = 0;
	char ethx[IFNAMSIZ + 4] = {"eth0"};
	char cmd[CMD_SIZE] = {0};
	struct ifreq ethreq = {0};
	int c;
	char *end;
	char ethernet_recv_buf[UDP_BUF_SIZE];
	const char ethernet_send_msg[] =
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
	struct sockaddr_ll *sll;
	int poll_times_s = 5;
	size_t leng;

	if (argc > 7) {
		printf("Argument error\n");
		exit(-1);
	}
	while ((c = getopt(argc, argv, "n:d:i:h")) != -1) {
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
		case 'i':
			if (strncpy(ethx, optarg, IFNAMSIZ) == NULL) {
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

	/* start up net interface */
	leng = strlen(ethx);
	snprintf(cmd, CMD_SIZE, "ifconfig %.*s up", (int)leng, ethx);
	system(cmd);

	socket_fd = socket(AF_PACKET, SOCK_RAW, htons(FRAME_TYPE));
	if (socket_fd < 0) {
		printf("socket alloc error!\n");
		exit(-1);
	}

	sll = (struct sockaddr_ll *)malloc(sizeof(struct sockaddr_ll));
	if (sll == NULL) {
		printf("sockaddr_ll malloc error\n");
		close(socket_fd);
		exit(-1);
	}

	strncpy(ethreq.ifr_name, ethx, IFNAMSIZ);

	/* check phy connected */
	if (phy_link_poll(socket_fd, &ethreq, poll_times_s) < 0) {
		printf("Phy connect timeout...\n");
		goto out;
	}

	/* bind net interface */
	if (ioctl(socket_fd, SIOCGIFINDEX, &ethreq) < 0) {
		printf("Failed to bind interface: %s\n", ethreq.ifr_name);
		goto out;
	}
	sll->sll_ifindex = ethreq.ifr_ifindex;

	/* set socket send/recv timeout 1S */
	if (sock_timeout_set_1s(socket_fd) < 0) {
		goto out;
	}

	while (1) {
		ret_val = sendto(socket_fd, ethernet_send_msg, sizeof(ethernet_send_msg), 0, (void *)sll, sizeof(*sll));
		if (ret_val <= 0) {
			printf("send error! errno is: %d\n", errno);
			goto err_out;
		}
		ret_val = recvfrom(socket_fd, ethernet_recv_buf, UDP_BUF_SIZE, 0, NULL, NULL);
		if (ret_val <= 0) {
			printf("recv error! errno is:%d\n", errno);
			goto err_out;
		}
		if (memcmp(ethernet_send_msg, ethernet_recv_buf, sizeof(ethernet_send_msg)) != 0) {
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
	free(sll);
	close(socket_fd);

	snprintf(cmd, CMD_SIZE, "ifconfig %.*s down", (int)leng, ethx);
	system(cmd);

	exit(0);
}
