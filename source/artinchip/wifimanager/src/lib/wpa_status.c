#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "wpa_status.h"
#include "wpa_core.h"
#include "log.h"

#include "wifi.h"
static struct wpa_status *ptrStaInfo;
enum wpa_states wpa_supplicant_state_convert(char *str)
{
	if(!strncmp(str, "DISCONNECTED", 16)) {
		return WPA_DISCONNECTED;
	}
	if(!strncmp(str, "INTERFACE_DISABLED", 22)) {
		return WPA_INTERFACE_DISABLED;
	}
	if(!strncmp(str, "INACTIVE", 12)) {
		return WPA_INACTIVE;
	}
	if(!strncmp(str, "SCANNING", 12)) {
		return WPA_SCANNING;
	}
	if(!strncmp(str, "AUTHENTICATING", 18)) {
		return WPA_AUTHENTICATING;
	}
	if(!strncmp(str, "ASSOCIATED", 14)) {
		return WPA_ASSOCIATED;
	}
	if(!strncmp(str, "4WAY_HANDSHAKE", 19)) {
		return WPA_4WAY_HANDSHAKE;
	}
	if(!strncmp(str, "GROUP_HANDSHAKE", 19)) {
		return WPA_GROUP_HANDSHAKE;
	}
	if(!strncmp(str, "COMPLETED", 13)) {
		return WPA_COMPLETED;
	}
	if(!strncmp(str, "DISCONNECTED", 16)) {
		return WPA_DISCONNECTED;
	}
	return WPA_UNKNOWN;
}

char *strstr_wpa(const char *src, const char *obj, const char pre_str[2], int pst_len)
{
	const char *p=src;
	int length;
	int i=0;
	if(src == NULL || obj == NULL || pre_str ==NULL) {
		wifimanager_debug(MSG_DEBUG, "src or obj or pre_str is NULL");
		return NULL;
	}
	if(pst_len > strlen(pre_str)) {
		wifimanager_debug(MSG_ERROR, "pst_len length is illegal");
		return NULL;
	}
	length = strlen(obj);
	for(;;p++, i++) {
		p=strchr(p, *obj);
		if(p == NULL) {
			return NULL;
		}
		if(strncmp(p, obj, length) == 0) {
			if(i > 1 && *(p-1) != pre_str[0] && *(p-1) != pre_str[1]) {
					return (char*)p;
			}
		}
	}
}

int search_wpa_string(const char *src, const char *obj, int max, char *get_str)
{
	int i=0;
	const char *sptr = NULL;
	const char *pnext = NULL;
	if(obj == NULL || src == NULL) {
		wifimanager_debug(MSG_DEBUG, "src or obj is NULL");
		return 0;
	}
	if(!strncmp("id=", obj, 3)) {
		sptr = strstr_wpa(src, obj, "su", 2);
	}else if(!strncmp("ssid=", obj, 5)) {
		sptr = strstr_wpa(src, obj, "bb", 2);
	}else if(!strncmp("address=", obj, 8)) {
		sptr = strstr_wpa(src, obj, "__", 2);
	}else{
		sptr = strstr(src, obj);
	}

	if(sptr != NULL) {
		pnext=sptr+strlen(obj);
		i=0;
		while(1) {
			i++;
			if(i >max ) {
				wifimanager_debug(MSG_ERROR, "Data overflow, %s, i: %d, max: %d\n", obj, i, max);
				break;
			}
			pnext++;
			if(*pnext == '\n' || *pnext == '\0')
				break;
		}
		strncpy(get_str, sptr+strlen(obj), i);
		get_str[i]='\0';
		return 1;
	}
	return -1;
}

static int clear_wpa_status_info()
{
	int i;
	if(ptrStaInfo == NULL)
		ptrStaInfo = (struct wpa_status *)malloc(sizeof(struct wpa_status));
	if(ptrStaInfo == NULL) {
		wifimanager_debug(MSG_WARNING, "malloc status failed!\n");
		return -1;
	}
	memset(ptrStaInfo, 0, sizeof(struct wpa_status));
	ptrStaInfo->id = -1;
	ptrStaInfo->freq = -1;
	for(i = 0;i< WPA_STA_MAX_SSID;i++) {
		ptrStaInfo->ssid[i] = '\0';
		if(i < WPA_STA_MAX_BSSID)
			ptrStaInfo->bssid[i] = '\0';
		if(i < WPA_STA_MAX_IP_ADDR)
			ptrStaInfo->ip_address[i] = '\0';
		if(i < WPA_STA_MAX_KEY_MGMT)
			ptrStaInfo->key_mgmt[i]='\0';
		if(i < WPA_STA_MAX_MAC_ADDR)
			ptrStaInfo->mac_address[i]='\0';
	}
	ptrStaInfo->wpa_state = WPA_UNKNOWN;
	return 0;
}

void wpa_status_info_free()
{
	if(ptrStaInfo !=NULL) {
		free(ptrStaInfo);
		ptrStaInfo = NULL;
	}
}

struct wpa_status *get_wpa_status_info()
{
	char reply[4096] = {0};
	char wpa_result[512];

	clear_wpa_status_info();

	wifi_command("STATUS", reply, sizeof(reply));

	if(search_wpa_string(reply, "wpa_state=", 32, wpa_result) >0)
		ptrStaInfo->wpa_state = wpa_supplicant_state_convert(wpa_result);
	if(search_wpa_string(reply, "ssid=", 512, wpa_result) >0)
		strncpy(ptrStaInfo->ssid, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "id=", 4, wpa_result) >0)
		ptrStaInfo->id = atoi(wpa_result);
	if(search_wpa_string(reply, "freq=", 6, wpa_result) >0)
		ptrStaInfo->freq = atoi(wpa_result);
	if(search_wpa_string(reply, "bssid=", 18, wpa_result) >0)
		strncpy(ptrStaInfo->bssid, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "key_mgmt=", 16, wpa_result) >0)
		strncpy(ptrStaInfo->key_mgmt, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "address=", 18, wpa_result) >0)
		strncpy(ptrStaInfo->mac_address, wpa_result, strlen(wpa_result));
	if(search_wpa_string(reply, "ip_address=", 16, wpa_result) >0)
		strncpy(ptrStaInfo->ip_address, wpa_result, strlen(wpa_result));

	return ptrStaInfo;
}
