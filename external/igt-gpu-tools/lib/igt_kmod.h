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
 */

#ifndef IGT_KMOD_H
#define IGT_KMOD_H

#include <libkmod.h>

#include "igt_list.h"

bool igt_kmod_is_loaded(const char *mod_name);
void igt_kmod_list_loaded(void);

int igt_kmod_load(const char *mod_name, const char *opts);
int igt_kmod_unload(const char *mod_name, unsigned int flags);

int igt_i915_driver_load(const char *opts);
int igt_i915_driver_unload(void);

void igt_kselftests(const char *module_name,
		    const char *module_options,
		    const char *result_option,
		    const char *filter);

struct igt_kselftest {
	struct kmod_module *kmod;
	char *module_name;
	int kmsg;
};

struct igt_kselftest_list {
	struct igt_list link;
	unsigned int number;
	char *name;
	char param[];
};

int igt_kselftest_init(struct igt_kselftest *tst,
		       const char *module_name);
int igt_kselftest_begin(struct igt_kselftest *tst);

void igt_kselftest_get_tests(struct kmod_module *kmod,
			     const char *filter,
			     struct igt_list *tests);
int igt_kselftest_execute(struct igt_kselftest *tst,
			  struct igt_kselftest_list *tl,
			  const char *module_options,
			  const char *result);

void igt_kselftest_end(struct igt_kselftest *tst);
void igt_kselftest_fini(struct igt_kselftest *tst);

#endif /* IGT_KMOD_H */
