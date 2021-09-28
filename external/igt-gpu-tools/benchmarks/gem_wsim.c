/*
 * Copyright © 2017 Intel Corporation
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

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>

#include "intel_chipset.h"
#include "intel_reg.h"
#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"

#include "intel_io.h"
#include "igt_aux.h"
#include "igt_rand.h"
#include "igt_perf.h"
#include "sw_sync.h"
#include "i915/gem_mman.h"

#include "ewma.h"

enum intel_engine_id {
	DEFAULT,
	RCS,
	BCS,
	VCS,
	VCS1,
	VCS2,
	VECS,
	NUM_ENGINES
};

struct duration {
	unsigned int min, max;
};

enum w_type
{
	BATCH,
	SYNC,
	DELAY,
	PERIOD,
	THROTTLE,
	QD_THROTTLE,
	SW_FENCE,
	SW_FENCE_SIGNAL,
	CTX_PRIORITY,
	PREEMPTION,
	ENGINE_MAP,
	LOAD_BALANCE,
	BOND,
	TERMINATE,
	SSEU
};

struct deps
{
	int nr;
	bool submit_fence;
	int *list;
};

struct w_arg {
	char *filename;
	char *desc;
	int prio;
	bool sseu;
};

struct bond {
	uint64_t mask;
	enum intel_engine_id master;
};

struct w_step
{
	/* Workload step metadata */
	enum w_type type;
	unsigned int context;
	unsigned int engine;
	struct duration duration;
	bool unbound_duration;
	struct deps data_deps;
	struct deps fence_deps;
	int emit_fence;
	union {
		int sync;
		int delay;
		int period;
		int target;
		int throttle;
		int fence_signal;
		int priority;
		struct {
			unsigned int engine_map_count;
			enum intel_engine_id *engine_map;
		};
		bool load_balance;
		struct {
			uint64_t bond_mask;
			enum intel_engine_id bond_master;
		};
		int sseu;
	};

	/* Implementation details */
	unsigned int idx;
	struct igt_list rq_link;
	unsigned int request;
	unsigned int preempt_us;

	struct drm_i915_gem_execbuffer2 eb;
	struct drm_i915_gem_exec_object2 *obj;
	struct drm_i915_gem_relocation_entry reloc[5];
	unsigned long bb_sz;
	uint32_t bb_handle;
	uint32_t *seqno_value;
	uint32_t *seqno_address;
	uint32_t *rt0_value;
	uint32_t *rt0_address;
	uint32_t *rt1_address;
	uint32_t *latch_value;
	uint32_t *latch_address;
	uint32_t *recursive_bb_start;
};

DECLARE_EWMA(uint64_t, rt, 4, 2)

struct ctx {
	uint32_t id;
	int priority;
	unsigned int engine_map_count;
	enum intel_engine_id *engine_map;
	unsigned int bond_count;
	struct bond *bonds;
	bool targets_instance;
	bool wants_balance;
	unsigned int static_vcs;
	uint64_t sseu;
};

struct workload
{
	unsigned int id;

	unsigned int nr_steps;
	struct w_step *steps;
	int prio;
	bool sseu;

	pthread_t thread;
	bool run;
	bool background;
	const struct workload_balancer *balancer;
	unsigned int repeat;
	unsigned int flags;
	bool print_stats;

	uint32_t bb_prng;
	uint32_t prng;

	struct timespec repeat_start;

	unsigned int nr_ctxs;
	struct ctx *ctx_list;

	int sync_timeline;
	uint32_t sync_seqno;

	uint32_t seqno[NUM_ENGINES];
	struct drm_i915_gem_exec_object2 status_object[2];
	uint32_t *status_page;
	uint32_t *status_cs;
	unsigned int vcs_rr;

	unsigned long qd_sum[NUM_ENGINES];
	unsigned long nr_bb[NUM_ENGINES];

	struct igt_list requests[NUM_ENGINES];
	unsigned int nrequest[NUM_ENGINES];

	struct workload *global_wrk;
	const struct workload_balancer *global_balancer;
	pthread_mutex_t mutex;

	union {
		struct rtavg {
			struct ewma_rt avg[NUM_ENGINES];
			uint32_t last[NUM_ENGINES];
		} rt;
	};

	struct busy_balancer {
		int fd;
		bool first;
		unsigned int num_engines;
		unsigned int engine_map[NUM_ENGINES];
		uint64_t t_prev;
		uint64_t prev[NUM_ENGINES];
		double busy[NUM_ENGINES];
	} busy_balancer;
};

static const unsigned int nop_calibration_us = 1000;
static unsigned long nop_calibration;

static unsigned int master_prng;

static unsigned int context_vcs_rr;

static int verbose = 1;
static int fd;
static struct drm_i915_gem_context_param_sseu device_sseu = {
	.slice_mask = -1 /* Force read on first use. */
};

#define SWAPVCS		(1<<0)
#define SEQNO		(1<<1)
#define BALANCE		(1<<2)
#define RT		(1<<3)
#define VCS2REMAP	(1<<4)
#define INITVCSRR	(1<<5)
#define SYNCEDCLIENTS	(1<<6)
#define HEARTBEAT	(1<<7)
#define GLOBAL_BALANCE	(1<<8)
#define DEPSYNC		(1<<9)
#define I915		(1<<10)
#define SSEU		(1<<11)

#define SEQNO_IDX(engine) ((engine) * 16)
#define SEQNO_OFFSET(engine) (SEQNO_IDX(engine) * sizeof(uint32_t))

#define RCS_TIMESTAMP (0x2000 + 0x358)
#define REG(x) (volatile uint32_t *)((volatile char *)igt_global_mmio + x)

static const char *ring_str_map[NUM_ENGINES] = {
	[DEFAULT] = "DEFAULT",
	[RCS] = "RCS",
	[BCS] = "BCS",
	[VCS] = "VCS",
	[VCS1] = "VCS1",
	[VCS2] = "VCS2",
	[VECS] = "VECS",
};

static int
parse_dependencies(unsigned int nr_steps, struct w_step *w, char *_desc)
{
	char *desc = strdup(_desc);
	char *token, *tctx = NULL, *tstart = desc;

	igt_assert(desc);
	igt_assert(!w->data_deps.nr && w->data_deps.nr == w->fence_deps.nr);
	igt_assert(!w->data_deps.list &&
		   w->data_deps.list == w->fence_deps.list);

	while ((token = strtok_r(tstart, "/", &tctx)) != NULL) {
		bool submit_fence = false;
		char *str = token;
		struct deps *deps;
		int dep;

		tstart = NULL;

		if (str[0] == '-' || (str[0] >= '0' && str[0] <= '9')) {
			deps = &w->data_deps;
		} else {
			if (str[0] == 's')
				submit_fence = true;
			else if (str[0] != 'f')
				return -1;

			deps = &w->fence_deps;
			str++;
		}

		dep = atoi(str);
		if (dep > 0 || ((int)nr_steps + dep) < 0) {
			if (deps->list)
				free(deps->list);
			return -1;
		}

		if (dep < 0) {
			deps->nr++;
			/* Multiple fences not yet supported. */
			igt_assert(deps->nr == 1 || deps != &w->fence_deps);
			deps->list = realloc(deps->list,
					     sizeof(*deps->list) * deps->nr);
			igt_assert(deps->list);
			deps->list[deps->nr - 1] = dep;
			deps->submit_fence = submit_fence;
		}
	}

	free(desc);

	return 0;
}

static void __attribute__((format(printf, 1, 2)))
wsim_err(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define check_arg(cond, fmt, ...) \
{ \
	if (cond) { \
		wsim_err(fmt, __VA_ARGS__); \
		return NULL; \
	} \
}

static int str_to_engine(const char *str)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ring_str_map); i++) {
		if (!strcasecmp(str, ring_str_map[i]))
			return i;
	}

	return -1;
}

static bool __engines_queried;
static unsigned int __num_engines;
static struct i915_engine_class_instance *__engines;

static int
__i915_query(int i915, struct drm_i915_query *q)
{
	if (igt_ioctl(i915, DRM_IOCTL_I915_QUERY, q))
		return -errno;
	return 0;
}

static int
__i915_query_items(int i915, struct drm_i915_query_item *items, uint32_t n_items)
{
	struct drm_i915_query q = {
		.num_items = n_items,
		.items_ptr = to_user_pointer(items),
	};
	return __i915_query(i915, &q);
}

static void
i915_query_items(int i915, struct drm_i915_query_item *items, uint32_t n_items)
{
	igt_assert_eq(__i915_query_items(i915, items, n_items), 0);
}

static bool has_engine_query(int i915)
{
	struct drm_i915_query_item item = {
		.query_id = DRM_I915_QUERY_ENGINE_INFO,
	};

	return __i915_query_items(i915, &item, 1) == 0 && item.length > 0;
}

static void query_engines(void)
{
	struct i915_engine_class_instance *engines;
	unsigned int num;

	if (__engines_queried)
		return;

	__engines_queried = true;

	if (!has_engine_query(fd)) {
		unsigned int num_bsd = gem_has_bsd(fd) + gem_has_bsd2(fd);
		unsigned int i = 0;

		igt_assert(num_bsd);

		num = 1 + num_bsd;

		if (gem_has_blt(fd))
			num++;

		if (gem_has_vebox(fd))
			num++;

		engines = calloc(num,
				 sizeof(struct i915_engine_class_instance));
		igt_assert(engines);

		engines[i].engine_class = I915_ENGINE_CLASS_RENDER;
		engines[i].engine_instance = 0;
		i++;

		if (gem_has_blt(fd)) {
			engines[i].engine_class = I915_ENGINE_CLASS_COPY;
			engines[i].engine_instance = 0;
			i++;
		}

		if (gem_has_bsd(fd)) {
			engines[i].engine_class = I915_ENGINE_CLASS_VIDEO;
			engines[i].engine_instance = 0;
			i++;
		}

		if (gem_has_bsd2(fd)) {
			engines[i].engine_class = I915_ENGINE_CLASS_VIDEO;
			engines[i].engine_instance = 1;
			i++;
		}

		if (gem_has_vebox(fd)) {
			engines[i].engine_class =
				I915_ENGINE_CLASS_VIDEO_ENHANCE;
			engines[i].engine_instance = 0;
			i++;
		}
	} else {
		struct drm_i915_query_engine_info *engine_info;
		struct drm_i915_query_item item = {
			.query_id = DRM_I915_QUERY_ENGINE_INFO,
		};
		const unsigned int sz = 4096;
		unsigned int i;

		engine_info = malloc(sz);
		igt_assert(engine_info);
		memset(engine_info, 0, sz);

		item.data_ptr = to_user_pointer(engine_info);
		item.length = sz;

		i915_query_items(fd, &item, 1);
		igt_assert(item.length > 0);
		igt_assert(item.length <= sz);

		num = engine_info->num_engines;

		engines = calloc(num,
				 sizeof(struct i915_engine_class_instance));
		igt_assert(engines);

		for (i = 0; i < num; i++) {
			struct drm_i915_engine_info *engine =
				(struct drm_i915_engine_info *)&engine_info->engines[i];

			engines[i] = engine->engine;
		}
	}

	__engines = engines;
	__num_engines = num;
}

static unsigned int num_engines_in_class(enum intel_engine_id class)
{
	unsigned int i, count = 0;

	igt_assert(class == VCS);

	query_engines();

	for (i = 0; i < __num_engines; i++) {
		if (__engines[i].engine_class == I915_ENGINE_CLASS_VIDEO)
			count++;
	}

	igt_assert(count);
	return count;
}

static void
fill_engines_class(struct i915_engine_class_instance *ci,
		   enum intel_engine_id class)
{
	unsigned int i, j = 0;

	igt_assert(class == VCS);

	query_engines();

	for (i = 0; i < __num_engines; i++) {
		if (__engines[i].engine_class != I915_ENGINE_CLASS_VIDEO)
			continue;

		ci[j].engine_class = __engines[i].engine_class;
		ci[j].engine_instance = __engines[i].engine_instance;
		j++;
	}
}

static void
fill_engines_id_class(enum intel_engine_id *list,
		      enum intel_engine_id class)
{
	enum intel_engine_id engine = VCS1;
	unsigned int i, j = 0;

	igt_assert(class == VCS);
	igt_assert(num_engines_in_class(VCS) <= 2);

	query_engines();

