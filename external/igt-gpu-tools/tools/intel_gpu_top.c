/*
 * Copyright © 2007-2019 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#include <limits.h>
#include <signal.h>

#include "igt_perf.h"

struct pmu_pair {
	uint64_t cur;
	uint64_t prev;
};

struct pmu_counter {
	bool present;
	uint64_t config;
	unsigned int idx;
	struct pmu_pair val;
};

struct engine {
	const char *name;
	char *display_name;
	char *short_name;

	unsigned int class;
	unsigned int instance;

	unsigned int num_counters;

	struct pmu_counter busy;
	struct pmu_counter wait;
	struct pmu_counter sema;
};

struct engines {
	unsigned int num_engines;
	unsigned int num_counters;
	DIR *root;
	int fd;
	struct pmu_pair ts;

	int rapl_fd;
	double rapl_scale;
	const char *rapl_unit;

	int imc_fd;
	double imc_reads_scale;
	const char *imc_reads_unit;
	double imc_writes_scale;
	const char *imc_writes_unit;

	struct pmu_counter freq_req;
	struct pmu_counter freq_act;
	struct pmu_counter irq;
	struct pmu_counter rc6;
	struct pmu_counter rapl;
	struct pmu_counter imc_reads;
	struct pmu_counter imc_writes;

	struct engine engine;
};

static uint64_t
get_pmu_config(int dirfd, const char *name, const char *counter)
{
	char buf[128], *p;
	int fd, ret;

	ret = snprintf(buf, sizeof(buf), "%s-%s", name, counter);
	if (ret < 0 || ret == sizeof(buf))
		return -1;

	fd = openat(dirfd, buf, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, sizeof(buf));
	close(fd);
	if (ret <= 0)
		return -1;

	p = index(buf, '0');
	if (!p)
		return -1;

	return strtoul(p, NULL, 0);
}

#define engine_ptr(engines, n) (&engines->engine + (n))

static const char *class_display_name(unsigned int class)
{
	switch (class) {
	case I915_ENGINE_CLASS_RENDER:
		return "Render/3D";
	case I915_ENGINE_CLASS_COPY:
		return "Blitter";
	case I915_ENGINE_CLASS_VIDEO:
		return "Video";
	case I915_ENGINE_CLASS_VIDEO_ENHANCE:
		return "VideoEnhance";
	default:
		return "[unknown]";
	}
}

static const char *class_short_name(unsigned int class)
{
	switch (class) {
	case I915_ENGINE_CLASS_RENDER:
		return "RCS";
	case I915_ENGINE_CLASS_COPY:
		return "BCS";
	case I915_ENGINE_CLASS_VIDEO:
		return "VCS";
	case I915_ENGINE_CLASS_VIDEO_ENHANCE:
		return "VECS";
	default:
		return "UNKN";
	}
}

static int engine_cmp(const void *__a, const void *__b)
{
	const struct engine *a = (struct engine *)__a;
	const struct engine *b = (struct engine *)__b;

	if (a->class != b->class)
		return a->class - b->class;
	else
		return a->instance - b->instance;
}

static struct engines *discover_engines(void)
{
	const char *sysfs_root = "/sys/devices/i915/events";
	struct engines *engines;
	struct dirent *dent;
	int ret = 0;
	DIR *d;

	engines = malloc(sizeof(struct engines));
	if (!engines)
		return NULL;

	memset(engines, 0, sizeof(*engines));

	engines->num_engines = 0;

	d = opendir(sysfs_root);
	if (!d)
		return NULL;

	while ((dent = readdir(d)) != NULL) {
		const char *endswith = "-busy";
		const unsigned int endlen = strlen(endswith);
		struct engine *engine =
				engine_ptr(engines, engines->num_engines);
		char buf[256];

		if (dent->d_type != DT_REG)
			continue;

		if (strlen(dent->d_name) >= sizeof(buf)) {
			ret = ENAMETOOLONG;
			break;
		}

		strcpy(buf, dent->d_name);

		/* xxxN-busy */
		if (strlen(buf) < (endlen + 4))
			continue;
		if (strcmp(&buf[strlen(buf) - endlen], endswith))
			continue;

		memset(engine, 0, sizeof(*engine));

		buf[strlen(buf) - endlen] = 0;
		engine->name = strdup(buf);
		if (!engine->name) {
			ret = errno;
			break;
		}

		engine->busy.config = get_pmu_config(dirfd(d), engine->name,
						     "busy");
		if (engine->busy.config == -1) {
			ret = ENOENT;
			break;
		}

		engine->class = (engine->busy.config &
				 (__I915_PMU_OTHER(0) - 1)) >>
				I915_PMU_CLASS_SHIFT;

		engine->instance = (engine->busy.config >>
				    I915_PMU_SAMPLE_BITS) &
				    ((1 << I915_PMU_SAMPLE_INSTANCE_BITS) - 1);

		ret = asprintf(&engine->display_name, "%s/%u",
			       class_display_name(engine->class),
			       engine->instance);
		if (ret <= 0) {
			ret = errno;
			break;
		}

		ret = asprintf(&engine->short_name, "%s/%u",
			       class_short_name(engine->class),
			       engine->instance);
		if (ret <= 0) {
			ret = errno;
			break;
		}

		engines->num_engines++;
		engines = realloc(engines, sizeof(struct engines) +
				  engines->num_engines * sizeof(struct engine));
		if (!engines) {
			ret = errno;
			break;
		}

		ret = 0;
	}

	if (ret) {
		free(engines);
		errno = ret;

		return NULL;
	}

	qsort(engine_ptr(engines, 0), engines->num_engines,
	      sizeof(struct engine), engine_cmp);

	engines->root = d;

	return engines;
}

