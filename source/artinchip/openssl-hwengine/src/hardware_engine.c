/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/engine.h>
#include "hardware_engine.h"

static const char *aic_engine_id = "aic";
static const char *aic_engine_name  = "Artinchip hardware crypto engine";

static int aic_eng_init(ENGINE *e)
{
	return 1;
}

static int aic_eng_finish(ENGINE *e)
{
	return 1;
}

static int aic_eng_destroy(ENGINE *e)
{
	aic_skciphers_free();
	aic_digests_free();
	aic_rsa_free();
	return 1;
}

static int aic_eng_bind(ENGINE *e, const char *id)
{
	RSA_METHOD *rsa_meth;
	int ret = 0;

	if (!e) {
		fprintf(stderr, "engine pointer is null.\n");
		return ret;
	}

	if (id != NULL && strcmp(id, aic_engine_id) != 0) {
		fprintf(stderr, "engine id is not match.\n");
		return ret;
	}

	if (!ENGINE_set_id(e, aic_engine_id)) {
		fprintf(stderr, "Failed to set engine id.\n");
		return ret;
	}

	if (!ENGINE_set_name(e, aic_engine_name)) {
		fprintf(stderr, "Failed to set engine name.\n");
		return ret;
	}

	if (!ENGINE_set_destroy_function(e, aic_eng_destroy) ||
	    !ENGINE_set_init_function(e, aic_eng_init) ||
	    !ENGINE_set_finish_function(e, aic_eng_finish)) {
		fprintf(stderr, "Failed to set functions.\n");
		return 0;
	}

	if (!ENGINE_set_ciphers(e, aic_skciphers)) {
		fprintf(stderr, "Failed to set hardware skciphers.\n");
		return ret;
	}

	if (!ENGINE_set_digests(e, aic_digests)) {
		fprintf(stderr, "Failed to set hardware digests.\n");
		return ret;
	}

	rsa_meth = aic_get_rsa_method();
	if (!rsa_meth) {
		fprintf(stderr, "Failed to get rsa method.\n");
	}
	if (rsa_meth && !ENGINE_set_RSA(e, rsa_meth)) {
		fprintf(stderr, "Failed to set hardware rsa.\n");
		return ret;
	}

	return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(aic_eng_bind)
IMPLEMENT_DYNAMIC_CHECK_FN()
