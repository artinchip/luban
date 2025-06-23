/*
 * Copyright (C) 2025 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  zrq <ruiqi.zheng@artinchip.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "wpa_ctrl.h"

#define EVENT_BUF_SIZE 512
struct wpa_ctrl *g_wpa_ctrl = NULL;

/* Connect the wpa_supplicant control interface */
static int p2p_connect_wpa_supplicant(char *if_name)
{
	char wpas_socket_path[64] = {0};

	if (if_name == NULL)
		return -1;

	snprintf(wpas_socket_path, sizeof(wpas_socket_path), "/var/run/wpa_supplicant/%s", if_name);

	g_wpa_ctrl = wpa_ctrl_open(wpas_socket_path);
	if (!g_wpa_ctrl) {
		perror("[P2P_AUTO]wpa_ctrl_open failed");
		return -1;
	}
	if (wpa_ctrl_attach(g_wpa_ctrl) != 0) {
		wpa_ctrl_close(g_wpa_ctrl);
		perror("[P2P_AUTO]wpa_ctrl_attach failed");
		return -1;
	}
	return 0;
}

/* Send control cmd and check the response */
static int p2p_send_command(const char *cmd, char *reply_buf, size_t *reply_len)
{
	if (cmd == NULL)
		return -1;

	printf("[P2P_AUTO][CMD] %s\n", cmd);

	int ret = wpa_ctrl_request(g_wpa_ctrl, cmd, strlen(cmd), reply_buf, reply_len, NULL);
	if (ret < 0) {
		fprintf(stderr, "[P2P_AUTO]Command '%s' failed\n", cmd);
		return -1;
	}

	if (reply_buf == NULL || reply_len == NULL)
		return 0;

	printf("[P2P_AUTO][RESP]: %s\n", reply_buf);
	reply_buf[*reply_len] = '\0';
	if (strncmp(reply_buf, "OK\n", 3) != 0) {
		fprintf(stderr, "[P2P_AUTO]Unexpected reply: %s", reply_buf);
		return -1;
	}
	return 0;
}

/* Start P2P listen */
static int p2p_start_listen(int duration) {

	if (duration < 0)
		return -1;

	char cmd[64] = {0}, reply[EVENT_BUF_SIZE] = {0};
	size_t reply_len = sizeof(reply);

	snprintf(cmd, sizeof(cmd), "P2P_LISTEN %d", duration);
	if (p2p_send_command(cmd, reply, &reply_len) == 0) {
		printf("[P2P_AUTO]P2P listening started\n");
	} else {
		perror("[P2P_AUTO]P2P listen fail");
		return -1;
	}

	return 0;
}

/* connect device */
static void p2p_connect_device(const char *peer_mac) {
	char cmd[128] = {0}, reply[EVENT_BUF_SIZE] = {0};
	size_t reply_len = EVENT_BUF_SIZE;

	snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s pbc", peer_mac);
	if (p2p_send_command(cmd, reply, &reply_len) == 0) {
		printf("[P2P_AUTO]Accepted invitation from %s\n", peer_mac);
	}
}

/* dhcpd/dhcpc */
static void p2p_setup_network(const char *ifname, const char *role) {
	char cmd[128] = {0};
	if (strcmp(role, "GO") == 0) {
		printf("[P2P_AUTO]Acting as Group Owner\n");
		snprintf(cmd, sizeof(cmd), "ifconfig %s 192.168.49.1", ifname);
		system(cmd);
		// Dynamically generate dhcpd configuration files
		char conf_path[64] = {0};
		snprintf(conf_path, sizeof(conf_path), "/tmp/udhcpd-%s.conf", ifname);

		FILE *fp = fopen(conf_path, "w");
		if (!fp) {
			perror("[P2P_AUTO]Failed to create DHCP config");
			return;
		}

		fprintf(fp,
			"start        192.168.49.20\n"
			"end         192.168.49.254\n"
			"interface   %s\n"
			"max_leases  86400\n"
			"option subnet  255.255.255.0\n"
			"option router 192.168.49.1\n"
			"option dns    8.8.8.8 8.8.4.4\n",
			ifname);

		fclose(fp);

		memset(cmd, 0, sizeof(cmd));
		snprintf(cmd, sizeof(cmd), "udhcpd %s", conf_path);
		system(cmd);

		usleep(200000);

		unlink(conf_path);
	} else {
		printf("[P2P_AUTO]Acting as Group Client\n");
		snprintf(cmd, sizeof(cmd), "udhcpc -i %s", ifname);
		system(cmd);
	}
	usleep(200000);
}

