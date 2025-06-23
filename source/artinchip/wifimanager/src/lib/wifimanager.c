#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <wpa_core.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include "log.h"
#include "wifi_do_cmd.h"
#include "wifi_udhcpc.h"
#include <time.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/prctl.h>

#include "wpa_ctrl.h"
#include "wifimanager.h"
#include "wifi_comm.h"
#include "wpa_status.h"

#define CMD_LEN                   32
#define TRY_CONNECT_WPA_MAX_TIME  5
#define WPA_INTERFACE			  WIFIDAEMOIN_RUN_STATE_DIR##"/wlan0"

static int do_signal_poll(struct wpa_ctrl *ctrl_conn, wifi_status_t *status)
{
	int ret = -1;
	char cmd[CMD_LEN+1] = {0};
	size_t len = 4096;
	char reply[len];
	enum wpa_states wpa_stat;
	char wpa_result[32];

	strncpy(cmd, "SIGNAL_POLL", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	len = 4096;

	ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, &len, NULL);
    if (ret == -2) {
        wifimanager_debug(MSG_ERROR, "'%s' command timed out.\n", cmd);
    } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
        ret = -1;
    } else {
		ret = 0;
	}

	if(search_wpa_string(reply, "RSSI=", 8, wpa_result) >0)
		status->rssi = atoi(wpa_result);
	if(search_wpa_string(reply, "LINKSPEED=", 8, wpa_result) >0)
		status->link_speed = atoi(wpa_result);
	if(search_wpa_string(reply, "NOISE=", 8, wpa_result) >0)
		status->noise = atoi(wpa_result);

	return ret;
}

static int do_get_status(struct wpa_ctrl *ctrl_conn, wifi_status_t *status)
{
	int ret = -1;
	char cmd[CMD_LEN+1] = {0};
	size_t len = 4096;
	char reply[len];
	enum wpa_states wpa_stat = WPA_UNKNOWN;
	char wpa_result[32];

	strncpy(cmd, "STATUS", CMD_LEN);
	cmd[CMD_LEN] = '\0';

	ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, &len, NULL);
    if (ret == -2) {
        wifimanager_debug(MSG_ERROR, "'%s' command timed out.\n", cmd);
    } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
        ret = -1;
    } else {
		ret = 0;
	}

	if(search_wpa_string(reply, "wpa_state=", 32, wpa_result) >0)
		wpa_stat = wpa_supplicant_state_convert(wpa_result);
	if(search_wpa_string(reply, "ssid=", 512, wpa_result) >0)
		strncpy(status->ssid, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "freq=", 6, wpa_result) >0)
		status->freq = atoi(wpa_result);
	if(search_wpa_string(reply, "bssid=", 18, wpa_result) >0)
		strncpy(status->bssid, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "key_mgmt=", 16, wpa_result) >0)
		strncpy(status->key_mgmt, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "address=", 18, wpa_result) >0)
		strncpy(status->mac_address, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "ip_address=", 16, wpa_result) >0)
		strncpy(status->ip_address, wpa_result, strlen(wpa_result));

	/*wpa_supplicant state == connected*/
	if(wpa_stat == WPA_COMPLETED) {
		/*not ip address*/
		if(status->ip_address[0] == '\0') {
			status->state = CONNECTED;
		} else {
			status->state = NETWORK_CONNECTED;
		}
	} else {
		status->state = DISCONNECTED;
	}

	return ret;
}


static void ctl_loop_stop(int sig) {
	/* Call to this handler restores the default action, so on the
	 * second call the program will be forcefully terminated. */
	struct sigaction sigact = { .sa_handler = SIG_DFL };
	sigaction(sig, &sigact, NULL);
	da_ctl_free();
}

static int create_daemon(int debug)
{
	int i ;
	pid_t pid;
	if((pid = fork()) < 0)
		return -1;
	else if(pid)
		return 1;
	if(setsid() < 0)
		return -1;
	signal(SIGHUP,SIG_IGN);
	if((pid = fork()) < 0)
		return -1;
	else if(pid)
		_exit(0);
	chdir("/");
	if(!debug) {
		for(i = 0; i < 64; i++)
			close(i);

		open("/dev/null",O_RDONLY);
		open("/dev/null",O_RDWR);
		open("/dev/null",O_RDWR);
	}

	FILE *fp = fopen("/var/run/wifi_daemon.pid", "w");
	if (fp == NULL) {
		wifimanager_debug(MSG_ERROR, "open file failed\n");
		return -1;
	}
	fprintf(fp, "%d\n", getpid());
	fclose(fp);

	return 0;
}

