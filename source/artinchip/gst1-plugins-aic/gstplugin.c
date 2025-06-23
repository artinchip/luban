/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * author: artinchip
 */
#include <gst/gst.h>

#include "gstvedec.h"
#include "gstfbsink.h"

#define AIC_GST_PLUGIN_RANK (GST_RANK_PRIMARY+1)

#define AUTHOR "<qi.xu@artinchip.com>"
#define PACKAGE_NAME "ArtInChip Gstreamer Decoder Plugins"
#define PACKAGE_ORIG "http://www.artinchip.com"
#define LICENSE "LGPL"
#define VERSION "0.1"

#ifndef PACKAGE
#define PACKAGE "aic"
#endif

static gboolean plugin_init (GstPlugin * plugin)
{
	if(!gst_element_register (plugin, "vedec", AIC_GST_PLUGIN_RANK, GST_TYPE_VE_DEC)) {
		GST_ERROR("regist vedec element failed");
		return FALSE;
	}

	if(!gst_element_register (plugin, "fbsink", AIC_GST_PLUGIN_RANK, GST_TYPE_FBSINK)) {
		GST_ERROR("regist fbsink element failed");
		return FALSE;
	}

  	return TRUE;
}

GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	aic,
	"ArtInChip plugins",
	plugin_init,
	VERSION,
	LICENSE,
	PACKAGE_NAME,
	PACKAGE_ORIG
)
