#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<poll.h>
#include<sys/types.h>
#include<sys/socket.h>

#include "wifi.h"
#include "wpa_event.h"
#include "wpa_core.h"
#include "wpa_supplicant_conf.h"
#include "log.h"
#include "scan.h"

struct wpaManager wpa = {
	.evt = WPAE_UNKNOWN,
	.assocRejectCnt = 0,
	.netNotFoundCnt = 0,
	.authFailCnt = 0,
	.evtFd = {-1, -1},
	.netIdConnecting = {'\0'},
};

struct wpaManager *a = &wpa;

int wifi_state_callback_index = 0;

void evtSockeExit()
{
	if(a->EvtSocketEnable) {
		clearEvtSocket();
		if(a->evtFd[0] >= 0) {
			close(a->evtFd[0]);
			a->evtFd[0] = -1;
		}
		if(a->evtFd[1] >= 0) {
			close(a->evtFd[1]);
			a->evtFd[1] = -1;
		}
	}
}
int evtSocketInit()
{
	int ret = -1;
	if(a->evtFd[0] >=0 || a->evtFd[1] >= 0) {
		evtSockeExit();
	}
	a->evtFd[0] = a->evtFd[1] = -1;
	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, a->evtFd);
	if(ret == -1) {
		wifimanager_debug(MSG_ERROR, "scan socketpair init error\n");
		return ret;
	}
	a->EvtSocketEnable = true;
	return 0;
}
int clearEvtSocket()
{
	int ret = -1;
	char c;
	int flags;
	if(! a->EvtSocketEnable) {
		wifimanager_debug(MSG_ERROR, "event socket is closed\n");
		return ret;
	}
	/* clear scan.sockets data before sending scan command*/
	if(flags = fcntl(a->evtFd[1], F_GETFL, 0) < 0) {
		wifimanager_debug(MSG_ERROR, "fcntl getfl error\n");
		return -1;
	}

	flags |= O_NONBLOCK;

	if(fcntl(a->evtFd[1], F_SETFL, flags) < 0) {
		wifimanager_debug(MSG_ERROR, "fcntl setfl error\n");
		return -1;
	}

	while ((ret= TEMP_FAILURE_RETRY(read(a->evtFd[1], &c, 1))) > 0) {
		wifimanager_debug(MSG_DEBUG, "clear data %d\n", c);
	}

}

int evtSend(enum wpaEvent event)
{
	int ret = -1;
	char data;

	if(! a->EvtSocketEnable) {
		wifimanager_debug(MSG_ERROR, "event socket is closed\n");
		return ret;
	}

	data = (char)event;
	ret = TEMP_FAILURE_RETRY(write(a->evtFd[0], &data, 1));

	return ret;
}
int evtRead(enum wpaEvent *event)
{
	int ret = -1;
	struct pollfd rfds;
	char c;

	if(! a->EvtSocketEnable) {
		wifimanager_debug(MSG_ERROR, "event socket is closed\n");
		return ret;
	}

	memset(&rfds, 0, sizeof(struct pollfd));
	rfds.fd = a->evtFd[1];
	rfds.events |= POLLIN;

	/*wait  event.*/
	ret = TEMP_FAILURE_RETRY(poll(&rfds, 1, 70000));
	if (ret < 0) {
		wifimanager_debug(MSG_ERROR, "Error poll = %d\n", ret);
		return ret;
	}
	if(ret == 0) {
		wifimanager_debug(MSG_ERROR, "poll time out!\n");
		return ret;
	}
	if (rfds.revents & POLLIN) {
		ret = TEMP_FAILURE_RETRY(read(a->evtFd[1], &c, 1));
		*event = (enum wpaEvent)c;
		wifimanager_debug(MSG_DEBUG, "read event %d\n", c);
	}
	return ret;
}

static void handle_event(int event, char * remainder) {
	char netid_connected[NET_ID_LEN+1] = {0};
	char cmd[255] = {0}, reply[16]={0};
	int len = NET_ID_LEN+1;
	switch (event) {
		case WPAE_DISCONNECTED:
			if(w->StaEvt.state == CONNECTED ||
				w->StaEvt.state == NETWORK_CONNECTED) {
				wifimanager_debug(MSG_INFO, "Network disconnected!\n");
				system("ipaddr flush dev wlan0");
				evtSend(DISCONNECTED);
				w->StaEvt.state = DISCONNECTED;
				w->StaEvt.event = WSE_AUTO_DISCONNECTED;
				wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
			}
			break;

		case WPAE_CONNECTED:
			if(w->StaEvt.state == CONNECTING) {
				evtSend(WPAE_CONNECTED);
			}else {
				w->StaEvt.state = CONNECTED;
				w->StaEvt.event = WSE_AUTO_CONNECTED;
				wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
			}
			break;

		case WPAE_SCAN_FAILED:
		case WPAE_SCAN_RESULTS:
			if(isScanEnable())
				evtSend(event);
			break;

		case WPAE_NETWORK_NOT_FOUND:
			a->netNotFoundCnt ++;
			wifimanager_debug(MSG_DEBUG, "NETWORK NOT FOUND %d times!\n", a->netNotFoundCnt);

			if((a->netNotFoundCnt >= NETNOTFOUNDCNT) &&
				w->StaEvt.state == CONNECTING) {
				evtSend(WPAE_NETWORK_NOT_FOUND);
			}
			break;
		case WPAE_UNKNOWN:
			break;
	}
}

