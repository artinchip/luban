#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "wifimanager.h"

#define LIST_NETWORK_MAX 4096

static const char *wifistate2string(wifistate_t state)
{
	switch (state) {
	case WIFI_STATE_GOT_IP:
		return "WIFI_STATE_GOT_IP";
	case WIFI_STATE_CONNECTING:
		return "WIFI_STATE_CONNECTING";
	case WIFI_STATE_DHCPC_REQUEST:
		return "WIFI_STATE_DHCPC_REQUEST";
	case WIFI_STATE_DISCONNECTED:
		return "WIFI_STATE_DISCONNECTED";
	case WIFI_STATE_CONNECTED:
		return "WIFI_STATE_CONNECTED";
	default:
		return "WIFI_STATE_ERROR";
	}
}

static const char *disconn_reason2string(wifimanager_disconn_reason_t reason)
{
	switch (reason) {
	case AUTO_DISCONNECT:
		return "wpa auto disconnect";
	case ACTIVE_DISCONNECT:
		return "active disconnect";
	case KEYMT_NO_SUPPORT:
		return "keymt is not supported";
	case CMD_OR_PARAMS_ERROR:
		return "wpas command error";
	case IS_CONNECTTING:
		return "wifi is still connecting";
	case CONNECT_TIMEOUT:
		return "connect timeout";
	case REQUEST_IP_TIMEOUT:
		return "request ip address timeout";
	case WPA_TERMINATING:
		return "wpa_supplicant is closed";
	case AP_ASSOC_REJECT:
		return "AP assoc reject";
	case NETWORK_NOT_FOUND:
		return "can't search such ssid";
	case PASSWORD_INCORRECT:
		return "incorrect password";
	default:
		return "other reason";
	}
}

static void print_scan_result(char *result)
{
	printf("%s\n", result);
}

static void print_stat_change(int stat, int reason)
{
	printf("%s\n", wifistate2string(stat));
	if (stat == WIFI_STATE_DISCONNECTED)
		printf("disconnect reason: %s\n", disconn_reason2string(reason));
}

int main(int argc, char *argv[])
{
	int opt;
	const char *opts = "c:hslriod";
	char list_net_results[LIST_NETWORK_MAX];
	wifi_status_t status = {0};

	int ret = -1;

	struct option longopts[] = {
		{"connect", required_argument, NULL, 'c'},
		{"scan", no_argument, NULL, 's'},
		{"list_network", no_argument, NULL, 'l'},
		{"remove_network", no_argument, NULL, 'r'},
		{"status", no_argument, NULL, 'i'},
		{"help", no_argument, NULL, 'h'},
		{"open", no_argument, NULL, 'o'},
		{"close", no_argument, NULL, 'd'},
		{ 0, 0, 0, 0 },
	};

	wifimanager_cb_t cb = {
		.scan_result_cb = print_scan_result,
		.stat_change_cb = print_stat_change,
	};

	if(argc == 1)
		goto usage;

	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
		switch(opt) {
			case 'c':
				if(argc > 5 || argc < 3) {
					printf("  -c, --connect\t\tconnect AP, -c <ssid> [password]\n");
				} else if (argc == 3) {
					/* Open */
					printf("connecting open wifi [ssid:%s] \n", argv[2]);
					wifimanager_connect(argv[2], NULL);
				} else {
					printf("connecting ssid:%s passward:%s\n", argv[optind-1], argv[optind]);
					wifimanager_connect(argv[optind-1], argv[optind]);
				}
				return EXIT_SUCCESS;

			case 'h':
usage:
				printf("  -h, --help\t\tprint this help\n");
				printf("  -c, --connect\t\tconnect AP, -c <ssid> [password]\n");
				printf("  -s, --scan\t\tscan AP\n");
				printf("  -l, --list\tlist all networks\n");
				printf("  -i, --status\t\tget wifi status information\n");
				printf("  -r, --remove\tremove network in config, -r <ssid>\n");
				printf("  -o, --open\topen WifiManager\n");
				printf("  -d, --close\tclose WifiManager\n");
				return EXIT_SUCCESS;

			case 's':
				ret = wifimanager_scan();
				return EXIT_SUCCESS;

			case 'l':
				ret = wifimanager_list_networks(list_net_results, LIST_NETWORK_MAX);
				if(ret != -1) {
					printf("%s\n", list_net_results);
				}
				return EXIT_SUCCESS;

			case 'i':
				ret = wifimanager_get_status(&status);
				if(ret >= 0) {
					printf("wifi state:   %s\n", wifistate2string(status.state));
					if((status.state == WIFI_STATE_GOT_IP) ||
					   (status.state == WIFI_STATE_CONNECTED) ||
					   (status.state == WIFI_STATE_DHCPC_REQUEST)) {
						printf("ssid:%s\n", status.ssid);
						printf("bssid:%s\n", status.bssid);
						printf("key mgmt: %s\n", status.key_mgmt);
						printf("current frequency: %d GHz\n", status.freq);
						printf("ip address: %s\n", status.ip_address);
						printf("mac address: %s\n", status.mac_address);

						printf("rssi: %d\n", status.rssi);
						printf("link speed: %d\n", status.link_speed);
						printf("noise: %d\n", status.noise);
					}

				}
				return EXIT_SUCCESS;

			case 'r':
				if(argc < 2) {
					printf("  -r, --remove_net\tremove network in config, -r <ssid>\n");
					return EXIT_SUCCESS;
				}
				printf("removing ssid:%s\n", argv[optind]);
				wifimanager_remove_networks(argv[optind], strlen(argv[optind]));
				return EXIT_SUCCESS;

			case 'o':
				wifimanager_init(&cb);
				printf("wifimanager init ok\n");
				return EXIT_SUCCESS;

			case 'd':
				wifimanager_deinit();
				printf("wifimanager deinit ok\n");
				return EXIT_SUCCESS;

			default:
				printf("Try '%s -h' for more information.\n", argv[0]);
				return EXIT_FAILURE;
		}
		if (optind == argc)
			goto usage;
	}
}