static int
filename_to_buf(const char *filename, char *buf, unsigned int bufsize)
{
	int fd, err;
	ssize_t ret;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, bufsize - 1);
	err = errno;
	close(fd);
	if (ret < 1) {
		errno = ret < 0 ? err : ENOMSG;

		return -1;
	}

	if (ret > 1 && buf[ret - 1] == '\n')
		buf[ret - 1] = '\0';
	else
		buf[ret] = '\0';

	return 0;
}

static uint64_t filename_to_u64(const char *filename, int base)
{
	char buf[64], *b;

	if (filename_to_buf(filename, buf, sizeof(buf)))
		return 0;

	/*
	 * Handle both single integer and key=value formats by skipping
	 * leading non-digits.
	 */
	b = buf;
	while (*b && !isdigit(*b))
		b++;

	return strtoull(b, NULL, base);
}

static double filename_to_double(const char *filename)
{
	char *oldlocale;
	char buf[80];
	double v;

	if (filename_to_buf(filename, buf, sizeof(buf)))
		return 0;

	oldlocale = setlocale(LC_ALL, "C");
	v = strtod(buf, NULL);
	setlocale(LC_ALL, oldlocale);

	return v;
}

#define RAPL_ROOT "/sys/devices/power/"
#define RAPL_EVENT "/sys/devices/power/events/"

static uint64_t rapl_type_id(void)
{
	return filename_to_u64(RAPL_ROOT "type", 10);
}

static uint64_t rapl_gpu_power(void)
{
	return filename_to_u64(RAPL_EVENT "energy-gpu", 0);
}

static double rapl_gpu_power_scale(void)
{
	return filename_to_double(RAPL_EVENT "energy-gpu.scale");
}

static const char *rapl_gpu_power_unit(void)
{
	char buf[32];

	if (filename_to_buf(RAPL_EVENT "energy-gpu.unit",
			    buf, sizeof(buf)) == 0)
		if (!strcmp(buf, "Joules"))
			return strdup("Watts");
		else
			return strdup(buf);
	else
		return NULL;
}

#define IMC_ROOT "/sys/devices/uncore_imc/"
#define IMC_EVENT "/sys/devices/uncore_imc/events/"

static uint64_t imc_type_id(void)
{
	return filename_to_u64(IMC_ROOT "type", 10);
}

static uint64_t imc_data_reads(void)
{
	return filename_to_u64(IMC_EVENT "data_reads", 0);
}

static double imc_data_reads_scale(void)
{
	return filename_to_double(IMC_EVENT "data_reads.scale");
}

static const char *imc_data_reads_unit(void)
{
	char buf[32];

	if (filename_to_buf(IMC_EVENT "data_reads.unit", buf, sizeof(buf)) == 0)
		return strdup(buf);
	else
		return NULL;
}

