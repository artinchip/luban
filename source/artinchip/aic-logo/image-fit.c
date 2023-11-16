// SPDX-License-Identifier: GPL-2.0+

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "image.h"

#define debug printf

static void fit_get_debug(const void *fit, int noffset,
		char *prop_name, int err)
{
	debug("Can't get '%s' property from FIT 0x%08lx, node: offset %d, name %s (%s)\n",
	      prop_name, (unsigned long)fit, noffset,
	      fit_get_name(fit, noffset, NULL),
	      fdt_strerror(err));
}

static int fit_get_desc(const void *fit, int noffset, char **desc)
{
	int len;

	*desc = (char *)fdt_getprop(fit, noffset, FIT_DESC_PROP, &len);
	if (*desc == NULL) {
		fit_get_debug(fit, noffset, FIT_DESC_PROP, len);
		return -1;
	}
	return 0;
}

/**
 * fit_image_get_data - get data property and its size for a given component image node
 * @fit: pointer to the FIT format image header
 * @noffset: component image node offset
 * @data: double pointer to void, will hold data property's data address
 * @size: pointer to size_t, will hold data property's data size
 *
 * fit_image_get_data() finds data property in a given component image node.
 * If the property is found its data start address and size are returned to
 * the caller.
 *
 * returns:
 *     0, on success
 *     -1, on failure
 */
int fit_image_get_data(const void *fit, int noffset,
		const void **data, size_t *size)
{
	int len;

	*data = fdt_getprop(fit, noffset, FIT_DATA_PROP, &len);
	if (*data == NULL) {
		fit_get_debug(fit, noffset, FIT_DATA_PROP, len);
		*size = 0;
		return -1;
	}

	*size = len;
	return 0;
}

/**
 * fit_image_get_node - get node offset for component image of a given unit name
 * @fit: pointer to the FIT format image header
 * @image_uname: component image node unit name
 *
 * fit_image_get_node() finds a component image (within the '/images'
 * node) of a provided unit name. If image is found its node offset is
 * returned to the caller.
 *
 * returns:
 *     image node offset when found (>=0)
 *     negative number on failure (FDT_ERR_* code)
 */
int fit_image_get_node(const void *fit, const char *image_uname)
{
	int noffset, images_noffset;

	images_noffset = fdt_path_offset(fit, FIT_IMAGES_PATH);
	if (images_noffset < 0) {
		debug("Can't find images parent node '%s' (%s)\n",
		      FIT_IMAGES_PATH, fdt_strerror(images_noffset));
		return images_noffset;
	}

	noffset = fdt_subnode_offset(fit, images_noffset, image_uname);
	if (noffset < 0) {
		debug("Can't get node offset for image unit name: '%s' (%s)\n",
		      image_uname, fdt_strerror(noffset));
	}

	return noffset;
}

void genimg_print_size(uint32_t size)
{
	printf("%d Bytes = %.2f KiB = %.2f MiB\n",
		   size, (double)size / 1.024e3,
		   (double)size / 1.048576e6);
}

static void fit_image_print(const void *fit, int image_noffset)
{
	const void *data;
	char *desc;
	size_t size;
	int ret;

	/* Mandatory properties */
	ret = fit_get_desc(fit, image_noffset, &desc);
	printf("   Description:  ");
	if (ret)
		printf("unavailable\n");
	else
		printf("%s\n", desc);

	ret = fit_image_get_data_and_size(fit, image_noffset, &data, &size);

	printf("   Data Size:    ");
	if (ret)
		printf("unavailable\n");
	else
		genimg_print_size(size);
}

int fit_print_contents(const void *fit)
{
	int images_noffset;
	int noffset;
	int ndepth;
	int count = 0;

	printf("itb totalsize: %d\n", fdt_totalsize(fit));

	images_noffset = fdt_path_offset(fit, FIT_IMAGES_PATH);
	if (images_noffset < 0) {
		printf("Can't find images parent node '%s' (%s)\n",
		       FIT_IMAGES_PATH, fdt_strerror(images_noffset));
		return -images_noffset;
	}

	/* Process its subnodes, print out component images details */
	for (ndepth = 0, count = 0,
			noffset = fdt_next_node(fit, images_noffset, &ndepth);
			(noffset >= 0) && (ndepth > 0);
			noffset = fdt_next_node(fit, noffset, &ndepth)) {
		if (ndepth == 1) {
			/*
			 * Direct child node of the images parent node,
			 * i.e. component image node.
			 */
			printf("  Image %u (%s)\n", count++,
			       fit_get_name(fit, noffset, NULL));

			fit_image_print(fit, noffset);
		}
	}

	return 0;
}
