// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <artinchip/sample_base.h>
#include <sys/time.h>
#include <linux/watchdog.h>

/* Global macro and variables */

#define WDT_CHAN_NUM			4
#define WDT_MAX_TIMEOUT			(60 * 60)
#define WDT_MIN_TIMEOUT			1
#define WDT_DEV_PATH			"/dev/watchdog"

static const char sopts[] = "ic:s:gp:Gku";
static const struct option lopts[] = {
	{"info",		no_argument, NULL, 'd'},
	{"channel",		required_argument, NULL, 'c'},
	{"set-timeout",		required_argument, NULL, 's'},
	{"get-timeout",		no_argument, NULL, 'g'},
	{"set-pretimeout",	required_argument, NULL, 'p'},
	{"get-pretimeout",	no_argument, NULL, 'G'},
	{"keepalive",		no_argument, NULL, 'k'},
	{"usage",		no_argument, NULL, 'u'},
	{0, 0, 0, 0}
};

/* Functions */

int usage(char *program)
{
	printf("Compile time: %s %s\n", __DATE__, __TIME__);
	printf("Usage: %s [options]\n", program);
	printf("\t -i, --info\t\tPrint the status and infomation\n");
	printf("\t -c, --channel\tSelect a channel, [0, 3]\n");
	printf("\t -s, --set-timeout\tSet a timeout, in second\n");
	printf("\t -g, --get-timeout\tGet the current timeout, in second\n");
	printf("\t -p, --set-pretimeout\tSet a pretimeout, in second\n");
	printf("\t -G, --get-pretimeout\tGet the current pretimeout, in second\n");
	printf("\t -k, --keepalive\tKeepalive the watchdog\n");
	printf("\t -u, --usage \n");
	printf("\n");
	printf("Example: %s -c 0 -s 12\n", program);
	printf("Example: %s -c 1 -s 100 -p 90\n\n", program);
	return 0;
}

/* Open a device file to be needed. */
int wdt_open(int chan)
{
	s32 fd = -1;
	char filename[16] = {0};

	snprintf(filename, 16, "%s%d", WDT_DEV_PATH, chan);
	fd = open(filename, O_RDWR);
	if (fd < 0)
		ERR("Failed to open %s errno: %d[%s]\n",
			filename, errno, strerror(errno));

	return fd;
}

int wdt_enable(int fd, int enable)
{
	int ret = 0;
	int cmd = enable ? WDIOS_ENABLECARD : WDIOS_DISABLECARD;

	ret = ioctl(fd, WDIOC_SETOPTIONS, &cmd);
	if (ret < 0)
		ERR("Failed to %s wdt %d[%s]\n", enable ? "enable" : "disable",
			errno, strerror(errno));

	return ret;
}

int wdt_info(int chan)
{
	int ret = 0, devfd = -1;
	int status = 0;
	struct watchdog_info info = {0};

	devfd = wdt_open(chan);
	if (devfd < 0)
		return -1;

	ret = ioctl(devfd, WDIOC_GETSUPPORT, &info);
	if (ret < 0) {
		ERR("Failed to get support %d[%s]\n", errno, strerror(errno));
		goto err;
	}

	printf("In %s watchdog V%d, options %#x\n",
		info.identity, info.firmware_version, info.options);

	ret = ioctl(devfd, WDIOC_GETSTATUS, &status);
	if (ret < 0) {
		ERR("Failed to get status %d[%s]\n", errno, strerror(errno));
		goto err;
	}
	printf("Status: %d\n", status);

	ret = ioctl(devfd, WDIOC_GETBOOTSTATUS, &status);
	if (ret < 0) {
		ERR("Failed to get bootstatus %d[%s]\n", errno, strerror(errno));
		goto err;
	}
	printf("Boot status: %d\n", status);

err:
	wdt_enable(devfd, 0);
	close(devfd);
	return ret;
}

int wdt_set_timeout(int chan, int timeout, int pretimeout)
{
	int ret = 0, devfd = -1;

	devfd = wdt_open(chan);
	if (devfd < 0)
		return -1;

	DBG("Set chan%d timeout %d, pretimeout %d\n", chan, timeout, pretimeout);
	ret = ioctl(devfd, WDIOC_SETTIMEOUT, &timeout);
	if (ret < 0)
		ERR("Failed to set timeout %d[%s]\n", errno, strerror(errno));

	if (pretimeout) {
		ret = ioctl(devfd, WDIOC_SETPRETIMEOUT, &pretimeout);
		if (ret < 0)
			ERR("Failed to set pretimeout %d[%s]\n",
			    errno, strerror(errno));
	}

	wdt_enable(devfd, 0);
	close(devfd);
	return ret;
}

int wdt_get_timeout(int chan)
{
	int ret = 0, devfd = -1;
	int timeout;

	devfd = wdt_open(chan);
	if (devfd < 0)
		return -1;

	ret = ioctl(devfd, WDIOC_GETTIMEOUT, &timeout);
	if (ret < 0)
		ERR("Failed to get timeout %d[%s]\n", errno, strerror(errno));
	else
		DBG("Get chan%d timeout %d\n", chan, timeout);

	wdt_enable(devfd, 0);
	close(devfd);
	return ret;
}

int wdt_set_pretimeout(int chan, int pretimeout)
{
	int ret = 0, devfd = -1;

	devfd = wdt_open(chan);
	if (devfd < 0)
		return -1;

	DBG("Set chan%d pretimeout %d\n", chan, pretimeout);
	ret = ioctl(devfd, WDIOC_SETPRETIMEOUT, &pretimeout);
	if (ret < 0)
		ERR("Failed to set pretimeout %d[%s]\n", errno, strerror(errno));

	wdt_enable(devfd, 0);
	close(devfd);
	return ret;
}

int wdt_get_pretimeout(int chan)
{
	int ret = 0, devfd = -1;
	int pretimeout;

	devfd = wdt_open(chan);
	if (devfd < 0)
		return -1;

	ret = ioctl(devfd, WDIOC_GETPRETIMEOUT, &pretimeout);
	if (ret < 0)
		ERR("Failed to get pretimeout %d[%s]\n", errno, strerror(errno));
	else
		DBG("Get chan%d pretimeout %d\n", chan, pretimeout);

	wdt_enable(devfd, 0);
	close(devfd);
	return ret;
}

int wdt_keepalive(int chan)
{
	int ret = 0, devfd = -1;

	devfd = wdt_open(chan);
	if (devfd < 0)
		return -1;

	ret = ioctl(devfd, WDIOC_KEEPALIVE, NULL);
	if (ret < 0)
		ERR("Failed to keepalive %d[%s]\n", errno, strerror(errno));
	else
		DBG("keepalive chan%d\n", chan);

	wdt_enable(devfd, 0);
	close(devfd);
	return ret;
}

int main(int argc, char **argv)
{
	int c, chan = 0;
	int timeout = 0, pretimeout = 0;

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'c':
			chan = str2int(optarg);
			if ((chan < 0) || (chan >= WDT_CHAN_NUM)) {
				ERR("Invalid channel No.%s\n", optarg);
				return -1;
			}
			DBG("You select the channel %d\n", chan);
			continue;
		case 's':
			timeout = str2int(optarg);
			continue;
		case 'g':
			return wdt_get_timeout(chan);
		case 'p':
			pretimeout = str2int(optarg);
			continue;
		case 'G':
			return wdt_get_pretimeout(chan);
		case 'i':
			return wdt_info(chan);
		case 'k':
			return wdt_keepalive(chan);
		case 'u':
		default:
			return usage(argv[0]);
		}
	}

	return wdt_set_timeout(chan, timeout, pretimeout);
}
