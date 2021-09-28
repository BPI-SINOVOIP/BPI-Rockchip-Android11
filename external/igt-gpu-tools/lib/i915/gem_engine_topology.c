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

#include "drmtest.h"
#include "ioctl_wrappers.h"

#include "i915/gem_engine_topology.h"

/*
 * Limit what we support for simplicity due limitation in how much we
 * can address via execbuf2.
 */
#define SIZEOF_CTX_PARAM	offsetof(struct i915_context_param_engines, \
					 engines[GEM_MAX_ENGINES])
#define SIZEOF_QUERY		offsetof(struct drm_i915_query_engine_info, \
					 engines[GEM_MAX_ENGINES])

#define DEFINE_CONTEXT_ENGINES_PARAM(e__, p__, c__, N__) \
		I915_DEFINE_CONTEXT_PARAM_ENGINES(e__, N__); \
		struct drm_i915_gem_context_param p__ = { \
			.param = I915_CONTEXT_PARAM_ENGINES, \
			.ctx_id = c__, \
			.size = SIZEOF_CTX_PARAM, \
			.value = to_user_pointer(&e__), \
		}

static int __gem_query(int fd, struct drm_i915_query *q)
{
	int err = 0;

	if (igt_ioctl(fd, DRM_IOCTL_I915_QUERY, q))
		err = -errno;

	errno = 0;
	return err;
}

static void gem_query(int fd, struct drm_i915_query *q)
{
	igt_assert_eq(__gem_query(fd, q), 0);
}

static void query_engines(int fd,
			  struct drm_i915_query_engine_info *query_engines,
			  int length)
{
	struct drm_i915_query_item item = { };
	struct drm_i915_query query = { };

	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	query.items_ptr = to_user_pointer(&item);
	query.num_items = 1;
	item.length = length;

	item.data_ptr = to_user_pointer(query_engines);

	gem_query(fd, &query);
}

static void ctx_map_engines(int fd, struct intel_engine_data *ed,
			    struct drm_i915_gem_context_param *param)
{
	struct i915_context_param_engines *engines =
			from_user_pointer(param->value);
	int i = 0;

	for (typeof(engines->engines[0]) *p =
	     &engines->engines[0];
	     i < ed->nengines; i++, p++) {
		p->engine_class = ed->engines[i].class;
		p->engine_instance = ed->engines[i].instance;
	}

	param->size = offsetof(typeof(*engines), engines[i]);
	engines->extensions = 0;

	gem_context_set_param(fd, param);
}

static void init_engine(struct intel_execution_engine2 *e2,
			int class, int instance, uint64_t flags)
{
	const struct intel_execution_engine2 *__e2;
	static const char *unknown_name = "unknown",
			  *virtual_name = "virtual";

	e2->class    = class;
	e2->instance = instance;
	e2->flags    = flags;

	/* engine is a virtual engine */
	if (class == I915_ENGINE_CLASS_INVALID &&
	    instance == I915_ENGINE_CLASS_INVALID_VIRTUAL) {
		e2->name = virtual_name;
		e2->is_virtual = true;
		return;
	}

	__for_each_static_engine(__e2)
		if (__e2->class == class && __e2->instance == instance)
			break;

	if (__e2->name) {
		e2->name = __e2->name;
	} else {
		igt_warn("found unknown engine (%d, %d)\n", class, instance);
		e2->name = unknown_name;
		e2->flags = -1;
	}

	/* just to remark it */
	e2->is_virtual = false;
}

static void query_engine_list(int fd, struct intel_engine_data *ed)
{
	uint8_t buff[SIZEOF_QUERY] = { };
	struct drm_i915_query_engine_info *query_engine =
			(struct drm_i915_query_engine_info *) buff;
	int i;

	query_engines(fd, query_engine, SIZEOF_QUERY);

	for (i = 0; i < query_engine->num_engines; i++)
		init_engine(&ed->engines[i],
			    query_engine->engines[i].engine.engine_class,
			    query_engine->engines[i].engine.engine_instance, i);

	ed->nengines = query_engine->num_engines;
}

struct intel_execution_engine2 *
intel_get_current_engine(struct intel_engine_data *ed)
{
	if (!ed->n)
		ed->current_engine = &ed->engines[0];
	else if (ed->n >= ed->nengines)
		ed->current_engine = NULL;

	return ed->current_engine;
}

void intel_next_engine(struct intel_engine_data *ed)
{
	if (ed->n + 1 < ed->nengines) {
		ed->n++;
		ed->current_engine = &ed->engines[ed->n];
	} else {
		ed->n = ed->nengines;
		ed->current_engine = NULL;
	}
}

