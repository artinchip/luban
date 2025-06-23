#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/types.h>
#include <poll.h>
#include "wifi.h"
#include "wpa_event.h"
#include "scan.h"
#include "wpa_supplicant_conf.h"
#include "wpa_core.h"
#include "log.h"
#include "wifi_udhcpc.h"
#include "wifimanager.h"

#define WPA_SSID_LENTH  512

struct Manager wmg = {
	.StaEvt = {
		.state = DISCONNECTED,
		.event = WSE_UNKNOWN,
	},
	.enable = 0,
};

struct Manager *w = &wmg;

static wifimanager_cb_t wifi_manager_callback;

static char wpa_scan_ssid[WPA_SSID_LENTH];
static char wpa_conf_ssid[WPA_SSID_LENTH];
static int  ssid_contain_chinese = 0;

const char * wmg_state_txt(enum wmgState state)
{
	switch (state) {
	case DISCONNECTED:
		return "DISCONNECTED";
	case CONNECTING:
		return "CONNECTING";
	case CONNECTED:
		return "CONNECTED";
	case OBTAINING_IP:
		return "OBTAINING_IP";
	case NETWORK_CONNECTED:
		return "NETWORK_CONNECTED";
	default:
		return "UNKNOWN";
	}
}

const char* wmg_event_txt(enum wmgEvent event)
{
	switch (event) {
	case WSE_ACTIVE_CONNECT:
		return "WSE_ACTIVE_CONNECT";
	case WSE_WPA_TERMINATING:
		return "WSE_WPA_TERMINATING";
	case WSE_AP_ASSOC_REJECT:
		return "WSE_AP_ASSOC_REJECT";
	case WSE_NETWORK_NOT_EXIST:
		return "WSE_NETWORK_NOT_EXIST";
	case WSE_PASSWORD_INCORRECT:
		return "WSE_PASSWORD_INCORRECT";
	case WSE_OBTAINED_IP_TIMEOUT:
		return "WSE_OBTAINED_IP_TIMEOUT";
	case WSE_CONNECTED_TIMEOUT:
		return "WSE_CONNECTED_TIMEOUT";
	case WSE_DEV_BUSING:
		return "WSE_DEV_BUSING";
	case WSE_CMD_OR_PARAMS_ERROR:
		return "WSE_CMD_OR_PARAMS_ERROR";
	case WSE_KEYMT_NO_SUPPORT:
		return "WSE_KEYMT_NO_SUPPORT";
	case WSE_AUTO_DISCONNECTED:
		return "WSE_AUTO_DISCONNECTED";
	case WSE_ACTIVE_DISCONNECT:
		return "WSE_ACTIVE_DISCONNECT";
	case WSE_STARTUP_AUTO_CONNECT:
		return "WSE_STARTUP_AUTO_CONNECT";
	case WSE_AUTO_CONNECTED:
		return "WSE_AUTO_CONNECTED";
	case WSE_ACTIVE_OBTAINED_IP:
		return "WSE_ACTIVE_OBTAINED_IP";
	case WSE_UNKNOWN:
		return "WSE_UNKNOWN";
	default:
		return "UNKNOWN";
	}
}

void wifi_fsm_handle(enum wmgState state, enum wmgEvent event)
{
	if (wifi_manager_callback.stat_change_cb)
		wifi_manager_callback.stat_change_cb(state, event);

	switch(state)
	{
		case CONNECTING:
		{
			wifimanager_debug(MSG_DEBUG, "[wifi state machine]:Connecting to the network......\n");
			break;
		}
		case CONNECTED:
		{
			wifimanager_debug(MSG_DEBUG, "[wifi state machine]:Connected to the AP\n");
			start_udhcpc();
			break;
		}

		 case OBTAINING_IP:
		 {
			wifimanager_debug(MSG_DEBUG, "[wifi state machine]:Request IP address...\n");
			break;
		 }

		case NETWORK_CONNECTED:
		{
			wifimanager_debug(MSG_DEBUG, "[wifi state machine]:Successful network connection\n");
			break;
		}
		case DISCONNECTED:
		{
			extern int wifimanager_deinit();
			wifimanager_debug(MSG_DEBUG, "[wifi state machine]:Disconnected, the reason:%s\n", wmg_event_txt(w->StaEvt.event));
			if (event == WSE_WPA_TERMINATING) {
				wifimanager_debug(MSG_ERROR, "ERROR! wpa_supplicant is closed\n");
				wifimanager_deinit();
			}
				
			break;
		}
		default:
			wifimanager_debug(MSG_DEBUG, "[wifi state machine]:unknown state");
			break;
	}
}

