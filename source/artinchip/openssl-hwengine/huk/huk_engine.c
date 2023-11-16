// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/engine.h>
#include "huk_engine.h"

static const char *huk_engine_id = "huk";
static const char *huk_engine_name  = "Artinchip huk-protected crypto engine";

static int huk_eng_bind(ENGINE *e, const char *id)
{
	int ret = 0;

	if (!e) {
		fprintf(stderr, "engine pointer is null.\n");
		return ret;
	}

	if (id != NULL && strcmp(id, huk_engine_id) != 0) {
		fprintf(stderr, "engine id is not match.\n");
		return ret;
	}

	if (!ENGINE_set_id(e, huk_engine_id)) {
		fprintf(stderr, "Failed to set engine id.\n");
		return ret;
	}

	if (!ENGINE_set_name(e, huk_engine_name)) {
		fprintf(stderr, "Failed to set engine name.\n");
		return ret;
	}

	if (!ENGINE_set_ciphers(e, huk_skciphers)) {
		fprintf(stderr, "Failed to set hardware skciphers.\n");
		return ret;
	}


	return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(huk_eng_bind)
IMPLEMENT_DYNAMIC_CHECK_FN()
