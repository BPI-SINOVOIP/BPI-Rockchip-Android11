/*
 * Copyright © 2020 Google LLC.
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
 */

/**
 * SECTION:igt_dummy
 * @short_description: GT support library for non-GT devices
 * @title: GT
 * @include: igt.h
 *
 * This library provides various auxiliary helper functions to stub out general
 * interactions like forcewake handling, injecting hangs or stopping engines
 * that some tests rely on, but which a non-GT device does not support.
 */

#include "igt_gt.h"

igt_hang_t igt_hang_ring(int fd, int ring) {
    return (igt_hang_t) { NULL, 0, 0, 0 };
}

void igt_post_hang_ring(int fd, igt_hang_t arg) {
}

igt_hang_t igt_allow_hang(int fd, unsigned ctx, unsigned flags) {
    return (igt_hang_t) { NULL, 0, 0, 0 };
}

void igt_disallow_hang(int fd, igt_hang_t arg) {
}
