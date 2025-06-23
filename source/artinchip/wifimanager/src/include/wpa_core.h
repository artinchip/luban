#ifndef __WPA_CORE_H__
#define __WPA_CORE_H__

#include <stdbool.h>
#include "wifi_udhcpc.h"
#include "wpa_status.h"

#if __cplusplus
extern "C" {
#endif

typedef enum {
	WIFIMG_NONE = 0,
	WIFIMG_WPA_PSK,
	WIFIMG_WPA2_PSK,
	WIFIMG_WEP,
}tKEY_MGMT;

enum wmgState {
	NETWORK_CONNECTED = 0,
	CONNECTING,
	OBTAINING_IP,
	DISCONNECTED,
	CONNECTED,
	STATE_UNKNOWN,
};

enum wmgEvent{
	WSE_AUTO_DISCONNECTED,
	WSE_ACTIVE_DISCONNECT,

	WSE_KEYMT_NO_SUPPORT,
	WSE_CMD_OR_PARAMS_ERROR,
	WSE_DEV_BUSING,
	WSE_CONNECTED_TIMEOUT,
	WSE_OBTAINED_IP_TIMEOUT,

	WSE_WPA_TERMINATING,
	WSE_AP_ASSOC_REJECT,
	WSE_NETWORK_NOT_EXIST,
	WSE_PASSWORD_INCORRECT,

	WSE_UNKNOWN,

	WSE_STARTUP_AUTO_CONNECT,
	WSE_AUTO_CONNECTED,
	WSE_ACTIVE_CONNECT,
	WSE_ACTIVE_OBTAINED_IP,
};

#define NETNOTFOUNDCNT 						  1
#define MAX_ASSOC_REJECT_COUNT  			  1
#define MAX_RETRIES_ON_AUTHENTICATION_FAILURE 1

#define SSID_MAX 48
#define PWD	 48

struct WmgStaEvt {
	enum wmgState state;
	enum wmgEvent event;
};

struct Manager {
	struct WmgStaEvt StaEvt;
	bool enable;
};

extern struct Manager *w;



int wifi_on();
int wifi_off();
void wifi_fsm_handle(enum wmgState state, enum wmgEvent event);
const char *wmg_event_txt(enum wmgEvent event);
const char *wmg_state_txt(enum wmgState state);

enum wmgState wifi_get_wifi_state();
enum wmgEvent wifi_get_wifi_event();

int wifi_connect_ap(const char *ssid, const char *passwd, int event_label);
int wifi_get_scan_results(char *result, int *len);
int wifi_clear_network(const char *ssid);
int wifi_is_busing();
#if __cplusplus
};  // extern "C"
#endif

#endif