static uint64_t imc_data_writes(void)
{
	return filename_to_u64(IMC_EVENT "data_writes", 0);
}

static double imc_data_writes_scale(void)
{
	return filename_to_double(IMC_EVENT "data_writes.scale");
}

static const char *imc_data_writes_unit(void)
{
	char buf[32];

	if (filename_to_buf(IMC_EVENT "data_writes.unit",
			    buf, sizeof(buf)) == 0)
		return strdup(buf);
	else
		return NULL;
}

#define _open_pmu(cnt, pmu, fd) \
({ \
	int fd__; \
\
	fd__ = perf_i915_open_group((pmu)->config, (fd)); \
	if (fd__ >= 0) { \
		if ((fd) == -1) \
			(fd) = fd__; \
		(pmu)->present = true; \
		(pmu)->idx = (cnt)++; \
	} \
\
	fd__; \
})

#define _open_imc(cnt, pmu, fd) \
({ \
	int fd__; \
\
	fd__ = igt_perf_open_group(imc_type_id(), (pmu)->config, (fd)); \
	if (fd__ >= 0) { \
		if ((fd) == -1) \
			(fd) = fd__; \
		(pmu)->present = true; \
		(pmu)->idx = (cnt)++; \
	} \
\
	fd__; \
})

static int pmu_init(struct engines *engines)
{
	unsigned int i;
	int fd;

	engines->fd = -1;
	engines->num_counters = 0;

	engines->irq.config = I915_PMU_INTERRUPTS;
	fd = _open_pmu(engines->num_counters, &engines->irq, engines->fd);
	if (fd < 0)
		return -1;

	engines->freq_req.config = I915_PMU_REQUESTED_FREQUENCY;
	_open_pmu(engines->num_counters, &engines->freq_req, engines->fd);

	engines->freq_act.config = I915_PMU_ACTUAL_FREQUENCY;
	_open_pmu(engines->num_counters, &engines->freq_act, engines->fd);

	engines->rc6.config = I915_PMU_RC6_RESIDENCY;
	_open_pmu(engines->num_counters, &engines->rc6, engines->fd);

	for (i = 0; i < engines->num_engines; i++) {
		struct engine *engine = engine_ptr(engines, i);
		struct {
			struct pmu_counter *pmu;
			const char *counter;
		} *cnt, counters[] = {
			{ .pmu = &engine->busy, .counter = "busy" },
			{ .pmu = &engine->wait, .counter = "wait" },
			{ .pmu = &engine->sema, .counter = "sema" },
			{ .pmu = NULL, .counter = NULL },
		};

		for (cnt = counters; cnt->pmu; cnt++) {
			if (!cnt->pmu->config)
				cnt->pmu->config =
					get_pmu_config(dirfd(engines->root),
						       engine->name,
						       cnt->counter);
			fd = _open_pmu(engines->num_counters, cnt->pmu,
				       engines->fd);
			if (fd >= 0)
				engine->num_counters++;
		}
	}

	engines->rapl_fd = -1;
	if (rapl_type_id()) {
		engines->rapl_scale = rapl_gpu_power_scale();
		engines->rapl_unit = rapl_gpu_power_unit();
		if (!engines->rapl_unit)
			return -1;

		engines->rapl.config = rapl_gpu_power();
		if (!engines->rapl.config)
			return -1;

		engines->rapl_fd = igt_perf_open(rapl_type_id(),
						 engines->rapl.config);
		if (engines->rapl_fd < 0)
			return -1;

		engines->rapl.present = true;
	}

	engines->imc_fd = -1;
	if (imc_type_id()) {
		unsigned int num = 0;

		engines->imc_reads_scale = imc_data_reads_scale();
		engines->imc_writes_scale = imc_data_writes_scale();

		engines->imc_reads_unit = imc_data_reads_unit();
		if (!engines->imc_reads_unit)
			return -1;

		engines->imc_writes_unit = imc_data_writes_unit();
		if (!engines->imc_writes_unit)
			return -1;

		engines->imc_reads.config = imc_data_reads();
		if (!engines->imc_reads.config)
			return -1;

		engines->imc_writes.config = imc_data_writes();
		if (!engines->imc_writes.config)
			return -1;

		fd = _open_imc(num, &engines->imc_reads, engines->imc_fd);
		if (fd < 0)
			return -1;
		fd = _open_imc(num, &engines->imc_writes, engines->imc_fd);
		if (fd < 0)
			return -1;

		engines->imc_reads.present = true;
		engines->imc_writes.present = true;
	}

	return 0;
}

