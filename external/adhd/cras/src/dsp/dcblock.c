/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include "dcblock.h"

#define RAMP_TIME_MS 20

struct dcblock {
	float R;
	float x_prev;
	float y_prev;
	float ramp_factor;
	float ramp_increment;
	int initialized;
};

struct dcblock *dcblock_new(float R, unsigned long sample_rate)
{
	struct dcblock *dcblock = (struct dcblock *)calloc(1, sizeof(*dcblock));
	dcblock->R = R;
	dcblock->ramp_increment = 1000. / (float)(RAMP_TIME_MS * sample_rate);
	return dcblock;
}

void dcblock_free(struct dcblock *dcblock)
{
	free(dcblock);
}

/* This is the prototype of the processing loop. */
void dcblock_process(struct dcblock *dcblock, float *data, int count)
{
	int n;
	float x_prev = dcblock->x_prev;
	float y_prev = dcblock->y_prev;
	float R = dcblock->R;

	if (!dcblock->initialized) {
		x_prev = data[0];
		dcblock->initialized = 1;
	}

	for (n = 0; n < count; n++) {
		float x = data[n];
		float d = x - x_prev + R * y_prev;

		y_prev = d;
		x_prev = x;

		/*
		 * It takes a while for this DC-block filter to completely
		 * filter out a large DC-offset, so apply a mix-in ramp to
		 * avoid any residual jump discontinuities that can lead to
		 * "pop" during capture.
		 */
		if (dcblock->ramp_factor < 1.0) {
			d *= dcblock->ramp_factor;
			dcblock->ramp_factor += dcblock->ramp_increment;
		}

		data[n] = d;
	}
	dcblock->x_prev = x_prev;
	dcblock->y_prev = y_prev;
}
