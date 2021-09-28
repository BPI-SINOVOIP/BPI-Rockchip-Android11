/*
 * Copyright © 2019 Intel Corporation
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors: Simon Ser <simon.ser@intel.com>
 */

#ifndef IGT_CHAMELIUM_STREAM_H
#define IGT_CHAMELIUM_STREAM_H

#include "config.h"

enum chamelium_stream_realtime_mode {
	CHAMELIUM_STREAM_REALTIME_NONE = 0,
	/* stop dumping when overflow */
	CHAMELIUM_STREAM_REALTIME_STOP_WHEN_OVERFLOW = 1,
	/* drop data on overflow */
	CHAMELIUM_STREAM_REALTIME_BEST_EFFORT = 2,
};

struct chamelium_stream;

struct chamelium_stream *chamelium_stream_init(void);
void chamelium_stream_deinit(struct chamelium_stream *client);
bool chamelium_stream_dump_realtime_audio(struct chamelium_stream *client,
					  enum chamelium_stream_realtime_mode mode);
bool chamelium_stream_receive_realtime_audio(struct chamelium_stream *client,
					     size_t *page_count,
					     int32_t **buf, size_t *buf_len);
bool chamelium_stream_stop_realtime_audio(struct chamelium_stream *client);

#endif