static uint64_t pmu_read_multi(int fd, unsigned int num, uint64_t *val)
{
	uint64_t buf[2 + num];
	unsigned int i;
	ssize_t len;

	memset(buf, 0, sizeof(buf));

	len = read(fd, buf, sizeof(buf));
	assert(len == sizeof(buf));

	for (i = 0; i < num; i++)
		val[i] = buf[2 + i];

	return buf[1];
}

static double pmu_calc(struct pmu_pair *p, double d, double t, double s)
{
	double v;

	v = p->cur - p->prev;
	v /= d;
	v /= t;
	v *= s;

	if (s == 100.0 && v > 100.0)
		v = 100.0;

	return v;
}

static void fill_str(char *buf, unsigned int bufsz, char c, unsigned int num)
{
	unsigned int i;

	for (i = 0; i < num && i < (bufsz - 1); i++)
		*buf++ = c;

	*buf = 0;
}

static uint64_t __pmu_read_single(int fd, uint64_t *ts)
{
	uint64_t data[2] = { };
	ssize_t len;

	len = read(fd, data, sizeof(data));
	assert(len == sizeof(data));

	if (ts)
		*ts = data[1];

	return data[0];
}

static uint64_t pmu_read_single(int fd)
{
	return __pmu_read_single(fd, NULL);
}

static void __update_sample(struct pmu_counter *counter, uint64_t val)
{
	counter->val.prev = counter->val.cur;
	counter->val.cur = val;
}

static void update_sample(struct pmu_counter *counter, uint64_t *val)
{
	if (counter->present)
		__update_sample(counter, val[counter->idx]);
}

static void pmu_sample(struct engines *engines)
{
	const int num_val = engines->num_counters;
	uint64_t val[2 + num_val];
	unsigned int i;

	engines->ts.prev = engines->ts.cur;

	if (engines->rapl_fd >= 0)
		__update_sample(&engines->rapl,
				pmu_read_single(engines->rapl_fd));

	if (engines->imc_fd >= 0) {
		pmu_read_multi(engines->imc_fd, 2, val);
		update_sample(&engines->imc_reads, val);
		update_sample(&engines->imc_writes, val);
	}

	engines->ts.cur = pmu_read_multi(engines->fd, num_val, val);

	update_sample(&engines->freq_req, val);
	update_sample(&engines->freq_act, val);
	update_sample(&engines->irq, val);
	update_sample(&engines->rc6, val);

	for (i = 0; i < engines->num_engines; i++) {
		struct engine *engine = engine_ptr(engines, i);

		update_sample(&engine->busy, val);
		update_sample(&engine->sema, val);
		update_sample(&engine->wait, val);
	}
}

static const char *bars[] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█" };

static void
print_percentage_bar(double percent, int max_len)
{
	int bar_len = percent * (8 * (max_len - 2)) / 100.0;
	int i;

	putchar('|');

	for (i = bar_len; i >= 8; i -= 8)
		printf("%s", bars[8]);
	if (i)
		printf("%s", bars[i]);

	for (i = 0; i < (max_len - 2 - (bar_len + 7) / 8); i++)
		putchar(' ');

	putchar('|');
}

#define DEFAULT_PERIOD_MS (1000)

static void
usage(const char *appname)
{
	printf("intel_gpu_top - Display a top-like summary of Intel GPU usage\n"
		"\n"
		"Usage: %s [parameters]\n"
		"\n"
		"\tThe following parameters are optional:\n\n"
		"\t[-h]            Show this help text.\n"
		"\t[-J]            Output JSON formatted data.\n"
		"\t[-l]            List plain text data.\n"
		"\t[-o <file|->]   Output to specified file or '-' for standard out.\n"
		"\t[-s <ms>]       Refresh period in milliseconds (default %ums).\n"
		"\n",
		appname, DEFAULT_PERIOD_MS);
}

static enum {
	INTERACTIVE,
	STDOUT,
	JSON
} output_mode;

