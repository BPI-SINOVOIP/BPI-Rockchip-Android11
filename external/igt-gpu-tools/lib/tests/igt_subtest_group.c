/*
 * Copyright © 2016 Intel Corporation
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

#include "igt_core.h"

#include "igt_tests_common.h"

igt_main
{
	/*
	 * local variables have to be volatile here otherwise they end up being
	 * undefined when modified in igt_subtest, fixture, etc. because of
	 * longjmps
	 */
	volatile bool t1 = false;
	volatile int t2 = 0;

	igt_subtest_group {
		igt_fixture {
			igt_require(true);
		}

		igt_subtest_group {
			igt_fixture {
				igt_require(false);
			}

			igt_subtest("not-run") {
				internal_assert(0);
			}

			igt_subtest_group {
				/* need to make sure we don't accidentally
				 * restore to "run testcases" when an outer
				 * group is already in SKIP state. */
				igt_subtest("still-not-run") {
					internal_assert(0);
				}
			}
		}

		igt_subtest("run") {
			t1 = true;
			internal_assert(1);
		}
	}

	igt_subtest_group {
		igt_fixture {
			internal_assert(t2 == 0);
			t2 = 1;
		}

		igt_subtest("run-again") {
			internal_assert(t2 == 1);
			t2 = 2;
		}

		igt_fixture {
			internal_assert(t2 == 2);
			t2 = 3;

		}
	}

	internal_assert(t1);
	internal_assert(t2 == 3);
}
