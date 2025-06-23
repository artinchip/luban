/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: aicp wrap
 *
 */

#define LOG_TAG "aicp_wrap"

#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include "mpp_decoder.h"
#include "mpp_codec.h"
#include "mpp_log.h"
#include "ve.h"
#include "mpp_mem.h"

void*                   g_aicp_handle = NULL;
pthread_mutex_t 	g_aicp_mutex = PTHREAD_MUTEX_INITIALIZER;
int                     g_aicp_ref = 0;

int destroy_aicp_wrapper()
{
	pthread_mutex_lock(&g_aicp_mutex);
	g_aicp_ref --;
	if (g_aicp_ref == 0 && g_aicp_handle) {
		dlclose(g_aicp_handle);
		g_aicp_handle = NULL;
	}
	pthread_mutex_unlock(&g_aicp_mutex);

	return 0;
}

struct mpp_decoder* create_aicp_wrapper()
{
	pthread_mutex_lock(&g_aicp_mutex);
	if (g_aicp_ref == 0) {
		g_aicp_handle = dlopen("/usr/local/lib/libmpp_aicp_dec.so", RTLD_NOW);
		if (g_aicp_handle == NULL) {
			loge("dlopen /usr/local/lib/libmpp_aicp_dec.so failed");
			return NULL;
		}
	}
	g_aicp_ref ++;
	pthread_mutex_unlock(&g_aicp_mutex);

	typedef struct mpp_decoder* (*create_func)();
	create_func create = (create_func)dlsym(g_aicp_handle, "create_aicp_decoder");

	return create();
}

