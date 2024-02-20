// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023 ArtInChip Technology Co., Ltd.
 */
/*
 * This sample code sets one gpio pin as output,
 * flip the output level every second.
 */
#include <stdio.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

volatile int gpio_abort = 0;

void print_usage(void)
{
	fprintf(stderr, "Usage: test_gpio_output <GPIO_NAME> <LINE_OFFSET>\n"
		"    Example:\n"
		"    test_gpio_output gpiochip3 6\n");
}

void signal_handler(int sig)
{
	printf("test_gpio_output aborted by signal %s...\n", strsignal(sig));
	gpio_abort = 1;
}

int main(int argc, char *argv[])
{
	int gfd, lfd, ret, offset;
	struct gpio_v2_line_request req;
	struct gpio_v2_line_values value;
	char device_name[10];
	char gpio_dev[20];

	if (argc != 3) {
		print_usage();
		exit(-1);
	}

	memcpy(device_name, argv[1], 10);
	offset = (int)strtoul(argv[2], NULL, 10);
	memset(&req, 0, sizeof(req));
	/* Configure output mode */
	req.config.flags |= GPIO_V2_LINE_FLAG_OUTPUT;
	snprintf(gpio_dev, 20, "/dev/%s", device_name);
	/* open GPIO group device, get the fd */
	gfd = open(gpio_dev, 0);
	if (gfd == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to open %s, %s\n",
			gpio_dev, strerror(errno));
		return ret;
	}

	/* line offset in requested GPIO group */
	req.offsets[0] = offset;
	/* just request one line */
	req.num_lines = 1;
	strcpy(req.consumer, "gpio output pin");

	ret = ioctl(gfd, GPIO_V2_GET_LINE_IOCTL, &req);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIO_GET_LINE_IOCTL", ret, strerror(errno));
		goto __exit_device_close;
	} else {
		/* request line success, get the gpio line fd */
		lfd = req.fd;
	}

	/* a bitmap identifying the lines to get or set */
	value.mask = 1;
	/* a bitmap containing the value of the lines */
	value.bits = 1;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);
	gpio_abort = 0;

	printf("%s line %d output level will be flipped every second...\n",
	       device_name, offset);

	while (!gpio_abort) {
		ret = ioctl(lfd, GPIO_V2_LINE_SET_VALUES_IOCTL, &value);
		if (ret == -1) {
			ret = -errno;
			fprintf(stderr, "Failed to issue %s (%d), %s\n",
				"GPIOHANDLE_SET_LINE_VALUES_IOCTL", ret,
				strerror(errno));
			break;
		}
		value.bits ^= 1;
		sleep(1);
	}

	if (close(lfd) == -1)
		perror("Failed to close line file");
__exit_device_close:
	if (close(gfd))
		perror("Failed to close GPIO group file");

	return ret;
}