struct intel_execution_engine2 *
intel_get_current_physical_engine(struct intel_engine_data *ed)
{
	struct intel_execution_engine2 *e;

	for (e = intel_get_current_engine(ed);
	     e && e->is_virtual;
	     intel_next_engine(ed))
		;

	return e;
}

static int gem_topology_get_param(int fd,
				  struct drm_i915_gem_context_param *p)
{
	if (igt_only_list_subtests())
		return -ENODEV;

	if (__gem_context_get_param(fd, p))
		return -1; /* using default engine map */

	if (!p->size)
		return 0;

	/* size will store the engine count */
	p->size = (p->size - sizeof(struct i915_context_param_engines)) /
		  (offsetof(struct i915_context_param_engines,
			    engines[1]) -
		  sizeof(struct i915_context_param_engines));

	igt_assert_f(p->size <= GEM_MAX_ENGINES, "unsupported engine count\n");

	return 0;
}

struct intel_engine_data intel_init_engine_list(int fd, uint32_t ctx_id)
{
	DEFINE_CONTEXT_ENGINES_PARAM(engines, param, ctx_id, GEM_MAX_ENGINES);
	struct intel_engine_data engine_data = { };
	int i;

	if (gem_topology_get_param(fd, &param)) {
		/* if kernel does not support engine/context mapping */
		const struct intel_execution_engine2 *e2;

		igt_debug("using pre-allocated engine list\n");

		__for_each_static_engine(e2) {
			struct intel_execution_engine2 *__e2 =
				&engine_data.engines[engine_data.nengines];

			__e2->name       = e2->name;
			__e2->instance   = e2->instance;
			__e2->class      = e2->class;
			__e2->flags      = e2->flags;
			__e2->is_virtual = false;

			if (igt_only_list_subtests() ||
			    gem_has_ring(fd, e2->flags))
				engine_data.nengines++;
		}
		return engine_data;
	}

	if (!param.size) {
		query_engine_list(fd, &engine_data);
		ctx_map_engines(fd, &engine_data, &param);
	} else {
		/* param.size contains the engine count */
		for (i = 0; i < param.size; i++)
			init_engine(&engine_data.engines[i],
				    engines.engines[i].engine_class,
				    engines.engines[i].engine_instance,
				    i);

		engine_data.nengines = i;
	}

	return engine_data;
}

int gem_context_lookup_engine(int fd, uint64_t engine, uint32_t ctx_id,
			      struct intel_execution_engine2 *e)
{
	DEFINE_CONTEXT_ENGINES_PARAM(engines, param, ctx_id, GEM_MAX_ENGINES);

	/* a bit paranoic */
	igt_assert(e);

	if (gem_topology_get_param(fd, &param) || !param.size)
		return -EINVAL;

	e->class = engines.engines[engine].engine_class;
	e->instance = engines.engines[engine].engine_instance;

	return 0;
}

void gem_context_set_all_engines(int fd, uint32_t ctx)
{
	DEFINE_CONTEXT_ENGINES_PARAM(engines, param, ctx, GEM_MAX_ENGINES);
	struct intel_engine_data engine_data = { };

	if (!gem_topology_get_param(fd, &param) && !param.size) {
		query_engine_list(fd, &engine_data);
		ctx_map_engines(fd, &engine_data, &param);
	}
}

bool gem_has_engine_topology(int fd)
{
	struct drm_i915_gem_context_param param = {
		.param = I915_CONTEXT_PARAM_ENGINES,
	};

	return !__gem_context_get_param(fd, &param);
}

struct intel_execution_engine2 gem_eb_flags_to_engine(unsigned int flags)
{
	const unsigned int ring = flags & (I915_EXEC_RING_MASK | 3 << 13);
	struct intel_execution_engine2 e2__ = {
		.class = -1,
		.instance = -1,
		.flags = -1,
		.name = "invalid"
	};

	if (ring == I915_EXEC_DEFAULT) {
		e2__.flags = I915_EXEC_DEFAULT;
		e2__.name = "default";
	} else {
		const struct intel_execution_engine2 *e2;

		__for_each_static_engine(e2) {
			if (e2->flags == ring)
				return *e2;
		}
	}

	return e2__;
}

bool gem_context_has_engine_map(int fd, uint32_t ctx)
{
	struct drm_i915_gem_context_param param = {
		.param = I915_CONTEXT_PARAM_ENGINES,
		.ctx_id = ctx
	};

	/*
	 * If the kernel is too old to support PARAM_ENGINES,
	 * then naturally the context has no engine map.
	 */
	if (__gem_context_get_param(fd, &param))
		return false;

	return param.size;
}

bool gem_engine_is_equal(const struct intel_execution_engine2 *e1,
			 const struct intel_execution_engine2 *e2)
{
	return e1->class == e2->class && e1->instance == e2->instance;
}
