/*
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include_next <xf86drm.h>

#include <string.h>
#include <errno.h>

/*
 * FIXME: Below code comes from https://patchwork.kernel.org/patch/10368203/
 * FIXME: Remove or rework once that has been merged
 */

typedef enum drm_match_key {
	/* Match against DRM_NODE_{PRIMARY,RENDER,...} type */
	DRM_MATCH_NODE_TYPE = 1,
	DRM_MATCH_DRIVER_NAME = 2,
	DRM_MATCH_BUS_PCI_VENDOR = 3,
	DRM_MATCH_FUNCTION = 4,
} drm_match_key_t;

typedef struct drm_match_func {
	void *data;
	int (*fp)(int, void*); /* Intended arguments are fp(fd, data) */
} drm_match_func_t;

typedef struct drm_match {
	drm_match_key_t type;
	union {
		int s;
		uint16_t u16;
		char *str;
		drm_match_func_t func;
	};
} drm_match_t;

static inline int drmHandleMatch(int fd, drm_match_t *filters, int nbr_filters)
{
	if (fd < 0)
		goto error;

	if (nbr_filters > 0 && filters == NULL)
		goto error;

	drmVersionPtr ver = drmGetVersion(fd);
	if (!ver)
		goto fail;

	drmDevicePtr dev = NULL;
	if (drmGetDevice2(fd, 0, &dev) != 0) {
		goto fail;
	}

	for (int i = 0; i < nbr_filters; i++) {
		drm_match_t *f = &filters[i];
		switch (f->type) {
		case DRM_MATCH_NODE_TYPE:
			if (!(dev->available_nodes & (1 << f->s)))
				goto fail;
			break;
		case DRM_MATCH_DRIVER_NAME:
			if (!f->str)
				goto error;

			/* This bypass is used by when the driver name is used
			   by the Android property_get() func, when it hasn't found
			   the property and the string is empty as a result. */
			if (strlen(f->str) == 0)
				continue;

			if (strncmp(ver->name, f->str, strlen(ver->name)))
				goto fail;
			break;
		case DRM_MATCH_BUS_PCI_VENDOR:
			if (dev->bustype != DRM_BUS_PCI)
				goto fail;
			if (dev->deviceinfo.pci->vendor_id != f->u16)
				goto fail;
			break;
		case DRM_MATCH_FUNCTION:
			if (!f->func.fp)
				goto error;
			int (*fp)(int, void*) = f->func.fp;
			void *data = f->func.data;
			if (!fp(fd, data))
				goto fail;
			break;
		default:
			goto error;
		}
	}

success:
	drmFreeVersion(ver);
	drmFreeDevice(&dev);
	return 0;
error:
	drmFreeVersion(ver);
	drmFreeDevice(&dev);
	return -EINVAL;
fail:
	drmFreeVersion(ver);
	drmFreeDevice(&dev);
	return 1;
}

