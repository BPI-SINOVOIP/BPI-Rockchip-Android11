/*
 * Copyright © 2018-2019 Intel Corporation
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

#include <sched.h>

#include "igt.h"
#include "igt_perf.h"
#include "i915/gem_ring.h"
#include "sw_sync.h"

IGT_TEST_DESCRIPTION("Exercise in-kernel load-balancing");

#define INSTANCE_COUNT (1 << I915_PMU_SAMPLE_INSTANCE_BITS)

static size_t sizeof_load_balance(int count)
{
	return offsetof(struct i915_context_engines_load_balance,
			engines[count]);
}

static size_t sizeof_param_engines(int count)
{
	return offsetof(struct i915_context_param_engines,
			engines[count]);
}

static size_t sizeof_engines_bond(int count)
{
	return offsetof(struct i915_context_engines_bond,
			engines[count]);
}

#define alloca0(sz) ({ size_t sz__ = (sz); memset(alloca(sz__), 0, sz__); })

static bool has_class_instance(int i915, uint16_t class, uint16_t instance)
{
	int fd;

	fd = perf_i915_open(I915_PMU_ENGINE_BUSY(class, instance));
	if (fd != -1) {
		close(fd);
		return true;
	}

	return false;
}

static struct i915_engine_class_instance *
list_engines(int i915, uint32_t class_mask, unsigned int *out)
{
	unsigned int count = 0, size = 64;
	struct i915_engine_class_instance *engines;

	engines = malloc(size * sizeof(*engines));
	igt_assert(engines);

	for (enum drm_i915_gem_engine_class class = I915_ENGINE_CLASS_RENDER;
	     class_mask;
	     class++, class_mask >>= 1) {
		if (!(class_mask & 1))
			continue;

		for (unsigned int instance = 0;
		     instance < INSTANCE_COUNT;
		     instance++) {
			if (!has_class_instance(i915, class, instance))
				continue;

			if (count == size) {
				size *= 2;
				engines = realloc(engines,
						  size * sizeof(*engines));
				igt_assert(engines);
			}

			engines[count++] = (struct i915_engine_class_instance){
				.engine_class = class,
				.engine_instance = instance,
			};
		}
	}

	if (!count) {
		free(engines);
		engines = NULL;
	}

	*out = count;
	return engines;
}

static int __set_engines(int i915, uint32_t ctx,
			 const struct i915_engine_class_instance *ci,
			 unsigned int count)
{
	struct i915_context_param_engines *engines =
		alloca0(sizeof_param_engines(count));
	struct drm_i915_gem_context_param p = {
		.ctx_id = ctx,
		.param = I915_CONTEXT_PARAM_ENGINES,
		.size = sizeof_param_engines(count),
		.value = to_user_pointer(engines)
	};

	engines->extensions = 0;
	memcpy(engines->engines, ci, count * sizeof(*ci));

	return __gem_context_set_param(i915, &p);
}

static void set_engines(int i915, uint32_t ctx,
			const struct i915_engine_class_instance *ci,
			unsigned int count)
{
	igt_assert_eq(__set_engines(i915, ctx, ci, count), 0);
}

static int __set_load_balancer(int i915, uint32_t ctx,
			       const struct i915_engine_class_instance *ci,
			       unsigned int count,
			       void *ext)
{
	struct i915_context_engines_load_balance *balancer =
		alloca0(sizeof_load_balance(count));
	struct i915_context_param_engines *engines =
		alloca0(sizeof_param_engines(count + 1));
	struct drm_i915_gem_context_param p = {
		.ctx_id = ctx,
		.param = I915_CONTEXT_PARAM_ENGINES,
		.size = sizeof_param_engines(count + 1),
		.value = to_user_pointer(engines)
	};

	balancer->base.name = I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
	balancer->base.next_extension = to_user_pointer(ext);

	igt_assert(count);
	balancer->num_siblings = count;
	memcpy(balancer->engines, ci, count * sizeof(*ci));

	engines->extensions = to_user_pointer(balancer);
	engines->engines[0].engine_class =
		I915_ENGINE_CLASS_INVALID;
	engines->engines[0].engine_instance =
		I915_ENGINE_CLASS_INVALID_NONE;
	memcpy(engines->engines + 1, ci, count * sizeof(*ci));

	return __gem_context_set_param(i915, &p);
}

static void set_load_balancer(int i915, uint32_t ctx,
			      const struct i915_engine_class_instance *ci,
			      unsigned int count,
			      void *ext)
{
	igt_assert_eq(__set_load_balancer(i915, ctx, ci, count, ext), 0);
}

static uint32_t load_balancer_create(int i915,
				     const struct i915_engine_class_instance *ci,
				     unsigned int count)
{
	uint32_t ctx;

	ctx = gem_context_create(i915);
	set_load_balancer(i915, ctx, ci, count, NULL);

	return ctx;
}

static uint32_t __batch_create(int i915, uint32_t offset)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t handle;

	handle = gem_create(i915, ALIGN(offset + 4, 4096));
	gem_write(i915, handle, offset, &bbe, sizeof(bbe));

	return handle;
}

static uint32_t batch_create(int i915)
{
	return __batch_create(i915, 0);
}

static void invalid_balancer(int i915)
{
	I915_DEFINE_CONTEXT_ENGINES_LOAD_BALANCE(balancer, 64);
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines, 64);
	struct drm_i915_gem_context_param p = {
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines)
	};
	uint32_t handle;
	void *ptr;

	/*
	 * Assume that I915_CONTEXT_PARAM_ENGINE validates the array
	 * of engines[], our job is to determine if the load_balancer
	 * extension explodes.
	 */

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		unsigned int count;

		ci = list_engines(i915, 1 << class, &count);
		if (!ci)
			continue;

		igt_assert_lte(count, 64);

		p.ctx_id = gem_context_create(i915);
		p.size = (sizeof(struct i915_context_param_engines) +
			  (count + 1) * sizeof(*engines.engines));

		memset(&engines, 0, sizeof(engines));
		engines.engines[0].engine_class = I915_ENGINE_CLASS_INVALID;
		engines.engines[0].engine_instance = I915_ENGINE_CLASS_INVALID_NONE;
		memcpy(engines.engines + 1, ci, count * sizeof(*ci));
		gem_context_set_param(i915, &p);

		engines.extensions = -1ull;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

		engines.extensions = 1ull;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

		memset(&balancer, 0, sizeof(balancer));
		balancer.base.name = I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
		balancer.num_siblings = count;
		memcpy(balancer.engines, ci, count * sizeof(*ci));

		engines.extensions = to_user_pointer(&balancer);
		gem_context_set_param(i915, &p);

		balancer.engine_index = 1;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EEXIST);

		balancer.engine_index = count;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EEXIST);

		balancer.engine_index = count + 1;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EINVAL);

		balancer.engine_index = 0;
		gem_context_set_param(i915, &p);

		balancer.base.next_extension = to_user_pointer(&balancer);
		igt_assert_eq(__gem_context_set_param(i915, &p), -EEXIST);

		balancer.base.next_extension = -1ull;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

		handle = gem_create(i915, 4096 * 3);
		ptr = gem_mmap__gtt(i915, handle, 4096 * 3, PROT_WRITE);
		gem_close(i915, handle);

		memset(&engines, 0, sizeof(engines));
		engines.engines[0].engine_class = I915_ENGINE_CLASS_INVALID;
		engines.engines[0].engine_instance = I915_ENGINE_CLASS_INVALID_NONE;
		engines.engines[1].engine_class = I915_ENGINE_CLASS_INVALID;
		engines.engines[1].engine_instance = I915_ENGINE_CLASS_INVALID_NONE;
		memcpy(engines.engines + 2, ci, count * sizeof(ci));
		p.size = (sizeof(struct i915_context_param_engines) +
			  (count + 2) * sizeof(*engines.engines));
		gem_context_set_param(i915, &p);

		balancer.base.next_extension = 0;
		balancer.engine_index = 1;
		engines.extensions = to_user_pointer(&balancer);
		gem_context_set_param(i915, &p);

		memcpy(ptr + 4096 - 8, &balancer, sizeof(balancer));
		memcpy(ptr + 8192 - 8, &balancer, sizeof(balancer));
		balancer.engine_index = 0;

		engines.extensions = to_user_pointer(ptr) + 4096 - 8;
		gem_context_set_param(i915, &p);

		balancer.base.next_extension = engines.extensions;
		engines.extensions = to_user_pointer(&balancer);
		gem_context_set_param(i915, &p);

		munmap(ptr, 4096);
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);
		engines.extensions = to_user_pointer(ptr) + 4096 - 8;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

		engines.extensions = to_user_pointer(ptr) + 8192 - 8;
		gem_context_set_param(i915, &p);

		balancer.base.next_extension = engines.extensions;
		engines.extensions = to_user_pointer(&balancer);
		gem_context_set_param(i915, &p);

		munmap(ptr + 8192, 4096);
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);
		engines.extensions = to_user_pointer(ptr) + 8192 - 8;
		igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

		munmap(ptr + 4096, 4096);

		gem_context_destroy(i915, p.ctx_id);
		free(ci);
	}
}