static int dispatch_event(const char *event_str, int nread)
{
	int i = 0, event = 0;
	char event_nae[18];
	char cmd[255] = {0}, reply[16]={0};
	char *nae_start = NULL, *nae_end = NULL;
	char *event_data = NULL;

	if(!event_str || !event_str[0]) {
		wifimanager_debug(MSG_WARNING, "event is NULL!\n");
		return 0;
	}

	/* WPAS send command to client, eg...
	 * 1. CTRL-REQ-<field name>-<network id>-<human readable text>, 
	 *    We need to reply such command like "CTRL-RSP-<field name>-<network id>-<value>"
	 * 2. WPA:XXX
	 * 3. WPS-XXX
	 * 4. CTRL-EVENT-<field>-<text> means WPAS notice client info
	 */
	wifimanager_debug(MSG_DEBUG, "WPAS Request <<< %s\n", event_str);
	if(strncmp(event_str, "CTRL-EVENT-", 11)) {
		if (!strncmp(event_str, "WPA:", 4)) {
			if (strstr(event_str, "pre-shared key may be incorrect")) {
				a->authFailCnt ++;
				wifimanager_debug(MSG_DEBUG, "pre-shared key may be incorrect %d times\n", a->authFailCnt);
				if(a->authFailCnt >= MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
					evtSend(WPAE_PASSWORD_INCORRECT);
				}
				return 0;
			}
		}
		return 0;
	}

	nae_start = (char *)((unsigned long)event_str+11);
	nae_end = strchr(nae_start, ' ');
	if(nae_end) {
		while((nae_start < nae_end) && (i < 18)) {
			event_nae[i] = *nae_start++;
			i++;
		}
		i = (i == 18) ? 17: i;
		event_nae[i] = '\0';
	} else {
		wifimanager_debug(MSG_DEBUG, "Received wpa_supplicant event with empty event nae!\n");
		return 0;
	}

	/*
	 * Map event nae into event enum
	*/
	if(!strcmp(event_nae, "CONNECTED")) {
		event = WPAE_CONNECTED;
	}else if(!strcmp(event_nae, "DISCONNECTED")) {
		event = WPAE_DISCONNECTED;
	}else if(!strcmp(event_nae, "STATE-CHANGE")) {
		event = WPAE_STATE_CHANGE;
	}else if(!strcmp(event_nae, "SCAN-FAILED")) {
		event = WPAE_SCAN_FAILED;
	}else if(!strcmp(event_nae, "SCAN-RESULTS")) {
		event = WPAE_SCAN_RESULTS;
	}else if(!strcmp(event_nae, "LINK-SPEED")) {
		event = WPAE_LINK_SPEED;
	}else if(!strcmp(event_nae, "TERMINATING")) {
		event = WPAE_TERMINATING;
	}else if(!strcmp(event_nae, "DRIVER-STATE")) {
		event = WPAE_DRIVER_STATE;
	}else if(!strcmp(event_nae, "EAP-FAILURE")) {
		event = WPAE_EAP_FAILURE;
	}else if(!strcmp(event_nae, "NETWORK-NOT-FOUND")) {
		event = WPAE_NETWORK_NOT_FOUND;
	}else if(!strcmp(event_nae, "ASSOC-REJECT")) {
		event = WPAE_ASSOC_REJECT;
	}else{
		event = WPAE_UNKNOWN;
	}

	event_data = (char *)((unsigned long)event_str);
	if(event == WPAE_DRIVER_STATE || event == WPAE_LINK_SPEED) {
		return 0;
	}else if(event == WPAE_STATE_CHANGE || event == WPAE_EAP_FAILURE) {
		event_data = strchr(event_str, ' ');
		if(event_data) {
			event_data++;
		}
	}else{
		event_data = strstr(event_str, " - ");
		if (event_data) {
			event_data += 3;
		}
	}

	if(event == WPAE_STATE_CHANGE) {
		wifimanager_debug(MSG_DEBUG, "STATE_CHANGE, no care!\n");
		return 0;
	} else if(event == WPAE_DRIVER_STATE) {
		wifimanager_debug(MSG_DEBUG, "DRIVER_STATE, no care!\n");
		return 0;
	}else if(event == WPAE_TERMINATING) {
		wifimanager_debug(MSG_ERROR, "Wpa supplicant terminated!\n");
		evtSend(WPAE_TERMINATING);
		w->enable = false;
		w->StaEvt.state = DISCONNECTED;
		w->StaEvt.event = WSE_WPA_TERMINATING;
		wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
		return 1;
	}else if(event == WPAE_EAP_FAILURE) {
		wifimanager_debug(MSG_ERROR, "EAP FAILURE!\n");
		return 0;
	}else if(event == WPAE_ASSOC_REJECT) {
		a->assocRejectCnt ++;
		if(a->assocRejectCnt >= MAX_ASSOC_REJECT_COUNT) {
			/* send disconnect */
			sprintf(cmd, "%s", "DISCONNECT");
			wifi_command(cmd, reply, sizeof(reply));
			evtSend(WPAE_ASSOC_REJECT);
			wifimanager_debug(MSG_ERROR, "ASSOC REJECT!\n");
		}
		wifimanager_debug(MSG_DEBUG, "ASSOC REJECT!\n");
		return 0;
	}else{
		handle_event(event, event_data);
	}
	return 0;
}
void *event_handle_thread(void* args)
{
	char buf[EVENT_BUF_SIZE] = {0};
	int nread = 0, ret = 0;

	for(;;) {
		nread = wifi_wait_for_event(buf, sizeof(buf));
		if (nread > 0) {
			ret = dispatch_event(buf, nread);
			if(ret == 1) {
				break;  // wpa_supplicant terminated
			}
		} else {
			continue;
		}
	}

	pthread_exit(NULL);
}

void wpa_start_event_loop()
{
	pthread_create(&a->evtThreadId, NULL, event_handle_thread, NULL);
}

void wpa_stop_event_loop()
{
	pthread_cancel(a->evtThreadId);
	pthread_join(a->evtThreadId, NULL);
	usleep(10000);
}
