/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <sys/time.h>
#include <pthread.h>
#include <linux/rpmsg_aic.h>

#include <artinchip/sample_base.h>

/* Global macro and variables */

#define MSG_CNT_DEFAULT	10
#define MSG_MAX_LEN	512

static const char sopts[] = "d:c:h";
static const struct option lopts[] = {
	{"device",	  required_argument, NULL, 'd'},
	{"count",	  required_argument, NULL, 'c'},
	{"usage",		no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static int g_msg_fd = -1;
static char g_dev_name[32] = "";
static char g_send_buf[MSG_MAX_LEN] = "";
static char g_recv_buf[MSG_MAX_LEN] = "";

/* Functions */

void usage(char *program)
{
	printf("Usage: %s [options]: \n", program);
	printf("\t -d, --device\t\tthe device file of RPMsg device\n");
	printf("\t -c, --count\t\tthe number of RPMsg msg, 10 as default\n");
	printf("\t -h, --usage \n");
	printf("\n");
	printf("Example: %s -d /dev/rpmsg0 -c 3\n", program);
}

/* Open a device file to be needed. */
int device_open(char *_fname, int _flag)
{
	s32 fd = -1;

	fd = open(_fname, _flag);
	if (fd < 0) {
		ERR("Failed to open %s errno: %d[%s]\n",
		    _fname, errno, strerror(errno));
		exit(0);
	}
	return fd;
}

static void *recv_msg_thread(void *arg)
{
	int i = 0, ret = 0, msg_cnt = *(int *)arg;
	struct aic_rpmsg *msg = (struct aic_rpmsg *)g_recv_buf;

	while (i < msg_cnt && g_msg_fd > 0) {
		memset(msg, 0, MSG_MAX_LEN);
		ret = read(g_msg_fd, msg, MSG_MAX_LEN);
		if (ret < 0) {
			ERR("Failed to read msg!\n");
			return NULL;
		}

		printf("\n%s: Recv cmd 0x%x, seq %d, len %d, data |%s|\n",
		       g_dev_name,
		       msg->cmd, msg->seq, msg->len, (char *)msg->data);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	int c, msg_cnt = MSG_CNT_DEFAULT;
	int i, ret = 0;
	struct timeval start, end;
	struct aic_rpmsg *msg = (struct aic_rpmsg *)g_send_buf;
	pthread_t thid = 0;

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			strncpy(g_dev_name, optarg, 32);
			continue;
		case 'c':
			msg_cnt = str2int(optarg);
			continue;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			break;
		}
	}

	g_msg_fd = device_open(g_dev_name, O_RDWR);
	if (strstr(g_dev_name, "rpmsg"))
		printf("Will send %d msg by RPMsg mode\n", msg_cnt);
	else
		printf("Will send %d msg by Mailbox mode\n", msg_cnt);

	ret = pthread_create(&thid, NULL, recv_msg_thread, &msg_cnt);
	if (ret < 0) {
		ERR("Failed to create recv_msg thread!\n");
		goto end;
	}

	gettimeofday(&start, NULL);
	for (i = 0; i < msg_cnt; i++) {
		msg->cmd = RPMSG_CMD_DDR_RDY;
		msg->seq = i;
		msg->len = 6;
		snprintf((char *)msg->data, 32, "Hi! I'm CSYS %d", i);
		printf("\n%s: Send cmd 0x%x, seq %d, len %d, data |%s|\n",
		       g_dev_name,
		       msg->cmd, msg->seq, msg->len, (char *)msg->data);
		ret = write(g_msg_fd, msg, AIC_RPMSG_REAL_SIZE(msg->len));
		if (ret < 0) {
			ERR("%s: Failed to send msg %d\n", g_dev_name, i);
			break;
		}

		sleep(2);
	}
	gettimeofday(&end, NULL);

end:
	if (g_msg_fd > 0)
		close(g_msg_fd);

	if (ret < 0)
		return ret;
	else
		return 0;
}
