
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "wifimanager.h"
#include "wifi_do_cmd.h"
#include "wifi_comm.h"
#include "log.h"



static int wifi_post_cmd(int fd, const struct da_requst *req_ptr)
{
	if (send(fd, req_ptr, sizeof(struct da_requst), MSG_NOSIGNAL) == -1) {
		wifimanager_debug(MSG_ERROR, "send command %d error:%s\n", req_ptr->command, strerror(errno));
		return -1;
	}
	return 0;
}

static int wifi_connect_daemon(const char *interface, struct client *c)
{
	int fd, err;

	struct sockaddr_un saddr = { .sun_family = AF_UNIX };
	snprintf(saddr.sun_path, sizeof(saddr.sun_path) - 1,
			WIFIDAEMOIN_RUN_STATE_DIR "/%s", interface);

	if ((fd = socket(PF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0)) == -1)
		return -1;

	wifimanager_debug(MSG_DEBUG, "Connecting to socket: %s\n", saddr.sun_path);
	if (connect(fd, (struct sockaddr *)(&saddr), sizeof(saddr)) == -1) {
		err = errno;
		close(fd);
		errno = err;
		wifimanager_debug(MSG_ERROR, "WifiManager has been closed\n");
		return -1;
	}

	c->da_fd = fd;

	return 0;
}

int wifi_command_request(struct da_requst *ptr_req, struct client *c)
{
	if(wifi_connect_daemon(DAEMON_IF, c) < 0)
		return -1;

	if(wifi_post_cmd(c->da_fd, ptr_req) < 0)
		return -1;

	close(c->da_fd);
}
