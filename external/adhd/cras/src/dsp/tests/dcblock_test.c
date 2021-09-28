/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dsp_test_util.h"
#include "dsp_util.h"
#include "dcblock.h"
#include "raw.h"

#ifndef min
#define min(a, b)                                                              \
	({                                                                     \
		__typeof__(a) _a = (a);                                        \
		__typeof__(b) _b = (b);                                        \
		_a < _b ? _a : _b;                                             \
	})
#endif

static double tp_diff(struct timespec *tp2, struct timespec *tp1)
{
	return (tp2->tv_sec - tp1->tv_sec) +
	       (tp2->tv_nsec - tp1->tv_nsec) * 1e-9;
}

/* Processes a buffer of data chunk by chunk using the filter */
static void process(struct dcblock *dcblock, float *data, int count)
{
	int start;
	for (start = 0; start < count; start += 128)
		dcblock_process(dcblock, data + start, min(128, count - start));
}

/* Runs the filters on an input file */
static void test_file(const char *input_filename, const char *output_filename)
{
	size_t frames;
	struct timespec tp1, tp2;
	struct dcblock *dcblockl;
	struct dcblock *dcblockr;

	float *data = read_raw(input_filename, &frames);

	dcblockl = dcblock_new(0.995, 48000);
	dcblockr = dcblock_new(0.995, 48000);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp1);
	process(dcblockl, data, frames);
	process(dcblockr, data + frames, frames);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp2);
	printf("processing takes %g seconds for %zu samples\n",
	       tp_diff(&tp2, &tp1), frames);
	dcblock_free(dcblockl);
	dcblock_free(dcblockr);

	write_raw(output_filename, data, frames);
	free(data);
}

int main(int argc, char **argv)
{
	dsp_enable_flush_denormal_to_zero();
	if (dsp_util_has_denormal())
		printf("denormal still supported?\n");
	else
		printf("denormal disabled\n");
	dsp_util_clear_fp_exceptions();

	if (argc == 3)
		test_file(argv[1], argv[2]);
	else
		printf("Usage: dcblock_test input.raw output.raw\n");

	dsp_util_print_fp_exceptions();
	return 0;
}
