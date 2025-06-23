// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Siyao.Li <lisy@artinchip.com>
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <linux/input.h>

#define KEYADC_EVENT_NAME "adc-keys"

static const char sopts[] = "d:h";
static const struct option lopts[] = {
	{"device",	  required_argument, NULL, 'd'},
	{"usage",		no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static void usage(char *program)
{
	printf("\nUsage:  %s [options]: \n", program);
	printf("\t -d, --device \t\tThe name of event device\n");
	printf("\t -h, --usage \n");
	printf("Example: keyadc_test -d event0\n");
}

int check_keyadc_data(const char *event_name)
{
	char event_path[12] = "";
	struct input_event t;
	char name[12] = "Unknown";

	sprintf(event_path, "/dev/input/%s", event_name);
	int fd = open(event_path, O_RDONLY);

	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0){
		printf("Get name failed\n");
		return -1;
	}

	if (strncmp(name, KEYADC_EVENT_NAME, 12)) {
		printf("%s is invalid keyadc device name\n", name);
		return -1;
	}

	while(1) {
		int len = read(fd, &t, sizeof(t));
		if (len != sizeof(t))
			continue;

		if (t.type == EV_KEY && t.value != KEY_RESERVED) {
			switch (t.code) {
			case KEY_UP:
				printf("key up pressed\n");
				break;
			case KEY_DOWN:
				printf("key down pressed\n");
				break;
			case KEY_LEFT:
				printf("key left pressed\n");
				break;
			case KEY_RIGHT:
				printf("key right pressed\n");
				break;
			default:
				printf("unknown key pressed\n");
				break;
			}
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int c = 1;

	if (argc != 3) {
		usage(argv[0]);
		return -1;
	}

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			return check_keyadc_data(optarg);
			break;
		case 'h':
			usage(argv[0]);
			break;
		default:
			break;
		}
	}

	return 0;
}
