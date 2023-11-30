// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
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

void print_usage(void)
{
	fprintf(stderr, "Usage: test-gpio [options]...\n"
		"    Test gpio interrupt, debounce is 10ms\n"
		"    -g <name>   gpio group name, such as gpiochip0, etc\n"
		"    -p <offset> gpio pin offset, such as 0/1/2/3, etc\n"
		"    -r          rising edge trigger interrupt\n"
		"    -f          falling edge trigger interrupt\n\n"
		"    -w          wait for an interrupt event\n"
		"    Example:\n"
		"    test-gpio -g gpiochip3 -p 6 -f\n");
}

int main(int argc, char *argv[])
{
	int gfd = -1, lfd = -1, ret = 0, c, offset = -1;
	struct gpio_v2_line_request req;
	struct gpio_v2_line_config *config;
	char gpio_dev[20] = "/dev/gpiochip3";
	unsigned int mode = 0;
	unsigned int irq_trigger_waiting = 0;

	if (argc == 1) {
		print_usage();
		exit(-1);
	}

	while ((c = getopt(argc, argv, "g:p:rfwh")) != -1) {
		switch (c) {
		case 'g':
			snprintf(gpio_dev, 20, "/dev/%s", optarg);
			break;
		case 'p':
			offset = strtoul(optarg, NULL, 10);
			break;
		case 'r':
			mode |= GPIO_V2_LINE_FLAG_EDGE_RISING;
			break;
		case 'f':
			mode |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
			break;
		case 'w':
			irq_trigger_waiting = 1;
			break;
		case 'h':
		default:
			print_usage();
			exit(-1);
		}
	}

	if (offset == -1)
		offset = 6;

	if (!mode)
		mode |= GPIO_V2_LINE_FLAG_EDGE_FALLING;

	memset(&req, 0, sizeof(req));
	/* Configure input mode */
	req.config.flags |= GPIO_V2_LINE_FLAG_INPUT | mode;

	/* Configure GPIO debounce, 10ms */
	config = &req.config;
	config->num_attrs = 1; /* one attribute: debounce */
	config->attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
	config->attrs[0].attr.debounce_period_us = 10000; /* unit is us */
	config->attrs[0].mask = 1;

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
	strcpy(req.consumer, "interrupt pin");

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

	while (irq_trigger_waiting) {
		printf("Waiting for a GPIO interrupt...\n");

		struct gpio_v2_line_event event = {0};

		/* check whether an interrupt is triggered */
		ret = read(lfd, &event, sizeof(event));
		if (ret == -1) {
			if (errno == -EAGAIN) {
				fprintf(stderr, "nothing available\n");
				continue;
			} else {
				ret = -errno;
				fprintf(stderr, "Failed to read event (%d)\n",
					ret);
				break;
			}
		}

		if (ret != sizeof(event)) {
			fprintf(stderr, "Reading event failed\n");
			ret = -EIO;
			break;
		}

		fprintf(stdout, "GPIO EVENT ");
		switch (event.id) {
		case GPIO_V2_LINE_EVENT_RISING_EDGE:
			fprintf(stdout, "rising edge");
			break;
		case GPIO_V2_LINE_EVENT_FALLING_EDGE:
			fprintf(stdout, "falling edge");
			break;
		default:
			fprintf(stdout, "unknown event");
		}

		fprintf(stdout, "\n");
	}

	if ((lfd > 2) && (close(lfd) == -1))
		perror("Failed to close line file");
__exit_device_close:
	if ((gfd > 2) && close(gfd) == -1)
		perror("Failed to close GPIO group file");

	return ret;
}
