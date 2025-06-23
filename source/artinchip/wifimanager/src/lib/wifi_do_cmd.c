
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdint.h>

#include "wifimanager.h"
#include "wifi_do_cmd.h"
#include "log.h"

struct da_ctl ctl = {
	.socket_created = false,
	.enable = false,
};

static void do_connect(const struct da_requst *r, int fd)
{
	wifi_connect_ap(r->ssid, r->pwd, fd);
}

#define SCAN_MAX 4096

static void do_scan(const struct da_requst *r, int fd)
{
	char scan_results[SCAN_MAX] = {0};
	int len = SCAN_MAX;

	if(wifi_get_scan_results(scan_results, &len) < 0)
		wifimanager_debug(MSG_ERROR, "Scan error\n");
}

static void do_rm_metwork(const struct da_requst *r, int fd)
{
	if(wifi_clear_network(r->ssid) < 0)
		wifimanager_debug(MSG_ERROR, "remove %s error\n", r->ssid);
}

void da_ctl_free(void)
{
	size_t i;
	for (i = 0; i < ARRAYSIZE(ctl.pfds); i++)
		if (ctl.pfds[i].fd != -1)
			close(ctl.pfds[i].fd);

	if (ctl.socket_created) {
		char tmp[256] = WIFIDAEMOIN_RUN_STATE_DIR "/";
		unlink(strcat(tmp, DAEMON_IF));
		ctl.socket_created = false;
	}
	ctl.enable = false;
}

int wifi_daemon_ctl_init(void)
{
	size_t i;
	for (i = 0; i < ARRAYSIZE(ctl.pfds); i++) {
		ctl.pfds[i].events = POLLIN;
		ctl.pfds[i].fd = -1;
	}

	struct sockaddr_un saddr = { .sun_family = AF_UNIX };
	snprintf(saddr.sun_path, sizeof(saddr.sun_path) - 1,
			WIFIDAEMOIN_RUN_STATE_DIR "/%s", DAEMON_IF);

	if (mkdir(WIFIDAEMOIN_RUN_STATE_DIR, 0755) == -1 && errno != EEXIST) {
		wifimanager_debug(MSG_ERROR, "mkdir %s error\n", WIFIDAEMOIN_RUN_STATE_DIR);
		goto fail;
	}
	if ((ctl.pfds[CTL_IDX_SRV].fd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
		wifimanager_debug(MSG_ERROR, "socket creat %d error\n", ctl.pfds[CTL_IDX_SRV].fd);
		goto fail;
	}
	if (bind(ctl.pfds[CTL_IDX_SRV].fd, (struct sockaddr *)(&saddr), sizeof(saddr)) == -1) {
		wifimanager_debug(MSG_ERROR, "bind addr %s error\n", saddr.sun_path);
		goto fail;
	}

	ctl.socket_created = true;
	if (chmod(saddr.sun_path, 0660) == -1) {
		wifimanager_debug(MSG_ERROR, "Chmod error\n");
		goto fail;
	}

	if (listen(ctl.pfds[CTL_IDX_SRV].fd, 2) == -1) {
		wifimanager_debug(MSG_ERROR, "Cannot listen\n");
		goto fail;
	}

	ctl.enable = true;
	return 0;

fail:
	da_ctl_free();
	return -1;
}

void ctl_loop()
{
	static void (*commands[__DA_COMMAND_MAX])(const struct da_requst *, int) = {
		[DA_COMMAND_CONNECT] = do_connect,
		[DA_COMMAND_SCAN] = do_scan,
		[DA_COMMAND_REMOVE_NET] = do_rm_metwork,
	};

	while(ctl.enable) {
		if (poll(ctl.pfds, ARRAYSIZE(ctl.pfds), -1) == -1) {
			if (errno == EINTR)
				continue;
			wifimanager_debug(MSG_ERROR, "Controller poll error: %s", strerror(errno));
			break;
		}

		struct pollfd *pfd = NULL;
		size_t i;

		/* handle data transmission with connected clients */
		for (i = __CTL_IDX_MAX; i < __CTL_IDX_MAX + WIFI_MAX_CLIENTS; i++) {
			const int fd = ctl.pfds[i].fd;

			if(fd == -1) {
				/*pointed to clt.pfds */
				pfd = &ctl.pfds[i];
				continue;
			}
			if (ctl.pfds[i].revents & POLLIN) {

				struct da_requst request;
				ssize_t len;
				if ((len = recv(fd, &request, sizeof(request), MSG_DONTWAIT)) != sizeof(request)) {
					/* if the request cannot be retrieved, release resources */
					if (len == 0)
						wifimanager_debug(MSG_DEBUG, "Client closed connection: %d\n", fd);
					else
						wifimanager_debug(MSG_DEBUG, "Invalid request length: %zd != %zd\n", len, sizeof(request));

					close(fd);
					ctl.pfds[i].fd = -1;

					continue;
				}

				/* validate and execute requested command */
				if (request.command < __DA_COMMAND_MAX && commands[request.command] != NULL)
					commands[request.command](&request, fd);
				else
					wifimanager_debug(MSG_WARNING, "Invalid command: %u\n", request.command);

			}

		}

		/* process new connections to our controller */
		if (ctl.pfds[CTL_IDX_SRV].revents & POLLIN && pfd != NULL) {

			struct pollfd fd = { -1, POLLIN, 0 };
			uint16_t ver = 0;
			fd.fd = accept(ctl.pfds[CTL_IDX_SRV].fd, NULL, NULL);
			wifimanager_debug(MSG_DEBUG, "Received new connection: %d\n", fd.fd);
			//add poll waiting queue
			pfd->fd = fd.fd;
		}
	}
}