static void invalid_bonds(int i915)
{
	I915_DEFINE_CONTEXT_ENGINES_BOND(bonds[16], 1);
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines, 1);
	struct drm_i915_gem_context_param p = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines),
		.size = sizeof(engines),
	};
	uint32_t handle;
	void *ptr;

	memset(&engines, 0, sizeof(engines));
	gem_context_set_param(i915, &p);

	memset(bonds, 0, sizeof(bonds));
	for (int n = 0; n < ARRAY_SIZE(bonds); n++) {
		bonds[n].base.name = I915_CONTEXT_ENGINES_EXT_BOND;
		bonds[n].base.next_extension =
			n ? to_user_pointer(&bonds[n - 1]) : 0;
		bonds[n].num_bonds = 1;
	}
	engines.extensions = to_user_pointer(&bonds);
	gem_context_set_param(i915, &p);

	bonds[0].base.next_extension = -1ull;
	igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

	bonds[0].base.next_extension = to_user_pointer(&bonds[0]);
	igt_assert_eq(__gem_context_set_param(i915, &p), -E2BIG);

	engines.extensions = to_user_pointer(&bonds[1]);
	igt_assert_eq(__gem_context_set_param(i915, &p), -E2BIG);
	bonds[0].base.next_extension = 0;
	gem_context_set_param(i915, &p);

	handle = gem_create(i915, 4096 * 3);
	ptr = gem_mmap__gtt(i915, handle, 4096 * 3, PROT_WRITE);
	gem_close(i915, handle);

	memcpy(ptr + 4096, &bonds[0], sizeof(bonds[0]));
	engines.extensions = to_user_pointer(ptr) + 4096;
	gem_context_set_param(i915, &p);

	memcpy(ptr, &bonds[0], sizeof(bonds[0]));
	bonds[0].base.next_extension = to_user_pointer(ptr);
	memcpy(ptr + 4096, &bonds[0], sizeof(bonds[0]));
	gem_context_set_param(i915, &p);

	munmap(ptr, 4096);
	igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

	bonds[0].base.next_extension = 0;
	memcpy(ptr + 8192, &bonds[0], sizeof(bonds[0]));
	bonds[0].base.next_extension = to_user_pointer(ptr) + 8192;
	memcpy(ptr + 4096, &bonds[0], sizeof(bonds[0]));
	gem_context_set_param(i915, &p);

	munmap(ptr + 8192, 4096);
	igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

	munmap(ptr + 4096, 4096);
	igt_assert_eq(__gem_context_set_param(i915, &p), -EFAULT);

	gem_context_destroy(i915, p.ctx_id);
}