struct cnt_item {
	struct pmu_counter *pmu;
	unsigned int fmt_width;
	unsigned int fmt_precision;
	double d;
	double t;
	double s;
	const char *name;
	const char *unit;

	/* Internal fields. */
	char buf[16];
};

struct cnt_group {
	const char *name;
	const char *display_name;
	struct cnt_item *items;
};

static unsigned int json_indent_level;

static const char *json_indent[] = {
	"",
	"\t",
	"\t\t",
	"\t\t\t",
	"\t\t\t\t",
	"\t\t\t\t\t",
};

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

static unsigned int json_prev_struct_members;
static unsigned int json_struct_members;

FILE *out;

static void
json_open_struct(const char *name)
{
	assert(json_indent_level < ARRAY_SIZE(json_indent));

	json_prev_struct_members = json_struct_members;
	json_struct_members = 0;

	if (name)
		fprintf(out, "%s%s\"%s\": {\n",
			json_prev_struct_members ? ",\n" : "",
			json_indent[json_indent_level],
			name);
	else
		fprintf(out, "%s\n%s{\n",
			json_prev_struct_members ? "," : "",
			json_indent[json_indent_level]);

	json_indent_level++;
}

static void
json_close_struct(void)
{
	assert(json_indent_level > 0);

	fprintf(out, "\n%s}", json_indent[--json_indent_level]);

	if (json_indent_level == 0)
		fflush(stdout);
}

static unsigned int
json_add_member(const struct cnt_group *parent, struct cnt_item *item,
		unsigned int headers)
{
	assert(json_indent_level < ARRAY_SIZE(json_indent));

	fprintf(out, "%s%s\"%s\": ",
		json_struct_members ? ",\n" : "",
		json_indent[json_indent_level], item->name);

	json_struct_members++;

	if (!strcmp(item->name, "unit"))
		fprintf(out, "\"%s\"", item->unit);
	else
		fprintf(out, "%f",
			pmu_calc(&item->pmu->val, item->d, item->t, item->s));

	return 1;
}

static unsigned int stdout_level;

#define STDOUT_HEADER_REPEAT 20
static unsigned int stdout_lines = STDOUT_HEADER_REPEAT;

static void
stdout_open_struct(const char *name)
{
	stdout_level++;
	assert(stdout_level > 0);
}

static void
stdout_close_struct(void)
{
	assert(stdout_level > 0);
	if (--stdout_level == 0) {
		stdout_lines++;
		fputs("\n", out);
		fflush(out);
	}
}

static unsigned int
stdout_add_member(const struct cnt_group *parent, struct cnt_item *item,
		  unsigned int headers)
{
	unsigned int fmt_tot = item->fmt_width + (item->fmt_precision ? 1 : 0);
	char buf[fmt_tot + 1];
	double val;
	int len;

	if (!item->pmu)
		return 0;
	else if (!item->pmu->present)
		return 0;

	if (headers == 1) {
		unsigned int grp_tot = 0;
		struct cnt_item *it;

		if (item != parent->items)
			return 0;

		for (it = parent->items; it->pmu; it++) {
			if (!it->pmu->present)
				continue;

			grp_tot += 1 + it->fmt_width +
				   (it->fmt_precision ? 1 : 0);
		}

		fprintf(out, "%*s ", grp_tot - 1, parent->display_name);
		return 0;
	} else if (headers == 2) {
		fprintf(out, "%*s ", fmt_tot, item->unit ?: item->name);
		return 0;
	}

	val = pmu_calc(&item->pmu->val, item->d, item->t, item->s);

	len = snprintf(buf, sizeof(buf), "%*.*f",
		       fmt_tot, item->fmt_precision, val);
	if (len < 0 || len == sizeof(buf))
		fill_str(buf, sizeof(buf), 'X', fmt_tot);

	len = fprintf(out, "%s ", buf);

	return len > 0 ? len : 0;
}

static void
term_open_struct(const char *name)
{
}

static void
term_close_struct(void)
{
}