	for (i = 0; i < __num_engines; i++) {
		if (__engines[i].engine_class != I915_ENGINE_CLASS_VIDEO)
			continue;

		list[j++] = engine++;
	}
}

static unsigned int
find_physical_instance(enum intel_engine_id class, unsigned int logical)
{
	unsigned int i, j = 0;

	igt_assert(class == VCS);

	for (i = 0; i < __num_engines; i++) {
		if (__engines[i].engine_class != I915_ENGINE_CLASS_VIDEO)
			continue;

		/* Map logical to physical instances. */
		if (logical == j++)
			return __engines[i].engine_instance;
	}

	igt_assert(0);
	return 0;
}

static struct i915_engine_class_instance
get_engine(enum intel_engine_id engine)
{
	struct i915_engine_class_instance ci;

	query_engines();

	switch (engine) {
	case RCS:
		ci.engine_class = I915_ENGINE_CLASS_RENDER;
		ci.engine_instance = 0;
		break;
	case BCS:
		ci.engine_class = I915_ENGINE_CLASS_COPY;
		ci.engine_instance = 0;
		break;
	case VCS1:
	case VCS2:
		ci.engine_class = I915_ENGINE_CLASS_VIDEO;
		ci.engine_instance = find_physical_instance(VCS, engine - VCS1);
		break;
	case VECS:
		ci.engine_class = I915_ENGINE_CLASS_VIDEO_ENHANCE;
		ci.engine_instance = 0;
		break;
	default:
		igt_assert(0);
	};

	return ci;
}

static int parse_engine_map(struct w_step *step, const char *_str)
{
	char *token, *tctx = NULL, *tstart = (char *)_str;

	while ((token = strtok_r(tstart, "|", &tctx))) {
		enum intel_engine_id engine;
		unsigned int add;

		tstart = NULL;

		if (!strcmp(token, "DEFAULT"))
			return -1;

		engine = str_to_engine(token);
		if ((int)engine < 0)
			return -1;

		if (engine != VCS && engine != VCS1 && engine != VCS2 &&
		    engine != RCS)
			return -1; /* TODO */

		add = engine == VCS ? num_engines_in_class(VCS) : 1;
		step->engine_map_count += add;
		step->engine_map = realloc(step->engine_map,
					   step->engine_map_count *
					   sizeof(step->engine_map[0]));

		if (engine != VCS)
			step->engine_map[step->engine_map_count - add] = engine;
		else
			fill_engines_id_class(&step->engine_map[step->engine_map_count - add], VCS);
	}

	return 0;
}

static uint64_t engine_list_mask(const char *_str)
{
	uint64_t mask = 0;

	char *token, *tctx = NULL, *tstart = (char *)_str;

	while ((token = strtok_r(tstart, "|", &tctx))) {
		enum intel_engine_id engine = str_to_engine(token);

		if ((int)engine < 0 || engine == DEFAULT || engine == VCS)
			return 0;

		mask |= 1 << engine;

		tstart = NULL;
	}

	return mask;
}

#define int_field(_STEP_, _FIELD_, _COND_, _ERR_) \
	if ((field = strtok_r(fstart, ".", &fctx))) { \
		tmp = atoi(field); \
		check_arg(_COND_, _ERR_, nr_steps); \
		step.type = _STEP_; \
		step._FIELD_ = tmp; \
		goto add_step; \
	} \

static struct workload *
parse_workload(struct w_arg *arg, unsigned int flags, struct workload *app_w)
{
	struct workload *wrk;
	unsigned int nr_steps = 0;
	char *desc = strdup(arg->desc);
	char *_token, *token, *tctx = NULL, *tstart = desc;
	char *field, *fctx = NULL, *fstart;
	struct w_step step, *steps = NULL;
	bool bcs_used = false;
	unsigned int valid;
	int i, j, tmp;

	igt_assert(desc);

	while ((_token = strtok_r(tstart, ",", &tctx))) {
		tstart = NULL;
		token = strdup(_token);
		igt_assert(token);
		fstart = token;
		valid = 0;
		memset(&step, 0, sizeof(step));

		if ((field = strtok_r(fstart, ".", &fctx))) {
			fstart = NULL;

			if (!strcmp(field, "d")) {
				int_field(DELAY, delay, tmp <= 0,
					  "Invalid delay at step %u!\n");
			} else if (!strcmp(field, "p")) {
				int_field(PERIOD, period, tmp <= 0,
					  "Invalid period at step %u!\n");
			} else if (!strcmp(field, "P")) {
				unsigned int nr = 0;
				while ((field = strtok_r(fstart, ".", &fctx))) {
					tmp = atoi(field);
					check_arg(nr == 0 && tmp <= 0,
						  "Invalid context at step %u!\n",
						  nr_steps);
					check_arg(nr > 1,
						  "Invalid priority format at step %u!\n",
						  nr_steps);

					if (nr == 0)
						step.context = tmp;
					else
						step.priority = tmp;

					nr++;
				}

				step.type = CTX_PRIORITY;
				goto add_step;
			} else if (!strcmp(field, "s")) {
				int_field(SYNC, target,
					  tmp >= 0 || ((int)nr_steps + tmp) < 0,
					  "Invalid sync target at step %u!\n");
			} else if (!strcmp(field, "S")) {
				unsigned int nr = 0;
				while ((field = strtok_r(fstart, ".", &fctx))) {
					tmp = atoi(field);
					check_arg(tmp <= 0 && nr == 0,
						  "Invalid context at step %u!\n",
						  nr_steps);
					check_arg(nr > 1,
						  "Invalid SSEU format at step %u!\n",
						  nr_steps);

					if (nr == 0)
						step.context = tmp;
					else if (nr == 1)
						step.sseu = tmp;

					nr++;
				}

				step.type = SSEU;
				goto add_step;
			} else if (!strcmp(field, "t")) {
				int_field(THROTTLE, throttle,
					  tmp < 0,
					  "Invalid throttle at step %u!\n");
			} else if (!strcmp(field, "q")) {
				int_field(QD_THROTTLE, throttle,
					  tmp < 0,
					  "Invalid qd throttle at step %u!\n");
			} else if (!strcmp(field, "a")) {
				int_field(SW_FENCE_SIGNAL, target,
					  tmp >= 0,
					  "Invalid sw fence signal at step %u!\n");
			} else if (!strcmp(field, "f")) {
				step.type = SW_FENCE;
				goto add_step;
			} else if (!strcmp(field, "M")) {
				unsigned int nr = 0;
				while ((field = strtok_r(fstart, ".", &fctx))) {
					tmp = atoi(field);
					check_arg(nr == 0 && tmp <= 0,
						  "Invalid context at step %u!\n",
						  nr_steps);
					check_arg(nr > 1,
						  "Invalid engine map format at step %u!\n",
						  nr_steps);

					if (nr == 0) {
						step.context = tmp;
					} else {
						tmp = parse_engine_map(&step,
								       field);
						check_arg(tmp < 0,
							  "Invalid engine map list at step %u!\n",
							  nr_steps);
					}

					nr++;
				}

				step.type = ENGINE_MAP;
				goto add_step;
			} else if (!strcmp(field, "T")) {
				int_field(TERMINATE, target,
					  tmp >= 0 || ((int)nr_steps + tmp) < 0,
					  "Invalid terminate target at step %u!\n");
			} else if (!strcmp(field, "X")) {
				unsigned int nr = 0;
				while ((field = strtok_r(fstart, ".", &fctx))) {
					tmp = atoi(field);
					check_arg(nr == 0 && tmp <= 0,
						  "Invalid context at step %u!\n",
						  nr_steps);
					check_arg(nr == 1 && tmp < 0,
						  "Invalid preemption period at step %u!\n",
						  nr_steps);
					check_arg(nr > 1,
						  "Invalid preemption format at step %u!\n",
						  nr_steps);

					if (nr == 0)
						step.context = tmp;
					else
						step.period = tmp;

					nr++;
				}

				step.type = PREEMPTION;
				goto add_step;
			} else if (!strcmp(field, "B")) {
				unsigned int nr = 0;
				while ((field = strtok_r(fstart, ".", &fctx))) {
					tmp = atoi(field);
					check_arg(nr == 0 && tmp <= 0,
						  "Invalid context at step %u!\n",
						  nr_steps);
					check_arg(nr > 0,
						  "Invalid load balance format at step %u!\n",
						  nr_steps);

					step.context = tmp;
					step.load_balance = true;

					nr++;
				}

				step.type = LOAD_BALANCE;
				goto add_step;
			} else if (!strcmp(field, "b")) {
				unsigned int nr = 0;
				while ((field = strtok_r(fstart, ".", &fctx))) {
					check_arg(nr > 2,
						  "Invalid bond format at step %u!\n",
						  nr_steps);

					if (nr == 0) {
						tmp = atoi(field);
						step.context = tmp;
						check_arg(tmp <= 0,
							  "Invalid context at step %u!\n",
							  nr_steps);
					} else if (nr == 1) {
						step.bond_mask = engine_list_mask(field);
						check_arg(step.bond_mask == 0,
							"Invalid siblings list at step %u!\n",
							nr_steps);
					} else if (nr == 2) {
						tmp = str_to_engine(field);
						check_arg(tmp <= 0 ||
							  tmp == VCS ||
							  tmp == DEFAULT,
							  "Invalid master engine at step %u!\n",
							  nr_steps);
						step.bond_master = tmp;
					}

					nr++;
				}

				step.type = BOND;
				goto add_step;
			}

			if (!field) {
				if (verbose)
					fprintf(stderr,
						"Parse error at step %u!\n",
						nr_steps);
				return NULL;
			}

			tmp = atoi(field);
			check_arg(tmp < 0, "Invalid ctx id at step %u!\n",
				  nr_steps);
			step.context = tmp;

			valid++;
		}

		if ((field = strtok_r(fstart, ".", &fctx))) {
			fstart = NULL;

			i = str_to_engine(field);
			check_arg(i < 0,
				  "Invalid engine id at step %u!\n", nr_steps);

			valid++;

			step.engine = i;

			if (step.engine == BCS)
				bcs_used = true;
		}

		if ((field = strtok_r(fstart, ".", &fctx))) {
			char *sep = NULL;
			long int tmpl;

			fstart = NULL;

			if (field[0] == '*') {
				check_arg(intel_gen(intel_get_drm_devid(fd)) < 8,
					  "Infinite batch at step %u needs Gen8+!\n",
					  nr_steps);
				step.unbound_duration = true;
			} else {
				tmpl = strtol(field, &sep, 10);
				check_arg(tmpl <= 0 || tmpl == LONG_MIN ||
					  tmpl == LONG_MAX,
					  "Invalid duration at step %u!\n",
					  nr_steps);
				step.duration.min = tmpl;

				if (sep && *sep == '-') {
					tmpl = strtol(sep + 1, NULL, 10);
					check_arg(tmpl <= 0 ||
						tmpl <= step.duration.min ||
						tmpl == LONG_MIN ||
						tmpl == LONG_MAX,
						"Invalid duration range at step %u!\n",
						nr_steps);
					step.duration.max = tmpl;
				} else {
					step.duration.max = step.duration.min;
				}
			}

			valid++;
		}

		if ((field = strtok_r(fstart, ".", &fctx))) {
			fstart = NULL;

			tmp = parse_dependencies(nr_steps, &step, field);
			check_arg(tmp < 0,
				  "Invalid dependency at step %u!\n", nr_steps);

			valid++;
		}

		if ((field = strtok_r(fstart, ".", &fctx))) {
			fstart = NULL;

			check_arg(strlen(field) != 1 ||
				  (field[0] != '0' && field[0] != '1'),
				  "Invalid wait boolean at step %u!\n",
				  nr_steps);
			step.sync = field[0] - '0';

			valid++;
		}

		check_arg(valid != 5, "Invalid record at step %u!\n", nr_steps);

		step.type = BATCH;

add_step:
		step.idx = nr_steps++;
		step.request = -1;
		steps = realloc(steps, sizeof(step) * nr_steps);
		igt_assert(steps);

		memcpy(&steps[nr_steps - 1], &step, sizeof(step));

		free(token);
	}

	if (app_w) {
		steps = realloc(steps, sizeof(step) *
				(nr_steps + app_w->nr_steps));
		igt_assert(steps);

		memcpy(&steps[nr_steps], app_w->steps,
		       sizeof(step) * app_w->nr_steps);

		for (i = 0; i < app_w->nr_steps; i++)
			steps[nr_steps + i].idx += nr_steps;

		nr_steps += app_w->nr_steps;
	}

	wrk = malloc(sizeof(*wrk));
	igt_assert(wrk);

	wrk->nr_steps = nr_steps;
	wrk->steps = steps;
	wrk->prio = arg->prio;
	wrk->sseu = arg->sseu;

	free(desc);

	/*
	 * Tag all steps which need to emit a sync fence if another step is
	 * referencing them as a sync fence dependency.
	 */
	for (i = 0; i < nr_steps; i++) {
		for (j = 0; j < steps[i].fence_deps.nr; j++) {
			tmp = steps[i].idx + steps[i].fence_deps.list[j];
			check_arg(tmp < 0 || tmp >= i ||
				  (steps[tmp].type != BATCH &&
				   steps[tmp].type != SW_FENCE),
				  "Invalid dependency target %u!\n", i);
			steps[tmp].emit_fence = -1;
		}
	}

	/* Validate SW_FENCE_SIGNAL targets. */
	for (i = 0; i < nr_steps; i++) {
		if (steps[i].type == SW_FENCE_SIGNAL) {
			tmp = steps[i].idx + steps[i].target;
			check_arg(tmp < 0 || tmp >= i ||
				  steps[tmp].type != SW_FENCE,
				  "Invalid sw fence target %u!\n", i);
		}
	}

	if (bcs_used && (flags & VCS2REMAP) && verbose)
		printf("BCS usage in workload with VCS2 remapping enabled!\n");

	return wrk;
}