static void kick_kthreads(void)
{
	usleep(20 * 1000); /* 20ms should be enough for ksoftirqd! */
}

static double measure_load(int pmu, int period_us)
{
	uint64_t data[2];
	uint64_t d_t, d_v;

	kick_kthreads();

	igt_assert_eq(read(pmu, data, sizeof(data)), sizeof(data));
	d_v = -data[0];
	d_t = -data[1];

	usleep(period_us);

	igt_assert_eq(read(pmu, data, sizeof(data)), sizeof(data));
	d_v += data[0];
	d_t += data[1];

	return d_v / (double)d_t;
}

static double measure_min_load(int pmu, unsigned int num, int period_us)
{
	uint64_t data[2 + num];
	uint64_t d_t, d_v[num];
	uint64_t min = -1, max = 0;

	kick_kthreads();

	igt_assert_eq(read(pmu, data, sizeof(data)), sizeof(data));
	for (unsigned int n = 0; n < num; n++)
		d_v[n] = -data[2 + n];
	d_t = -data[1];

	usleep(period_us);

	igt_assert_eq(read(pmu, data, sizeof(data)), sizeof(data));

	d_t += data[1];
	for (unsigned int n = 0; n < num; n++) {
		d_v[n] += data[2 + n];
		igt_debug("engine[%d]: %.1f%%\n",
			  n, d_v[n] / (double)d_t * 100);
		if (d_v[n] < min)
			min = d_v[n];
		if (d_v[n] > max)
			max = d_v[n];
	}

	igt_debug("elapsed: %"PRIu64"ns, load [%.1f, %.1f]%%\n",
		  d_t, min / (double)d_t * 100,  max / (double)d_t * 100);

	return min / (double)d_t;
}

static void measure_all_load(int pmu, double *v, unsigned int num, int period_us)
{
	uint64_t data[2 + num];
	uint64_t d_t, d_v[num];

	kick_kthreads();

	igt_assert_eq(read(pmu, data, sizeof(data)), sizeof(data));
	for (unsigned int n = 0; n < num; n++)
		d_v[n] = -data[2 + n];
	d_t = -data[1];

	usleep(period_us);

	igt_assert_eq(read(pmu, data, sizeof(data)), sizeof(data));

	d_t += data[1];
	for (unsigned int n = 0; n < num; n++) {
		d_v[n] += data[2 + n];
		igt_debug("engine[%d]: %.1f%%\n",
			  n, d_v[n] / (double)d_t * 100);
		v[n] = d_v[n] / (double)d_t;
	}
}

static int add_pmu(int pmu, const struct i915_engine_class_instance *ci)
{
	return perf_i915_open_group(I915_PMU_ENGINE_BUSY(ci->engine_class,
							 ci->engine_instance),
				    pmu);
}

