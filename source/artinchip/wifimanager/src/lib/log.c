#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "log.h"

static int debug_level = MSG_INFO;

void wifimanager_set_debug_level(int level)
{
	debug_level = level;
}

void wifimanager_debug(int level, const char *fmt, ...)
{
	va_list args;
	struct timeval tv;

	if (level > debug_level)
		return ;

	va_start(args, fmt);

	printf("[WifiManager]: ");

	vprintf(fmt, args);

	va_end(args);
}
