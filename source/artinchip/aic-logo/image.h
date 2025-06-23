/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <libfdt.h>
#include <libfdt_env.h>
#include <libfdt_internal.h>
#include <fdt.h>

/*******************************************************************/
/* New uImage format specific code (prefixed with fit_) */
/*******************************************************************/

#define FIT_IMAGES_PATH		"/images"
#define FIT_CONFS_PATH		"/configurations"

/* image node */
#define FIT_DATA_PROP		"data"
#define FIT_DATA_POSITION_PROP	"data-position"
#define FIT_DATA_OFFSET_PROP	"data-offset"
#define FIT_DATA_SIZE_PROP	"data-size"
#define FIT_TIMESTAMP_PROP	"timestamp"
#define FIT_DESC_PROP		"description"
#define FIT_ARCH_PROP		"arch"
#define FIT_TYPE_PROP		"type"
#define FIT_OS_PROP		"os"
#define FIT_COMP_PROP		"compression"
#define FIT_ENTRY_PROP		"entry"
#define FIT_LOAD_PROP		"load"

static inline const char *fit_get_name(const void *fit_hdr,
		int noffset, int *len)
{
	return fdt_get_name(fit_hdr, noffset, len);
}

int fit_image_get_data(const void *fit, int noffset,
		const void **data, size_t *size);

int fit_image_get_node(const void *fit, const char *image_uname);

static inline int fit_image_get_data_and_size(const void *fit, int noffset,
		const void **data, size_t *size)
{
	return fit_image_get_data(fit, noffset, data, size);
}

int fit_print_contents(const void *fit);

#endif	/* __IMAGE_H__ */
