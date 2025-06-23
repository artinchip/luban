#ifndef __WIFI_DO_CMD_H__
#define __WIFI_DO_CMD_H__

#if __cplusplus
extern "C" {
#endif
#include <poll.h>
#include <wpa_core.h>

enum da_command {
	DA_COMMAND_CONNECT,
	DA_COMMAND_SCAN,
	DA_COMMAND_REMOVE_NET,
	DA_COMMAND_LIST_NETWOTK,
	DA_COMMAND_MSG_TRANSPORT,
	DA_COMMAND_NET_STATUS,
	__DA_COMMAND_MAX
};

struct da_requst {
	enum da_command command;
	char ssid[SSID_MAX];
	char pwd[PWD];
};

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

#define WIFIDAEMOIN_RUN_STATE_DIR "/var/run/wifidaemon"
#define DAEMON_IF "socket"

/* Indexes of special file descriptors in the poll array. */
#define CTL_IDX_SRV 0
#define __CTL_IDX_MAX 1

#define WIFI_MAX_CLIENTS 1

struct da_ctl {
	bool socket_created;
	struct pollfd pfds[__CTL_IDX_MAX + WIFI_MAX_CLIENTS];
	bool enable;
};

void da_ctl_free(void);
int wifi_daemon_ctl_init(void);
void ctl_loop();

#if __cplusplus
};
#endif

#endif