static unsigned int
term_add_member(const struct cnt_group *parent, struct cnt_item *item,
		unsigned int headers)
{
	unsigned int fmt_tot = item->fmt_width + (item->fmt_precision ? 1 : 0);
	double val;
	int len;

	if (!item->pmu)
		return 0;

	assert(fmt_tot <= sizeof(item->buf));

	if (!item->pmu->present) {
		fill_str(item->buf, sizeof(item->buf), '-', fmt_tot);
		return 1;
	}

	val = pmu_calc(&item->pmu->val, item->d, item->t, item->s);
	len = snprintf(item->buf, sizeof(item->buf),
		       "%*.*f",
		       fmt_tot, item->fmt_precision, val);

	if (len < 0 || len == sizeof(item->buf))
		fill_str(item->buf, sizeof(item->buf), 'X', fmt_tot);

	return 1;
}

struct print_operations {
	void (*open_struct)(const char *name);
	void (*close_struct)(void);
	unsigned int (*add_member)(const struct cnt_group *parent,
				   struct cnt_item *item,
				   unsigned int headers);
	bool (*print_group)(struct cnt_group *group, unsigned int headers);
};

static const struct print_operations *pops;

static unsigned int
present_in_group(const struct cnt_group *grp)
{
	unsigned int present = 0;
	struct cnt_item *item;

	for (item = grp->items; item->name; item++) {
		if (item->pmu && item->pmu->present)
			present++;
	}

	return present;
}

static bool
print_group(struct cnt_group *grp, unsigned int headers)
{
	unsigned int consumed = 0;
	struct cnt_item *item;

	if (!present_in_group(grp))
		return false;

	pops->open_struct(grp->name);

	for (item = grp->items; item->name; item++)
		consumed += pops->add_member(grp, item, headers);

	pops->close_struct();

	return consumed;
}

static bool
term_print_group(struct cnt_group *grp, unsigned int headers)
{
	unsigned int consumed = 0;
	struct cnt_item *item;

	pops->open_struct(grp->name);

	for (item = grp->items; item->name; item++)
		consumed += pops->add_member(grp, item, headers);

	pops->close_struct();

	return consumed;
}

static const struct print_operations json_pops = {
	.open_struct = json_open_struct,
	.close_struct = json_close_struct,
	.add_member = json_add_member,
	.print_group = print_group,
};

static const struct print_operations stdout_pops = {
	.open_struct = stdout_open_struct,
	.close_struct = stdout_close_struct,
	.add_member = stdout_add_member,
	.print_group = print_group,
};

static const struct print_operations term_pops = {
	.open_struct = term_open_struct,
	.close_struct = term_close_struct,
	.add_member = term_add_member,
	.print_group = term_print_group,
};

static bool print_groups(struct cnt_group **groups)
{
	unsigned int headers = stdout_lines % STDOUT_HEADER_REPEAT + 1;
	bool print_data = true;

	if (output_mode == STDOUT && (headers == 1 || headers == 2)) {
		for (struct cnt_group **grp = groups; *grp; grp++)
			print_data = pops->print_group(*grp, headers);
	}

	for (struct cnt_group **grp = groups; print_data && *grp; grp++)
		pops->print_group(*grp, false);

	return print_data;
}

