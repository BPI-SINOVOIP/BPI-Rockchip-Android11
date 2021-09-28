/*
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * This fairly simple test program dumps output in a similar format to the
 * "xrandr" tool everyone knows & loves.  It's necessarily slightly different
 * since the kernel separates outputs into encoder and connector structures,
 * each with their own unique ID.  The program also allows test testing of the
 * memory management and mode setting APIs by allowing the user to specify a
 * connector and mode to use for mode setting.  If all works as expected, a
 * blue background should be painted on the monitor attached to the specified
 * connector after the selected mode is set.
 *
 * TODO: use cairo to write the mode info on the selected output once
 *       the mode has been programmed, along with possible test patterns.
 */
#include "drmgamma.h"

namespace android {

DrmGamma::DrmGamma(){
}

DrmGamma::~DrmGamma(){
}

static uint32_t get_property_id(int fd, drmModeObjectProperties *props,
				const char *name)
{
	drmModePropertyPtr property;
	uint32_t i, id = 0;

	/* find property according to the name */
	for (i = 0; i < props->count_props; i++) {
		property = drmModeGetProperty(fd, props->props[i]);
		if (!strcmp(property->name, name))
			id = property->prop_id;
		drmModeFreeProperty(property);

		if (id)
			break;
	}

	return id;
}

int DrmGamma::set_3x1d_gamma(int fd, unsigned crtc_id, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
	unsigned blob_id = 0;
	drmModeObjectProperties *props;
	struct drm_color_lut gamma_lut[size];
	int i, ret;
	for (i = 0; i < size; i++) {
		gamma_lut[i].red = r[i];
		gamma_lut[i].green = g[i];
		gamma_lut[i].blue = b[i];
	}
	props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	uint32_t property_id = get_property_id(fd, props, "GAMMA_LUT");
	if(property_id == 0){
		ALOGE("can't find GAMMA_LUT");
	}
	drmModeCreatePropertyBlob(fd, gamma_lut, sizeof(gamma_lut), &blob_id);
	ret = drmModeObjectSetProperty(fd, crtc_id, DRM_MODE_OBJECT_CRTC, property_id, blob_id);
	return ret;
}

int DrmGamma::set_cubic_lut(int fd, unsigned crtc_id, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
	unsigned blob_id = 0;
	drmModeObjectProperties *props;
	struct drm_color_lut cubic_lut[size];
	int i, ret;
	for (i = 0; i < size; i++) {
		cubic_lut[i].red = r[i];
		cubic_lut[i].green = g[i];
		cubic_lut[i].blue = b[i];
	}
	props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	uint32_t property_id = get_property_id(fd, props, "CUBIC_LUT");
	if(property_id == 0){
		ALOGE("can't find CUBIC_LUT");
	}
	drmModeCreatePropertyBlob(fd, cubic_lut, sizeof(cubic_lut), &blob_id);
	ret = drmModeObjectSetProperty(fd, crtc_id, DRM_MODE_OBJECT_CRTC, property_id, blob_id);
	return ret;
}

}