static struct workload *
clone_workload(struct workload *_wrk)
{
	struct workload *wrk;
	int i;

	wrk = malloc(sizeof(*wrk));
	igt_assert(wrk);
	memset(wrk, 0, sizeof(*wrk));

	wrk->prio = _wrk->prio;
	wrk->sseu = _wrk->sseu;
	wrk->nr_steps = _wrk->nr_steps;
	wrk->steps = calloc(wrk->nr_steps, sizeof(struct w_step));
	igt_assert(wrk->steps);

	memcpy(wrk->steps, _wrk->steps, sizeof(struct w_step) * wrk->nr_steps);

	/* Check if we need a sw sync timeline. */
	for (i = 0; i < wrk->nr_steps; i++) {
		if (wrk->steps[i].type == SW_FENCE) {
			wrk->sync_timeline = sw_sync_timeline_create();
			igt_assert(wrk->sync_timeline >= 0);
			break;
		}
	}

	for (i = 0; i < NUM_ENGINES; i++)
		igt_list_init(&wrk->requests[i]);

	return wrk;
}

#define rounddown(x, y) (x - (x%y))
#ifndef PAGE_SIZE
#define PAGE_SIZE (4096)
#endif

static unsigned int get_duration(struct workload *wrk, struct w_step *w)
{
	struct duration *dur = &w->duration;

	if (dur->min == dur->max)
		return dur->min;
	else
		return dur->min + hars_petruska_f54_1_random(&wrk->bb_prng) %
		       (dur->max + 1 - dur->min);
}

static unsigned long get_bb_sz(unsigned int duration)
{
	return ALIGN(duration * nop_calibration * sizeof(uint32_t) /
		     nop_calibration_us, sizeof(uint32_t));
}