static int
print_header(struct engines *engines, double t,
	     int lines, int con_w, int con_h, bool *consumed)
{
	struct pmu_counter fake_pmu = {
		.present = true,
		.val.cur = 1,
	};
	struct cnt_item period_items[] = {
		{ &fake_pmu, 0, 0, 1.0, 1.0, t * 1e3, "duration" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit", "ms" },
		{ },
	};
	struct cnt_group period_group = {
		.name = "period",
		.items = period_items,
	};
	struct cnt_item freq_items[] = {
		{ &engines->freq_req, 4, 0, 1.0, t, 1, "requested", "req" },
		{ &engines->freq_act, 4, 0, 1.0, t, 1, "actual", "act" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit", "MHz" },
		{ },
	};
	struct cnt_group freq_group = {
		.name = "frequency",
		.display_name = "Freq MHz",
		.items = freq_items,
	};
	struct cnt_item irq_items[] = {
		{ &engines->irq, 8, 0, 1.0, t, 1, "count", "/s" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit", "irq/s" },
		{ },
	};
	struct cnt_group irq_group = {
		.name = "interrupts",
		.display_name = "IRQ",
		.items = irq_items,
	};
	struct cnt_item rc6_items[] = {
		{ &engines->rc6, 3, 0, 1e9, t, 100, "value", "%" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit", "%" },
		{ },
	};
	struct cnt_group rc6_group = {
		.name = "rc6",
		.display_name = "RC6",
		.items = rc6_items,
	};
	struct cnt_item power_items[] = {
		{ &engines->rapl, 4, 2, 1.0, t, engines->rapl_scale, "value",
		  "W" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit", "W" },
		{ },
	};
	struct cnt_group power_group = {
		.name = "power",
		.display_name = "Power",
		.items = power_items,
	};
	struct cnt_group *groups[] = {
		&period_group,
		&freq_group,
		&irq_group,
		&rc6_group,
		&power_group,
		NULL
	};

	if (output_mode != JSON)
		memmove(&groups[0], &groups[1],
			sizeof(groups) - sizeof(groups[0]));

	pops->open_struct(NULL);

	*consumed = print_groups(groups);

	if (output_mode == INTERACTIVE) {
		printf("\033[H\033[J");

		if (lines++ < con_h)
			printf("intel-gpu-top - %s/%s MHz;  %s%% RC6; %s %s; %s irqs/s\n",
			       freq_items[1].buf, freq_items[0].buf,
			       rc6_items[0].buf, power_items[0].buf,
			       engines->rapl_unit,
			       irq_items[0].buf);

		if (lines++ < con_h)
			printf("\n");
	}

	return lines;
}

static int
print_imc(struct engines *engines, double t, int lines, int con_w, int con_h)
{
	struct cnt_item imc_items[] = {
		{ &engines->imc_reads, 6, 0, 1.0, t, engines->imc_reads_scale,
		  "reads", "rd" },
		{ &engines->imc_writes, 6, 0, 1.0, t, engines->imc_writes_scale,
		  "writes", "wr" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit" },
		{ },
	};
	struct cnt_group imc_group = {
		.name = "imc-bandwidth",
		.items = imc_items,
	};
	struct cnt_group *groups[] = {
		&imc_group,
		NULL
	};
	int ret;

	ret = asprintf((char **)&imc_group.display_name, "IMC %s/s",
			engines->imc_reads_unit);
	assert(ret >= 0);

	ret = asprintf((char **)&imc_items[2].unit, "%s/s",
			engines->imc_reads_unit);
	assert(ret >= 0);

	print_groups(groups);

	free((void *)imc_group.display_name);
	free((void *)imc_items[2].unit);

	if (output_mode == INTERACTIVE) {
		if (lines++ < con_h)
			printf("      IMC reads:   %s %s/s\n",
			       imc_items[0].buf, engines->imc_reads_unit);

		if (lines++ < con_h)
			printf("     IMC writes:   %s %s/s\n",
			       imc_items[1].buf, engines->imc_writes_unit);

		if (lines++ < con_h)
			printf("\n");
	}

	return lines;
}

static int
print_engines_header(struct engines *engines, double t,
		     int lines, int con_w, int con_h)
{
	for (unsigned int i = 0;
	     i < engines->num_engines && lines < con_h;
	     i++) {
		struct engine *engine = engine_ptr(engines, i);

		if (!engine->num_counters)
			continue;

		pops->open_struct("engines");

		if (output_mode == INTERACTIVE) {
			const char *a = "          ENGINE      BUSY ";
			const char *b = " MI_SEMA MI_WAIT";

			printf("\033[7m%s%*s%s\033[0m\n",
			       a, (int)(con_w - 1 - strlen(a) - strlen(b)),
			       " ", b);

			lines++;
		}

		break;
	}

	return lines;
}

static int
print_engine(struct engines *engines, unsigned int i, double t,
	     int lines, int con_w, int con_h)
{
	struct engine *engine = engine_ptr(engines, i);
	struct cnt_item engine_items[] = {
		{ &engine->busy, 6, 2, 1e9, t, 100, "busy", "%" },
		{ &engine->sema, 3, 0, 1e9, t, 100, "sema", "se" },
		{ &engine->wait, 3, 0, 1e9, t, 100, "wait", "wa" },
		{ NULL, 0, 0, 0.0, 0.0, 0.0, "unit", "%" },
		{ },
	};
	struct cnt_group engine_group = {
		.name = engine->display_name,
		.display_name = engine->short_name,
		.items = engine_items,
	};
	struct cnt_group *groups[] = {
		&engine_group,
		NULL
	};

	if (!engine->num_counters)
		return lines;

	print_groups(groups);

	if (output_mode == INTERACTIVE) {
		unsigned int max_w = con_w - 1;
		unsigned int len;
		char buf[128];
		double val;

		len = snprintf(buf, sizeof(buf), "    %s%%    %s%%",
			       engine_items[1].buf, engine_items[2].buf);

		len += printf("%16s %s%% ",
			      engine->display_name, engine_items[0].buf);

		val = pmu_calc(&engine->busy.val, 1e9, t, 100);
		print_percentage_bar(val, max_w - len);

		printf("%s\n", buf);

		lines++;
	}

	return lines;
}

static int
print_engines_footer(struct engines *engines, double t,
		     int lines, int con_w, int con_h)
{
	pops->close_struct();
	pops->close_struct();

	if (output_mode == INTERACTIVE) {
		if (lines++ < con_h)
			printf("\n");
	}

	return lines;
}

static bool stop_top;

static void sigint_handler(int  sig)
{
	stop_top = true;
}

int main(int argc, char **argv)
{
	unsigned int period_us = DEFAULT_PERIOD_MS * 1000;
	int con_w = -1, con_h = -1;
	char *output_path = NULL;
	struct engines *engines;
	unsigned int i;
	int ret, ch;

	/* Parse options */
	while ((ch = getopt(argc, argv, "o:s:Jlh")) != -1) {
		switch (ch) {
		case 'o':
			output_path = optarg;
			break;
		case 's':
			period_us = atoi(optarg) * 1000;
			break;
		case 'J':
			output_mode = JSON;
			break;
		case 'l':
			output_mode = STDOUT;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		default:
			fprintf(stderr, "Invalid option %c!\n", (char)optopt);
			usage(argv[0]);
			exit(1);
		}
	}

	if (output_mode == INTERACTIVE && (output_path || isatty(1) != 1))
		output_mode = STDOUT;

	if (output_path && strcmp(output_path, "-")) {
		out = fopen(output_path, "w");

		if (!out) {
			fprintf(stderr, "Failed to open output file - '%s'!\n",
				strerror(errno));
			exit(1);
		}
	} else {
		out = stdout;
	}

	if (output_mode != INTERACTIVE) {
		sighandler_t sig = signal(SIGINT, sigint_handler);

		if (sig == SIG_ERR)
			fprintf(stderr, "Failed to install signal handler!\n");
	}

	switch (output_mode) {
	case INTERACTIVE:
		pops = &term_pops;
		break;
	case STDOUT:
		pops = &stdout_pops;
		break;
	case JSON:
		pops = &json_pops;
		break;
	default:
		assert(0);
		break;
	};

	engines = discover_engines();
	if (!engines) {
		fprintf(stderr,
			"Failed to detect engines! (%s)\n(Kernel 4.16 or newer is required for i915 PMU support.)\n",
			strerror(errno));
		return 1;
	}

	ret = pmu_init(engines);
	if (ret) {
		fprintf(stderr,
			"Failed to initialize PMU! (%s)\n", strerror(errno));
		return 1;
	}

	pmu_sample(engines);

	while (!stop_top) {
		bool consumed = false;
		int lines = 0;
		struct winsize ws;
		double t;

		/* Update terminal size. */
		if (output_mode != INTERACTIVE) {
			con_w = con_h = INT_MAX;
		} else if (ioctl(0, TIOCGWINSZ, &ws) != -1) {
			con_w = ws.ws_col;
			con_h = ws.ws_row;
		}

		pmu_sample(engines);
		t = (double)(engines->ts.cur - engines->ts.prev) / 1e9;

		if (stop_top)
			break;

		while (!consumed) {
			lines = print_header(engines, t, lines, con_w, con_h,
					     &consumed);

			if (engines->imc_fd)
				lines = print_imc(engines, t, lines, con_w,
						  con_h);

			lines = print_engines_header(engines, t, lines, con_w,
						     con_h);

			for (i = 0;
			     i < engines->num_engines && lines < con_h;
			     i++)
				lines = print_engine(engines, i, t, lines,
						     con_w, con_h);

			lines = print_engines_footer(engines, t, lines, con_w,
						     con_h);
		}

		if (stop_top)
			break;

		usleep(period_us);
	}

	return 0;
}