static int p2p_event_handle(char *payload)
{
	if (payload == NULL)
		return -1;

	if (strstr(payload, "P2P-DEVICE-FOUND")) {
		char mac[18] = {0}, name[64] = {0};
		if (sscanf(payload, "P2P-DEVICE-FOUND %17s p2p_dev_addr=%*s pri_dev_type=%*s name='%63[^']",
			mac, name) >= 1) {
			printf("[P2P_AUTO][EVENT RES]Discovered device: MAC=%s Name=%s\n", mac, name);
		}
	} else if (strstr(payload, "P2P-INVITATION-RECEIVED")) {
		char sa_mac[18] = {0}, go_mac[18] = {0};
		if (sscanf(payload, "P2P-INVITATION-RECEIVED sa=%17s go_dev_addr=%17s", sa_mac, go_mac) == 2) {
			printf("[P2P_AUTO][EVENT RES]Received invitation from: SA=%s GO=%s\n", sa_mac, go_mac);
			p2p_connect_device(sa_mac);
		}
	} else if (strstr(payload, "P2P-GO-NEG-REQUEST")) {
		char mac[18] = {0};
		int dev_passwd_id, go_intent;
		if (sscanf(payload, "P2P-GO-NEG-REQUEST %17s dev_passwd_id=%d go_intent=%d",
			mac, &dev_passwd_id, &go_intent) >= 3) {
			printf("[P2P_AUTO][EVENT RES]GO negotiation request from %s (passwd_id=%d)\n", mac, dev_passwd_id);
		}
	} else if (strstr(payload, "P2P-GROUP-STARTED")) {
		char ifname[32] = {0}, role[4] = {0}, ssid[33] = {0};
		if (sscanf(payload, "P2P-GROUP-STARTED %31s %3s ssid=\"%32[^\"]", ifname, role, ssid) >= 2) {
			printf("[P2P_AUTO][EVENT RES]Group started: Interface=%s Role=%s SSID=%s\n", ifname, role, ssid);
			p2p_setup_network(ifname, role);
			return 1;
		}
	} else if (strstr(payload, "P2P-PROV-DISC-PBC-REQ")) {
		char mac[18] = {0}, name[64] = {0};
		if (sscanf(payload, "P2P-PROV-DISC-PBC-REQ %17s p2p_dev_addr=%*s %*s name='%63[^']",
			mac, name) >= 1) {
			printf("[P2P_AUTO][EVENT RES]PBC Request from %s (%s)\n", mac, name);
			p2p_connect_device(mac);
		}
	} else if (strstr(payload, "AP-STA-CONNECTED")) {
		char sta_mac[18] = {0},  p2p_dev_addr[18] = {0};

		// Two formats：
		// 1. AP-STA-CONNECTED d2:16:b4:86:4b:ef
		// 2. AP-STA-CONNECTED d2:16:b4:86:4b:ef p2p_dev_addr=d2:16:b4:86:cb:ef
		if (sscanf(payload, "AP-STA-CONNECTED %17s p2p_dev_addr=%17s", sta_mac, p2p_dev_addr) >= 1) {
			printf("[P2P_AUTO][EVENT RES] STA MAC: %s", sta_mac);
			if (strlen(p2p_dev_addr) > 0) {
				printf(" | P2P Device: %s", p2p_dev_addr);
			}
			printf("\n");
		}
		return 1;
	}
	return 0;
}

/* event loop */
static void p2p_event_loop() {
	char event[EVENT_BUF_SIZE] = {0};
	size_t event_len;

	while (1) {
		event_len = EVENT_BUF_SIZE - 1;
		if (wpa_ctrl_pending(g_wpa_ctrl) > 0 &&
			wpa_ctrl_recv(g_wpa_ctrl, event, &event_len) == 0) {
			event[event_len] = '\0';

			printf("[P2P_AUTO][RAW EVENT] %s\n", event);

			/* Processing log priority tags */
			char *payload = event;
			if (*payload == '<') {
				payload = strchr(payload, '>');
				if (payload)
					payload++;
			}

			if (p2p_event_handle(payload) == 1)
				return;
		}
		usleep(200000);
	}
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage:p2p_auto <interface name> <listen duration> \n"
							"eg.p2p_auto p2p-dev-wlan0 120\n");
		return -1;
	}

	char cmd[128] = {0};
	// start the wpa_supplicant
	snprintf(cmd, sizeof(cmd), "wpa_supplicant -iwlan0 -Dnl80211 -c/etc/wifi/p2p_supplicant.conf &");
	system(cmd);
	sleep(2);

	// connect to wpa_supplicant
	if(p2p_connect_wpa_supplicant(argv[1]) < 0) {
		return -1;
	}

	// start p2p listen
	if(p2p_start_listen(atoi(argv[2])) < 0) {
		wpa_ctrl_detach(g_wpa_ctrl);
		wpa_ctrl_close(g_wpa_ctrl);
		return -1;
	}

	// event loop
	p2p_event_loop();

	wpa_ctrl_detach(g_wpa_ctrl);
	wpa_ctrl_close(g_wpa_ctrl);

	return 0;
}