static void
init_bb(struct w_step *w, unsigned int flags)
{
	const unsigned int arb_period =
			get_bb_sz(w->preempt_us) / sizeof(uint32_t);
	const unsigned int mmap_len = ALIGN(w->bb_sz, 4096);
	unsigned int i;
	uint32_t *ptr;

	if (w->unbound_duration || !arb_period)
		return;

	gem_set_domain(fd, w->bb_handle,
		       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	ptr = gem_mmap__wc(fd, w->bb_handle, 0, mmap_len, PROT_WRITE);

	for (i = arb_period; i < w->bb_sz / sizeof(uint32_t); i += arb_period)
		ptr[i] = 0x5 << 23; /* MI_ARB_CHK */

	munmap(ptr, mmap_len);
}

static unsigned int
terminate_bb(struct w_step *w, unsigned int flags)
{
	const uint32_t bbe = 0xa << 23;
	unsigned long mmap_start, mmap_len;
	unsigned long batch_start = w->bb_sz;
	unsigned int r = 0;
	uint32_t *ptr, *cs;

	igt_assert(((flags & RT) && (flags & SEQNO)) || !(flags & RT));

	batch_start -= sizeof(uint32_t); /* bbend */
	if (flags & SEQNO)
		batch_start -= 4 * sizeof(uint32_t);
	if (flags & RT)
		batch_start -= 12 * sizeof(uint32_t);

	if (w->unbound_duration)
		batch_start -= 4 * sizeof(uint32_t); /* MI_ARB_CHK + MI_BATCH_BUFFER_START */

	mmap_start = rounddown(batch_start, PAGE_SIZE);
	mmap_len = ALIGN(w->bb_sz - mmap_start, PAGE_SIZE);

	gem_set_domain(fd, w->bb_handle,
		       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	ptr = gem_mmap__wc(fd, w->bb_handle, mmap_start, mmap_len, PROT_WRITE);
	cs = (uint32_t *)((char *)ptr + batch_start - mmap_start);

	if (w->unbound_duration) {
		w->reloc[r++].offset = batch_start + 2 * sizeof(uint32_t);
		batch_start += 4 * sizeof(uint32_t);

		*cs++ = w->preempt_us ? 0x5 << 23 /* MI_ARB_CHK; */ : MI_NOOP;
		w->recursive_bb_start = cs;
		*cs++ = MI_BATCH_BUFFER_START | 1 << 8 | 1;
		*cs++ = 0;
		*cs++ = 0;
	}

	if (flags & SEQNO) {
		w->reloc[r++].offset = batch_start + sizeof(uint32_t);
		batch_start += 4 * sizeof(uint32_t);

		*cs++ = MI_STORE_DWORD_IMM;
		w->seqno_address = cs;
		*cs++ = 0;
		*cs++ = 0;
		w->seqno_value = cs;
		*cs++ = 0;
	}

	if (flags & RT) {
		w->reloc[r++].offset = batch_start + sizeof(uint32_t);
		batch_start += 4 * sizeof(uint32_t);

		*cs++ = MI_STORE_DWORD_IMM;
		w->rt0_address = cs;
		*cs++ = 0;
		*cs++ = 0;
		w->rt0_value = cs;
		*cs++ = 0;

		w->reloc[r++].offset = batch_start + 2 * sizeof(uint32_t);
		batch_start += 4 * sizeof(uint32_t);

		*cs++ = 0x24 << 23 | 2; /* MI_STORE_REG_MEM */
		*cs++ = RCS_TIMESTAMP;
		w->rt1_address = cs;
		*cs++ = 0;
		*cs++ = 0;

		w->reloc[r++].offset = batch_start + sizeof(uint32_t);
		batch_start += 4 * sizeof(uint32_t);

		*cs++ = MI_STORE_DWORD_IMM;
		w->latch_address = cs;
		*cs++ = 0;
		*cs++ = 0;
		w->latch_value = cs;
		*cs++ = 0;
	}

	*cs = bbe;

	return r;
}

static const unsigned int eb_engine_map[NUM_ENGINES] = {
	[DEFAULT] = I915_EXEC_DEFAULT,
	[RCS] = I915_EXEC_RENDER,
	[BCS] = I915_EXEC_BLT,
	[VCS] = I915_EXEC_BSD,
	[VCS1] = I915_EXEC_BSD | I915_EXEC_BSD_RING1,
	[VCS2] = I915_EXEC_BSD | I915_EXEC_BSD_RING2,
	[VECS] = I915_EXEC_VEBOX
};

static void
eb_set_engine(struct drm_i915_gem_execbuffer2 *eb,
	      enum intel_engine_id engine,
	      unsigned int flags)
{
	if (engine == VCS2 && (flags & VCS2REMAP))
		engine = BCS;

	if ((flags & I915) && engine == VCS)
		eb->flags = 0;
	else
		eb->flags = eb_engine_map[engine];
}

static unsigned int
find_engine_in_map(struct ctx *ctx, enum intel_engine_id engine)
{
	unsigned int i;

	for (i = 0; i < ctx->engine_map_count; i++) {
		if (ctx->engine_map[i] == engine)
			return i + 1;
	}

	igt_assert(ctx->wants_balance);
	return 0;
}

static struct ctx *
__get_ctx(struct workload *wrk, struct w_step *w)
{
	return &wrk->ctx_list[w->context * 2];
}

static void
eb_update_flags(struct workload *wrk, struct w_step *w,
		enum intel_engine_id engine, unsigned int flags)
{
	struct ctx *ctx = __get_ctx(wrk, w);

	if (ctx->engine_map)
		w->eb.flags = find_engine_in_map(ctx, engine);
	else
		eb_set_engine(&w->eb, engine, flags);

	w->eb.flags |= I915_EXEC_HANDLE_LUT;
	w->eb.flags |= I915_EXEC_NO_RELOC;

	igt_assert(w->emit_fence <= 0);
	if (w->emit_fence)
		w->eb.flags |= I915_EXEC_FENCE_OUT;
}

static struct drm_i915_gem_exec_object2 *
get_status_objects(struct workload *wrk)
{
	if (wrk->flags & GLOBAL_BALANCE)
		return wrk->global_wrk->status_object;
	else
		return wrk->status_object;
}

static uint32_t
get_ctxid(struct workload *wrk, struct w_step *w)
{
	struct ctx *ctx = __get_ctx(wrk, w);

	if (ctx->targets_instance && ctx->wants_balance && w->engine == VCS)
		return wrk->ctx_list[w->context * 2 + 1].id;
	else
		return wrk->ctx_list[w->context * 2].id;
}

static void
alloc_step_batch(struct workload *wrk, struct w_step *w, unsigned int flags)
{
	enum intel_engine_id engine = w->engine;
	unsigned int j = 0;
	unsigned int nr_obj = 3 + w->data_deps.nr;
	unsigned int i;

	w->obj = calloc(nr_obj, sizeof(*w->obj));
	igt_assert(w->obj);

	w->obj[j].handle = gem_create(fd, 4096);
	w->obj[j].flags = EXEC_OBJECT_WRITE;
	j++;
	igt_assert(j < nr_obj);

	if (flags & SEQNO) {
		w->obj[j++] = get_status_objects(wrk)[0];
		igt_assert(j < nr_obj);
	}

	for (i = 0; i < w->data_deps.nr; i++) {
		igt_assert(w->data_deps.list[i] <= 0);
		if (w->data_deps.list[i]) {
			int dep_idx = w->idx + w->data_deps.list[i];

			igt_assert(dep_idx >= 0 && dep_idx < w->idx);
			igt_assert(wrk->steps[dep_idx].type == BATCH);

			w->obj[j].handle = wrk->steps[dep_idx].obj[0].handle;
			j++;
			igt_assert(j < nr_obj);
		}
	}

	if (w->unbound_duration)
		/* nops + MI_ARB_CHK + MI_BATCH_BUFFER_START */
		w->bb_sz = max(PAGE_SIZE, get_bb_sz(w->preempt_us)) +
			   (1 + 3) * sizeof(uint32_t);
	else
		w->bb_sz = get_bb_sz(w->duration.max);
	w->bb_handle = w->obj[j].handle = gem_create(fd, w->bb_sz + (w->unbound_duration ? 4096 : 0));
	init_bb(w, flags);
	w->obj[j].relocation_count = terminate_bb(w, flags);

	if (w->obj[j].relocation_count) {
		w->obj[j].relocs_ptr = to_user_pointer(&w->reloc);
		for (i = 0; i < w->obj[j].relocation_count; i++)
			w->reloc[i].target_handle = 1;
		if (w->unbound_duration)
			w->reloc[0].target_handle = j;
	}

	w->eb.buffers_ptr = to_user_pointer(w->obj);
	w->eb.buffer_count = j + 1;
	w->eb.rsvd1 = get_ctxid(wrk, w);

	if (flags & SWAPVCS && engine == VCS1)
		engine = VCS2;
	else if (flags & SWAPVCS && engine == VCS2)
		engine = VCS1;
	eb_update_flags(wrk, w, engine, flags);
#ifdef DEBUG
	printf("%u: %u:|", w->idx, w->eb.buffer_count);
	for (i = 0; i <= j; i++)
		printf("%x|", w->obj[i].handle);
	printf(" %10lu flags=%llx bb=%x[%u] ctx[%u]=%u\n",
		w->bb_sz, w->eb.flags, w->bb_handle, j, w->context,
		get_ctxid(wrk, w));
#endif
}

static void __ctx_set_prio(uint32_t ctx_id, unsigned int prio)
{
	struct drm_i915_gem_context_param param = {
		.ctx_id = ctx_id,
		.param = I915_CONTEXT_PARAM_PRIORITY,
		.value = prio,
	};

	if (prio)
		gem_context_set_param(fd, &param);
}

static int __vm_destroy(int i915, uint32_t vm_id)
{
	struct drm_i915_gem_vm_control ctl = { .vm_id = vm_id };
	int err = 0;

	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_VM_DESTROY, &ctl)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

static void vm_destroy(int i915, uint32_t vm_id)
{
	igt_assert_eq(__vm_destroy(i915, vm_id), 0);
}

static unsigned int
find_engine(struct i915_engine_class_instance *ci, unsigned int count,
	    enum intel_engine_id engine)
{
	struct i915_engine_class_instance e = get_engine(engine);
	unsigned int i;

	for (i = 0; i < count; i++, ci++) {
		if (!memcmp(&e, ci, sizeof(*ci)))
			return i;
	}

	igt_assert(0);
	return 0;
}

static struct drm_i915_gem_context_param_sseu get_device_sseu(void)
{
	struct drm_i915_gem_context_param param = { };

	if (device_sseu.slice_mask == -1) {
		param.param = I915_CONTEXT_PARAM_SSEU;
		param.value = (uintptr_t)&device_sseu;

		gem_context_get_param(fd, &param);
	}

	return device_sseu;
}

static uint64_t
set_ctx_sseu(struct ctx *ctx, uint64_t slice_mask)
{
	struct drm_i915_gem_context_param_sseu sseu = get_device_sseu();
	struct drm_i915_gem_context_param param = { };

	if (slice_mask == -1)
		slice_mask = device_sseu.slice_mask;

	if (ctx->engine_map && ctx->wants_balance) {
		sseu.flags = I915_CONTEXT_SSEU_FLAG_ENGINE_INDEX;
		sseu.engine.engine_class = I915_ENGINE_CLASS_INVALID;
		sseu.engine.engine_instance = 0;
	}

	sseu.slice_mask = slice_mask;

	param.ctx_id = ctx->id;
	param.param = I915_CONTEXT_PARAM_SSEU;
	param.size = sizeof(sseu);
	param.value = (uintptr_t)&sseu;

	gem_context_set_param(fd, &param);

	return slice_mask;
}

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

static int
prepare_workload(unsigned int id, struct workload *wrk, unsigned int flags)
{
	unsigned int ctx_vcs;
	int max_ctx = -1;
	struct w_step *w;
	int i, j;

	wrk->id = id;
	wrk->prng = rand();
	wrk->bb_prng = (wrk->flags & SYNCEDCLIENTS) ? master_prng : rand();
	wrk->run = true;

	ctx_vcs =  0;
	if (flags & INITVCSRR)
		ctx_vcs = id & 1;
	wrk->vcs_rr = ctx_vcs;

	if (flags & GLOBAL_BALANCE) {
		int ret = pthread_mutex_init(&wrk->mutex, NULL);
		igt_assert(ret == 0);
	}

	if (flags & SEQNO) {
		if (!(flags & GLOBAL_BALANCE) || id == 0) {
			uint32_t handle;

			handle = gem_create(fd, 4096);
			gem_set_caching(fd, handle, I915_CACHING_CACHED);
			wrk->status_object[0].handle = handle;
			wrk->status_page = gem_mmap__cpu(fd, handle, 0, 4096,
							 PROT_READ);

			handle = gem_create(fd, 4096);
			wrk->status_object[1].handle = handle;
			wrk->status_cs = gem_mmap__wc(fd, handle,
						      0, 4096, PROT_WRITE);
		}
	}

	/*
	 * Pre-scan workload steps to allocate context list storage.
	 */
	for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
		int ctx = w->context * 2 + 1; /* Odd slots are special. */
		int delta;

		if (ctx <= max_ctx)
			continue;

		delta = ctx + 1 - wrk->nr_ctxs;

		wrk->nr_ctxs += delta;
		wrk->ctx_list = realloc(wrk->ctx_list,
					wrk->nr_ctxs * sizeof(*wrk->ctx_list));
		memset(&wrk->ctx_list[wrk->nr_ctxs - delta], 0,
			delta * sizeof(*wrk->ctx_list));

		max_ctx = ctx;
	}

	/*
	 * Identify if contexts target specific engine instances and if they
	 * want to be balanced.
	 *
	 * Transfer over engine map configuration from the workload step.
	 */
	for (j = 0; j < wrk->nr_ctxs; j += 2) {
		struct ctx *ctx = &wrk->ctx_list[j];

		bool targets = false;
		bool balance = false;

		for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
			if (w->context != (j / 2))
				continue;

			if (w->type == BATCH) {
				if (w->engine == VCS)
					balance = true;
				else
					targets = true;
			} else if (w->type == ENGINE_MAP) {
				ctx->engine_map = w->engine_map;
				ctx->engine_map_count = w->engine_map_count;
			} else if (w->type == LOAD_BALANCE) {
				if (!ctx->engine_map) {
					wsim_err("Load balancing needs an engine map!\n");
					return 1;
				}
				ctx->wants_balance = w->load_balance;
			} else if (w->type == BOND) {
				if (!ctx->wants_balance) {
					wsim_err("Engine bonds need load balancing engine map!\n");
					return 1;
				}
				ctx->bond_count++;
				ctx->bonds = realloc(ctx->bonds,
						     ctx->bond_count *
						     sizeof(struct bond));
				igt_assert(ctx->bonds);
				ctx->bonds[ctx->bond_count - 1].mask =
					w->bond_mask;
				ctx->bonds[ctx->bond_count - 1].master =
					w->bond_master;
			}
		}

		wrk->ctx_list[j].targets_instance = targets;
		if (flags & I915)
			wrk->ctx_list[j].wants_balance |= balance;
	}

	/*
	 * Ensure VCS is not allowed with engine map contexts.
	 */
	for (j = 0; j < wrk->nr_ctxs; j += 2) {
		for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
			if (w->context != (j / 2))
				continue;

			if (w->type != BATCH)
				continue;

			if (wrk->ctx_list[j].engine_map &&
			    !wrk->ctx_list[j].wants_balance &&
			    (w->engine == VCS || w->engine == DEFAULT)) {
				wsim_err("Batches targetting engine maps must use explicit engines!\n");
				return -1;
			}
		}
	}


	/*
	 * Create and configure contexts.
	 */
	for (i = 0; i < wrk->nr_ctxs; i += 2) {
		struct ctx *ctx = &wrk->ctx_list[i];
		uint32_t ctx_id, share_vm = 0;

		if (ctx->id)
			continue;

		if ((flags & I915) || ctx->engine_map) {
			struct drm_i915_gem_context_create_ext_setparam ext = {
				.base.name = I915_CONTEXT_CREATE_EXT_SETPARAM,
				.param.param = I915_CONTEXT_PARAM_VM,
			};
			struct drm_i915_gem_context_create_ext args = { };

			/* Find existing context to share ppgtt with. */
			for (j = 0; j < wrk->nr_ctxs; j++) {
				struct drm_i915_gem_context_param param = {
					.param = I915_CONTEXT_PARAM_VM,
				};

				if (!wrk->ctx_list[j].id)
					continue;

				param.ctx_id = wrk->ctx_list[j].id;

				gem_context_get_param(fd, &param);
				igt_assert(param.value);

				share_vm = param.value;

				ext.param.value = share_vm;
				args.flags =
				    I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS;
				args.extensions = to_user_pointer(&ext);
				break;
			}

			if ((!ctx->engine_map && !ctx->targets_instance) ||
			    (ctx->engine_map && ctx->wants_balance))
				args.flags |=
				     I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE;

			drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT,
				 &args);

			ctx_id = args.ctx_id;
		} else {
			struct drm_i915_gem_context_create args = {};

			drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &args);
			ctx_id = args.ctx_id;
		}

		igt_assert(ctx_id);
		ctx->id = ctx_id;
		ctx->sseu = device_sseu.slice_mask;

		if (flags & GLOBAL_BALANCE) {
			ctx->static_vcs = context_vcs_rr;
			context_vcs_rr ^= 1;
		} else {
			ctx->static_vcs = ctx_vcs;
			ctx_vcs ^= 1;
		}

		__ctx_set_prio(ctx_id, wrk->prio);

		/*
		 * Do we need a separate context to satisfy this workloads which
		 * both want to target specific engines and be balanced by i915?
		 */
		if ((flags & I915) && ctx->wants_balance &&
		    ctx->targets_instance && !ctx->engine_map) {
			struct drm_i915_gem_context_create_ext_setparam ext = {
				.base.name = I915_CONTEXT_CREATE_EXT_SETPARAM,
				.param.param = I915_CONTEXT_PARAM_VM,
				.param.value = share_vm,
			};
			struct drm_i915_gem_context_create_ext args = {
				.extensions = to_user_pointer(&ext),
				.flags =
				    I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS |
				    I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE,
			};

			igt_assert(share_vm);

			drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT,
				 &args);

			igt_assert(args.ctx_id);
			ctx_id = args.ctx_id;
			wrk->ctx_list[i + 1].id = args.ctx_id;

			__ctx_set_prio(ctx_id, wrk->prio);
		}

		if (ctx->engine_map) {
			struct i915_context_param_engines *set_engines =
				alloca0(sizeof_param_engines(ctx->engine_map_count + 1));
			struct i915_context_engines_load_balance *load_balance =
				alloca0(sizeof_load_balance(ctx->engine_map_count));
			struct drm_i915_gem_context_param param = {
				.ctx_id = ctx_id,
				.param = I915_CONTEXT_PARAM_ENGINES,
				.size = sizeof_param_engines(ctx->engine_map_count + 1),
				.value = to_user_pointer(set_engines),
			};
			struct i915_context_engines_bond *last = NULL;

			if (ctx->wants_balance) {
				set_engines->extensions =
					to_user_pointer(load_balance);

				load_balance->base.name =
					I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
				load_balance->num_siblings =
					ctx->engine_map_count;

				for (j = 0; j < ctx->engine_map_count; j++)
					load_balance->engines[j] =
						get_engine(ctx->engine_map[j]);
			}

			/* Reserve slot for virtual engine. */
			set_engines->engines[0].engine_class =
				I915_ENGINE_CLASS_INVALID;
			set_engines->engines[0].engine_instance =
				I915_ENGINE_CLASS_INVALID_NONE;

			for (j = 1; j <= ctx->engine_map_count; j++)
				set_engines->engines[j] =
					get_engine(ctx->engine_map[j - 1]);

			last = NULL;
			for (j = 0; j < ctx->bond_count; j++) {
				unsigned long mask = ctx->bonds[j].mask;
				struct i915_context_engines_bond *bond =
					alloca0(sizeof_engines_bond(__builtin_popcount(mask)));
				unsigned int b, e;

				bond->base.next_extension = to_user_pointer(last);
				bond->base.name = I915_CONTEXT_ENGINES_EXT_BOND;

				bond->virtual_index = 0;
				bond->master = get_engine(ctx->bonds[j].master);

				for (b = 0, e = 0; mask; e++, mask >>= 1) {
					unsigned int idx;

					if (!(mask & 1))
						continue;

					idx = find_engine(&set_engines->engines[1],
							  ctx->engine_map_count,
							  e);
					bond->engines[b++] =
						set_engines->engines[1 + idx];
				}

				last = bond;
			}
			load_balance->base.next_extension = to_user_pointer(last);

			gem_context_set_param(fd, &param);
		} else if (ctx->wants_balance) {
			const unsigned int count = num_engines_in_class(VCS);
			struct i915_context_engines_load_balance *load_balance =
				alloca0(sizeof_load_balance(count));
			struct i915_context_param_engines *set_engines =
				alloca0(sizeof_param_engines(count + 1));
			struct drm_i915_gem_context_param param = {
				.ctx_id = ctx_id,
				.param = I915_CONTEXT_PARAM_ENGINES,
				.size = sizeof_param_engines(count + 1),
				.value = to_user_pointer(set_engines),
			};

			set_engines->extensions = to_user_pointer(load_balance);

			set_engines->engines[0].engine_class =
				I915_ENGINE_CLASS_INVALID;
			set_engines->engines[0].engine_instance =
				I915_ENGINE_CLASS_INVALID_NONE;
			fill_engines_class(&set_engines->engines[1], VCS);

			load_balance->base.name =
				I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
			load_balance->num_siblings = count;

			fill_engines_class(&load_balance->engines[0], VCS);

			gem_context_set_param(fd, &param);
		}

		if (wrk->sseu) {
			/* Set to slice 0 only, one slice. */
			ctx->sseu = set_ctx_sseu(ctx, 1);
		}

		if (share_vm)
			vm_destroy(fd, share_vm);
	}

	/* Record default preemption. */
	for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
		if (w->type == BATCH)
			w->preempt_us = 100;
	}

	/*
	 * Scan for contexts with modified preemption config and record their
	 * preemption period for the following steps belonging to the same
	 * context.
	 */
	for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
		struct w_step *w2;

		if (w->type != PREEMPTION)
			continue;

		for (j = i + 1; j < wrk->nr_steps; j++) {
			w2 = &wrk->steps[j];

			if (w2->context != w->context)
				continue;
			else if (w2->type == PREEMPTION)
				break;
			else if (w2->type != BATCH)
				continue;

			w2->preempt_us = w->period;
		}
	}

	/*
	 * Scan for SSEU control steps.
	 */
	for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
		if (w->type == SSEU) {
			get_device_sseu();
			break;
		}
	}

	/*
	 * Allocate batch buffers.
	 */
	for (i = 0, w = wrk->steps; i < wrk->nr_steps; i++, w++) {
		unsigned int _flags = flags;
		enum intel_engine_id engine = w->engine;

		if (w->type != BATCH)
			continue;

		if (engine == VCS)
			_flags &= ~SWAPVCS;

		alloc_step_batch(wrk, w, _flags);
	}

	return 0;
}