enum wmgState wifi_get_wifi_state()
{
	return w->StaEvt.state;
}
enum wmgEvent wifi_get_wifi_event()
{
	return w->StaEvt.event;
}

static int clearManagerdata()
{
	w->StaEvt.state = STATE_UNKNOWN;
	w->StaEvt.event = WSE_UNKNOWN;
	w->enable = false;
}

int wifi_get_scan_results(char *result, int *len)
{
	if(! w->enable) {
		return -1;
	}

	if(direct_get_scan_results_inner(result, len) != 0)
	{
		wifimanager_debug(MSG_ERROR, "%s: There is a scan or scan_results error, Please try scan again later!\n", __func__);
		return -1;
	}

	if (wifi_manager_callback.scan_result_cb)
		wifi_manager_callback.scan_result_cb(result);

	return 0;
}

/* check wpa/wpa2 passwd is right */
int check_wpa_passwd(const char *passwd)
{
	int result = 0;
	int i=0;

	if(!passwd || *passwd =='\0') {
		return 0;
	}

	for(i=0; passwd[i]!='\0'; i++) {
		/* non printable char */
		if((passwd[i]<32) || (passwd[i] > 126)) {
			result = 0;
			break;
		}
	}

	if(passwd[i] == '\0') {
		result = 1;
	}

	return result;
}

/* convert app ssid which contain chinese in utf-8 to wpa scan ssid */
int ssid_app_to_wpa_scan(const char *app_ssid, char *scan_ssid)
{
	unsigned char h_val = 0, l_val = 0;
	int i = 0;
	int chinese_in = 0;

	if(!app_ssid || !app_ssid[0])
	{
		wifimanager_debug(MSG_ERROR, "Error: app ssid is NULL!\n");
		return -1;
	}

	if(!scan_ssid)
	{
		wifimanager_debug(MSG_ERROR, "Error: wpa ssid buf is NULL\n");
		return -1;
	}

	i = 0;
	while(app_ssid[i] != '\0')
	{
		/* ascii code */
		if((unsigned char)app_ssid[i] <= 0x7f)
		{
			*(scan_ssid++) = app_ssid[i++];
		}
		else /* covert to wpa ssid for chinese code */
		{
			*(scan_ssid++) = '\\';
			*(scan_ssid++) = 'x';
			h_val = (app_ssid[i] & 0xf0) >> 4;
			if((h_val >= 0) && (h_val <= 9)) {
				*(scan_ssid++) = h_val + '0';
			}else if((h_val >= 0x0a) && (h_val <= 0x0f)) {
				*(scan_ssid++) = h_val + 'a' - 0xa;
			}

			l_val = app_ssid[i] & 0x0f;
			if((l_val >= 0) && (l_val <= 9)) {
				*(scan_ssid++) = l_val + '0';
			}else if((l_val >= 0x0a) && (l_val <= 0x0f)) {
				*(scan_ssid++) = l_val + 'a' - 0xa;
			}
			i++;
			chinese_in = 1;
		}
	}
	*scan_ssid = '\0';

	if(chinese_in == 1) {
		return 1;
	}

	return 0;
}

/* convert app ssid which contain chinese in utf-8 to wpa conf ssid */
int ssid_app_to_wpa_conf(const char *app_ssid, char *conf_ssid)
{
	unsigned char h_val = 0, l_val = 0;
	int i = 0;
	int chinese_in = 0;

	if(!app_ssid || !app_ssid[0])
	{
		wifimanager_debug(MSG_ERROR, "Error: app ssid is NULL!\n");
		return -1;
	}

	if(!conf_ssid)
	{
		wifimanager_debug(MSG_ERROR, "Error: wpa ssid buf is NULL\n");
		return -1;
	}

	i = 0;
	while(app_ssid[i] != '\0')
	{
		h_val = (app_ssid[i] & 0xf0) >> 4;
		if((h_val >= 0) && (h_val <= 9)) {
			*(conf_ssid++) = h_val + '0';
		}else if((h_val >= 0x0a) && (h_val <= 0x0f)) {
			*(conf_ssid++) = h_val + 'a' - 0xa;
		}

		l_val = app_ssid[i] & 0x0f;
		if((l_val >= 0) && (l_val <= 9)) {
			*(conf_ssid++) = l_val + '0';
		}else if((l_val >= 0x0a) && (l_val <= 0x0f)) {
			*(conf_ssid++) = l_val + 'a' - 0xa;
		}
		i++;
	}
	*conf_ssid = '\0';

	return 0;
}

