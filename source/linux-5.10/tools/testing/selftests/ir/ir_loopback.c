// SPDX-License-Identifier: GPL-2.0
// test ir decoder
//
// Copyright (C) 2018 Sean Young <sean@mess.org>

// When sending LIRC_MODE_SCANCODE, the IR will be encoded. rc-loopback
// will send this IR to the receiver side, where we try to read the decoded
// IR. Decoding happens in a separate kernel thread, so we will need to
// wait until that is scheduled, hence we use poll to check for read
// readiness.

#include <linux/lirc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include "../kselftest.h"

#define TEST_SCANCODES	10
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define SYSFS_PATH_MAX 256
#define DNAME_PATH_MAX 256

static const struct {
	enum rc_proto proto;
	const char *name;
	unsigned int mask;
	const char *decoder;
} protocols[] = {
	{ RC_PROTO_RC5, "rc-5", 0x1f7f, "rc-5" },
	{ RC_PROTO_RC5X_20, "rc-5x-20", 0x1f7f3f, "rc-5" },
	{ RC_PROTO_RC5_SZ, "rc-5-sz", 0x2fff, "rc-5-sz" },
	{ RC_PROTO_JVC, "jvc", 0xffff, "jvc" },
	{ RC_PROTO_SONY12, "sony-12", 0x1f007f, "sony" },
	{ RC_PROTO_SONY15, "sony-15", 0xff007f, "sony" },
	{ RC_PROTO_SONY20, "sony-20", 0x1fff7f, "sony" },
	{ RC_PROTO_NEC, "nec", 0xffff, "nec" },
	{ RC_PROTO_NECX, "nec-x", 0xffffff, "nec" },
	{ RC_PROTO_NEC32, "nec-32", 0xffffffff, "nec" },
	{ RC_PROTO_SANYO, "sanyo", 0x1fffff, "sanyo" },
	{ RC_PROTO_RC6_0, "rc-6-0", 0xffff, "rc-6" },
	{ RC_PROTO_RC6_6A_20, "rc-6-6a-20", 0xfffff, "rc-6" },
	{ RC_PROTO_RC6_6A_24, "rc-6-6a-24", 0xffffff, "rc-6" },
	{ RC_PROTO_RC6_6A_32, "rc-6-6a-32", 0xffffffff, "rc-6" },
	{ RC_PROTO_RC6_MCE, "rc-6-mce", 0x00007fff, "rc-6" },
	{ RC_PROTO_SHARP, "sharp", 0x1fff, "sharp" },
	{ RC_PROTO_IMON, "imon", 0x7fffffff, "imon" },
	{ RC_PROTO_RCMM12, "rcmm-12", 0x00000fff, "rc-mm" },
	{ RC_PROTO_RCMM24, "rcmm-24", 0x00ffffff, "rc-mm" },
	{ RC_PROTO_RCMM32, "rcmm-32", 0xffffffff, "rc-mm" },
};

int lirc_open(const char *rc)
{
	struct dirent *dent;
	char buf[SYSFS_PATH_MAX + DNAME_PATH_MAX];
	DIR *d;
	int fd;

	snprintf(buf, sizeof(buf), "/sys/class/rc/%s", rc);

	d = opendir(buf);
	if (!d)
		ksft_exit_fail_msg("cannot open %s: %m\n", buf);

	while ((dent = readdir(d)) != NULL) {
		if (!strncmp(dent->d_name, "lirc", 4)) {
			snprintf(buf, sizeof(buf), "/dev/%s", dent->d_name);
			break;
		}
	}

	if (!dent)
		ksft_exit_skip("cannot find lirc device for %s\n", rc);

	closedir(d);

	fd = open(buf, O_RDWR | O_NONBLOCK);
	if (fd == -1)
		ksft_exit_fail_msg("cannot open: %s: %m\n", buf);

	return fd;
}

int lirc_close(int rlircfd, int wlircfd, int protocolfd)
{
	close(rlircfd);
	close(wlircfd);
	close(protocolfd);

	if (ksft_get_fail_cnt() > 0)
		ksft_exit_fail();
	else
		ksft_exit_pass();
	return 0;
}