static double elapsed(const struct timespec *start, const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) +
	       (end->tv_nsec - start->tv_nsec) / 1e9;
}

static int elapsed_us(const struct timespec *start, const struct timespec *end)
{
	return elapsed(start, end) * 1e6;
}

static enum intel_engine_id get_vcs_engine(unsigned int n)
{
	const enum intel_engine_id vcs_engines[2] = { VCS1, VCS2 };

	igt_assert(n < ARRAY_SIZE(vcs_engines));

	return vcs_engines[n];
}

static uint32_t new_seqno(struct workload *wrk, enum intel_engine_id engine)
{
	uint32_t seqno;
	int ret;

	if (wrk->flags & GLOBAL_BALANCE) {
		igt_assert(wrk->global_wrk);
		wrk = wrk->global_wrk;

		ret = pthread_mutex_lock(&wrk->mutex);
		igt_assert(ret == 0);
	}

	seqno = ++wrk->seqno[engine];

	if (wrk->flags & GLOBAL_BALANCE) {
		ret = pthread_mutex_unlock(&wrk->mutex);
		igt_assert(ret == 0);
	}

	return seqno;
}

static uint32_t
current_seqno(struct workload *wrk, enum intel_engine_id engine)
{
	if (wrk->flags & GLOBAL_BALANCE)
		return wrk->global_wrk->seqno[engine];
	else
		return wrk->seqno[engine];
}

static uint32_t
read_status_page(struct workload *wrk, unsigned int idx)
{
	if (wrk->flags & GLOBAL_BALANCE)
		return READ_ONCE(wrk->global_wrk->status_page[idx]);
	else
		return READ_ONCE(wrk->status_page[idx]);
}

static uint32_t
current_gpu_seqno(struct workload *wrk, enum intel_engine_id engine)
{
       return read_status_page(wrk, SEQNO_IDX(engine));
}

struct workload_balancer {
	unsigned int id;
	const char *name;
	const char *desc;
	unsigned int flags;
	unsigned int min_gen;

	int (*init)(const struct workload_balancer *balancer,
		    struct workload *wrk);
	unsigned int (*get_qd)(const struct workload_balancer *balancer,
			       struct workload *wrk,
			       enum intel_engine_id engine);
	enum intel_engine_id (*balance)(const struct workload_balancer *balancer,
					struct workload *wrk, struct w_step *w);
};

static enum intel_engine_id
rr_balance(const struct workload_balancer *balancer,
	   struct workload *wrk, struct w_step *w)
{
	unsigned int engine;

	engine = get_vcs_engine(wrk->vcs_rr);
	wrk->vcs_rr ^= 1;

	return engine;
}

static enum intel_engine_id
rand_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w)
{
	return get_vcs_engine(hars_petruska_f54_1_random(&wrk->prng) & 1);
}

static unsigned int
get_qd_depth(const struct workload_balancer *balancer,
	     struct workload *wrk, enum intel_engine_id engine)
{
	return current_seqno(wrk, engine) - current_gpu_seqno(wrk, engine);
}

static enum intel_engine_id
__qd_select_engine(struct workload *wrk, const unsigned long *qd, bool random)
{
	unsigned int n;

	if (qd[VCS1] < qd[VCS2])
		n = 0;
	else if (qd[VCS1] > qd[VCS2])
		n = 1;
	else if (random)
		n = hars_petruska_f54_1_random(&wrk->prng) & 1;
	else
		n = wrk->vcs_rr;
	wrk->vcs_rr = n ^ 1;

	return get_vcs_engine(n);
}

static enum intel_engine_id
__qd_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w, bool random)
{
	enum intel_engine_id engine;
	unsigned long qd[NUM_ENGINES];

	igt_assert(w->engine == VCS);

	qd[VCS1] = balancer->get_qd(balancer, wrk, VCS1);
	wrk->qd_sum[VCS1] += qd[VCS1];

	qd[VCS2] = balancer->get_qd(balancer, wrk, VCS2);
	wrk->qd_sum[VCS2] += qd[VCS2];

	engine = __qd_select_engine(wrk, qd, random);

#ifdef DEBUG
	printf("qd_balance[%u]: 1:%ld 2:%ld rr:%u = %u\t(%u - %u) (%u - %u)\n",
	       wrk->id, qd[VCS1], qd[VCS2], wrk->vcs_rr, engine,
	       current_seqno(wrk, VCS1), current_gpu_seqno(wrk, VCS1),
	       current_seqno(wrk, VCS2), current_gpu_seqno(wrk, VCS2));
#endif
	return engine;
}

static enum intel_engine_id
qd_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w)
{
	return __qd_balance(balancer, wrk, w, false);
}

static enum intel_engine_id
qdr_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w)
{
	return __qd_balance(balancer, wrk, w, true);
}

static enum intel_engine_id
qdavg_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w)
{
	unsigned long qd[NUM_ENGINES];
	unsigned int engine;

	igt_assert(w->engine == VCS);

	for (engine = VCS1; engine <= VCS2; engine++) {
		qd[engine] = balancer->get_qd(balancer, wrk, engine);
		wrk->qd_sum[engine] += qd[engine];

		ewma_rt_add(&wrk->rt.avg[engine], qd[engine]);
		qd[engine] = ewma_rt_read(&wrk->rt.avg[engine]);
	}

	engine = __qd_select_engine(wrk, qd, false);
#ifdef DEBUG
	printf("qdavg_balance[%u]: 1:%ld 2:%ld rr:%u = %u\t(%u - %u) (%u - %u)\n",
	       wrk->id, qd[VCS1], qd[VCS2], wrk->vcs_rr, engine,
	       current_seqno(wrk, VCS1), current_gpu_seqno(wrk, VCS1),
	       current_seqno(wrk, VCS2), current_gpu_seqno(wrk, VCS2));
#endif
	return engine;
}

static enum intel_engine_id
__rt_select_engine(struct workload *wrk, unsigned long *qd, bool random)
{
	qd[VCS1] >>= 10;
	qd[VCS2] >>= 10;

	return __qd_select_engine(wrk, qd, random);
}

struct rt_depth {
	uint32_t seqno;
	uint32_t submitted;
	uint32_t completed;
};

static void get_rt_depth(struct workload *wrk,
			 unsigned int engine,
			 struct rt_depth *rt)
{
	const unsigned int idx = SEQNO_IDX(engine);
	uint32_t latch;

	do {
		latch = read_status_page(wrk, idx + 3);
		rt->submitted = read_status_page(wrk, idx + 1);
		rt->completed = read_status_page(wrk, idx + 2);
		rt->seqno = read_status_page(wrk, idx);
	} while (latch != rt->seqno);
}

static enum intel_engine_id
__rt_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w, bool random)
{
	unsigned long qd[NUM_ENGINES];
	unsigned int engine;

	igt_assert(w->engine == VCS);

	/* Estimate the "speed" of the most recent batch
	 *    (finish time - submit time)
	 * and use that as an approximate for the total remaining time for
	 * all batches on that engine, plus the time we expect this batch to
	 * take. We try to keep the total balanced between the engines.
	 */
	for (engine = VCS1; engine <= VCS2; engine++) {
		struct rt_depth rt;

		get_rt_depth(wrk, engine, &rt);
		qd[engine] = current_seqno(wrk, engine) - rt.seqno;
		wrk->qd_sum[engine] += qd[engine];
		qd[engine] = (qd[engine] + 1) * (rt.completed - rt.submitted);
#ifdef DEBUG
		printf("rt[0] = %d (%d - %d) x %d (%d - %d) = %ld\n",
		       current_seqno(wrk, engine) - rt.seqno,
		       current_seqno(wrk, engine), rt.seqno,
		       rt.completed - rt.submitted,
		       rt.completed, rt.submitted,
		       qd[engine]);
#endif
	}

	return __rt_select_engine(wrk, qd, random);
}

static enum intel_engine_id
rt_balance(const struct workload_balancer *balancer,
	   struct workload *wrk, struct w_step *w)
{

	return __rt_balance(balancer, wrk, w, false);
}

static enum intel_engine_id
rtr_balance(const struct workload_balancer *balancer,
	   struct workload *wrk, struct w_step *w)
{
	return __rt_balance(balancer, wrk, w, true);
}

static enum intel_engine_id
rtavg_balance(const struct workload_balancer *balancer,
	   struct workload *wrk, struct w_step *w)
{
	unsigned long qd[NUM_ENGINES];
	unsigned int engine;

	igt_assert(w->engine == VCS);

	/* Estimate the average "speed" of the most recent batches
	 *    (finish time - submit time)
	 * and use that as an approximate for the total remaining time for
	 * all batches on that engine plus the time we expect to execute in.
	 * We try to keep the total remaining balanced between the engines.
	 */
	for (engine = VCS1; engine <= VCS2; engine++) {
		struct rt_depth rt;

		get_rt_depth(wrk, engine, &rt);
		if (rt.seqno != wrk->rt.last[engine]) {
			igt_assert((long)(rt.completed - rt.submitted) > 0);
			ewma_rt_add(&wrk->rt.avg[engine],
				    rt.completed - rt.submitted);
			wrk->rt.last[engine] = rt.seqno;
		}
		qd[engine] = current_seqno(wrk, engine) - rt.seqno;
		wrk->qd_sum[engine] += qd[engine];
		qd[engine] =
			(qd[engine] + 1) * ewma_rt_read(&wrk->rt.avg[engine]);

#ifdef DEBUG
		printf("rtavg[%d] = %d (%d - %d) x %ld (%d) = %ld\n",
		       engine,
		       current_seqno(wrk, engine) - rt.seqno,
		       current_seqno(wrk, engine), rt.seqno,
		       ewma_rt_read(&wrk->rt.avg[engine]),
		       rt.completed - rt.submitted,
		       qd[engine]);
#endif
	}

	return __rt_select_engine(wrk, qd, false);
}

static enum intel_engine_id
context_balance(const struct workload_balancer *balancer,
		struct workload *wrk, struct w_step *w)
{
	return get_vcs_engine(__get_ctx(wrk, w)->static_vcs);
}

static unsigned int
get_engine_busy(const struct workload_balancer *balancer,
		struct workload *wrk, enum intel_engine_id engine)
{
	struct busy_balancer *bb = &wrk->busy_balancer;

	if (engine == VCS2 && (wrk->flags & VCS2REMAP))
		engine = BCS;

	return bb->busy[bb->engine_map[engine]];
}