static int connect_command_handle(char *cmd, char *net_id)
{
	int ret;
	char reply[REPLY_BUF_SIZE] = {0};

	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret) {
		wifimanager_debug(MSG_ERROR, "%s failed, Remove the information just connected!\n", cmd);
		sprintf(cmd, "REMOVE_NETWORK %s", net_id);
		wifi_command(cmd, reply, sizeof(reply));
		sprintf(cmd, "%s", "SAVE_CONFIG");
		wifi_command(cmd, reply, sizeof(reply));
		ret = -1;
		return ret;
	}

	return 0;
}

void cancel_saved_conf_handle(const char *net_id)
{
	char reply[REPLY_BUF_SIZE] = {0};

	char cmd[CMD_LEN+1] = {0};

	sprintf(cmd, "DISABLE_NETWORK %s", net_id);
	wifi_command(cmd, reply, sizeof(reply));

	sprintf(cmd, "%s", "DISCONNECT");
	wifi_command(cmd, reply, sizeof(reply));

	sprintf(cmd, "REMOVE_NETWORK %s", net_id);
	cmd[CMD_LEN] = '\0';
	wifi_command(cmd, reply, sizeof(reply));

	sprintf(cmd, "%s", "SAVE_CONFIG");
	wifi_command(cmd, reply, sizeof(reply));
}

int wifi_is_busing()
{
	if(w->StaEvt.state == CONNECTING ||
		w->StaEvt.state == OBTAINING_IP) {
		return -1;
	}else {
		return 0;
	}
}

static int wait_event(const char *netIdOld, const char *netIdNew, int isExist)
{
	char reply[REPLY_BUF_SIZE] = {0};
	char cmd[CMD_LEN+1] = {0};
	int ret ;
	enum wpaEvent evt = WPAE_UNKNOWN;

	a->assocRejectCnt = 0;
	a->netNotFoundCnt = 0;
	a->authFailCnt = 0;

	ret = evtRead(&evt);

	if(ret >= 0) {
		switch(evt) {
			case WPAE_CONNECTED:
				if(isExist == 1 || isExist == 3) {
				  //network is exist or connected
					sprintf(cmd, "REMOVE_NETWORK %s", netIdOld);
					cmd[CMD_LEN] = '\0';
					wifi_command(cmd, reply, sizeof(reply));
				}
				if(isExist != -1) {
					/* save config */
					sprintf(cmd, "%s", "SAVE_CONFIG");
					wifi_command(cmd, reply, sizeof(reply));
					wifimanager_debug(MSG_DEBUG, "wifi connected in inner!\n");
				}
				w->StaEvt.state = CONNECTED;
				break;

			case WPAE_PASSWORD_INCORRECT:
				wifimanager_debug(MSG_DEBUG, "password incorrect!\n");
				w->StaEvt.event = WSE_PASSWORD_INCORRECT;
				break;

			case WPAE_NETWORK_NOT_FOUND:
				wifimanager_debug(MSG_DEBUG, "network not found!\n");
				w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
				break;

			case WPAE_ASSOC_REJECT:
				wifimanager_debug(MSG_DEBUG, "assoc reject!\n");
				w->StaEvt.event = WSE_AP_ASSOC_REJECT;
				break;

			case WPAE_TERMINATING:
				wifimanager_debug(MSG_DEBUG, "wpa_supplicant terminating!\n");
				w->StaEvt.event = WSE_WPA_TERMINATING;
				break;

			default:
				break;
		}
		if(evt != WPAE_CONNECTED) {
			if(ret == 0) {
				w->StaEvt.event = WSE_CONNECTED_TIMEOUT;
				wifimanager_debug(MSG_DEBUG, "connected timeout!\n");
			}
			if(netIdNew)
				cancel_saved_conf_handle(netIdNew);
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
		}
	}
	return ret;
}

