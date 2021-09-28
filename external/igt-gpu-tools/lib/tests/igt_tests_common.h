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
 */

#ifndef IGT_LIB_TESTS_COMMON_H
#define IGT_LIB_TESTS_COMMON_H

#include <assert.h>

/*
 * We need to hide assert from the cocci igt test refactor spatch.
 *
 * IMPORTANT: Test infrastructure tests are the only valid places where using
 * assert is allowed.
 */
#define internal_assert assert

static inline void internal_assert_wexited(int wstatus, int exitcode)
{
	internal_assert(WIFEXITED(wstatus) &&
			WEXITSTATUS(wstatus) == exitcode);
}

static inline void internal_assert_wsignaled(int wstatus, int signal)
{
	internal_assert(WIFSIGNALED(wstatus) &&
			WTERMSIG(wstatus) == signal);
}
#endif
