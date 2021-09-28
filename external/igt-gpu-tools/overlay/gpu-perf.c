/*
 * Copyright © 2013 Intel Corporation
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

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "igt_perf.h"

#include "gpu-perf.h"
#include "debugfs.h"

#if defined(__i386__)
#define rmb()           asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define wmb()           asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#endif

#if defined(__x86_64__)
#define rmb()           asm volatile("lfence" ::: "memory")
#define wmb()           asm volatile("sfence" ::: "memory")
#endif

#define N_PAGES 32

struct sample_event {
	struct perf_event_header header;
	uint32_t pid, tid;
	uint64_t time;
	uint64_t id;
	uint32_t raw_size;
	uint8_t tracepoint_data[0];
};

enum {
	TP_GEM_REQUEST_ADD,
	TP_GEM_REQUEST_WAIT_BEGIN,
	TP_GEM_REQUEST_WAIT_END,
	TP_FLIP_COMPLETE,
	TP_GEM_RING_SYNC_TO,
	TP_GEM_RING_SWITCH_CONTEXT,

	TP_NB
};

struct tracepoint {
	const char *name;
	int event_id;

	struct {
		char name[128];
		int offset;
		int size;
		int is_signed;
	} fields[20];
	int n_fields;

	int device_field;
	int ctx_field;
	int class_field;
	int instance_field;
	int seqno_field;
	int global_seqno_field;
	int plane_field;
} tracepoints[TP_NB] = {
	[TP_GEM_REQUEST_ADD]         = { .name = "i915/i915_request_add", },
	[TP_GEM_REQUEST_WAIT_BEGIN]  = { .name = "i915/i915_request_wait_begin", },
	[TP_GEM_REQUEST_WAIT_END]    = { .name = "i915/i915_request_wait_end", },
	[TP_FLIP_COMPLETE]           = { .name = "i915/flip_complete", },
	[TP_GEM_RING_SYNC_TO]        = { .name = "i915/gem_ring_sync_to", },
	[TP_GEM_RING_SWITCH_CONTEXT] = { .name = "i915/gem_ring_switch_context", },
};

union parser_value {
    char *string;
    int integer;
};

struct parser_ctx {
	struct tracepoint *tp;
	FILE *fp;
};

#define YY_CTX_LOCAL
#define YY_CTX_MEMBERS struct parser_ctx ctx;
#define YYSTYPE union parser_value
#define YY_LOCAL(T) static __attribute__((unused)) T
#define YY_PARSE(T) static T
#define YY_INPUT(yy, buf, result, max)				\
	{							\
		int c = getc(yy->ctx.fp);			\
		result = (EOF == c) ? 0 : (*(buf)= c, 1);	\
		if (EOF != c) {					\
			yyprintf((stderr, "<%c>", c));		\
		}						\
	}

#include "tracepoint_format.h"

static int
tracepoint_id(int tp_id)
{
	struct tracepoint *tp = &tracepoints[tp_id];
	yycontext ctx;
	char buf[1024];

	/* Already parsed? */
	if (tp->event_id != 0)
		return tp->event_id;

	snprintf(buf, sizeof(buf), "%s/tracing/events/%s/format",
		 debugfs_path, tp->name);

	memset(&ctx, 0, sizeof(ctx));
	ctx.ctx.tp = tp;
	ctx.ctx.fp = fopen(buf, "r");

	if (ctx.ctx.fp == NULL)
		return 0;

	if (yyparse(&ctx)) {
		for (int f = 0; f < tp->n_fields; f++) {
			if (!strcmp(tp->fields[f].name, "device")) {
				tp->device_field = f;
			} else if (!strcmp(tp->fields[f].name, "ctx")) {
				tp->ctx_field = f;
			} else if (!strcmp(tp->fields[f].name, "class")) {
				tp->class_field = f;
			} else if (!strcmp(tp->fields[f].name, "instance")) {
				tp->instance_field = f;
			} else if (!strcmp(tp->fields[f].name, "seqno")) {
				tp->seqno_field = f;
			} else if (!strcmp(tp->fields[f].name, "global_seqno")) {
				tp->global_seqno_field = f;
			} else if (!strcmp(tp->fields[f].name, "plane")) {
				tp->plane_field = f;
			}
		}
	} else
		tp->event_id = tp->n_fields = 0;

	yyrelease(&ctx);
	fclose(ctx.ctx.fp);

	return tp->event_id;
}

