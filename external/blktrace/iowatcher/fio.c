/*
 * Copyright (C) 2013 Fusion-io
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License v2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>
#include <asm/types.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>

#include "plot.h"
#include "blkparse.h"
#include "list.h"
#include "tracers.h"
#include "fio.h"

static int past_eof(struct trace *trace, char *cur)
{
	if (cur >= trace->fio_start + trace->fio_len)
		return 1;
	return 0;
}

static int parse_fio_line(struct trace *trace, int *time, int *rate, int *dir, int *bs)
{
	char *cur = trace->fio_cur;
	char *p;
	int *res[] = { time, rate, dir, bs, NULL };
	int val;
	int i = 0;
	int *t;
	char *end = index(cur, '\n');
	char *tmp;

	if (!end)
		return 1;

	tmp = strndup(cur, end - cur);
	if (!tmp)
		return 1;
	p = strtok(tmp, ",");
	while (p && *res) {
		val = atoi(p);
		t = res[i++];
		*t = val;
		p = strtok(NULL, ",");
	}

	free(tmp);

	if (i < 3)
		return 1;
	return 0;
}

int next_fio_line(struct trace *trace)
{
	char *next;
	char *cur = trace->fio_cur;

	next = strchr(cur, '\n');
	if (!next)
		return 1;
	next++;
	if (past_eof(trace, next))
		return 1;
	trace->fio_cur = next;
	return 0;
}

char *first_fio(struct trace *trace)
{
	trace->fio_cur = trace->fio_start;
	return trace->fio_cur;
}

static void find_last_fio_time(struct trace *trace)
{
	double d;
	int time, rate, dir, bs;
	int ret;
	int last_time = 0;

	if (trace->fio_len == 0)
		return;

	first_fio(trace);
	while (1) {
		ret = parse_fio_line(trace, &time, &rate, &dir, &bs);
		if (ret)
			break;
		if (dir <= 1 && time > last_time)
			last_time = time;
		ret = next_fio_line(trace);
		if (ret)
			break;
	}
	d = (double)time / 1000;
	trace->fio_seconds = ceil(d);
	return;
}

static int read_fio(struct trace *trace, char *trace_name)
{
	int fd;
	struct stat st;
	int ret;
	char *p;

	fd = open(trace_name, O_RDONLY);
	if (fd < 0)
		return 1;

	ret = fstat(fd, &st);
	if (ret < 0) {
		fprintf(stderr, "stat failed on %s err %s\n",
			trace_name, strerror(errno));
		goto fail_fd;
	}
	p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED) {
		fprintf(stderr, "Unable to mmap trace file %s, err %s\n",
			trace_name, strerror(errno));
		goto fail_fd;
	}
	trace->fio_start = p;
	trace->fio_len = st.st_size;
	trace->fio_cur = p;
	trace->fio_fd = fd;
	find_last_fio_time(trace);
	first_fio(trace);
	return 0;

fail_fd:
	close(fd);
	return 1;
}

struct trace *open_fio_trace(char *path)
{
	int ret;
	struct trace *trace;

	trace = calloc(1, sizeof(*trace));
	if (!trace) {
		fprintf(stderr, "unable to allocate memory for trace\n");
		exit(1);
	}

	ret = read_fio(trace, path);
	if (ret) {
		free(trace);
		return NULL;
	}

	return trace;
}

int read_fio_event(struct trace *trace, int *time_ret, u64 *bw_ret, int *dir_ret)
{
	char *cur = trace->fio_cur;
	int time, rate, dir, bs;
	int ret;

	if (past_eof(trace, cur))
		return 1;

	ret = parse_fio_line(trace, &time, &rate, &dir, &bs);
	if (ret)
		return 1;

	time = floor((double)time / 1000);
	*time_ret = time;
	*bw_ret = (u64)rate * 1024;

	*dir_ret = dir;
	return 0;
}

int add_fio_gld(unsigned int time, u64 bw, struct graph_line_data *gld)
{
	double val;

	if (time > gld->max_seconds)
		return 0;

	gld->data[time].sum += bw;
	gld->data[time].count++;

	val = ((double)gld->data[time].sum) / gld->data[time].count;

	if (val > gld->max)
		gld->max = ceil(val);
	return 0;

}
