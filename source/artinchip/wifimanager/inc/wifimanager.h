#ifndef __WIFI_MANAGER_H__
#define __WIFI_MANAGER_H__

#if __cplusplus
extern "C" {
#endif

#define WPA_STA_MAX_SSID	 48
#define WPA_STA_MAX_BSSID	 18
#define WPA_STA_MAX_IP_ADDR  16
#define WPA_STA_MAX_KEY_MGMT 16
#define WPA_STA_MAX_MAC_ADDR 18

typedef enum {
	WIFI_STATE_GOT_IP = 0,
	WIFI_STATE_CONNECTING,
	WIFI_STATE_DHCPC_REQUEST,
	WIFI_STATE_DISCONNECTED,
	WIFI_STATE_CONNECTED,
	WIFI_STATE_ERROR,
}wifistate_t;

typedef struct {
	int freq;
	wifistate_t state;
	int rssi;
	int link_speed;
	int noise;
	char bssid[WPA_STA_MAX_BSSID];
	char ssid[WPA_STA_MAX_SSID];
	char ip_address[WPA_STA_MAX_IP_ADDR];
	char key_mgmt[WPA_STA_MAX_KEY_MGMT];
	char mac_address[WPA_STA_MAX_MAC_ADDR];
}wifi_status_t;

typedef enum {
	AUTO_DISCONNECT,
	ACTIVE_DISCONNECT,
	KEYMT_NO_SUPPORT,
	CMD_OR_PARAMS_ERROR,
	IS_CONNECTTING,
	CONNECT_TIMEOUT,
	REQUEST_IP_TIMEOUT,
	WPA_TERMINATING,
	AP_ASSOC_REJECT,
	NETWORK_NOT_FOUND,
	PASSWORD_INCORRECT,
	OTHERS,
}wifimanager_disconn_reason_t;

typedef struct {
	void (*stat_change_cb)(wifistate_t stat, wifimanager_disconn_reason_t reason);
	void (*scan_result_cb)(char *result);
}wifimanager_cb_t;

int wifimanager_init(wifimanager_cb_t *cb);
int wifimanager_deinit(void);
int wifimanager_connect(const char *ssid, const char *passwd);
int wifimanager_scan(void);
int wifimanager_list_networks(char *reply, size_t len);
int wifimanager_get_status(wifi_status_t *status);
int wifimanager_remove_networks(char *pssid, int len);

#if __cplusplus
};
#endif

#endif