#define READ_TP_FIELD_U32(sample, tp_id, field_name)			\
	(*(const uint32_t *)((sample)->tracepoint_data +		\
			     tracepoints[tp_id].fields[			\
				     tracepoints[tp_id].field_name##_field].offset))

#define READ_TP_FIELD_U16(sample, tp_id, field_name)			\
	(*(const uint16_t *)((sample)->tracepoint_data +		\
			     tracepoints[tp_id].fields[			\
				     tracepoints[tp_id].field_name##_field].offset))

#define GET_RING_ID(sample, tp_id) \
({ \
	unsigned char class, instance, ring; \
\
	class = READ_TP_FIELD_U16(sample, tp_id, class); \
	instance = READ_TP_FIELD_U16(sample, tp_id, instance); \
\
	assert(class <= I915_ENGINE_CLASS_VIDEO_ENHANCE); \
	assert(instance <= 4); \
\
	ring = class * 4 + instance; \
\
	ring; \
})

static int perf_tracepoint_open(struct gpu_perf *gp, int tp_id,
				int (*func)(struct gpu_perf *, const void *))
{
	struct perf_event_attr attr;
	struct gpu_perf_sample *sample;
	int n, *fd;

	memset(&attr, 0, sizeof (attr));

	attr.type = PERF_TYPE_TRACEPOINT;
	attr.config = tracepoint_id(tp_id);
	if (attr.config == 0)
		return ENOENT;

	attr.sample_period = 1;
	attr.sample_type = (PERF_SAMPLE_TIME | PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_TID | PERF_SAMPLE_RAW);
	attr.read_format = PERF_FORMAT_ID;

	attr.exclude_guest = 1;

	n = gp->nr_cpus * (gp->nr_events+1);
	fd = realloc(gp->fd, n*sizeof(int));
	sample = realloc(gp->sample, n*sizeof(*gp->sample));
	if (fd == NULL || sample == NULL)
		return ENOMEM;
	gp->fd = fd;
	gp->sample = sample;

	fd += gp->nr_events * gp->nr_cpus;
	sample += gp->nr_events * gp->nr_cpus;
	for (n = 0; n < gp->nr_cpus; n++) {
		uint64_t track[2];

		fd[n] = perf_event_open(&attr, -1, n, -1, 0);
		if (fd[n] == -1)
			return errno;

		/* read back the event to establish id->tracepoint */
		if (read(fd[n], track, sizeof(track)) < 0)
			return errno;
		sample[n].id = track[1];
		sample[n].func = func;
	}

	gp->nr_events++;
	return 0;
}

static int perf_mmap(struct gpu_perf *gp)
{
	int size = (1 + N_PAGES) * gp->page_size;
	int *fd, i, j;

	gp->map = malloc(sizeof(void *)*gp->nr_cpus);
	if (gp->map == NULL)
		return ENOMEM;

	fd = gp->fd;
	for (j = 0; j < gp->nr_cpus; j++) {
		gp->map[j] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd++, 0);
		if (gp->map[j] == (void *)-1)
			goto err;
	}

	for (i = 1; i < gp->nr_events; i++) {
		for (j = 0; j < gp->nr_cpus; j++)
			ioctl(*fd++, PERF_EVENT_IOC_SET_OUTPUT, gp->fd[j]);
	}

	return 0;

err:
	while (--j > 0)
		munmap(gp->map[j], size);
	free(gp->map);
	gp->map = NULL;
	return EINVAL;
}

static int get_comm(pid_t pid, char *comm, int len)
{
	char filename[1024];
	int fd;

	*comm = '\0';
	snprintf(filename, sizeof(filename), "/proc/%d/comm", pid);

	fd = open(filename, 0);
	if (fd >= 0) {
		len = read(fd, comm, len-1);
		if (len >= 0)
			comm[len-1] = '\0';
		close(fd);
	} else
		len = -1;

	return len;
}

static struct gpu_perf_comm *
lookup_comm(struct gpu_perf *gp, pid_t pid)
{
	struct gpu_perf_comm *comm;

	if (pid == 0)
		return NULL;

	for (comm = gp->comm; comm != NULL; comm = comm->next) {
		if (comm->pid == pid)
			break;
	}
	if (comm == NULL) {
		comm = calloc(1, sizeof(*comm));
		if (comm == NULL)
			return NULL;

		if (get_comm(pid, comm->name, sizeof(comm->name)) < 0) {
			free(comm);
			return NULL;
		}

		comm->pid = pid;
		comm->next = gp->comm;
		gp->comm = comm;
	}

	return comm;
}

static int request_add(struct gpu_perf *gp, const void *event)
{
	const struct sample_event *sample = event;
	struct gpu_perf_comm *comm;

	comm = lookup_comm(gp, sample->pid);
	if (comm == NULL)
		return 0;

	comm->nr_requests[GET_RING_ID(sample, TP_GEM_REQUEST_ADD)]++;
	return 1;
}

static int flip_complete(struct gpu_perf *gp, const void *event)
{
	const struct sample_event *sample = event;

	gp->flip_complete[READ_TP_FIELD_U32(sample, TP_FLIP_COMPLETE, plane)]++;
	return 1;
}

static int ctx_switch(struct gpu_perf *gp, const void *event)
{
	const struct sample_event *sample = event;

	gp->ctx_switch[GET_RING_ID(sample, TP_GEM_RING_SWITCH_CONTEXT)]++;
	return 1;
}

static int ring_sync(struct gpu_perf *gp, const void *event)
{
	const struct sample_event *sample = event;
	struct gpu_perf_comm *comm;

	comm = lookup_comm(gp, sample->pid);
	if (comm == NULL)
		return 0;

	comm->nr_sema++;
	return 1;
}

static int wait_begin(struct gpu_perf *gp, const void *event)
{
	const struct sample_event *sample = event;
	struct gpu_perf_comm *comm;
	struct gpu_perf_time *wait;

	comm = lookup_comm(gp, sample->pid);
	if (comm == NULL)
		return 0;

	wait = malloc(sizeof(*wait));
	if (wait == NULL)
		return 0;

	/* XXX argument order CTX == ENGINE! */

	wait->comm = comm;
	wait->comm->active = true;
	wait->context = READ_TP_FIELD_U32(sample, TP_GEM_REQUEST_WAIT_BEGIN, ctx);
	wait->seqno = READ_TP_FIELD_U32(sample, TP_GEM_REQUEST_WAIT_BEGIN, seqno);
	wait->time = sample->time;
	wait->next = gp->wait[GET_RING_ID(sample, TP_GEM_REQUEST_WAIT_BEGIN)];
	gp->wait[GET_RING_ID(sample, TP_GEM_REQUEST_WAIT_BEGIN)] = wait;

	return 0;
}

static int wait_end(struct gpu_perf *gp, const void *event)
{
	const struct sample_event *sample = event;
	struct gpu_perf_time *wait, **prev;
	uint32_t engine = GET_RING_ID(sample, TP_GEM_REQUEST_WAIT_END);
	uint32_t context = READ_TP_FIELD_U32(sample, TP_GEM_REQUEST_WAIT_END, ctx);
	uint32_t seqno = READ_TP_FIELD_U32(sample, TP_GEM_REQUEST_WAIT_END, seqno);

	for (prev = &gp->wait[engine]; (wait = *prev) != NULL; prev = &wait->next) {
		if (wait->context != context || wait->seqno != seqno)
			continue;

		wait->comm->wait_time += sample->time - wait->time;
		wait->comm->active = false;

		*prev = wait->next;
		free(wait);
		return 1;
	}

	return 0;
}

void gpu_perf_init(struct gpu_perf *gp, unsigned flags)
{
	memset(gp, 0, sizeof(*gp));
	gp->nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	gp->page_size = getpagesize();

	perf_tracepoint_open(gp, TP_GEM_REQUEST_ADD, request_add);
	if (perf_tracepoint_open(gp, TP_GEM_REQUEST_WAIT_BEGIN, wait_begin) == 0)
		perf_tracepoint_open(gp, TP_GEM_REQUEST_WAIT_END, wait_end);
	perf_tracepoint_open(gp, TP_FLIP_COMPLETE, flip_complete);
	perf_tracepoint_open(gp, TP_GEM_RING_SYNC_TO, ring_sync);
	perf_tracepoint_open(gp, TP_GEM_RING_SWITCH_CONTEXT, ctx_switch);

	if (gp->nr_events == 0) {
		gp->error = "i915.ko tracepoints not available";
		return;
	}

	if (perf_mmap(gp))
		return;
}

static int process_sample(struct gpu_perf *gp, int cpu,
			  const struct perf_event_header *header)
{
	const struct sample_event *sample = (const struct sample_event *)header;
	int n, update = 0;

	/* hash me! */
	for (n = 0; n < gp->nr_events; n++) {
		int m = n * gp->nr_cpus + cpu;
		if (gp->sample[m].id != sample->id)
			continue;

		update = gp->sample[m].func(gp, sample);
		break;
	}

	return update;
}

int gpu_perf_update(struct gpu_perf *gp)
{
	const int size = N_PAGES * gp->page_size;
	const int mask = size - 1;
	uint8_t *buffer = NULL;
	int buffer_size = 0;
	int n, update = 0;

	if (gp->map == NULL)
		return 0;

	for (n = 0; n < gp->nr_cpus; n++) {
		struct perf_event_mmap_page *mmap = gp->map[n];
		const uint8_t *data;
		uint64_t head, tail;
		int wrap = 0;

		tail = mmap->data_tail;
		head = mmap->data_head;
		rmb();

		if (head < tail) {
			wrap = 1;
			tail &= mask;
			head &= mask;
			head += size;
		}

		data = (uint8_t *)mmap + gp->page_size;
		while (head - tail >= sizeof (struct perf_event_header)) {
			const struct perf_event_header *header;

			header = (const struct perf_event_header *)(data + (tail & mask));
			assert(header->size > 0);
			if (header->size > head - tail)
				break;

			if ((const uint8_t *)header + header->size > data + size) {
				int before;

				if (header->size > buffer_size) {
					uint8_t *b = realloc(buffer, header->size);
					if (b == NULL)
						break;

					buffer = b;
					buffer_size = header->size;
				}

				before = data + size - (const uint8_t *)header;

				memcpy(buffer, header, before);
				memcpy(buffer + before, data, header->size - before);

				header = (struct perf_event_header *)buffer;
			}

			if (header->type == PERF_RECORD_SAMPLE)
				update += process_sample(gp, n, header);
			tail += header->size;
		}

		if (wrap)
			tail &= mask;
		mmap->data_tail = tail;
		wmb();
	}

	free(buffer);
	return update;
}
