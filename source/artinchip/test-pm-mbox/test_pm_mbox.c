/*
 * Copyright (C) 2023-2025 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  dwj <weijie.ding@artinchip.com>
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <sys/time.h>
#include <pthread.h>
#include <linux/rpmsg_aic.h>
#include <artinchip/sample_base.h>

#define MAX_REMOTE_NUMBER               3
#define SE_IN_SECURE_MODE		0

#define GMAC_PARA_SIZE		        3


struct mbox_info {
        int fd;
        char *name;
};

struct eth_info {
        char mac[6];
        char mac_reserved[2];
        char ipv4[4];
        char ipv4_reserved[4];
};

/* Open a device file to be needed. */
static int device_open(char *_fname, int _flag)
{
        s32 fd = -1;

        fd = open(_fname, _flag);
        if (fd < 0) {
                ERR("Failed to open %s errno: %d[%s]\n",
                    _fname, errno, strerror(errno));
                exit(0);
        }
        return fd;
}

static int get_eth_info(const char *eth_name, struct eth_info *info)
{
        struct ifreq ifr;
        int sock = -1;

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock == -1) {
                return -1;
        }

        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth_name, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
                close(sock);
                ERR("Interface %s: Can't get IF status.\n", eth_name);
                return -1;
        }

        if (!(ifr.ifr_ifru.ifru_flags & IFF_RUNNING)) {
                system("ifconfig eth0 up");
                sleep(1);
        }

        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
                close(sock);
                return -1;
        }
        memcpy(info->mac, ifr.ifr_hwaddr.sa_data, sizeof(info->mac));

        if (ioctl(sock, SIOCGIFADDR, &ifr) == -1) {
                close(sock);
                if (errno == 19)
                        ERR("Interface %s: No such device.\n", eth_name);
                if (errno == 99)
                        ERR("Interface %s: No IPv4 address assigned.\n", eth_name);
                return -1;
        }
        memcpy(info->ipv4, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, sizeof(info->ipv4));

        close(sock);

        return 0;
}