int test_scancode(int rlfd, int wlfd, int prtfd,
		  int i, unsigned int data)
{
	int n;
	unsigned int scancode, rc_proto;

	if (i >= ARRAY_SIZE(protocols) || i < 0) {
		printf("protocol is not supported\n");
		lirc_close(rlfd, wlfd, prtfd);
		exit(-1);
	}

	srand(time(NULL));

	if (write(prtfd, protocols[i].decoder, strlen(protocols[i].decoder))
		== -1)
		ksft_exit_fail_msg("failed to set write decoder\n");

	printf("Testing protocol %s for decoder %s (%d/%d)...\n",
	       protocols[i].name, protocols[i].decoder, i + 1,
	       (int)ARRAY_SIZE(protocols));

	for (n = 0; n < TEST_SCANCODES; n++) {
		if (data == 0)
			scancode = rand() & protocols[i].mask;
		else
			scancode = data & protocols[i].mask;
		rc_proto = protocols[i].proto;

		if (rc_proto == RC_PROTO_RC6_MCE)
			scancode |= 0x800f0000;

		if (rc_proto == RC_PROTO_NECX && (((scancode >> 16)
			^ ~(scancode >> 8)) & 0xff) == 0)
			continue;

		if (rc_proto == RC_PROTO_NEC32 && (((scancode
			>> 8) ^ ~scancode) & 0xff) == 0)
			continue;

		if (rc_proto == RC_PROTO_RCMM32 && (scancode &
			0x000c0000) != 0x000c0000 && scancode & 0x00008000)
			continue;

		struct lirc_scancode lsc = {
			.rc_proto = rc_proto,
			.scancode = scancode
		};

		printf("Testing scancode:%x\n", scancode);

		while (write(wlfd, &lsc, sizeof(lsc)) < 0) {
			if (errno == EINTR)
				continue;

			ksft_exit_fail_msg("failed to send ir: %m\n");
		}

		struct pollfd pfd = { .fd = rlfd, .events = POLLIN };
		struct lirc_scancode lsc2;

		poll(&pfd, 1, 1000);

		bool decoded = true;

		while (read(rlfd, &lsc2, sizeof(lsc2)) < 0) {
			if (errno == EINTR)
				continue;

			ksft_test_result_error("no scancode decoded: %m\n");
			decoded = false;
			break;
		}

		if (!decoded)
			continue;

		if (lsc.rc_proto != lsc2.rc_proto)
			ksft_test_result_error("protocol is different: %d\n",
					       lsc2.rc_proto);

		else if (lsc.scancode != lsc2.scancode)
			ksft_test_result_error("scancode is different: %llx\n",
					       lsc2.scancode);
		else
			ksft_inc_pass_cnt();
	}

	printf("OK\n");
	return 0;
}

int usage_if(void)
{
	fprintf(stderr, "\n"
	"Usage: ./ir_looptback -a or -p <protocol> <rcN> <rcN>\n"
	"Use -a to test all protocols and need to be added to the kernel\n"
	"Use -p <protocol> to test the one protocol\n"
	"Use -w <data> to test specific key scancode\n"
	"\n"
#ifdef CONFIG_ARCH_ARTINCHIP
	"ArtInChip support protocols:\n"
	"<nec> <nec-x> <nec-32>\n"
	"<rc-5> <rc-5x-20> <rc-5-sz>\n"
	"<rc-6-0> <rc-6-6a-20> <rc-6-6a-24> <rc-6-6a-32> <rc-6-mce>\n"
	"<rcmm-12> <rcmm-24> <rcmm-32>\n"
	"<sony-12> <sony-15> <sony-20>\n"
	"<sanyo>\n"
#endif
	);
	return 0;
}

int main(int argc, char **argv)
{
	unsigned int mode, data = 0;
	char buf[100];
	int rlircfd, wlircfd, protocolfd;
	int ret, all_prot, one_prot, i = 0;
	char *protocol_name = NULL;
	char *rx_dev, *tx_dev;

	all_prot = 1;
	one_prot = 0;
	rlircfd = 0;
	wlircfd = 0;
	protocolfd = 0;
	rx_dev = NULL;
	tx_dev = NULL;

	if (argc < 2) {
		usage_if();
		exit(0);
	}

	if (argc < 3 && (strcmp(argv[1], "-h") != 0))
		ksft_exit_fail_msg("Usage: ./ir_looptback -h to get help\n");

	while ((ret = getopt(argc, argv, "ahp:w:")) != -1) {
		switch (ret) {
		case 'a':
			all_prot = 1;
			break;
		case 'p':
			all_prot = 0;
			one_prot = 1;
			protocol_name = optarg;
			break;
		case 'w':
			data = atoi(optarg);
			break;
		case 'h':
			usage_if();
			break;
		default:
			printf("Usage: ./ir_looptback -h to get help\n");
			exit(-1);
		}
	}

	switch (argc - optind) {
	case 0:
		ksft_exit_fail_msg("command not chose ir device\n");
		break;
	case 1:
		ksft_exit_fail_msg("only chose ir send device\n");
		break;
	case 2:
		tx_dev = argv[optind];
		rx_dev = argv[optind + 1];
		break;
	default:
		usage_if();
	}

	if (tx_dev && rx_dev) {
		rlircfd = lirc_open(rx_dev);
		mode = LIRC_MODE_SCANCODE;
		if (ioctl(rlircfd, LIRC_SET_REC_MODE, &mode))
			ksft_exit_fail_msg("failed to set rec mode %s: %m\n",
					   rx_dev);

		wlircfd = lirc_open(tx_dev);
		if (ioctl(wlircfd, LIRC_SET_SEND_MODE, &mode))
			ksft_exit_fail_msg("failed to set send mode %s: %m\n",
					   tx_dev);

		snprintf(buf, sizeof(buf), "/sys/class/rc/%s/protocols",
			 tx_dev);

		protocolfd = open(buf, O_WRONLY);
		if (protocolfd == -1)
			ksft_exit_fail_msg("failed to open %s: %m\n", buf);

		printf("Sending IR on %s and receiving IR on %s.\n",
		       tx_dev, rx_dev);
	}

	if (all_prot == 1) {
		printf("test all infrared protocols\n");

		for (i = 0; i < ARRAY_SIZE(protocols); i++)
			test_scancode(rlircfd, wlircfd, protocolfd, i, data);

	} else if (one_prot == 1) {
		for (i = 0; i < ARRAY_SIZE(protocols); i++) {
			if (strcmp(protocols[i].name, protocol_name) == 0)
				break;
		}
		test_scancode(rlircfd, wlircfd, protocolfd, i, data);
	}

	lirc_close(rlircfd, wlircfd, protocolfd);

	return 0;
}