extern int wifi_register_cb(wifimanager_cb_t *cb);
int wifimanager_init(wifimanager_cb_t *cb)
{
	int ret = 0, len = 0;
	int times = 0;
	int c;

	int i, connect_wpa_time = 0;

	wifi_register_cb(cb);

	if (create_daemon(1) > 0)
		return 0; // if currunt process owned by user, return.

	prctl(PR_SET_NAME, "WifiManager", 0, 0, 0);
	wifimanager_set_debug_level(MSG_INFO);
	if(wifi_daemon_ctl_init() < 0) {
		wifimanager_debug(MSG_ERROR, "Failed to start WifiManager.\n");
		exit(-1);
		return 0;
	}

	wifi_start_supplicant(0);

	for (i = 0 ; i<= TRY_CONNECT_WPA_MAX_TIME ;i++) {
		ret = wifi_on();
		if(ret == 0) {
			break;
		}
		sleep(1);
	}

	if (ret) {
		wifimanager_debug(MSG_ERROR, "WifiManager start failed\n");
		wifi_off();
		exit(-1);
		return -1;
	}

	wifimanager_debug(MSG_INFO, "WifiManager start Successful\n");

	/* In order to receive EPIPE while writing to the pipe whose reading end
	 * is closed, the SIGPIPE signal has to be handled. For more information
	 * see the msg_pipe_write() function. */
	struct sigaction sigact = { .sa_handler = SIG_IGN };
	sigaction(SIGPIPE, &sigact, NULL);

	/* free ctl resource */
	sigact.sa_handler = ctl_loop_stop;
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);

	ctl_loop();
	wifimanager_debug(MSG_INFO, "WifiManager has been closed\n");
	exit(0);
	return 0;
failed:
	da_ctl_free();
	exit(-1);
	return -1;
}

int wifimanager_deinit(void)
{
	pid_t wifi_daemon_pid;

	FILE *fp = fopen("/var/run/wifi_daemon.pid", "r");
	if (fp == NULL) {
		wifimanager_debug(MSG_ERROR, "open file failed\n");
		return -1;
	}
	fscanf(fp, "%d", &wifi_daemon_pid);
	fclose(fp);

	wifimanager_debug(MSG_INFO, "kill WifiManager pid %d\n", wifi_daemon_pid);
	kill(wifi_daemon_pid, SIGTERM);

	return 0;
}

int wifimanager_scan()
{
	struct da_requst req = {
		.command = DA_COMMAND_SCAN,
		.ssid = {0},
		.pwd = {0},
	};

	struct client cli = {
		0
	};

	int ret = 0;
	if(wifi_command_request(&req, &cli) < 0)
		return -1;

	return ret ;
}

int wifimanager_connect(const char *ssid, const char *passwd)
{
	struct da_requst req = {
		.command = DA_COMMAND_CONNECT,
		.ssid = {0},
		.pwd = {0},
	};
	struct client cli = {
		0
	};

	if (NULL != ssid) {
		strncpy(req.ssid, ssid, strlen(ssid));
	}

	if (NULL != passwd) {
		strncpy(req.pwd, passwd, strlen(passwd));
	}

	if(wifi_command_request(&req, &cli) < 0)
		return -1;

	return 0;
}

int wifimanager_remove_networks(char *pssid, int len)
{
	struct da_requst req = {
		.command = DA_COMMAND_REMOVE_NET,
		.ssid = {0},
		.pwd = {0},
	};
	struct client cli = {
		0
	};

	if (NULL != pssid) {
		strncpy(req.ssid, pssid, len);
	}

	if(wifi_command_request(&req, &cli) < 0)
		return -1;

	return 0 ;
}

int wifimanager_list_networks(char *reply, size_t len)
{
	struct wpa_ctrl *ctrl_conn;
	int ret = -1;
	char cmd[CMD_LEN+1] = {0};

	if(wifi_is_busing() < 0) {
		return -1;
	}

	memset(reply, 0, len);
    ctrl_conn = wpa_ctrl_open("/var/run/wifidaemon/wlan0");
    if (ctrl_conn == NULL) {
		printf("wpa_ctrl_open /var/run/wifidaemon/wlan0 failed\n");
	    return -1;
    }

	strncpy(cmd, "LIST_NETWORKS", CMD_LEN);

	ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, &len, NULL);
    if (ret == -2) {
        wifimanager_debug(MSG_ERROR, "'%s' command timed out.\n", cmd);
    } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
        ret = -1;
    }

	wpa_ctrl_close(ctrl_conn);

	return ret;
}

int wifimanager_get_status(wifi_status_t *status)
{
	struct wpa_ctrl *ctrl_conn;

	if(wifi_is_busing() < 0) {
		printf("Device is busing\n");
		return -1;
	}

	memset(status, 0, sizeof(wifi_status_t));

    ctrl_conn = wpa_ctrl_open("/var/run/wifidaemon/wlan0");
    if (ctrl_conn == NULL) {
		printf("wpa_ctrl_open /var/run/wifidaemon/wlan0 failed\n");
	    return -1;
    }

	do_get_status(ctrl_conn, status);

	if (status->state == WIFI_STATE_GOT_IP)
		do_signal_poll(ctrl_conn, status);

	wpa_ctrl_close(ctrl_conn);

	return 0;
}