int main(int argc, char **argv)
{
        int ret = 0, i;
        struct aic_rpmsg *msg;
        void *msg_new;
        struct mbox_info mbox_dev[MAX_REMOTE_NUMBER];

        memset(mbox_dev, 0, sizeof(mbox_dev));
        mbox_dev[0].name = "/dev/rpmsg0";
        mbox_dev[1].name = "/dev/rpmsg1";
        mbox_dev[2].name = "/dev/mbox0";

        for (i = 0; i < MAX_REMOTE_NUMBER; i++) {
                mbox_dev[i].fd = device_open(mbox_dev[i].name, O_RDWR);
	}

	/* check idle */
        msg = malloc(sizeof(struct aic_rpmsg));
        if (!msg)
                goto __exit_close;
	msg->cmd = RPMSG_CMD_IS_IDLE;
        msg->seq = 0;
        msg->len = 0;
#if SE_IN_SECURE_MODE
	for (i = 0; i < MAX_REMOTE_NUMBER; i++) {
#else
	for (i = 0; i < MAX_REMOTE_NUMBER - 1; i++) {
#endif
                ret = write(mbox_dev[i].fd, msg, sizeof(struct aic_rpmsg));
                if (ret < 0) {
                        ERR("Failed to send msg RPMSG_CMD_IS_IDLE to %s\n",
                            mbox_dev[i].name);
                        goto __exit_eth;
                }
        }

#if SE_IN_SECURE_MODE
	for (i = 0; i < MAX_REMOTE_NUMBER; i++) {
#else
	for (i = 0; i < MAX_REMOTE_NUMBER - 1; i++) {
#endif
                ret = read(mbox_dev[i].fd, msg, sizeof(struct aic_rpmsg));
                if (ret < 0) {
                        ERR("Failed to read msg from %s!\n", mbox_dev[i].name);
                        goto __exit_eth;
                } else if (msg->cmd != RPMSG_CMD_ACK) {
			ERR("Comunication error %s!\n", mbox_dev[i].name);
                        goto __exit_eth;
		}
        }

	/* pass GMAC params to SPSS */
        struct eth_info *eth_info;
        eth_info = malloc(sizeof(struct eth_info));
        if (!eth_info)
                goto __exit_eth;
        memset(eth_info, 0, sizeof(struct eth_info));
        get_eth_info("eth0", eth_info);
        DBG("get mac info MAC addr: %x:%x:%x:%x:%x:%x, IPv4:%d.%d.%d.%d\n",
                 eth_info->mac[0], eth_info->mac[1], eth_info->mac[2], eth_info->mac[3], eth_info->mac[4], eth_info->mac[5],
                 eth_info->ipv4[0], eth_info->ipv4[1], eth_info->ipv4[2], eth_info->ipv4[3]);
	int *info = (int *)eth_info;
        msg_new = realloc(msg, sizeof(struct aic_rpmsg) + sizeof(unsigned int) * GMAC_PARA_SIZE);
        if (!msg_new)
                goto __exit;
        msg = (struct aic_rpmsg *)msg_new;
	msg->cmd = RPMSG_CMD_GMAC_PARAMS;
        msg->seq = 0;
        msg->len = GMAC_PARA_SIZE;
	msg->data[0] = info[0]; //MAC addr low 6 bytes
	msg->data[1] = info[1]; //MAC addr high 2 bytes
	msg->data[2] = info[2]; //IP addr

	ret = write(mbox_dev[0].fd, msg, AIC_RPMSG_REAL_SIZE(GMAC_PARA_SIZE));
        if (ret < 0) {
                ERR("Failed to send msg RPMSG_CMD_GMAC_PARAMS to %s\n",
                    mbox_dev[0].name);
                goto __exit;
        }

	ret = read(mbox_dev[0].fd, msg, AIC_RPMSG_REAL_SIZE(GMAC_PARA_SIZE));
        if (ret < 0) {
                ERR("Failed to read msg from %s!\n", mbox_dev[0].name);
                goto __exit;
        } else if (msg->cmd != RPMSG_CMD_ACK) {
		ERR("Comunication GMAC_PARAMS error %s!\n", mbox_dev[0].name);
                goto __exit;
	}

	/* request standby */
        msg->cmd = RPMSG_CMD_REQ_STANDBY;
        msg->seq = 0;
        msg->len = 0;

        /*********Notify SPSS/SCSS/SESS to standby********/
        for (i = 0; i < MAX_REMOTE_NUMBER; i++) {
                ret = write(mbox_dev[i].fd, msg, sizeof(struct aic_rpmsg));
                if (ret < 0) {
                        ERR("Failed to send msg REQ_STANDBY to %s\n",
                            mbox_dev[i].name);
                        goto __exit;
                }
        }

#if SE_IN_SECURE_MODE
	for (i = 0; i < MAX_REMOTE_NUMBER; i++) {
#else
	for (i = 0; i < MAX_REMOTE_NUMBER - 1; i++) {
#endif
                ret = read(mbox_dev[i].fd, msg, sizeof(struct aic_rpmsg));
                if (ret < 0) {
                        ERR("Failed to read msg from %s!\n", mbox_dev[i].name);
                        goto __exit;
                } else if (msg->cmd != RPMSG_CMD_ACK) {
			ERR("Comunication error %s!\n", mbox_dev[i].name);
                        goto __exit;
		}
        }

        // CSYS enter to standby
        system("echo mem > /sys/power/state");
        printf("CSYS resumed\n");
__exit:
        free(eth_info);

__exit_eth:
        free(msg);

__exit_close:
        /* close fd */
        for (i = 0; i < MAX_REMOTE_NUMBER; i++)
                close(mbox_dev[i].fd);

        return ret;
}
