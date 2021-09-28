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
 */

#ifndef GEM_ENGINE_TOPOLOGY_H
#define GEM_ENGINE_TOPOLOGY_H

#include "igt_gt.h"
#include "i915_drm.h"

#define GEM_MAX_ENGINES		I915_EXEC_RING_MASK + 1

struct intel_engine_data {
	uint32_t nengines;
	uint32_t n;
	struct intel_execution_engine2 *current_engine;
	struct intel_execution_engine2 engines[GEM_MAX_ENGINES];
};

bool gem_has_engine_topology(int fd);
struct intel_engine_data intel_init_engine_list(int fd, uint32_t ctx_id);

/* iteration functions */
struct intel_execution_engine2 *
intel_get_current_engine(struct intel_engine_data *ed);

struct intel_execution_engine2 *
intel_get_current_physical_engine(struct intel_engine_data *ed);

void intel_next_engine(struct intel_engine_data *ed);

int gem_context_lookup_engine(int fd, uint64_t engine, uint32_t ctx_id,
			      struct intel_execution_engine2 *e);

void gem_context_set_all_engines(int fd, uint32_t ctx);

bool gem_context_has_engine_map(int fd, uint32_t ctx);

bool gem_engine_is_equal(const struct intel_execution_engine2 *e1,
			 const struct intel_execution_engine2 *e2);

struct intel_execution_engine2 gem_eb_flags_to_engine(unsigned int flags);

#define __for_each_static_engine(e__) \
	for ((e__) = intel_execution_engines2; (e__)->name; (e__)++)

#define for_each_context_engine(fd__, ctx__, e__) \
	for (struct intel_engine_data i__ = intel_init_engine_list(fd__, ctx__); \
	     ((e__) = intel_get_current_engine(&i__)); \
	     intel_next_engine(&i__))

/* needs to replace "for_each_physical_engine" when conflicts are fixed */
#define __for_each_physical_engine(fd__, e__) \
	for (struct intel_engine_data i__ = intel_init_engine_list(fd__, 0); \
	     ((e__) = intel_get_current_physical_engine(&i__)); \
	     intel_next_engine(&i__))

#endif /* GEM_ENGINE_TOPOLOGY_H */