static const char *class_to_str(int class)
{
	const char *str[] = {
		[I915_ENGINE_CLASS_RENDER] = "rcs",
		[I915_ENGINE_CLASS_COPY] = "bcs",
		[I915_ENGINE_CLASS_VIDEO] = "vcs",
		[I915_ENGINE_CLASS_VIDEO_ENHANCE] = "vecs",
	};

	if (class < ARRAY_SIZE(str))
		return str[class];

	return "unk";
}

static void check_individual_engine(int i915,
				    uint32_t ctx,
				    const struct i915_engine_class_instance *ci,
				    int idx)
{
	igt_spin_t *spin;
	double load;
	int pmu;

	pmu = perf_i915_open(I915_PMU_ENGINE_BUSY(ci[idx].engine_class,
						  ci[idx].engine_instance));

	spin = igt_spin_new(i915, .ctx = ctx, .engine = idx + 1);
	load = measure_load(pmu, 10000);
	igt_spin_free(i915, spin);

	close(pmu);

	igt_assert_f(load > 0.90,
		     "engine %d (class:instance %d:%d) was found to be only %.1f%% busy\n",
		     idx, ci[idx].engine_class, ci[idx].engine_instance, load*100);
}

static void individual(int i915)
{
	uint32_t ctx;

	/*
	 * I915_CONTEXT_PARAM_ENGINE allows us to index into the user
	 * supplied array from gem_execbuf(). Our check is to build the
	 * ctx->engine[] with various different engine classes, feed in
	 * a spinner and then ask pmu to confirm it the expected engine
	 * was busy.
	 */

	ctx = gem_context_create(i915);

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		unsigned int count;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci)
			continue;

		for (int pass = 0; pass < count; pass++) { /* approx. count! */
			igt_assert(sizeof(*ci) == sizeof(int));
			igt_permute_array(ci, count, igt_exchange_int);
			set_load_balancer(i915, ctx, ci, count, NULL);
			for (unsigned int n = 0; n < count; n++)
				check_individual_engine(i915, ctx, ci, n);
		}

		free(ci);
	}

	gem_context_destroy(i915, ctx);
	gem_quiescent_gpu(i915);
}

static void bonded(int i915, unsigned int flags)
#define CORK 0x1
{
	I915_DEFINE_CONTEXT_ENGINES_BOND(bonds[16], 1);
	struct i915_engine_class_instance *master_engines;
	uint32_t master;

	/*
	 * I915_CONTEXT_PARAM_ENGINE provides an extension that allows us
	 * to specify which engine(s) to pair with a parallel (EXEC_SUBMIT)
	 * request submitted to another engine.
	 */

	master = gem_queue_create(i915);

	memset(bonds, 0, sizeof(bonds));
	for (int n = 0; n < ARRAY_SIZE(bonds); n++) {
		bonds[n].base.name = I915_CONTEXT_ENGINES_EXT_BOND;
		bonds[n].base.next_extension =
			n ? to_user_pointer(&bonds[n - 1]) : 0;
		bonds[n].num_bonds = 1;
	}

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *siblings;
		unsigned int count, limit, *order;
		uint32_t ctx;
		int n;

		siblings = list_engines(i915, 1u << class, &count);
		if (!siblings)
			continue;

		if (count < 2) {
			free(siblings);
			continue;
		}

		master_engines = list_engines(i915, ~(1u << class), &limit);
		set_engines(i915, master, master_engines, limit);

		limit = min(count, limit);
		igt_assert(limit <= ARRAY_SIZE(bonds));
		for (n = 0; n < limit; n++) {
			bonds[n].master = master_engines[n];
			bonds[n].engines[0] = siblings[n];
		}

		ctx = gem_context_clone(i915,
					master, I915_CONTEXT_CLONE_VM,
					I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE);
		set_load_balancer(i915, ctx, siblings, count, &bonds[limit - 1]);

		order = malloc(sizeof(*order) * 8 * limit);
		igt_assert(order);
		for (n = 0; n < limit; n++)
			order[2 * limit - n - 1] = order[n] = n % limit;
		memcpy(order + 2 * limit, order, 2 * limit * sizeof(*order));
		memcpy(order + 4 * limit, order, 4 * limit * sizeof(*order));
		igt_permute_array(order + 2 * limit, 6 * limit, igt_exchange_int);

		for (n = 0; n < 8 * limit; n++) {
			struct drm_i915_gem_execbuffer2 eb;
			igt_spin_t *spin, *plug;
			IGT_CORK_HANDLE(cork);
			double v[limit];
			int pmu[limit + 1];
			int bond = order[n];

			pmu[0] = -1;
			for (int i = 0; i < limit; i++)
				pmu[i] = add_pmu(pmu[0], &siblings[i]);
			pmu[limit] = add_pmu(pmu[0], &master_engines[bond]);

			igt_assert(siblings[bond].engine_class !=
				   master_engines[bond].engine_class);

			plug = NULL;
			if (flags & CORK) {
				plug = __igt_spin_new(i915,
						      .ctx = master,
						      .engine = bond,
						      .dependency = igt_cork_plug(&cork, i915));
			}

			spin = __igt_spin_new(i915,
					      .ctx = master,
					      .engine = bond,
					      .flags = IGT_SPIN_FENCE_OUT);

			eb = spin->execbuf;
			eb.rsvd1 = ctx;
			eb.rsvd2 = spin->out_fence;
			eb.flags = I915_EXEC_FENCE_SUBMIT;
			gem_execbuf(i915, &eb);

			if (plug) {
				igt_cork_unplug(&cork);
				igt_spin_free(i915, plug);
			}

			measure_all_load(pmu[0], v, limit + 1, 10000);
			igt_spin_free(i915, spin);

			igt_assert_f(v[bond] > 0.90,
				     "engine %d (class:instance %s:%d) was found to be only %.1f%% busy\n",
				     bond,
				     class_to_str(siblings[bond].engine_class),
				     siblings[bond].engine_instance,
				     100 * v[bond]);
			for (int other = 0; other < limit; other++) {
				if (other == bond)
					continue;

				igt_assert_f(v[other] == 0,
					     "engine %d (class:instance %s:%d) was not idle, and actually %.1f%% busy\n",
					     other,
					     class_to_str(siblings[other].engine_class),
					     siblings[other].engine_instance,
					     100 * v[other]);
			}
			igt_assert_f(v[limit] > 0.90,
				     "master (class:instance %s:%d) was found to be only %.1f%% busy\n",
				     class_to_str(master_engines[bond].engine_class),
				     master_engines[bond].engine_instance,
				     100 * v[limit]);

			close(pmu[0]);
		}

		free(order);
		gem_context_destroy(i915, ctx);
		free(master_engines);
		free(siblings);
	}

	gem_context_destroy(i915, master);
}