static void
get_pmu_stats(const struct workload_balancer *b, struct workload *wrk)
{
	struct busy_balancer *bb = &wrk->busy_balancer;
	uint64_t val[7];
	unsigned int i;

	igt_assert_eq(read(bb->fd, val, sizeof(val)),
		      (2 + bb->num_engines) * sizeof(uint64_t));

	if (!bb->first) {
		for (i = 0; i < bb->num_engines; i++) {
			double d;

			d = (val[2 + i] - bb->prev[i]) * 100;
			d /= val[1] - bb->t_prev;
			bb->busy[i] = d;
		}
	}

	for (i = 0; i < bb->num_engines; i++)
		bb->prev[i] = val[2 + i];

	bb->t_prev = val[1];
	bb->first = false;
}

static enum intel_engine_id
busy_avg_balance(const struct workload_balancer *balancer,
		 struct workload *wrk, struct w_step *w)
{
	get_pmu_stats(balancer, wrk);

	return qdavg_balance(balancer, wrk, w);
}

static enum intel_engine_id
busy_balance(const struct workload_balancer *balancer,
	     struct workload *wrk, struct w_step *w)
{
	get_pmu_stats(balancer, wrk);

	return qd_balance(balancer, wrk, w);
}

static int
busy_init(const struct workload_balancer *balancer, struct workload *wrk)
{
	struct busy_balancer *bb = &wrk->busy_balancer;
	struct engine_desc {
		unsigned class, inst;
		enum intel_engine_id id;
	} *d, engines[] = {
		{ I915_ENGINE_CLASS_RENDER, 0, RCS },
		{ I915_ENGINE_CLASS_COPY, 0, BCS },
		{ I915_ENGINE_CLASS_VIDEO, 0, VCS1 },
		{ I915_ENGINE_CLASS_VIDEO, 1, VCS2 },
		{ I915_ENGINE_CLASS_VIDEO_ENHANCE, 0, VECS },
		{ 0, 0, VCS }
	};

	bb->num_engines = 0;
	bb->first = true;
	bb->fd = -1;

	for (d = &engines[0]; d->id != VCS; d++) {
		int pfd;

		pfd = perf_i915_open_group(I915_PMU_ENGINE_BUSY(d->class,
							        d->inst),
					   bb->fd);
		if (pfd < 0) {
			if (d->id != VCS2)
				return -(10 + bb->num_engines);
			else
				continue;
		}

		if (bb->num_engines == 0)
			bb->fd = pfd;

		bb->engine_map[d->id] = bb->num_engines++;
	}

	if (bb->num_engines < 5 && !(wrk->flags & VCS2REMAP))
		return -1;

	return 0;
}

static const struct workload_balancer all_balancers[] = {
	{
		.id = 0,
		.name = "rr",
		.desc = "Simple round-robin.",
		.balance = rr_balance,
	},
	{
		.id = 6,
		.name = "rand",
		.desc = "Random selection.",
		.balance = rand_balance,
	},
	{
		.id = 1,
		.name = "qd",
		.desc = "Queue depth estimation with round-robin on equal depth.",
		.flags = SEQNO,
		.min_gen = 8,
		.get_qd = get_qd_depth,
		.balance = qd_balance,
	},
	{
		.id = 5,
		.name = "qdr",
		.desc = "Queue depth estimation with random selection on equal depth.",
		.flags = SEQNO,
		.min_gen = 8,
		.get_qd = get_qd_depth,
		.balance = qdr_balance,
	},
	{
		.id = 7,
		.name = "qdavg",
		.desc = "Like qd, but using an average queue depth estimator.",
		.flags = SEQNO,
		.min_gen = 8,
		.get_qd = get_qd_depth,
		.balance = qdavg_balance,
	},
	{
		.id = 2,
		.name = "rt",
		.desc = "Queue depth plus last runtime estimation.",
		.flags = SEQNO | RT,
		.min_gen = 8,
		.get_qd = get_qd_depth,
		.balance = rt_balance,
	},
	{
		.id = 3,
		.name = "rtr",
		.desc = "Like rt but with random engine selection on equal depth.",
		.flags = SEQNO | RT,
		.min_gen = 8,
		.get_qd = get_qd_depth,
		.balance = rtr_balance,
	},
	{
		.id = 4,
		.name = "rtavg",
		.desc = "Improved version rt tracking average execution speed per engine.",
		.flags = SEQNO | RT,
		.min_gen = 8,
		.get_qd = get_qd_depth,
		.balance = rtavg_balance,
	},
	{
		.id = 8,
		.name = "context",
		.desc = "Static round-robin VCS assignment at context creation.",
		.balance = context_balance,
	},
	{
		.id = 9,
		.name = "busy",
		.desc = "Engine busyness based balancing.",
		.init = busy_init,
		.get_qd = get_engine_busy,
		.balance = busy_balance,
	},
	{
		.id = 10,
		.name = "busy-avg",
		.desc = "Average engine busyness based balancing.",
		.init = busy_init,
		.get_qd = get_engine_busy,
		.balance = busy_avg_balance,
	},
	{
		.id = 11,
		.name = "i915",
		.desc = "i915 balancing.",
		.flags = I915,
	},
};

static unsigned int
global_get_qd(const struct workload_balancer *balancer,
	      struct workload *wrk, enum intel_engine_id engine)
{
	igt_assert(wrk->global_wrk);
	igt_assert(wrk->global_balancer);

	return wrk->global_balancer->get_qd(wrk->global_balancer,
					    wrk->global_wrk, engine);
}

static enum intel_engine_id
global_balance(const struct workload_balancer *balancer,
	       struct workload *wrk, struct w_step *w)
{
	enum intel_engine_id engine;
	int ret;

	igt_assert(wrk->global_wrk);
	igt_assert(wrk->global_balancer);

	wrk = wrk->global_wrk;

	ret = pthread_mutex_lock(&wrk->mutex);
	igt_assert(ret == 0);

	engine = wrk->global_balancer->balance(wrk->global_balancer, wrk, w);

	ret = pthread_mutex_unlock(&wrk->mutex);
	igt_assert(ret == 0);

	return engine;
}

static const struct workload_balancer global_balancer = {
		.id = ~0,
		.name = "global",
		.desc = "Global balancer",
		.get_qd = global_get_qd,
		.balance = global_balance,
	};