int wifi_connect_ap_inner(const char *ssid, tKEY_MGMT key_mgmt, const char *passwd, int event_label)
{
	int i=0, ret = -1, len = 0, max_prio = -1;
	char cmd[CMD_LEN+1] = {0};
	char reply[REPLY_BUF_SIZE] = {0}, netid1[NET_ID_LEN+1]={0}, netid2[NET_ID_LEN+1] = {'\0'};
	int is_exist = 0;
	int passwd_len = 0;

	w->StaEvt.state = CONNECTING;
	w->StaEvt.event = WSE_ACTIVE_CONNECT;

	wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);

	/* set connecting event label */
	a->label= event_label;

	/*clear event data in socket*/
	clearEvtSocket();

	/* check already exist or connected */
	len = NET_ID_LEN+1;
	is_exist = wpa_conf_is_ap_exist(ssid, key_mgmt, netid1, &len);

	/* add network */
	strncpy(cmd, "ADD_NETWORK", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = wifi_command(cmd, netid2, sizeof(netid2));
	if(ret) {
		wifimanager_debug(MSG_ERROR, "do add network results error!\n");
		ret = -1;
		w->StaEvt.event= WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}
	/* set network ssid */
	if(ssid_contain_chinese == 0) {
		sprintf(cmd, "SET_NETWORK %s ssid \"%s\"", netid2, ssid);
	}else{
		sprintf(cmd, "SET_NETWORK %s ssid %s", netid2, ssid);
	}

	if(connect_command_handle(cmd, netid2)) {
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	/* no passwd */
	if (key_mgmt == WIFIMG_NONE) {
		/* set network no passwd */
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid2);
		if(connect_command_handle(cmd, netid2)) {
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}
	} else if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK) {
		/* set network psk passwd */
		sprintf(cmd, "SET_NETWORK %s key_mgmt WPA-PSK", netid2);
		if(connect_command_handle(cmd, netid2)) {
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}

		ret = check_wpa_passwd(passwd);
		if(ret == 0) {
			wifimanager_debug(MSG_ERROR, "check wpa-psk passwd is error!\n");

			/* cancel saved in wpa_supplicant.conf */
			sprintf(cmd, "REMOVE_NETWORK %s", netid2);
			wifi_command(cmd, reply, sizeof(reply));
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}

		sprintf(cmd, "SET_NETWORK %s psk \"%s\"", netid2, passwd);
		if(connect_command_handle(cmd, netid2)) {
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}
	} else if(key_mgmt == WIFIMG_WEP) {
		/* set network  key_mgmt none */
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid2);
		if(connect_command_handle(cmd, netid2)) {
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}

		/* set network wep_key0 */
		passwd_len = strlen(passwd);
		if((passwd_len == 10) || (passwd_len == 26)) {
			sprintf(cmd, "SET_NETWORK %s wep_key0 %s", netid2, passwd);
			wifimanager_debug(MSG_DEBUG, "The passwd is HEX format!\n");
		} else if((passwd_len == 5) || (passwd_len == 13)) {
			sprintf(cmd, "SET_NETWORK %s wep_key0 \"%s\"", netid2, passwd);
			wifimanager_debug(MSG_DEBUG, "The passwd is ASCII format!\n");
		} else {
			wifimanager_debug(MSG_ERROR, "The password does not conform to the specification!\n");
			/* cancel saved in wpa_supplicant.conf */
			cancel_saved_conf_handle(netid2);
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}
		if(connect_command_handle(cmd, netid2)) {
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}

		/* set network auth_alg */
		sprintf(cmd, "SET_NETWORK %s auth_alg OPEN SHARED", netid2);
		if(connect_command_handle(cmd, netid2)) {
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}
	} else {
		wifimanager_debug(MSG_ERROR, "Error: key mgmt is not support!\n");

		/* cancel saved in wpa_supplicant.conf */
		cancel_saved_conf_handle(netid2);
		w->StaEvt.event = WSE_KEYMT_NO_SUPPORT;
		w->StaEvt.state = DISCONNECTED;
		ret = -1;
		goto end;
	}

	/* set scan_ssid to 1 for network */
	sprintf(cmd, "SET_NETWORK %s scan_ssid 1", netid2);
	if(connect_command_handle(cmd, netid2)) {
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	  /* get max priority in wpa_supplicant.conf */
	max_prio =  wpa_conf_get_max_priority();

	/* set priority for network */
	sprintf(cmd, "SET_NETWORK %s priority %d", netid2, (max_prio+1));
	if(connect_command_handle(cmd, netid2)) {
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	/* select network */
	sprintf(cmd, "SELECT_NETWORK %s", netid2);
	if(connect_command_handle(cmd, netid2)) {
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	/* save netid */
	strcpy(a->netIdConnecting, netid2);
	wifimanager_debug(MSG_DEBUG, "net id connecting %s\n", a->netIdConnecting);

	ret = wait_event(netid1, netid2, is_exist);
end:
	return ret;
}

int wifi_connect_ap(const char *ssid, const char *passwd, int event_label)
{
	int  i = 0, ret = 0;
	int  key[4] = {0};
	const char *p_ssid = NULL;

	if(! w->enable) {
		wifimanager_debug(MSG_ERROR, "Not connected to wpa_supplicant\n");
		return -1;
	}

	if(!ssid || !ssid[0]) {
		wifimanager_debug(MSG_ERROR, "Error: ssid is NULL!\n");
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}
	if(wifi_is_busing() < 0) {
		w->StaEvt.event = WSE_DEV_BUSING;
		w->StaEvt.state = DISCONNECTED;
		wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
		return -1;
	}

	 /* convert app ssid to wpa scan ssid */
	ret = ssid_app_to_wpa_scan(ssid, wpa_scan_ssid);
	if(ret < 0) {
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}else if(ret > 0) {
		ssid_contain_chinese = 1;
	}else {
		ssid_contain_chinese = 0;
	}

	/* has no chinese code */
	if(ssid_contain_chinese == 0) {
		p_ssid = ssid;
	}else{
		ssid_app_to_wpa_conf(ssid, wpa_conf_ssid);
		p_ssid = wpa_conf_ssid;
	}

	/* try connecting*/
	if(!passwd || !passwd[0]) {
		ret = wifi_connect_ap_inner(p_ssid, WIFIMG_NONE, passwd, event_label);
	} else {
		ret = wifi_connect_ap_inner(p_ssid, WIFIMG_WPA_PSK, passwd, event_label);
	}

	if(ret  >= 0 || w->StaEvt.event == WSE_PASSWORD_INCORRECT
		|| w->StaEvt.event == WSE_WPA_TERMINATING)
		goto end;

	wifimanager_debug(MSG_DEBUG, "The first connection failed, scan it and connect again\n");
	/*If the connection fails, scan it  and connect again. */

	/* checking network exist at first time */
	get_key_mgmt(p_ssid, key);

	/* no password */
	if(!passwd || !passwd[0]) {
		if(key[0] == 0) {
			ret = -1;
			w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}

		ret = wifi_connect_ap_inner(p_ssid, WIFIMG_NONE, passwd, event_label);
	}else{
		if((key[1] == 0) && (key[2] == 0) && (key[3] == 0)) {
			ret = -1;
			w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}
		/* wpa-psk */
		if(key[1] == 1 || key[3] == 1) {
			/* try WPA-PSK */
			ret = wifi_connect_ap_inner(p_ssid, WIFIMG_WPA_PSK, passwd, event_label);
		}

		/* wep */
		if(key[2] == 1) {
		/* try WEP */
			ret = wifi_connect_ap_inner(p_ssid, WIFIMG_WEP, passwd, event_label);
		}
	}
end:
	//enable all networks in wpa_supplicant.conf
	wpa_conf_enable_all_networks();
	wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
	return ret;
}

int wifi_clear_network(const char *ssid)
{
	int ret = 0;
	int len = 0;
	char cmd[CMD_LEN+1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char net_id[NET_ID_LEN+1] = {0};
	struct wpa_status *staInfo;
	char *ptr = NULL;
	char *ptr_id =NULL;
	if(! w->enable) {
		wifimanager_debug(MSG_ERROR, "Not connected to wpa_supplicant\n");
		return -1;
	}

	if(!ssid || !ssid[0]) {
		 wifimanager_debug(MSG_ERROR, "Error: ssid is null!\n");
		return -1;
	}

	if(wifi_is_busing() < 0) {
		return -1;
	}

	/* cancel saved in wpa_supplicant.conf */
	strncpy(cmd, "LIST_NETWORKS", 15);
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret) {
		wifimanager_debug(MSG_ERROR, "do remove network %s error!\n", net_id);
		ret = -1;
		goto end;

	}

	ptr = strstr(reply, ssid);
	if(ptr == NULL)
		goto end;
	while(1) {
		ptr--;
		if(*ptr == '\n') {
			break;
		}
	}
	while(1) {
		ptr++;
		if(*ptr >= '0' && *ptr <= '9') {
			ptr_id = ptr;
			break;
		}
	}

	if(NULL == ptr_id)
		return -1;

	while(*ptr >= '0' && *ptr <= '9') {
		len++;
		ptr++;
	}
	if(NULL == ptr_id)
		return -1;
	strncpy(net_id, ptr_id, len);
	net_id[len+1] = '\0';
	wifimanager_debug(MSG_DEBUG, "net id == %s\n", net_id);

	/* cancel saved in wpa_supplicant.conf */
	sprintf(cmd, "REMOVE_NETWORK %s", net_id);
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret) {
		wifimanager_debug(MSG_ERROR, "do remove network %s error!\n", net_id);
		ret = -1;
		goto end;

	}

	/* save config */
	sprintf(cmd, "%s", "SAVE_CONFIG");
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret) {
		wifimanager_debug(MSG_ERROR, "do save config error!\n");
		ret = -1;
		goto end;
	}
end:
	return ret;

}

