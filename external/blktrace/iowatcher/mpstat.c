/*
 * Copyright (C) 2012 Fusion-io
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
#include "mpstat.h"

char line[1024];
int line_len = 1024;

static char record_header[] = "CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle\n";
static char record_header_v2[] = "CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest  %gnice   %idle\n";

int record_header_len = sizeof(record_header);
int record_header_v2_len = sizeof(record_header_v2);

static int past_eof(struct trace *trace, char *cur)
{
	if (cur >= trace->mpstat_start + trace->mpstat_len)
		return 1;
	return 0;
}

int next_mpstat_line(struct trace *trace)
{
	char *next;
	char *cur = trace->mpstat_cur;

	next = strchr(cur, '\n');
	if (!next)
		return 1;
	next++;
	if (past_eof(trace, next))
		return 1;
	trace->mpstat_cur = next;
	return 0;
}

char *next_mpstat(struct trace *trace)
{
	char *cur;

	cur = strstr(trace->mpstat_cur, record_header);
	if (cur) {
		cur += record_header_len;
	} else {
		cur = strstr(trace->mpstat_cur, record_header_v2);
		if (cur)
			cur += record_header_v2_len;
	}
	if (!cur)
		return NULL;

	if (past_eof(trace, cur))
		return NULL;
	trace->mpstat_cur = cur;
	return cur;
}

char *first_mpstat(struct trace *trace)
{
	char *cur = trace->mpstat_cur;

	trace->mpstat_cur = trace->mpstat_start;

	cur = next_mpstat(trace);
	if (!cur)
		return NULL;
	return cur;
}

static void find_last_mpstat_time(struct trace *trace)
{
	int num_mpstats = 0;
	char *cur;

	first_mpstat(trace);

	cur = first_mpstat(trace);
	while (cur) {
		num_mpstats++;
		cur = next_mpstat(trace);
	}
	first_mpstat(trace);
	trace->mpstat_seconds = num_mpstats;
}

static int guess_mpstat_cpus(struct trace *trace)
{
	char *cur;
	int ret;
	int count = 0;

	cur = first_mpstat(trace);
	if (!cur)
		return 0;

	while (1) {
		ret = next_mpstat_line(trace);
		if (ret)
			break;

		cur = trace->mpstat_cur;
		count++;

		if (!cur)
			break;

		if (cur[0] == '\n')
			break;
	}
	trace->mpstat_num_cpus = count - 1;
	return 0;
}

static int count_mpstat_cpus(struct trace *trace)
{
	char *cur = trace->mpstat_start;
	char *cpu;
	char *record;
	int len; char *line;

	first_mpstat(trace);
	cpu = strstr(cur, " CPU)");
	if (!cpu)
		return guess_mpstat_cpus(trace);

	line = strndup(cur, cpu - cur);

	record = strrchr(line, '(');
	if (!record) {
		free(line);
		return 0;
	}
	record++;

	len = line + strlen(line) - record;

	cur = strndup(record, len);
	trace->mpstat_num_cpus = atoi(cur);
	first_mpstat(trace);
	free(line);

	return trace->mpstat_num_cpus;
}

static char *guess_filename(char *trace_name)
{
	struct stat st;
	int ret;
	char *cur;
	char *tmp;

	snprintf(line, line_len, "%s.mpstat", trace_name);
	ret = stat(line, &st);
	if (ret == 0)
		return trace_name;

	cur = strrchr(trace_name, '.');
	if (!cur) {
		return trace_name;
	}

	tmp = strndup(trace_name, cur - trace_name);
	snprintf(line, line_len, "%s.mpstat", tmp);
	ret = stat(line, &st);
	if (ret == 0)
		return tmp;

	free(tmp);
	return trace_name;
}

int read_mpstat(struct trace *trace, char *trace_name)
{
	int fd;
	struct stat st;
	int ret;
	char *p;

	if (record_header_len == 0) {
		record_header_len = strlen(record_header);
	}

	snprintf(line, line_len, "%s.mpstat", guess_filename(trace_name));
	fd = open(line, O_RDONLY);
	if (fd < 0)
		return 0;

	ret = fstat(fd, &st);
	if (ret < 0) {
		fprintf(stderr, "stat failed on %s err %s\n", line, strerror(errno));
		goto fail_fd;
	}
	p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED) {
		fprintf(stderr, "Unable to mmap trace file %s, err %s\n", line, strerror(errno));
		goto fail_fd;
	}
	trace->mpstat_start = p;
	trace->mpstat_len = st.st_size;
	trace->mpstat_cur = p;
	trace->mpstat_fd = fd;
	find_last_mpstat_time(trace);
	count_mpstat_cpus(trace);

	first_mpstat(trace);

	return 0;

fail_fd:
	close(fd);
	return 0;
}

/*
 * 09:56:26 AM  CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
 *
 * or
 *
 * 10:18:51 AM  CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest  %gnice   %idle
 *
 *
 * this reads just one line in the mpstat
 */
int read_mpstat_event(struct trace *trace, double *user,
		      double *sys, double *iowait, double *irq,
		      double *soft)
{
	char *cur = trace->mpstat_cur;
	char *nptr;
	double val;

	/* jump past the date and CPU number */
	cur += 16;
	if (past_eof(trace, cur))
		return 1;

	/* usr time */
	val = strtod(cur, &nptr);
	if (val == 0 && cur == nptr)
		return 1;
	*user = val;

	/* nice time, pitch this one */
	cur = nptr;
	val = strtod(cur, &nptr);
	if (val == 0 && cur == nptr)
		return 1;

	/* system time */
	cur = nptr;
	val = strtod(cur, &nptr);
	if (val == 0 && cur == nptr)
		return 1;
	*sys = val;

	cur = nptr;
	val = strtod(cur, &nptr);
	if (val == 0 && cur == nptr)
		return 1;
	*iowait = val;

	cur = nptr;
	val = strtod(cur, &nptr);
	if (val == 0 && cur == nptr)
		return 1;
	*irq = val;

	cur = nptr;
	val = strtod(cur, &nptr);
	if (val == 0 && cur == nptr)
		return 1;
	*soft = val;

	return 0;
}

int add_mpstat_gld(int time, double sys, struct graph_line_data *gld)
{
	gld->data[time].sum = sys;
	gld->data[time].count = 1;
	return 0;

}