static void indices(int i915)
{
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines, I915_EXEC_RING_MASK + 1);
	struct drm_i915_gem_context_param p = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines)
	};

	struct drm_i915_gem_exec_object2 batch = {
		.handle = batch_create(i915),
	};

	unsigned int nengines = 0;
	void *balancers = NULL;

	/*
	 * We can populate our engine map with multiple virtual engines.
	 * Do so.
	 */

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		unsigned int count;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci)
			continue;

		for (int n = 0; n < count; n++) {
			struct i915_context_engines_load_balance *balancer;

			engines.engines[nengines].engine_class =
				I915_ENGINE_CLASS_INVALID;
			engines.engines[nengines].engine_instance =
				I915_ENGINE_CLASS_INVALID_NONE;

			balancer = calloc(sizeof_load_balance(count), 1);
			igt_assert(balancer);

			balancer->base.name =
				I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
			balancer->base.next_extension =
				to_user_pointer(balancers);
			balancers = balancer;

			balancer->engine_index = nengines++;
			balancer->num_siblings = count;

			memcpy(balancer->engines,
			       ci, count * sizeof(*ci));
		}
		free(ci);
	}

	igt_require(balancers);
	engines.extensions = to_user_pointer(balancers);
	p.size = (sizeof(struct i915_engine_class_instance) * nengines +
		  sizeof(struct i915_context_param_engines));
	gem_context_set_param(i915, &p);

	for (unsigned int n = 0; n < nengines; n++) {
		struct drm_i915_gem_execbuffer2 eb = {
			.buffers_ptr = to_user_pointer(&batch),
			.buffer_count = 1,
			.flags = n,
			.rsvd1 = p.ctx_id,
		};
		igt_debug("Executing on index=%d\n", n);
		gem_execbuf(i915, &eb);
	}
	gem_context_destroy(i915, p.ctx_id);

	gem_sync(i915, batch.handle);
	gem_close(i915, batch.handle);

	while (balancers) {
		struct i915_context_engines_load_balance *b, *n;

		b = balancers;
		n = from_user_pointer(b->base.next_extension);
		free(b);

		balancers = n;
	}

	gem_quiescent_gpu(i915);
}