int wifi_on()
{
	int ret = -1;
	struct wpa_status *staInfo;

	if(w->enable) {
		wifimanager_debug(MSG_ERROR, "ERROR, Has been opened once!\n");
		return -1;
	}

	w->StaEvt.state = CONNECTING;
	w->StaEvt.event = WSE_STARTUP_AUTO_CONNECT;

	ret = wifi_connect_to_supplicant();
	if(ret < 0) {
		wifimanager_debug(MSG_ERROR, "connect wpa_supplicant failed, please check wifi driver!\n");
		return -1;
	}

	w->enable = true;

	wpa_start_event_loop();

	evtSocketInit();
	clearEvtSocket();

	staInfo = get_wpa_status_info();

	if(staInfo != NULL) {
		if(staInfo->wpa_state == WPA_INTERFACE_DISABLED) {
			system("ifconfig wlan0 up");
			staInfo = get_wpa_status_info();
			if(staInfo == NULL) {
				return -1;
			}
		}
		if(staInfo->wpa_state == WPA_4WAY_HANDSHAKE) {
			/* wpa_supplicant already run by other process and connected an ap */
			w->StaEvt.state = CONNECTING;
			wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
		}else if(staInfo->wpa_state == WPA_COMPLETED) {
			w->StaEvt.state = CONNECTED;
			if(is_ip_exist() == 0) {
				wifimanager_debug(MSG_DEBUG, "Wifi connected but not get ip!\n");
				wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
			} else {
				wifimanager_debug(MSG_DEBUG, "Wifi already connect to %s\n", staInfo->ssid);
			}
		}else {
			w->StaEvt.state = DISCONNECTED;
		}
	}else{
		wifimanager_debug(MSG_ERROR, "sta->wpa_state is NULL\n");
		return -1;
	}

	return 0;
}

int wifi_off()
{
	if(! w->enable) {
		return 0;
	}

	wifimanager_debug(MSG_INFO, "WifiManager close!\n");

	wpa_status_info_free();

	evtSockeExit();

	wpa_stop_event_loop();

	wifi_close_supplicant_connection();

	system("ifconfig wlan0 down");

	clearManagerdata();

	system("killall udhcpc");

	return 0;
}

int wifi_register_cb(wifimanager_cb_t *cb)
{
	wifi_manager_callback.scan_result_cb = cb->scan_result_cb;
	wifi_manager_callback.stat_change_cb = cb->stat_change_cb;

	return 0;
}