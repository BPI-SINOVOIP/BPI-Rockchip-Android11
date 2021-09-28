/*
 * Copyright © 2016 Collabora, Ltd.
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
 * Authors:
 *    Robert Foss <robert.foss@collabora.com>
 */

#ifndef SW_SYNC_H
#define SW_SYNC_H

#define SW_SYNC_FENCE_STATUS_ERROR		(-1)
#define SW_SYNC_FENCE_STATUS_ACTIVE		(0)
#define SW_SYNC_FENCE_STATUS_SIGNALED	(1)

void igt_require_sw_sync(void);

int sw_sync_timeline_create(void);
void sw_sync_timeline_inc(int timeline, uint32_t count);

int __sw_sync_timeline_create_fence(int timeline, uint32_t seqno);
int sw_sync_timeline_create_fence(int timeline, uint32_t seqno);

int sync_fence_merge(int fence1, int fence2);
int sync_fence_wait(int fence, int timeout);
int sync_fence_status(int fence);
int sync_fence_count(int fence);
int sync_fence_count_status(int fence, int status);

#define SYNC_FENCE_OK 1

#endif

