#ifndef __LOG_H__
#define __LOG_H__

#if __cplusplus
extern "C" {
#endif

enum {
	MSG_ERROR=0, MSG_WARNING, MSG_INFO, MSG_DEBUG,
};

void wifimanager_set_debug_level(int level);
void wifimanager_debug(int level, const char *fmt, ...) __attribute__ ((format (printf, (2), (3))));

#if __cplusplus
};  // extern "C"
#endif

#endif
