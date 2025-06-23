#ifndef __WIFI_COMM_H__
#define __WIFI_COMM_H__

#if __cplusplus
extern "C" {
#endif

struct client {
	int da_fd;
};

int wifi_command_request(struct da_requst *ptr_req, struct client *c);

#if __cplusplus
};
#endif

#endif
