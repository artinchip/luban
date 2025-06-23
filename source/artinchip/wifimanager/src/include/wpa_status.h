#ifndef __WPA_STATUS_H__
#define __WPA_STATUS_H__


#if __cplusplus
extern "C" {
#endif

#define WPA_STA_MAX_SSID	 48
#define WPA_STA_MAX_BSSID	 18
#define WPA_STA_MAX_IP_ADDR  16
#define WPA_STA_MAX_KEY_MGMT 16
#define WPA_STA_MAX_MAC_ADDR 18

enum wpa_states {
	WPA_UNKNOWN = 0,
	WPA_COMPLETED,
	WPA_DISCONNECTED,
	WPA_INTERFACE_DISABLED,
	WPA_INACTIVE,
	WPA_SCANNING,
	WPA_AUTHENTICATING,
	WPA_ASSOCIATING,
	WPA_ASSOCIATED,
	WPA_4WAY_HANDSHAKE,
	WPA_GROUP_HANDSHAKE,
};

struct wpa_status {
	int id;
	int freq;
	int rssi;
	int link_speed;
	int noise;
	char bssid[WPA_STA_MAX_BSSID];
	char ssid[WPA_STA_MAX_SSID];
	enum wpa_states wpa_state;
	char ip_address[WPA_STA_MAX_IP_ADDR];
	char key_mgmt[WPA_STA_MAX_KEY_MGMT];
	char mac_address[WPA_STA_MAX_MAC_ADDR];
};

struct wpa_status *get_wpa_status_info();
void wpa_status_info_free();
int search_wpa_string(const char *src, const char *obj, int max, char *get_str);
char *strstr_wpa(const char *src, const char *obj, const char pre_str[2], int pst_len);
enum wpa_states wpa_supplicant_state_convert(char *str);

#if __cplusplus
};  // extern "C"
#endif

#endif /* __STATUS_INFO_H */