static void busy(int i915)
{
	uint32_t scratch = gem_create(i915, 4096);

	/*
	 * Check that virtual engines are reported via GEM_BUSY.
	 *
	 * When running, the batch will be on the real engine and report
	 * the actual class.
	 *
	 * Prior to running, if the load-balancer is across multiple
	 * classes we don't know which engine the batch will
	 * execute on, so we report them all!
	 *
	 * However, as we only support (and test) creating a load-balancer
	 * from engines of only one class, that can be propagated accurately
	 * through to GEM_BUSY.
	 */

	for (int class = 0; class < 16; class++) {
		struct drm_i915_gem_busy busy;
		struct i915_engine_class_instance *ci;
		unsigned int count;
		igt_spin_t *spin[2];
		uint32_t ctx;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci)
			continue;

		ctx = load_balancer_create(i915, ci, count);
		free(ci);

		spin[0] = __igt_spin_new(i915,
					 .ctx = ctx,
					 .flags = IGT_SPIN_POLL_RUN);
		spin[1] = __igt_spin_new(i915,
					 .ctx = ctx,
					 .dependency = scratch);

		igt_spin_busywait_until_started(spin[0]);

		/* Running: actual class */
		busy.handle = spin[0]->handle;
		do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
		igt_assert_eq_u32(busy.busy, 1u << (class + 16));

		/* Queued(read): expected class */
		busy.handle = spin[1]->handle;
		do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
		igt_assert_eq_u32(busy.busy, 1u << (class + 16));

		/* Queued(write): expected class */
		busy.handle = scratch;
		do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
		igt_assert_eq_u32(busy.busy,
				  (1u << (class + 16)) | (class + 1));

		igt_spin_free(i915, spin[1]);
		igt_spin_free(i915, spin[0]);

		gem_context_destroy(i915, ctx);
	}

	gem_close(i915, scratch);
	gem_quiescent_gpu(i915);
}

static void full(int i915, unsigned int flags)
#define PULSE 0x1
#define LATE 0x2
{
	struct drm_i915_gem_exec_object2 batch = {
		.handle = batch_create(i915),
	};

	if (flags & LATE)
		igt_require_sw_sync();

	/*
	 * I915_CONTEXT_PARAM_ENGINE changes the meaning of engine selector in
	 * execbuf to utilize our own map, into which we replace I915_EXEC_DEFAULT
	 * to provide an automatic selection from the other ctx->engine[]. It
	 * employs load-balancing to evenly distribute the workload the
	 * array. If we submit N spinners, we expect them to be simultaneously
	 * running across N engines and use PMU to confirm that the entire
	 * set of engines are busy.
	 *
	 * We complicate matters by interspersing short-lived tasks to
	 * challenge the kernel to search for space in which to insert new
	 * batches.
	 */

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		igt_spin_t *spin = NULL;
		IGT_CORK_FENCE(cork);
		unsigned int count;
		double load;
		int fence = -1;
		int *pmu;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci)
			continue;

		pmu = malloc(sizeof(*pmu) * count);
		igt_assert(pmu);

		if (flags & LATE)
			fence = igt_cork_plug(&cork, i915);

		pmu[0] = -1;
		for (unsigned int n = 0; n < count; n++) {
			uint32_t ctx;

			pmu[n] = add_pmu(pmu[0], &ci[n]);

			if (flags & PULSE) {
				struct drm_i915_gem_execbuffer2 eb = {
					.buffers_ptr = to_user_pointer(&batch),
					.buffer_count = 1,
					.rsvd2 = fence,
					.flags = flags & LATE ? I915_EXEC_FENCE_IN : 0,
				};
				gem_execbuf(i915, &eb);
			}

			/*
			 * Each spinner needs to be one a new timeline,
			 * otherwise they will just sit in the single queue
			 * and not run concurrently.
			 */
			ctx = load_balancer_create(i915, ci, count);

			if (spin == NULL) {
				spin = __igt_spin_new(i915, .ctx = ctx);
			} else {
				struct drm_i915_gem_execbuffer2 eb = {
					.buffers_ptr = spin->execbuf.buffers_ptr,
					.buffer_count = spin->execbuf.buffer_count,
					.rsvd1 = ctx,
					.rsvd2 = fence,
					.flags = flags & LATE ? I915_EXEC_FENCE_IN : 0,
				};
				gem_execbuf(i915, &eb);
			}

			gem_context_destroy(i915, ctx);
		}

		if (flags & LATE) {
			igt_cork_unplug(&cork);
			close(fence);
		}

		load = measure_min_load(pmu[0], count, 10000);
		igt_spin_free(i915, spin);

		close(pmu[0]);
		free(pmu);

		free(ci);

		igt_assert_f(load > 0.90,
			     "minimum load for %d x class:%d was found to be only %.1f%% busy\n",
			     count, class, load*100);
		gem_quiescent_gpu(i915);
	}

	gem_close(i915, batch.handle);
	gem_quiescent_gpu(i915);
}

