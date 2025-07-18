#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "wpa_event.h"
#include "wifi.h"
#include "log.h"

extern void cancel_saved_conf_handle(const char *net_id);

static int get_net_ip(const char *if_name, char *ip, int *len, int *vflag)
{
	struct ifaddrs * ifAddrStruct=NULL, *pifaddr=NULL;
	void * tmpAddrPtr=NULL;

	*vflag = 0;
	getifaddrs(&ifAddrStruct);
	pifaddr = ifAddrStruct;
	while (pifaddr!=NULL) {
		if (pifaddr->ifa_addr->sa_family==AF_INET) { // check it is IP4
			tmpAddrPtr=&((struct sockaddr_in *)pifaddr->ifa_addr)->sin_addr;
			if(strcmp(pifaddr->ifa_name, if_name) == 0) {
				inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
				*vflag = 4;
				break;
			}
		} else if (pifaddr->ifa_addr->sa_family==AF_INET6) { // check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr=&((struct sockaddr_in *)pifaddr->ifa_addr)->sin_addr;
			if(strcmp(pifaddr->ifa_name, if_name) == 0) {
				inet_ntop(AF_INET6, tmpAddrPtr, ip, INET6_ADDRSTRLEN);
				*vflag=6;
				break;
			}
		}
		pifaddr=pifaddr->ifa_next;
	}

	if(ifAddrStruct != NULL) {
		freeifaddrs(ifAddrStruct);
	}
	return 0;
}

int is_ip_exist()
{
	int len = 0, vflag = 0;
	char ipaddr[INET6_ADDRSTRLEN];

	get_net_ip("wlan0", ipaddr, &len, &vflag);
	return vflag;
}

void start_udhcpc()
{
	int len = 0, vflag = 0, times = 0;
	char ipaddr[INET6_ADDRSTRLEN];
	char cmd[256] = {0}, reply[8] = {0};

	w->StaEvt.event = WSE_ACTIVE_OBTAINED_IP;
	w->StaEvt.state = OBTAINING_IP;
	wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);

	/* restart udhcpc */
	system("udhcpc -i wlan0 -t 5 -T 7 -q -n");
	sleep(2);

	/* check ip exist */
	len = INET6_ADDRSTRLEN;
	times = 0;
	do{
		get_net_ip("wlan0", ipaddr, &len, &vflag);
		sleep(1);
		times++;
	}while((vflag == 0) && (times < 30));

	if(vflag != 0) {
		w->StaEvt.state = NETWORK_CONNECTED;
		wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
	}else{
		wifimanager_debug(MSG_ERROR, "wlan0 get ip failed\n");

		cancel_saved_conf_handle(a->netIdConnecting);

		w->StaEvt.state = DISCONNECTED;
		w->StaEvt.event = WSE_OBTAINED_IP_TIMEOUT;
		wifi_fsm_handle(w->StaEvt.state, w->StaEvt.event);
	}
}