static void
update_bb_seqno(struct w_step *w, enum intel_engine_id engine, uint32_t seqno)
{
	gem_set_domain(fd, w->bb_handle,
		       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	w->reloc[0].delta = SEQNO_OFFSET(engine);

	*w->seqno_value = seqno;
	*w->seqno_address = w->reloc[0].presumed_offset + w->reloc[0].delta;

	/* If not using NO_RELOC, force the relocations */
	if (!(w->eb.flags & I915_EXEC_NO_RELOC))
		w->reloc[0].presumed_offset = -1;
}

static void
update_bb_rt(struct w_step *w, enum intel_engine_id engine, uint32_t seqno)
{
	gem_set_domain(fd, w->bb_handle,
		       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	w->reloc[1].delta = SEQNO_OFFSET(engine) + sizeof(uint32_t);
	w->reloc[2].delta = SEQNO_OFFSET(engine) + 2 * sizeof(uint32_t);
	w->reloc[3].delta = SEQNO_OFFSET(engine) + 3 * sizeof(uint32_t);

	*w->latch_value = seqno;
	*w->latch_address = w->reloc[3].presumed_offset + w->reloc[3].delta;

	*w->rt0_value = *REG(RCS_TIMESTAMP);
	*w->rt0_address = w->reloc[1].presumed_offset + w->reloc[1].delta;
	*w->rt1_address = w->reloc[2].presumed_offset + w->reloc[2].delta;

	/* If not using NO_RELOC, force the relocations */
	if (!(w->eb.flags & I915_EXEC_NO_RELOC)) {
		w->reloc[1].presumed_offset = -1;
		w->reloc[2].presumed_offset = -1;
		w->reloc[3].presumed_offset = -1;
	}
}

static void
update_bb_start(struct w_step *w)
{
	if (!w->unbound_duration)
		return;

	gem_set_domain(fd, w->bb_handle,
		       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	*w->recursive_bb_start = MI_BATCH_BUFFER_START | (1 << 8) | 1;
}

static void w_sync_to(struct workload *wrk, struct w_step *w, int target)
{
	if (target < 0)
		target = wrk->nr_steps + target;

	igt_assert(target < wrk->nr_steps);

	while (wrk->steps[target].type != BATCH) {
		if (--target < 0)
			target = wrk->nr_steps + target;
	}

	igt_assert(target < wrk->nr_steps);
	igt_assert(wrk->steps[target].type == BATCH);

	gem_sync(fd, wrk->steps[target].obj[0].handle);
}

static uint32_t *get_status_cs(struct workload *wrk)
{
	return wrk->status_cs;
}

#define INIT_CLOCKS 0x1
#define INIT_ALL (INIT_CLOCKS)
static void init_status_page(struct workload *wrk, unsigned int flags)
{
	struct drm_i915_gem_relocation_entry reloc[4] = {};
	struct drm_i915_gem_exec_object2 *status_object =
						get_status_objects(wrk);
	struct drm_i915_gem_execbuffer2 eb = {
		.buffer_count = ARRAY_SIZE(wrk->status_object),
		.buffers_ptr = to_user_pointer(status_object)
	};
	uint32_t *base = get_status_cs(wrk);

	/* Want to make sure that the balancer has a reasonable view of
	 * the background busyness of each engine. To do that we occasionally
	 * send a dummy batch down the pipeline.
	 */

	if (!base)
		return;

	gem_set_domain(fd, status_object[1].handle,
		       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	status_object[1].relocs_ptr = to_user_pointer(reloc);
	status_object[1].relocation_count = 2;
	if (flags & INIT_CLOCKS)
		status_object[1].relocation_count += 2;

	for (int engine = 0; engine < NUM_ENGINES; engine++) {
		struct drm_i915_gem_relocation_entry *r = reloc;
		uint64_t presumed_offset = status_object[0].offset;
		uint32_t offset = engine * 128;
		uint32_t *cs = base + offset / sizeof(*cs);
		uint64_t addr;

		r->offset = offset + sizeof(uint32_t);
		r->delta = SEQNO_OFFSET(engine);
		r->presumed_offset = presumed_offset;
		addr = presumed_offset + r->delta;
		r++;
		*cs++ = MI_STORE_DWORD_IMM;
		*cs++ = addr;
		*cs++ = addr >> 32;
		*cs++ = new_seqno(wrk, engine);
		offset += 4 * sizeof(uint32_t);

		/* When we are busy, we can just reuse the last set of timings.
		 * If we have been idle for a while, we want to resample the
		 * latency on each engine (to measure external load).
		 */
		if (flags & INIT_CLOCKS) {
			r->offset = offset + sizeof(uint32_t);
			r->delta = SEQNO_OFFSET(engine) + sizeof(uint32_t);
			r->presumed_offset = presumed_offset;
			addr = presumed_offset + r->delta;
			r++;
			*cs++ = MI_STORE_DWORD_IMM;
			*cs++ = addr;
			*cs++ = addr >> 32;
			*cs++ = *REG(RCS_TIMESTAMP);
			offset += 4 * sizeof(uint32_t);

			r->offset = offset + 2 * sizeof(uint32_t);
			r->delta = SEQNO_OFFSET(engine) + 2*sizeof(uint32_t);
			r->presumed_offset = presumed_offset;
			addr = presumed_offset + r->delta;
			r++;
			*cs++ = 0x24 << 23 | 2; /* MI_STORE_REG_MEM */
			*cs++ = RCS_TIMESTAMP;
			*cs++ = addr;
			*cs++ = addr >> 32;
			offset += 4 * sizeof(uint32_t);
		}

		r->offset = offset + sizeof(uint32_t);
		r->delta = SEQNO_OFFSET(engine) + 3*sizeof(uint32_t);
		r->presumed_offset = presumed_offset;
		addr = presumed_offset + r->delta;
		r++;
		*cs++ = MI_STORE_DWORD_IMM;
		*cs++ = addr;
		*cs++ = addr >> 32;
		*cs++ = current_seqno(wrk, engine);
		offset += 4 * sizeof(uint32_t);

		*cs++ = MI_BATCH_BUFFER_END;

		eb_set_engine(&eb, engine, wrk->flags);
		eb.flags |= I915_EXEC_HANDLE_LUT;
		eb.flags |= I915_EXEC_NO_RELOC;

		eb.batch_start_offset = 128 * engine;

		gem_execbuf(fd, &eb);
	}
}

static void
do_eb(struct workload *wrk, struct w_step *w, enum intel_engine_id engine,
      unsigned int flags)
{
	uint32_t seqno = new_seqno(wrk, engine);
	unsigned int i;

	eb_update_flags(wrk, w, engine, flags);

	if (flags & SEQNO)
		update_bb_seqno(w, engine, seqno);
	if (flags & RT)
		update_bb_rt(w, engine, seqno);

	update_bb_start(w);

	w->eb.batch_start_offset =
		w->unbound_duration ?
		0 :
		ALIGN(w->bb_sz - get_bb_sz(get_duration(wrk, w)),
		      2 * sizeof(uint32_t));

	for (i = 0; i < w->fence_deps.nr; i++) {
		int tgt = w->idx + w->fence_deps.list[i];

		/* TODO: fence merging needed to support multiple inputs */
		igt_assert(i == 0);
		igt_assert(tgt >= 0 && tgt < w->idx);
		igt_assert(wrk->steps[tgt].emit_fence > 0);

		if (w->fence_deps.submit_fence)
			w->eb.flags |= I915_EXEC_FENCE_SUBMIT;
		else
			w->eb.flags |= I915_EXEC_FENCE_IN;

		w->eb.rsvd2 = wrk->steps[tgt].emit_fence;
	}

	if (w->eb.flags & I915_EXEC_FENCE_OUT)
		gem_execbuf_wr(fd, &w->eb);
	else
		gem_execbuf(fd, &w->eb);

	if (w->eb.flags & I915_EXEC_FENCE_OUT) {
		w->emit_fence = w->eb.rsvd2 >> 32;
		igt_assert(w->emit_fence > 0);
	}
}

static bool sync_deps(struct workload *wrk, struct w_step *w)
{
	bool synced = false;
	unsigned int i;

	for (i = 0; i < w->data_deps.nr; i++) {
		int dep_idx;

		igt_assert(w->data_deps.list[i] <= 0);

		if (!w->data_deps.list[i])
			continue;

		dep_idx = w->idx + w->data_deps.list[i];

		igt_assert(dep_idx >= 0 && dep_idx < w->idx);
		igt_assert(wrk->steps[dep_idx].type == BATCH);

		gem_sync(fd, wrk->steps[dep_idx].obj[0].handle);

		synced = true;
	}

	return synced;
}

static void *run_workload(void *data)
{
	struct workload *wrk = (struct workload *)data;
	struct timespec t_start, t_end;
	struct w_step *w;
	bool last_sync = false;
	int throttle = -1;
	int qd_throttle = -1;
	int count;
	int i;

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	init_status_page(wrk, INIT_ALL);
	for (count = 0; wrk->run && (wrk->background || count < wrk->repeat);
	     count++) {
		unsigned int cur_seqno = wrk->sync_seqno;

		clock_gettime(CLOCK_MONOTONIC, &wrk->repeat_start);

		for (i = 0, w = wrk->steps; wrk->run && (i < wrk->nr_steps);
		     i++, w++) {
			enum intel_engine_id engine = w->engine;
			int do_sleep = 0;

			if (w->type == DELAY) {
				do_sleep = w->delay;
			} else if (w->type == PERIOD) {
				struct timespec now;

				clock_gettime(CLOCK_MONOTONIC, &now);
				do_sleep = w->period -
					   elapsed_us(&wrk->repeat_start, &now);
				if (do_sleep < 0) {
					if (verbose > 1)
						printf("%u: Dropped period @ %u/%u (%dus late)!\n",
						       wrk->id, count, i, do_sleep);
					continue;
				}
			} else if (w->type == SYNC) {
				unsigned int s_idx = i + w->target;

				igt_assert(s_idx >= 0 && s_idx < i);
				igt_assert(wrk->steps[s_idx].type == BATCH);
				gem_sync(fd, wrk->steps[s_idx].obj[0].handle);
				continue;
			} else if (w->type == THROTTLE) {
				throttle = w->throttle;
				continue;
			} else if (w->type == QD_THROTTLE) {
				qd_throttle = w->throttle;
				continue;
			} else if (w->type == SW_FENCE) {
				igt_assert(w->emit_fence < 0);
				w->emit_fence =
					sw_sync_timeline_create_fence(wrk->sync_timeline,
								      cur_seqno + w->idx);
				igt_assert(w->emit_fence > 0);
				continue;
			} else if (w->type == SW_FENCE_SIGNAL) {
				int tgt = w->idx + w->target;
				int inc;

				igt_assert(tgt >= 0 && tgt < i);
				igt_assert(wrk->steps[tgt].type == SW_FENCE);
				cur_seqno += wrk->steps[tgt].idx;
				inc = cur_seqno - wrk->sync_seqno;
				sw_sync_timeline_inc(wrk->sync_timeline, inc);
				continue;
			} else if (w->type == CTX_PRIORITY) {
				if (w->priority != wrk->ctx_list[w->context].priority) {
					struct drm_i915_gem_context_param param = {
						.ctx_id = wrk->ctx_list[w->context].id,
						.param = I915_CONTEXT_PARAM_PRIORITY,
						.value = w->priority,
					};

					gem_context_set_param(fd, &param);
					wrk->ctx_list[w->context].priority =
								    w->priority;
				}
				continue;
			} else if (w->type == TERMINATE) {
				unsigned int t_idx = i + w->target;

				igt_assert(t_idx >= 0 && t_idx < i);
				igt_assert(wrk->steps[t_idx].type == BATCH);
				igt_assert(wrk->steps[t_idx].unbound_duration);

				*wrk->steps[t_idx].recursive_bb_start =
					MI_BATCH_BUFFER_END;
				__sync_synchronize();
				continue;
			} else if (w->type == PREEMPTION ||
				   w->type == ENGINE_MAP ||
				   w->type == LOAD_BALANCE ||
				   w->type == BOND) {
				continue;
			} else if (w->type == SSEU) {
				if (w->sseu != wrk->ctx_list[w->context * 2].sseu) {
					wrk->ctx_list[w->context * 2].sseu =
						set_ctx_sseu(&wrk->ctx_list[w->context * 2],
							     w->sseu);
				}
				continue;
			}

			if (do_sleep || w->type == PERIOD) {
				usleep(do_sleep);
				continue;
			}

			igt_assert(w->type == BATCH);

			if ((wrk->flags & DEPSYNC) && engine == VCS)
				last_sync = sync_deps(wrk, w);

			if (last_sync && (wrk->flags & HEARTBEAT))
				init_status_page(wrk, 0);

			last_sync = false;

			wrk->nr_bb[engine]++;
			if (engine == VCS && wrk->balancer &&
			    wrk->balancer->balance) {
				engine = wrk->balancer->balance(wrk->balancer,
								wrk, w);
				wrk->nr_bb[engine]++;
			}

			if (throttle > 0)
				w_sync_to(wrk, w, i - throttle);

			do_eb(wrk, w, engine, wrk->flags);

			if (w->request != -1) {
				igt_list_del(&w->rq_link);
				wrk->nrequest[w->request]--;
			}
			w->request = engine;
			igt_list_add_tail(&w->rq_link, &wrk->requests[engine]);
			wrk->nrequest[engine]++;

			if (!wrk->run)
				break;

			if (w->sync) {
				gem_sync(fd, w->obj[0].handle);
				last_sync = true;
			}

			if (qd_throttle > 0) {
				while (wrk->nrequest[engine] > qd_throttle) {
					struct w_step *s;

					s = igt_list_first_entry(&wrk->requests[engine],
								 s, rq_link);

					gem_sync(fd, s->obj[0].handle);
					last_sync = true;

					s->request = -1;
					igt_list_del(&s->rq_link);
					wrk->nrequest[engine]--;
				}
			}
		}

		if (wrk->sync_timeline) {
			int inc;

			inc = wrk->nr_steps - (cur_seqno - wrk->sync_seqno);
			sw_sync_timeline_inc(wrk->sync_timeline, inc);
			wrk->sync_seqno += wrk->nr_steps;
		}

		/* Cleanup all fences instantiated in this iteration. */
		for (i = 0, w = wrk->steps; wrk->run && (i < wrk->nr_steps);
		     i++, w++) {
			if (w->emit_fence > 0) {
				close(w->emit_fence);
				w->emit_fence = -1;
			}
		}
	}

	for (i = 0; i < NUM_ENGINES; i++) {
		if (!wrk->nrequest[i])
			continue;

		w = igt_list_last_entry(&wrk->requests[i], w, rq_link);
		gem_sync(fd, w->obj[0].handle);
	}

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	if (wrk->print_stats) {
		double t = elapsed(&t_start, &t_end);

		printf("%c%u: %.3fs elapsed (%d cycles, %.3f workloads/s).",
		       wrk->background ? ' ' : '*', wrk->id,
		       t, count, count / t);
		if (wrk->balancer)
			printf(" %lu (%lu + %lu) total VCS batches.",
			       wrk->nr_bb[VCS], wrk->nr_bb[VCS1], wrk->nr_bb[VCS2]);
		if (wrk->balancer && wrk->balancer->get_qd)
			printf(" Average queue depths %.3f, %.3f.",
			       (double)wrk->qd_sum[VCS1] / wrk->nr_bb[VCS],
			       (double)wrk->qd_sum[VCS2] / wrk->nr_bb[VCS]);
		putchar('\n');
	}

	return NULL;
}

static void fini_workload(struct workload *wrk)
{
	free(wrk->steps);
	free(wrk);
}

static unsigned long calibrate_nop(unsigned int tolerance_pct)
{
	const uint32_t bbe = 0xa << 23;
	unsigned int loops = 17;
	unsigned int usecs = nop_calibration_us;
	struct drm_i915_gem_exec_object2 obj = {};
	struct drm_i915_gem_execbuffer2 eb =
		{ .buffer_count = 1, .buffers_ptr = (uintptr_t)&obj};
	long size, last_size;
	struct timespec t_0, t_end;

	clock_gettime(CLOCK_MONOTONIC, &t_0);

	size = 256 * 1024;
	do {
		struct timespec t_start;

		obj.handle = gem_create(fd, size);
		gem_write(fd, obj.handle, size - sizeof(bbe), &bbe,
			  sizeof(bbe));
		gem_execbuf(fd, &eb);
		gem_sync(fd, obj.handle);

		clock_gettime(CLOCK_MONOTONIC, &t_start);
		for (int loop = 0; loop < loops; loop++)
			gem_execbuf(fd, &eb);
		gem_sync(fd, obj.handle);
		clock_gettime(CLOCK_MONOTONIC, &t_end);

		gem_close(fd, obj.handle);

		last_size = size;
		size = loops * size / elapsed(&t_start, &t_end) / 1e6 * usecs;
		size = ALIGN(size, sizeof(uint32_t));
	} while (elapsed(&t_0, &t_end) < 5 ||
		 abs(size - last_size) > (size * tolerance_pct / 100));

	return size / sizeof(uint32_t);
}

static void print_help(void)
{
	unsigned int i;

	puts(
"Usage: gem_wsim [OPTIONS]\n"
"\n"
"Runs a simulated workload on the GPU.\n"
"When ran without arguments performs a GPU calibration result of which needs to\n"
"be provided when running the simulation in subsequent invocations.\n"
"\n"
"Options:\n"
"  -h              This text.\n"
"  -q              Be quiet - do not output anything to stdout.\n"
"  -n <n>          Nop calibration value.\n"
"  -t <n>          Nop calibration tolerance percentage.\n"
"                  Use when there is a difficulty obtaining calibration with the\n"
"                  default settings.\n"
"  -I <n>          Initial randomness seed.\n"
"  -p <n>          Context priority to use for the following workload on the\n"
"                  command line.\n"
"  -w <desc|path>  Filename or a workload descriptor.\n"
"                  Can be given multiple times.\n"
"  -W <desc|path>  Filename or a master workload descriptor.\n"
"                  Only one master workload can be optinally specified in which\n"
"                  case all other workloads become background ones and run as\n"
"                  long as the master.\n"
"  -a <desc|path>  Append a workload to all other workloads.\n"
"  -r <n>          How many times to emit the workload.\n"
"  -c <n>          Fork N clients emitting the workload simultaneously.\n"
"  -x              Swap VCS1 and VCS2 engines in every other client.\n"
"  -b <n>          Load balancing to use.\n"
"                  Available load balancers are:"
	);

	for (i = 0; i < ARRAY_SIZE(all_balancers); i++) {
		igt_assert(all_balancers[i].desc);
		printf(
"                     %s (%u): %s\n",
		       all_balancers[i].name, all_balancers[i].id,
		       all_balancers[i].desc);
	}
	puts(
"                  Balancers can be specified either as names or as their id\n"
"                  number as listed above.\n"
"  -2              Remap VCS2 to BCS.\n"
"  -R              Round-robin initial VCS assignment per client.\n"
"  -H              Send heartbeat on synchronisation points with seqno based\n"
"                  balancers. Gives better engine busyness view in some cases.\n"
"  -s              Turn on small SSEU config for the next workload on the\n"
"                  command line. Subsequent -s switches it off.\n"
"  -S              Synchronize the sequence of random batch durations between\n"
"                  clients.\n"
"  -G              Global load balancing - a single load balancer will be shared\n"
"                  between all clients and there will be a single seqno domain.\n"
"  -d              Sync between data dependencies in userspace."
	);
}

static char *load_workload_descriptor(char *filename)
{
	struct stat sbuf;
	char *buf;
	int infd, ret, i;
	ssize_t len;

	ret = stat(filename, &sbuf);
	if (ret || !S_ISREG(sbuf.st_mode))
		return filename;

	igt_assert(sbuf.st_size < 1024 * 1024); /* Just so. */
	buf = malloc(sbuf.st_size);
	igt_assert(buf);

	infd = open(filename, O_RDONLY);
	igt_assert(infd >= 0);
	len = read(infd, buf, sbuf.st_size);
	igt_assert(len == sbuf.st_size);
	close(infd);

	for (i = 0; i < len; i++) {
		if (buf[i] == '\n')
			buf[i] = ',';
	}

	len--;
	while (buf[len] == ',')
		buf[len--] = 0;

	return buf;
}

static struct w_arg *
add_workload_arg(struct w_arg *w_args, unsigned int nr_args, char *w_arg,
		 int prio, bool sseu)
{
	w_args = realloc(w_args, sizeof(*w_args) * nr_args);
	igt_assert(w_args);
	w_args[nr_args - 1] = (struct w_arg) { w_arg, NULL, prio, sseu };

	return w_args;
}

static int find_balancer_by_name(char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(all_balancers); i++) {
		if (!strcasecmp(name, all_balancers[i].name))
			return all_balancers[i].id;
	}

	return -1;
}

static const struct workload_balancer *find_balancer_by_id(unsigned int id)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(all_balancers); i++) {
		if (id == all_balancers[i].id)
			return &all_balancers[i];
	}

	return NULL;
}

static void init_clocks(void)
{
	struct timespec t_start, t_end;
	uint32_t rcs_start, rcs_end;
	double overhead, t;

	intel_register_access_init(intel_get_pci_device(), false, fd);

	if (verbose <= 1)
		return;

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	for (int i = 0; i < 100; i++)
		rcs_start = *REG(RCS_TIMESTAMP);
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	overhead = 2 * elapsed(&t_start, &t_end) / 100;

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	for (int i = 0; i < 100; i++)
		clock_gettime(CLOCK_MONOTONIC, &t_end);
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	overhead += elapsed(&t_start, &t_end) / 100;

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	rcs_start = *REG(RCS_TIMESTAMP);
	usleep(100);
	rcs_end = *REG(RCS_TIMESTAMP);
	clock_gettime(CLOCK_MONOTONIC, &t_end);

	t = elapsed(&t_start, &t_end) - overhead;
	printf("%d cycles in %.1fus, i.e. 1024 cycles takes %1.fus\n",
	       rcs_end - rcs_start, 1e6*t, 1024e6 * t / (rcs_end - rcs_start));
}

int main(int argc, char **argv)
{
	unsigned int repeat = 1;
	unsigned int clients = 1;
	unsigned int flags = 0;
	struct timespec t_start, t_end;
	struct workload **w, **wrk = NULL;
	struct workload *app_w = NULL;
	unsigned int nr_w_args = 0;
	int master_workload = -1;
	char *append_workload_arg = NULL;
	struct w_arg *w_args = NULL;
	unsigned int tolerance_pct = 1;
	const struct workload_balancer *balancer = NULL;
	char *endptr = NULL;
	int prio = 0;
	double t;
	int i, c;

	/*
	 * Open the device via the low-level API so we can do the GPU quiesce
	 * manually as close as possible in time to the start of the workload.
	 * This minimizes the gap in engine utilization tracking when observed
	 * via external tools like trace.pl.
	 */
	fd = __drm_open_driver(DRIVER_INTEL);
	igt_require(fd);

	init_clocks();

	master_prng = time(NULL);

	while ((c = getopt(argc, argv,
			   "hqv2RsSHxGdc:n:r:w:W:a:t:b:p:I:")) != -1) {
		switch (c) {
		case 'W':
			if (master_workload >= 0) {
				wsim_err("Only one master workload can be given!\n");
				return 1;
			}
			master_workload = nr_w_args;
			/* Fall through */
		case 'w':
			w_args = add_workload_arg(w_args, ++nr_w_args, optarg,
						  prio, flags & SSEU);
			break;
		case 'p':
			prio = atoi(optarg);
			break;
		case 'a':
			if (append_workload_arg) {
				wsim_err("Only one append workload can be given!\n");
				return 1;
			}
			append_workload_arg = optarg;
			break;
		case 'c':
			clients = strtol(optarg, NULL, 0);
			break;
		case 't':
			tolerance_pct = strtol(optarg, NULL, 0);
			break;
		case 'n':
			nop_calibration = strtol(optarg, NULL, 0);
			break;
		case 'r':
			repeat = strtol(optarg, NULL, 0);
			break;
		case 'q':
			verbose = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			flags |= SWAPVCS;
			break;
		case '2':
			flags |= VCS2REMAP;
			break;
		case 'R':
			flags |= INITVCSRR;
			break;
		case 'S':
			flags |= SYNCEDCLIENTS;
			break;
		case 's':
			flags ^= SSEU;
			break;
		case 'H':
			flags |= HEARTBEAT;
			break;
		case 'G':
			flags |= GLOBAL_BALANCE;
			break;
		case 'd':
			flags |= DEPSYNC;
			break;
		case 'b':
			i = find_balancer_by_name(optarg);
			if (i < 0) {
				i = strtol(optarg, &endptr, 0);
				if (endptr && *endptr)
					i = -1;
			}

			if (i >= 0) {
				balancer = find_balancer_by_id(i);
				if (balancer) {
					igt_assert(intel_gen(intel_get_drm_devid(fd)) >= balancer->min_gen);
					flags |= BALANCE | balancer->flags;
				}
			}

			if (!balancer) {
				wsim_err("Unknown balancing mode '%s'!\n",
					 optarg);
				return 1;
			}
			break;
		case 'I':
			master_prng = strtol(optarg, NULL, 0);
			break;
		case 'h':
			print_help();
			return 0;
		default:
			return 1;
		}
	}

	if ((flags & HEARTBEAT) && !(flags & SEQNO)) {
		wsim_err("Heartbeat needs a seqno based balancer!\n");
		return 1;
	}

	if ((flags & VCS2REMAP) && (flags & I915)) {
		wsim_err("VCS remapping not supported with i915 balancing!\n");
		return 1;
	}

	if (!nop_calibration) {
		if (verbose > 1)
			printf("Calibrating nop delay with %u%% tolerance...\n",
				tolerance_pct);
		nop_calibration = calibrate_nop(tolerance_pct);
		if (verbose)
			printf("Nop calibration for %uus delay is %lu.\n",
			       nop_calibration_us, nop_calibration);

		return 0;
	}

	if (!nr_w_args) {
		wsim_err("No workload descriptor(s)!\n");
		return 1;
	}

	if (nr_w_args > 1 && clients > 1) {
		wsim_err("Cloned clients cannot be combined with multiple workloads!\n");
		return 1;
	}

	if ((flags & GLOBAL_BALANCE) && !balancer) {
		wsim_err("Balancer not specified in global balancing mode!\n");
		return 1;
	}

	if (append_workload_arg) {
		append_workload_arg = load_workload_descriptor(append_workload_arg);
		if (!append_workload_arg) {
			wsim_err("Failed to load append workload descriptor!\n");
			return 1;
		}
	}

	if (append_workload_arg) {
		struct w_arg arg = { NULL, append_workload_arg, 0 };
		app_w = parse_workload(&arg, flags, NULL);
		if (!app_w) {
			wsim_err("Failed to parse append workload!\n");
			return 1;
		}
	}

	wrk = calloc(nr_w_args, sizeof(*wrk));
	igt_assert(wrk);

	for (i = 0; i < nr_w_args; i++) {
		w_args[i].desc = load_workload_descriptor(w_args[i].filename);

		if (!w_args[i].desc) {
			wsim_err("Failed to load workload descriptor %u!\n", i);
			return 1;
		}

		wrk[i] = parse_workload(&w_args[i], flags, app_w);
		if (!wrk[i]) {
			wsim_err("Failed to parse workload %u!\n", i);
			return 1;
		}
	}

	if (nr_w_args > 1)
		clients = nr_w_args;

	if (verbose > 1) {
		printf("Random seed is %u.\n", master_prng);
		printf("Using %lu nop calibration for %uus delay.\n",
		       nop_calibration, nop_calibration_us);
		printf("%u client%s.\n", clients, clients > 1 ? "s" : "");
		if (flags & SWAPVCS)
			printf("Swapping VCS rings between clients.\n");
		if (flags & GLOBAL_BALANCE) {
			if (flags & I915) {
				printf("Ignoring global balancing with i915!\n");
				flags &= ~GLOBAL_BALANCE;
			} else {
				printf("Using %s balancer in global mode.\n",
				       balancer->name);
			}
		} else if (balancer) {
			printf("Using %s balancer.\n", balancer->name);
		}
	}

	srand(master_prng);
	master_prng = rand();

	if (master_workload >= 0 && clients == 1)
		master_workload = -1;

	w = calloc(clients, sizeof(struct workload *));
	igt_assert(w);

	for (i = 0; i < clients; i++) {
		unsigned int flags_ = flags;

		w[i] = clone_workload(wrk[nr_w_args > 1 ? i : 0]);

		if (flags & SWAPVCS && i & 1)
			flags_ &= ~SWAPVCS;

		if ((flags & GLOBAL_BALANCE) && !(flags & I915)) {
			w[i]->balancer = &global_balancer;
			w[i]->global_wrk = w[0];
			w[i]->global_balancer = balancer;
		} else {
			w[i]->balancer = balancer;
		}

		w[i]->flags = flags;
		w[i]->repeat = repeat;
		w[i]->background = master_workload >= 0 && i != master_workload;
		w[i]->print_stats = verbose > 1 ||
				    (verbose > 0 && master_workload == i);

		if (prepare_workload(i, w[i], flags_)) {
			wsim_err("Failed to prepare workload %u!\n", i);
			return 1;
		}


		if (balancer && balancer->init) {
			int ret = balancer->init(balancer, w[i]);
			if (ret) {
				wsim_err("Failed to initialize balancing! (%u=%d)\n",
					 i, ret);
				return 1;
			}
		}
	}

	gem_quiescent_gpu(fd);

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	for (i = 0; i < clients; i++) {
		int ret;

		ret = pthread_create(&w[i]->thread, NULL, run_workload, w[i]);
		igt_assert_eq(ret, 0);
	}

	if (master_workload >= 0) {
		int ret = pthread_join(w[master_workload]->thread, NULL);

		igt_assert(ret == 0);

		for (i = 0; i < clients; i++)
			w[i]->run = false;
	}

	for (i = 0; i < clients; i++) {
		if (master_workload != i) {
			int ret = pthread_join(w[i]->thread, NULL);
			igt_assert(ret == 0);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	t = elapsed(&t_start, &t_end);
	if (verbose)
		printf("%.3fs elapsed (%.3f workloads/s)\n",
		       t, clients * repeat / t);

	for (i = 0; i < clients; i++)
		fini_workload(w[i]);
	free(w);
	for (i = 0; i < nr_w_args; i++)
		fini_workload(wrk[i]);
	free(w_args);

	return 0;
}