static void nop(int i915)
{
	struct drm_i915_gem_exec_object2 batch = {
		.handle = batch_create(i915),
	};

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		unsigned int count;
		uint32_t ctx;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci)
			continue;

		ctx = load_balancer_create(i915, ci, count);

		for (int n = 0; n < count; n++) {
			struct drm_i915_gem_execbuffer2 execbuf = {
				.buffers_ptr = to_user_pointer(&batch),
				.buffer_count = 1,
				.flags = n + 1,
				.rsvd1 = ctx,
			};
			struct timespec tv = {};
			unsigned long nops;
			double t;

			igt_nsec_elapsed(&tv);
			nops = 0;
			do {
				for (int r = 0; r < 1024; r++)
					gem_execbuf(i915, &execbuf);
				nops += 1024;
			} while (igt_seconds_elapsed(&tv) < 2);
			gem_sync(i915, batch.handle);

			t = igt_nsec_elapsed(&tv) * 1e-3 / nops;
			igt_info("%s:%d %.3fus\n", class_to_str(class), n, t);
		}

		{
			struct drm_i915_gem_execbuffer2 execbuf = {
				.buffers_ptr = to_user_pointer(&batch),
				.buffer_count = 1,
				.rsvd1 = ctx,
			};
			struct timespec tv = {};
			unsigned long nops;
			double t;

			igt_nsec_elapsed(&tv);
			nops = 0;
			do {
				for (int r = 0; r < 1024; r++)
					gem_execbuf(i915, &execbuf);
				nops += 1024;
			} while (igt_seconds_elapsed(&tv) < 2);
			gem_sync(i915, batch.handle);

			t = igt_nsec_elapsed(&tv) * 1e-3 / nops;
			igt_info("%s:* %.3fus\n", class_to_str(class), t);
		}


		igt_fork(child, count) {
			struct drm_i915_gem_execbuffer2 execbuf = {
				.buffers_ptr = to_user_pointer(&batch),
				.buffer_count = 1,
				.flags = child + 1,
				.rsvd1 = gem_context_clone(i915, ctx,
							   I915_CONTEXT_CLONE_ENGINES, 0),
			};
			struct timespec tv = {};
			unsigned long nops;
			double t;

			igt_nsec_elapsed(&tv);
			nops = 0;
			do {
				for (int r = 0; r < 1024; r++)
					gem_execbuf(i915, &execbuf);
				nops += 1024;
			} while (igt_seconds_elapsed(&tv) < 2);
			gem_sync(i915, batch.handle);

			t = igt_nsec_elapsed(&tv) * 1e-3 / nops;
			igt_info("[%d] %s:%d %.3fus\n",
				 child, class_to_str(class), child, t);

			memset(&tv, 0, sizeof(tv));
			execbuf.flags = 0;

			igt_nsec_elapsed(&tv);
			nops = 0;
			do {
				for (int r = 0; r < 1024; r++)
					gem_execbuf(i915, &execbuf);
				nops += 1024;
			} while (igt_seconds_elapsed(&tv) < 2);
			gem_sync(i915, batch.handle);

			t = igt_nsec_elapsed(&tv) * 1e-3 / nops;
			igt_info("[%d] %s:* %.3fus\n",
				 child, class_to_str(class), t);

			gem_context_destroy(i915, execbuf.rsvd1);
		}

		igt_waitchildren();

		gem_context_destroy(i915, ctx);
		free(ci);
	}

	gem_close(i915, batch.handle);
	gem_quiescent_gpu(i915);
}

static void ping(int i915, uint32_t ctx, unsigned int engine)
{
	struct drm_i915_gem_exec_object2 obj = {
		.handle = batch_create(i915),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = engine,
		.rsvd1 = ctx,
	};
	gem_execbuf(i915, &execbuf);
	gem_sync(i915, obj.handle);
	gem_close(i915, obj.handle);
}

static void semaphore(int i915)
{
	uint32_t block[2], scratch;
	igt_spin_t *spin[3];

	/*
	 * If we are using HW semaphores to launch serialised requests
	 * on different engine concurrently, we want to verify that real
	 * work is unimpeded.
	 */
	igt_require(gem_scheduler_has_preemption(i915));

	block[0] = gem_context_create(i915);
	block[1] = gem_context_create(i915);

	scratch = gem_create(i915, 4096);
	spin[2] = igt_spin_new(i915, .dependency = scratch);
	for (int class = 1; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		unsigned int count;
		uint32_t vip;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci)
			continue;

		if (count < ARRAY_SIZE(block))
			continue;

		/* Ensure that we completely occupy all engines in this group */
		count = ARRAY_SIZE(block);

		for (int i = 0; i < count; i++) {
			set_load_balancer(i915, block[i], ci, count, NULL);
			spin[i] = __igt_spin_new(i915,
						 .ctx = block[i],
						 .dependency = scratch);
		}

		/*
		 * Either we haven't blocked both engines with semaphores,
		 * or we let the vip through. If not, we hang.
		 */
		vip = gem_context_create(i915);
		set_load_balancer(i915, vip, ci, count, NULL);
		ping(i915, vip, 0);
		gem_context_destroy(i915, vip);

		for (int i = 0; i < count; i++)
			igt_spin_free(i915, spin[i]);

		free(ci);
	}
	igt_spin_free(i915, spin[2]);
	gem_close(i915, scratch);

	gem_context_destroy(i915, block[1]);
	gem_context_destroy(i915, block[0]);

	gem_quiescent_gpu(i915);
}

static void smoketest(int i915, int timeout)
{
	struct drm_i915_gem_exec_object2 batch[2] = {
		{ .handle = __batch_create(i915, 16380) }
	};
	unsigned int ncontext = 0;
	uint32_t *contexts = NULL;
	uint32_t *handles = NULL;

	igt_require_sw_sync();

	for (int class = 0; class < 32; class++) {
		struct i915_engine_class_instance *ci;
		unsigned int count = 0;

		ci = list_engines(i915, 1u << class, &count);
		if (!ci || count < 2) {
			free(ci);
			continue;
		}

		ncontext += 128;
		contexts = realloc(contexts, sizeof(*contexts) * ncontext);
		igt_assert(contexts);

		for (unsigned int n = ncontext - 128; n < ncontext; n++) {
			contexts[n] = load_balancer_create(i915, ci, count);
			igt_assert(contexts[n]);
		}

		free(ci);
	}
	igt_debug("Created %d virtual engines (one per context)\n", ncontext);
	igt_require(ncontext);

	contexts = realloc(contexts, sizeof(*contexts) * ncontext * 4);
	igt_assert(contexts);
	memcpy(contexts + ncontext, contexts, ncontext * sizeof(*contexts));
	ncontext *= 2;
	memcpy(contexts + ncontext, contexts, ncontext * sizeof(*contexts));
	ncontext *= 2;

	handles = malloc(sizeof(*handles) * ncontext);
	igt_assert(handles);
	for (unsigned int n = 0; n < ncontext; n++)
		handles[n] = gem_create(i915, 4096);

	igt_until_timeout(timeout) {
		unsigned int count = 1 + (rand() % (ncontext - 1));
		IGT_CORK_FENCE(cork);
		int fence = igt_cork_plug(&cork, i915);

		for (unsigned int n = 0; n < count; n++) {
			struct drm_i915_gem_execbuffer2 eb = {
				.buffers_ptr = to_user_pointer(batch),
				.buffer_count = ARRAY_SIZE(batch),
				.rsvd1 = contexts[n],
				.rsvd2 = fence,
				.flags = I915_EXEC_BATCH_FIRST | I915_EXEC_FENCE_IN,
			};
			batch[1].handle = handles[n];
			gem_execbuf(i915, &eb);
		}
		igt_permute_array(handles, count, igt_exchange_int);

		igt_cork_unplug(&cork);
		for (unsigned int n = 0; n < count; n++)
			gem_sync(i915, handles[n]);

		close(fence);
	}

	for (unsigned int n = 0; n < ncontext; n++) {
		gem_close(i915, handles[n]);
		__gem_context_destroy(i915, contexts[n]);
	}
	free(handles);
	free(contexts);
	gem_close(i915, batch[0].handle);
}

static bool has_context_engines(int i915)
{
	struct drm_i915_gem_context_param p = {
		.param = I915_CONTEXT_PARAM_ENGINES,
	};

	return __gem_context_set_param(i915, &p) == 0;
}

static bool has_load_balancer(int i915)
{
	struct i915_engine_class_instance ci = {};
	uint32_t ctx;
	int err;

	ctx = gem_context_create(i915);
	err = __set_load_balancer(i915, ctx, &ci, 1, NULL);
	gem_context_destroy(i915, ctx);

	return err == 0;
}

igt_main
{
	int i915 = -1;

	igt_skip_on_simulation();

	igt_fixture {
		i915 = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(i915);

		gem_require_contexts(i915);
		igt_require(has_context_engines(i915));
		igt_require(has_load_balancer(i915));

		igt_fork_hang_detector(i915);
	}

	igt_subtest("invalid-balancer")
		invalid_balancer(i915);

	igt_subtest("invalid-bonds")
		invalid_bonds(i915);

	igt_subtest("individual")
		individual(i915);

	igt_subtest("indices")
		indices(i915);

	igt_subtest("busy")
		busy(i915);

	igt_subtest_group {
		static const struct {
			const char *name;
			unsigned int flags;
		} phases[] = {
			{ "", 0 },
			{ "-pulse", PULSE },
			{ "-late", LATE },
			{ "-late-pulse", PULSE | LATE },
			{ }
		};
		for (typeof(*phases) *p = phases; p->name; p++)
			igt_subtest_f("full%s", p->name)
				full(i915, p->flags);
	}

	igt_subtest("nop")
		nop(i915);

	igt_subtest("semaphore")
		semaphore(i915);

	igt_subtest("smoke")
		smoketest(i915, 20);

	igt_subtest("bonded-imm")
		bonded(i915, 0);

	igt_subtest("bonded-cork")
		bonded(i915, CORK);

	igt_fixture {
		igt_stop_hang_detector();
	}
}
