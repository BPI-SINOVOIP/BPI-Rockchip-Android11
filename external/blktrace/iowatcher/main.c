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
 *
 *  Parts of this file were imported from Jens Axboe's blktrace sources (also GPL)
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
#include <getopt.h>
#include <limits.h>
#include <float.h>
#include <signal.h>

#include "plot.h"
#include "blkparse.h"
#include "list.h"
#include "tracers.h"
#include "mpstat.h"
#include "fio.h"

LIST_HEAD(all_traces);
LIST_HEAD(fio_traces);

static char line[1024];
static int line_len = 1024;
static int found_mpstat = 0;
static int make_movie = 0;
static int keep_movie_svgs = 0;
static int opt_graph_width = 0;
static int opt_graph_height = 0;

static int columns = 1;
static int num_xticks = 9;
static int num_yticks = 4;

static double min_time = 0;
static double max_time = DBL_MAX;
static unsigned long long min_mb = 0;
static unsigned long long max_mb = ULLONG_MAX >> 20;

int plot_io_action = 0;
int io_per_process = 0;
unsigned int longest_proc_name = 0;

/*
 * this doesn't include the IO graph,
 * but it counts the other graphs as they go out
 */
static int total_graphs_written = 1;

enum {
	IO_GRAPH_INDEX = 0,
	TPUT_GRAPH_INDEX,
	FIO_GRAPH_INDEX,
	CPU_SYS_GRAPH_INDEX,
	CPU_IO_GRAPH_INDEX,
	CPU_IRQ_GRAPH_INDEX,
	CPU_SOFT_GRAPH_INDEX,
	CPU_USER_GRAPH_INDEX,
	LATENCY_GRAPH_INDEX,
	QUEUE_DEPTH_GRAPH_INDEX,
	IOPS_GRAPH_INDEX,
	TOTAL_GRAPHS
};

enum {
	MPSTAT_SYS = 0,
	MPSTAT_IRQ,
	MPSTAT_IO,
	MPSTAT_SOFT,
	MPSTAT_USER,
	MPSTAT_GRAPHS
};

static char *graphs_by_name[] = {
	"io",
	"tput",
	"fio",
	"cpu-sys",
	"cpu-io",
	"cpu-irq",
	"cpu-soft",
	"cpu-user",
	"latency",
	"queue-depth",
	"iops",
};

enum {
	MOVIE_SPINDLE,
	MOVIE_RECT,
	NUM_MOVIE_STYLES,
};

char *movie_styles[] = {
	"spindle",
	"rect",
	NULL
};

static int movie_style = 0;

static int lookup_movie_style(char *str)
{
	int i;

	for (i = 0; i < NUM_MOVIE_STYLES; i++) {
		if (strcmp(str, movie_styles[i]) == 0)
			return i;
	}
	return -1;
}

static int active_graphs[TOTAL_GRAPHS];
static int last_active_graph = IOPS_GRAPH_INDEX;

static int label_index = 0;
static int num_traces = 0;
static int num_fio_traces = 0;
static int longest_label = 0;

static char *graph_title = "";
static char *output_filename = "trace.svg";
static char *blktrace_devices[MAX_DEVICES_PER_TRACE];
static int num_blktrace_devices = 0;
static char *blktrace_outfile = "trace";
static char *blktrace_dest_dir = ".";
static char **prog_argv = NULL;
static int prog_argc = 0;
static char *ffmpeg_codec = "libx264";

static void alloc_mpstat_gld(struct trace_file *tf)
{
	struct graph_line_data **ptr;

	if (tf->trace->mpstat_num_cpus == 0)
		return;

	ptr = calloc((tf->trace->mpstat_num_cpus + 1) * MPSTAT_GRAPHS,
		     sizeof(struct graph_line_data *));
	if (!ptr) {
		perror("Unable to allocate mpstat arrays\n");
		exit(1);
	}
	tf->mpstat_gld = ptr;
}

static void enable_all_graphs(void)
{
	int i;
	for (i = 0; i < TOTAL_GRAPHS; i++)
		active_graphs[i] = 1;
}

static void disable_all_graphs(void)
{
	int i;
	for (i = 0; i < TOTAL_GRAPHS; i++)
		active_graphs[i] = 0;
}

static int enable_one_graph(char *name)
{
	int i;
	for (i = 0; i < TOTAL_GRAPHS; i++) {
		if (strcmp(name, graphs_by_name[i]) == 0) {
			active_graphs[i] = 1;
			return 0;
		}
	}
	return -ENOENT;
}

static int disable_one_graph(char *name)
{
	int i;
	for (i = 0; i < TOTAL_GRAPHS; i++) {
		if (strcmp(name, graphs_by_name[i]) == 0) {
			active_graphs[i] = 0;
			return 0;
		}
	}
	return -ENOENT;
}

static int last_graph(void)
{
	int i;
	for (i = TOTAL_GRAPHS - 1; i >= 0; i--) {
		if (active_graphs[i]) {
			return i;
		}
	}
	return -ENOENT;
}

static int graphs_left(int cur)
{
	int i;
	int left = 0;
	for (i = cur; i < TOTAL_GRAPHS; i++) {
		if (active_graphs[i])
			left++;
	}
	return left;
}

static char * join_path(char *dest_dir, char *filename)
{
	/* alloc 2 extra bytes for '/' and '\0' */
	char *path = malloc(strlen(dest_dir) + strlen(filename) + 2);
	sprintf(path, "%s%s%s", dest_dir, "/", filename);

	return path;
}

static void add_trace_file(char *filename)
{
	struct trace_file *tf;

	tf = calloc(1, sizeof(*tf));
	if (!tf) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(1);
	}
	tf->label = "";
	tf->filename = strdup(filename);
	list_add_tail(&tf->list, &all_traces);
	tf->line_color = "black";
	num_traces++;
}

static void add_fio_trace_file(char *filename)
{
	struct trace_file *tf;

	tf = calloc(1, sizeof(*tf));
	if (!tf) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(1);
	}
	tf->label = "";
	tf->filename = strdup(filename);
	list_add_tail(&tf->list, &fio_traces);
	tf->line_color = pick_fio_color();
	tf->fio_trace = 1;
	num_fio_traces++;
}

static void setup_trace_file_graphs(void)
{
	struct trace_file *tf;
	int i;
	int alloc_ptrs;

	if (io_per_process)
		alloc_ptrs = 16;
	else
		alloc_ptrs = 1;

	list_for_each_entry(tf, &all_traces, list) {
		tf->tput_reads_gld = alloc_line_data(tf->min_seconds, tf->max_seconds, tf->stop_seconds);
		tf->tput_writes_gld = alloc_line_data(tf->min_seconds, tf->max_seconds, tf->stop_seconds);
		tf->latency_gld = alloc_line_data(tf->min_seconds, tf->max_seconds, tf->stop_seconds);
		tf->queue_depth_gld = alloc_line_data(tf->min_seconds, tf->max_seconds, tf->stop_seconds);

		tf->iop_gld = alloc_line_data(tf->min_seconds, tf->max_seconds, tf->stop_seconds);
		tf->gdd_writes = calloc(alloc_ptrs, sizeof(struct graph_dot_data *));
		tf->gdd_reads = calloc(alloc_ptrs, sizeof(struct graph_dot_data *));
		tf->io_plots_allocated = alloc_ptrs;

		if (tf->trace->mpstat_num_cpus == 0)
			continue;

		alloc_mpstat_gld(tf);
		for (i = 0; i < (tf->trace->mpstat_num_cpus + 1) * MPSTAT_GRAPHS; i++) {
			tf->mpstat_gld[i] =
				alloc_line_data(tf->mpstat_min_seconds,
						tf->mpstat_max_seconds,
						tf->mpstat_max_seconds);
			tf->mpstat_gld[i]->max = 100;
		}
	}

	list_for_each_entry(tf, &fio_traces, list) {
		if (tf->trace->fio_seconds > 0) {
			tf->fio_gld = alloc_line_data(tf->min_seconds,
						      tf->max_seconds,
						      tf->stop_seconds);
		}
	}

}

static void read_traces(void)
{
	struct trace_file *tf;
	struct trace *trace;
	u64 last_time;
	u64 ymin;
	u64 ymax;
	u64 max_bank;
	u64 max_bank_offset;
	char *path = NULL;

	list_for_each_entry(tf, &all_traces, list) {
		if (num_blktrace_devices)
			path = join_path(blktrace_dest_dir, tf->filename);
		else
			path = strdup(tf->filename);

		trace = open_trace(path);
		if (!trace)
			exit(1);

		last_time = find_last_time(trace);
		tf->trace = trace;
		tf->max_seconds = SECONDS(last_time) + 1;
		tf->stop_seconds = SECONDS(last_time) + 1;

		find_extreme_offsets(trace, &tf->min_offset, &tf->max_offset,
				    &max_bank, &max_bank_offset);
		filter_outliers(trace, tf->min_offset, tf->max_offset, &ymin, &ymax);
		tf->min_offset = ymin;
		tf->max_offset = ymax;

		read_mpstat(trace, path);
		tf->mpstat_stop_seconds = trace->mpstat_seconds;
		tf->mpstat_max_seconds = trace->mpstat_seconds;
		if (tf->mpstat_max_seconds)
			found_mpstat = 1;

		free(path);
	}

	list_for_each_entry(tf, &fio_traces, list) {
		trace = open_fio_trace(tf->filename);
		if (!trace)
			exit(1);
		tf->trace = trace;
		tf->max_seconds = tf->trace->fio_seconds;
		tf->stop_seconds = tf->trace->fio_seconds;
	}
}

static void pick_line_graph_color(void)
{
	struct trace_file *tf;
	int i;

	list_for_each_entry(tf, &all_traces, list) {
		for (i = 0; i < tf->io_plots; i++) {
			if (tf->gdd_reads[i]) {
				tf->line_color = tf->gdd_reads[i]->color;
				tf->reads_color = tf->gdd_reads[i]->color;
			}
			if (tf->gdd_writes[i]) {
				tf->line_color = tf->gdd_writes[i]->color;
				tf->writes_color = tf->gdd_writes[i]->color;
			}
			if (tf->writes_color && tf->reads_color)
				break;
		}
		if (!tf->reads_color)
			tf->reads_color = tf->line_color;
		if (!tf->writes_color)
			tf->writes_color = tf->line_color;
	}
}

static void read_fio_events(struct trace_file *tf)
{
	u64 bw = 0;
	int time = 0;
	int dir = 0;
	int ret;

	first_fio(tf->trace);
	while (1) {
		ret = read_fio_event(tf->trace, &time, &bw, &dir);
		if (ret)
			break;

		if (dir <= 1)
			add_fio_gld(time, bw, tf->fio_gld);
		if (next_fio_line(tf->trace))
			break;
	}
}

static void read_trace_events(void)
{

	struct trace_file *tf;
	struct trace *trace;
	int ret;
	int i;
	unsigned int time;
	double user, sys, iowait, irq, soft;
	double max_user = 0, max_sys = 0, max_iowait = 0,
	       max_irq = 0, max_soft = 0;

	list_for_each_entry(tf, &fio_traces, list)
		read_fio_events(tf);

	list_for_each_entry(tf, &all_traces, list) {
		trace = tf->trace;

		first_record(trace);
		do {
			check_record(trace);
			if (SECONDS(get_record_time(trace)) > tf->max_seconds)
				continue;
			add_tput(trace, tf->tput_writes_gld, tf->tput_reads_gld);
			add_iop(trace, tf->iop_gld);
			add_io(trace, tf);
			add_pending_io(trace, tf->queue_depth_gld);
			add_completed_io(trace, tf->latency_gld);
		} while (!(ret = next_record(trace)));
	}
	list_for_each_entry(tf, &all_traces, list) {
		trace = tf->trace;

		if (trace->mpstat_num_cpus == 0)
			continue;

		first_mpstat(trace);

		for (time = 0; time < tf->mpstat_stop_seconds; time++) {
			for (i = 0; i < (trace->mpstat_num_cpus + 1) * MPSTAT_GRAPHS; i += MPSTAT_GRAPHS) {
				ret = read_mpstat_event(trace, &user, &sys,
							&iowait, &irq, &soft);
				if (ret)
					goto mpstat_done;
				if (next_mpstat_line(trace))
					goto mpstat_done;

				if (sys > max_sys)
					max_sys = sys;
				if (user > max_user)
					max_user = user;
				if (irq > max_irq)
					max_irq = irq;
				if (iowait > max_iowait)
					max_iowait = iowait;

				add_mpstat_gld(time, sys, tf->mpstat_gld[i + MPSTAT_SYS]);
				add_mpstat_gld(time, irq, tf->mpstat_gld[i + MPSTAT_IRQ]);
				add_mpstat_gld(time, soft, tf->mpstat_gld[i + MPSTAT_SOFT]);
				add_mpstat_gld(time, user, tf->mpstat_gld[i + MPSTAT_USER]);
				add_mpstat_gld(time, iowait, tf->mpstat_gld[i + MPSTAT_IO]);
			}
			if (next_mpstat(trace) == NULL)
				break;
		}
	}

mpstat_done:
	list_for_each_entry(tf, &all_traces, list) {
		trace = tf->trace;

		if (trace->mpstat_num_cpus == 0)
			continue;

		tf->mpstat_gld[MPSTAT_SYS]->max = max_sys;
		tf->mpstat_gld[MPSTAT_IRQ]->max = max_irq;
		tf->mpstat_gld[MPSTAT_SOFT]->max = max_soft;
		tf->mpstat_gld[MPSTAT_USER]->max = max_user;
		tf->mpstat_gld[MPSTAT_IO]->max = max_iowait;;
	}
	return;
}

static void set_trace_label(char *label)
{
	int cur = 0;
	struct trace_file *tf;
	int len = strlen(label);

	if (len > longest_label)
		longest_label = len;

	list_for_each_entry(tf, &all_traces, list) {
		if (cur == label_index) {
			tf->label = strdup(label);
			label_index++;
			return;
			break;
		}
		cur++;
	}
	list_for_each_entry(tf, &fio_traces, list) {
		if (cur == label_index) {
			tf->label = strdup(label);
			label_index++;
			break;
		}
		cur++;
	}
}

static void set_blktrace_outfile(char *arg)
{
	char *s = strdup(arg);
	char *last_dot = strrchr(s, '.');

	if (last_dot) {
		if (strcmp(last_dot, ".dump") == 0)
			*last_dot = '\0';
	}
	blktrace_outfile = s;
}


static void compare_minmax_tf(struct trace_file *tf, unsigned int *max_seconds,
			      u64 *min_offset, u64 *max_offset)
{
	if (tf->max_seconds > *max_seconds)
		*max_seconds = tf->max_seconds;
	if (tf->max_offset > *max_offset)
		*max_offset = tf->max_offset;
	if (tf->min_offset < *min_offset)
		*min_offset = tf->min_offset;
}

static void set_all_minmax_tf(unsigned int min_seconds,
			      unsigned int max_seconds,
			      u64 min_offset, u64 max_offset)
{
	struct trace_file *tf;
	struct list_head *traces = &all_traces;
again:
	list_for_each_entry(tf, traces, list) {
		tf->min_seconds = min_seconds;
		tf->max_seconds = max_seconds;
		if (tf->stop_seconds > max_seconds)
			tf->stop_seconds = max_seconds;
		if (tf->mpstat_max_seconds) {
			tf->mpstat_min_seconds = min_seconds;
			tf->mpstat_max_seconds = max_seconds;
			if (tf->mpstat_stop_seconds > max_seconds)
				tf->mpstat_stop_seconds = max_seconds;
		}
		tf->min_offset = min_offset;
		tf->max_offset = max_offset;
	}
	if (traces == &all_traces) {
		traces = &fio_traces;
		goto again;
	}
}

static struct pid_plot_history *alloc_pid_plot_history(char *color)
{
	struct pid_plot_history *pph;

	pph = calloc(1, sizeof(struct pid_plot_history));
	if (!pph) {
		perror("memory allocation failed");
		exit(1);
	}
	pph->history = malloc(sizeof(double) * 4096);
	if (!pph->history) {
		perror("memory allocation failed");
		exit(1);
	}
	pph->history_len = 4096;
	pph->color = color;

	return pph;
}

static struct plot_history *alloc_plot_history(struct trace_file *tf)
{
	struct plot_history *ph = calloc(1, sizeof(struct plot_history));
	int i;

	if (!ph) {
		perror("memory allocation failed");
		exit(1);
	}
	ph->read_pid_history = calloc(tf->io_plots, sizeof(struct pid_plot_history *));
	if (!ph->read_pid_history) {
		perror("memory allocation failed");
		exit(1);
	}
	ph->write_pid_history = calloc(tf->io_plots, sizeof(struct pid_plot_history *));
	if (!ph->write_pid_history) {
		perror("memory allocation failed");
		exit(1);
	}
	ph->pid_history_count = tf->io_plots;
	for (i = 0; i < tf->io_plots; i++) {
		if (tf->gdd_reads[i])
			ph->read_pid_history[i] = alloc_pid_plot_history(tf->gdd_reads[i]->color);
		if (tf->gdd_writes[i])
			ph->write_pid_history[i] = alloc_pid_plot_history(tf->gdd_writes[i]->color);
	}
	return ph;
}

LIST_HEAD(movie_history);
int num_histories = 0;

static void free_plot_history(struct plot_history *ph)
{
	int pid;

	for (pid = 0; pid < ph->pid_history_count; pid++) {
		if (ph->read_pid_history[pid])
			free(ph->read_pid_history[pid]);
		if (ph->write_pid_history[pid])
			free(ph->write_pid_history[pid]);
	}
	free(ph->read_pid_history);
	free(ph->write_pid_history);
	free(ph);
}

static void add_history(struct plot_history *ph, struct list_head *list)
{
	struct plot_history *entry;

	list_add_tail(&ph->list, list);
	num_histories++;

	if (num_histories > 12) {
		num_histories--;
		entry = list_entry(list->next, struct plot_history, list);
		list_del(&entry->list);
		free_plot_history(entry);
	}
}

static void plot_movie_history(struct plot *plot, struct list_head *list)
{
	struct plot_history *ph;
	int pid;

	if (num_histories > 2)
		rewind_spindle_steps(num_histories - 1);

	list_for_each_entry(ph, list, list) {
		for (pid = 0; pid < ph->pid_history_count; pid++) {
			if (ph->read_pid_history[pid]) {
				if (movie_style == MOVIE_SPINDLE) {
					svg_io_graph_movie_array_spindle(plot,
						ph->read_pid_history[pid]);
				} else {
					svg_io_graph_movie_array(plot,
						ph->read_pid_history[pid]);
				}
			}
			if (ph->write_pid_history[pid]) {
				if (movie_style == MOVIE_SPINDLE) {
					svg_io_graph_movie_array_spindle(plot,
						ph->write_pid_history[pid]);
				} else {
					svg_io_graph_movie_array(plot,
						ph->write_pid_history[pid]);
				}
			}
		}
	 }
}

static void free_all_plot_history(struct list_head *head)
{
	struct plot_history *entry;

	while (!list_empty(head)) {
		entry = list_entry(head->next, struct plot_history, list);
		list_del(&entry->list);
		free_plot_history(entry);
	}
}

static int count_io_plot_types(void)
{
	struct trace_file *tf;
	int i;
	int total_io_types = 0;

	list_for_each_entry(tf, &all_traces, list) {
		for (i = 0; i < tf->io_plots; i++) {
			if (tf->gdd_reads[i])
				total_io_types++;
			if (tf->gdd_writes[i])
				total_io_types++;
		}
	}
	return total_io_types;
}

static void plot_io_legend(struct plot *plot, struct graph_dot_data *gdd, char *prefix, char *rw)
{
	int ret = 0;
	char *label = NULL;
	if (io_per_process)
		ret = asprintf(&label, "%s %s", prefix, gdd->label);
	else
		ret = asprintf(&label, "%s", prefix);
	if (ret < 0) {
		perror("Failed to process labels");
		exit(1);
	}
	svg_add_legend(plot, label, rw, gdd->color);
	free(label);
}

static void plot_io(struct plot *plot, unsigned int min_seconds,
		    unsigned int max_seconds, u64 min_offset, u64 max_offset)
{
	struct trace_file *tf;
	int i;

	if (active_graphs[IO_GRAPH_INDEX] == 0)
		return;

	setup_axis(plot);

	svg_alloc_legend(plot, count_io_plot_types() * 2);

	set_plot_label(plot, "Device IO");
	set_ylabel(plot, "Offset (MB)");
	set_yticks(plot, num_yticks, min_offset / (1024 * 1024),
		   max_offset / (1024 * 1024), "");
	set_xticks(plot, num_xticks, min_seconds, max_seconds);

	list_for_each_entry(tf, &all_traces, list) {
		char *prefix = tf->label ? tf->label : "";

		for (i = 0; i < tf->io_plots; i++) {
			if (tf->gdd_writes[i]) {
				svg_io_graph(plot, tf->gdd_writes[i]);
				plot_io_legend(plot, tf->gdd_writes[i], prefix, " Writes");
			}
			if (tf->gdd_reads[i]) {
				svg_io_graph(plot, tf->gdd_reads[i]);
				plot_io_legend(plot, tf->gdd_reads[i], prefix, " Reads");
			}
		}
	}
	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");
	svg_write_legend(plot);
	close_plot(plot);
}

static void plot_tput(struct plot *plot, unsigned int min_seconds,
		      unsigned int max_seconds, int with_legend)
{
	struct trace_file *tf;
	char *units;
	char line[128];
	u64 max = 0, val;

	if (active_graphs[TPUT_GRAPH_INDEX] == 0)
		return;

	if (with_legend)
		svg_alloc_legend(plot, num_traces * 2);

	list_for_each_entry(tf, &all_traces, list) {
		val = line_graph_roll_avg_max(tf->tput_writes_gld);
		if (val > max)
			max = val;
		val = line_graph_roll_avg_max(tf->tput_reads_gld);
		if (val > max)
			max = val;
	}
	list_for_each_entry(tf, &all_traces, list) {
		if (tf->tput_writes_gld->max > 0)
			tf->tput_writes_gld->max = max;
		if (tf->tput_reads_gld->max > 0)
			tf->tput_reads_gld->max = max;
	}

	setup_axis(plot);
	set_plot_label(plot, "Throughput");

	tf = list_entry(all_traces.next, struct trace_file, list);

	scale_line_graph_bytes(&max, &units, 1024);
	sprintf(line, "%sB/s", units);
	set_ylabel(plot, line);
	set_yticks(plot, num_yticks, 0, max, "");
	set_xticks(plot, num_xticks, min_seconds, max_seconds);

	list_for_each_entry(tf, &all_traces, list) {
		if (tf->tput_writes_gld->max > 0) {
			svg_line_graph(plot, tf->tput_writes_gld, tf->writes_color, 0, 0);
			if (with_legend)
				svg_add_legend(plot, tf->label, " Writes", tf->writes_color);
		}
		if (tf->tput_reads_gld->max > 0) {
			svg_line_graph(plot, tf->tput_reads_gld, tf->reads_color, 0, 0);
			if (with_legend)
				svg_add_legend(plot, tf->label, " Reads", tf->reads_color);
		}
	}

	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");

	if (with_legend)
		svg_write_legend(plot);

	close_plot(plot);
	total_graphs_written++;
}

static void plot_fio_tput(struct plot *plot,
			  unsigned int min_seconds, unsigned int max_seconds)
{
	struct trace_file *tf;
	char *units;
	char line[128];
	u64 max = 0, val;

	if (num_fio_traces == 0 || active_graphs[FIO_GRAPH_INDEX] == 0)
		return;

	if (num_fio_traces > 1)
		svg_alloc_legend(plot, num_fio_traces);

	list_for_each_entry(tf, &fio_traces, list) {
		val = line_graph_roll_avg_max(tf->fio_gld);
		if (val > max)
			max = val;
	}

	list_for_each_entry(tf, &fio_traces, list) {
		if (tf->fio_gld->max > 0)
			tf->fio_gld->max = max;
	}

	setup_axis(plot);
	set_plot_label(plot, "Fio Throughput");

	tf = list_entry(all_traces.next, struct trace_file, list);

	scale_line_graph_bytes(&max, &units, 1024);
	sprintf(line, "%sB/s", units);
	set_ylabel(plot, line);
	set_yticks(plot, num_yticks, 0, max, "");

	set_xticks(plot, num_xticks, min_seconds, max_seconds);
	list_for_each_entry(tf, &fio_traces, list) {
		if (tf->fio_gld->max > 0) {
			svg_line_graph(plot, tf->fio_gld, tf->line_color, 0, 0);
			if (num_fio_traces > 1)
				svg_add_legend(plot, tf->label, "", tf->line_color);
		}
	}

	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");

	if (num_fio_traces > 1)
		svg_write_legend(plot);
	close_plot(plot);
	total_graphs_written++;
}

static void plot_cpu(struct plot *plot, unsigned int max_seconds, char *label,
		     int active_index, int gld_index)
{
	struct trace_file *tf;
	int max = 0;
	int i;
	unsigned int gld_i;
	char *color;
	double avg = 0;
	int ymax;
	int plotted = 0;

	if (active_graphs[active_index] == 0)
		return;

	list_for_each_entry(tf, &all_traces, list) {
		if (tf->trace->mpstat_num_cpus > max)
			max = tf->trace->mpstat_num_cpus;
	}
	if (max == 0)
		return;

	tf = list_entry(all_traces.next, struct trace_file, list);

	ymax = tf->mpstat_gld[gld_index]->max;
	if (ymax == 0)
		return;

	svg_alloc_legend(plot, num_traces * max);

	setup_axis(plot);
	set_plot_label(plot, label);

	max_seconds = tf->mpstat_max_seconds;

	set_yticks(plot, num_yticks, 0, tf->mpstat_gld[gld_index]->max, "");
	set_ylabel(plot, "Percent");
	set_xticks(plot, num_xticks, tf->mpstat_min_seconds, max_seconds);

	reset_cpu_color();
	list_for_each_entry(tf, &all_traces, list) {
		if (tf->mpstat_gld == 0)
			break;
		for (gld_i = tf->mpstat_gld[0]->min_seconds;
		     gld_i < tf->mpstat_gld[0]->stop_seconds; gld_i++) {
			if (tf->mpstat_gld[gld_index]->data[gld_i].count) {
				avg += (tf->mpstat_gld[gld_index]->data[gld_i].sum /
					tf->mpstat_gld[gld_index]->data[gld_i].count);
			}
		}
		avg /= tf->mpstat_gld[gld_index]->stop_seconds -
		       tf->mpstat_gld[gld_index]->min_seconds;
		color = pick_cpu_color();
		svg_line_graph(plot, tf->mpstat_gld[0], color, 0, 0);
		svg_add_legend(plot, tf->label, " avg", color);

		for (i = 1; i < tf->trace->mpstat_num_cpus + 1; i++) {
			struct graph_line_data *gld = tf->mpstat_gld[i * MPSTAT_GRAPHS + gld_index];
			double this_avg = 0;

			for (gld_i = gld->min_seconds;
			     gld_i < gld->stop_seconds; gld_i++) {
				if (gld->data[i].count) {
					this_avg += gld->data[i].sum /
						gld->data[i].count;
				}
			}

			this_avg /= gld->stop_seconds - gld->min_seconds;

			for (gld_i = gld->min_seconds;
			     gld_i < gld->stop_seconds; gld_i++) {
				double val;

				if (gld->data[gld_i].count == 0)
					continue;
				val = (double)gld->data[gld_i].sum /
					gld->data[gld_i].count;

				if (this_avg > avg + 30 || val > 95) {
					color = pick_cpu_color();
					svg_line_graph(plot, gld, color, avg + 30, 95);
					snprintf(line, line_len, " CPU %d\n", i - 1);
					svg_add_legend(plot, tf->label, line, color);
					plotted++;
					break;
				}

			}
		}
	}

	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");

	if (!plot->no_legend) {
		svg_write_legend(plot);
		svg_free_legend(plot);
	}
	close_plot(plot);
	total_graphs_written++;
}

static void plot_queue_depth(struct plot *plot, unsigned int min_seconds,
			     unsigned int max_seconds)
{
	struct trace_file *tf;
	u64 max = 0, val;

	if (active_graphs[QUEUE_DEPTH_GRAPH_INDEX] == 0)
		return;

	setup_axis(plot);
	set_plot_label(plot, "Queue Depth");
	if (num_traces > 1)
		svg_alloc_legend(plot, num_traces);

	list_for_each_entry(tf, &all_traces, list) {
		val = line_graph_roll_avg_max(tf->queue_depth_gld);
		if (val > max)
			max = val;
	}

	list_for_each_entry(tf, &all_traces, list)
		tf->queue_depth_gld->max = max;

	set_ylabel(plot, "Pending IO");
	set_yticks(plot, num_yticks, 0, max, "");
	set_xticks(plot, num_xticks, min_seconds, max_seconds);

	list_for_each_entry(tf, &all_traces, list) {
		svg_line_graph(plot, tf->queue_depth_gld, tf->line_color, 0, 0);
		if (num_traces > 1)
			svg_add_legend(plot, tf->label, "", tf->line_color);
	}

	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");
	if (num_traces > 1)
		svg_write_legend(plot);
	close_plot(plot);
	total_graphs_written++;
}

static void system_check(const char *cmd)
{
	if (system(cmd) < 0) {
		int err = errno;

		fprintf(stderr, "system exec failed (%d): %s\n", err, cmd);
		exit(1);
	}
}

static void convert_movie_files(char *movie_dir)
{
	fprintf(stderr, "Converting svg files in %s\n", movie_dir);
	snprintf(line, line_len, "find %s -name \\*.svg | xargs -I{} -n 1 -P 8 rsvg-convert -o {}.png {}",
		 movie_dir);
	system_check(line);
}

static void mencode_movie(char *movie_dir)
{
	fprintf(stderr, "Creating movie %s with ffmpeg\n", movie_dir);
	snprintf(line, line_len, "ffmpeg -r 20 -y -i %s/%%10d-%s.svg.png -b:v 250k "
		 "-vcodec %s %s", movie_dir, output_filename, ffmpeg_codec,
		 output_filename);
	system_check(line);
}

static void tencode_movie(char *movie_dir)
{
	fprintf(stderr, "Creating movie %s with png2theora\n", movie_dir);
	snprintf(line, line_len, "png2theora -o %s %s/%%010d-%s.svg.png",
			output_filename, movie_dir, output_filename);
	system_check(line);
}

static void encode_movie(char *movie_dir)
{
	char *last_dot = strrchr(output_filename, '.');

	if (last_dot &&
	    (!strncmp(last_dot, ".ogg", 4) || !strncmp(last_dot, ".ogv", 4))) {
		tencode_movie(movie_dir);
	} else
		mencode_movie(movie_dir);
}

static void cleanup_movie(char *movie_dir)
{
	if (keep_movie_svgs) {
		fprintf(stderr, "Keeping movie dir %s\n", movie_dir);
		return;
	}
	fprintf(stderr, "Removing movie dir %s\n", movie_dir);
	snprintf(line, line_len, "rm %s/*", movie_dir);
	system_check(line);

	snprintf(line, line_len, "rmdir %s", movie_dir);
	system_check(line);
}

static void plot_io_movie(struct plot *plot)
{
	struct trace_file *tf;
	int i, pid;
	struct plot_history *history;
	int batch_i;
	int movie_len = 30;
	int movie_frames_per_sec = 20;
	int total_frames = movie_len * movie_frames_per_sec;
	int rows, cols;
	int batch_count;
	int graph_width_factor = 5;
	int orig_y_offset;
	char movie_dir[] = "io-movie-XXXXXX";

	if (mkdtemp(movie_dir) == NULL) {
		perror("Unable to create temp directory for movie files");
		exit(1);
	}

	get_graph_size(&cols, &rows);
	batch_count = cols / total_frames;

	if (batch_count == 0)
		batch_count = 1;

	list_for_each_entry(tf, &all_traces, list) {
		char *prefix = tf->label ? tf->label : "";

		i = 0;
		while (i < cols) {
			snprintf(line, line_len, "%s/%010d-%s.svg", movie_dir, i, output_filename);
			set_plot_output(plot, line);
			set_plot_title(plot, graph_title);
			orig_y_offset = plot->start_y_offset;

			plot->no_legend = 1;

			set_graph_size(cols / graph_width_factor, rows / 8);
			plot->timeline = i / graph_width_factor;

			plot_tput(plot, tf->min_seconds, tf->max_seconds, 0);

			plot_cpu(plot, tf->max_seconds,
				   "CPU System Time", CPU_SYS_GRAPH_INDEX, MPSTAT_SYS);

			plot->direction = PLOT_ACROSS;
			plot_queue_depth(plot, tf->min_seconds, tf->max_seconds);

			/* movie graph starts here */
			plot->start_y_offset = orig_y_offset;
			set_graph_size(cols - cols / graph_width_factor, rows);
			plot->no_legend = 0;
			plot->timeline = 0;
			plot->direction = PLOT_DOWN;;

			if (movie_style == MOVIE_SPINDLE)
				setup_axis_spindle(plot);
			else
				setup_axis(plot);

			svg_alloc_legend(plot, count_io_plot_types() * 2);

			history = alloc_plot_history(tf);
			history->col = i;

			for (pid = 0; pid < tf->io_plots; pid++) {
				if (tf->gdd_reads[pid])
					plot_io_legend(plot, tf->gdd_reads[pid], prefix, " Reads");
				if (tf->gdd_writes[pid])
					plot_io_legend(plot, tf->gdd_writes[pid], prefix, " Writes");
			}

			batch_i = 0;
			while (i < cols && batch_i < batch_count) {
				for (pid = 0; pid < tf->io_plots; pid++) {
					if (tf->gdd_reads[pid]) {
						svg_io_graph_movie(tf->gdd_reads[pid],
								   history->read_pid_history[pid],
								   i);
					}
					if (tf->gdd_writes[pid]) {
						svg_io_graph_movie(tf->gdd_writes[pid],
								   history->write_pid_history[pid],
								   i);
					}
				}
				i++;
				batch_i++;
			}

			add_history(history, &movie_history);

			plot_movie_history(plot, &movie_history);

			svg_write_legend(plot);
			close_plot(plot);
			close_plot(plot);

			close_plot_file(plot);
		}
		free_all_plot_history(&movie_history);
	}
	convert_movie_files(movie_dir);
	encode_movie(movie_dir);
	cleanup_movie(movie_dir);
}

static void plot_latency(struct plot *plot, unsigned int min_seconds,
			 unsigned int max_seconds)
{
	struct trace_file *tf;
	char *units;
	char line[128];
	u64 max = 0, val;

	if (active_graphs[LATENCY_GRAPH_INDEX] == 0)
		return;

	if (num_traces > 1)
		svg_alloc_legend(plot, num_traces);

	list_for_each_entry(tf, &all_traces, list) {
		val = line_graph_roll_avg_max(tf->latency_gld);
		if (val > max)
			max = val;
	}

	list_for_each_entry(tf, &all_traces, list)
		tf->latency_gld->max = max;

	setup_axis(plot);
	set_plot_label(plot, "IO Latency");

	tf = list_entry(all_traces.next, struct trace_file, list);

	scale_line_graph_time(&max, &units);
	sprintf(line, "latency (%ss)", units);
	set_ylabel(plot, line);
	set_yticks(plot, num_yticks, 0, max, "");
	set_xticks(plot, num_xticks, min_seconds, max_seconds);

	list_for_each_entry(tf, &all_traces, list) {
		svg_line_graph(plot, tf->latency_gld, tf->line_color, 0, 0);
		if (num_traces > 1)
			svg_add_legend(plot, tf->label, "", tf->line_color);
	}

	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");
	if (num_traces > 1)
		svg_write_legend(plot);
	close_plot(plot);
	total_graphs_written++;
}

static void plot_iops(struct plot *plot, unsigned int min_seconds,
		      unsigned int max_seconds)
{
	struct trace_file *tf;
	char *units;
	u64 max = 0, val;

	if (active_graphs[IOPS_GRAPH_INDEX] == 0)
		return;

	list_for_each_entry(tf, &all_traces, list) {
		val = line_graph_roll_avg_max(tf->iop_gld);
		if (val > max)
			max = val;
	}

	list_for_each_entry(tf, &all_traces, list)
		tf->iop_gld->max = max;

	setup_axis(plot);
	set_plot_label(plot, "IOPs");
	if (num_traces > 1)
		svg_alloc_legend(plot, num_traces);

	tf = list_entry(all_traces.next, struct trace_file, list);

	scale_line_graph_bytes(&max, &units, 1000);
	set_ylabel(plot, "IO/s");

	set_yticks(plot, num_yticks, 0, max, units);
	set_xticks(plot, num_xticks, min_seconds, max_seconds);

	list_for_each_entry(tf, &all_traces, list) {
		svg_line_graph(plot, tf->iop_gld, tf->line_color, 0, 0);
		if (num_traces > 1)
			svg_add_legend(plot, tf->label, "", tf->line_color);
	}

	if (plot->add_xlabel)
		set_xlabel(plot, "Time (seconds)");
	if (num_traces > 1)
		svg_write_legend(plot);

	close_plot(plot);
	total_graphs_written++;
}

static void check_plot_columns(struct plot *plot, int index)
{
	int count;

	if (columns > 1 && (total_graphs_written == 0 ||
	    total_graphs_written % columns != 0)) {
		count = graphs_left(index);
		if (plot->direction == PLOT_DOWN) {
			plot->start_x_offset = 0;
			if (count <= columns)
				plot->add_xlabel = 1;
		}
		plot->direction = PLOT_ACROSS;

	} else {
		plot->direction = PLOT_DOWN;
		if (index == last_active_graph)
			plot->add_xlabel = 1;
	}

}

enum {
	HELP_LONG_OPT = 1,
};

char *option_string = "+F:T:t:o:l:r:O:N:d:D:pm::h:w:c:x:y:a:C:PK";
static struct option long_options[] = {
	{"columns", required_argument, 0, 'c'},
	{"fio-trace", required_argument, 0, 'F'},
	{"title", required_argument, 0, 'T'},
	{"trace", required_argument, 0, 't'},
	{"output", required_argument, 0, 'o'},
	{"label", required_argument, 0, 'l'},
	{"rolling", required_argument, 0, 'r'},
	{"no-graph", required_argument, 0, 'N'},
	{"only-graph", required_argument, 0, 'O'},
	{"device", required_argument, 0, 'd'},
	{"blktrace-destination", required_argument, 0, 'D'},
	{"prog", no_argument, 0, 'p'},
	{"movie", optional_argument, 0, 'm'},
	{"codec", optional_argument, 0, 'C'},
	{"keep-movie-svgs", no_argument, 0, 'K'},
	{"width", required_argument, 0, 'w'},
	{"height", required_argument, 0, 'h'},
	{"xzoom", required_argument, 0, 'x'},
	{"yzoom", required_argument, 0, 'y'},
	{"io-plot-action", required_argument, 0, 'a'},
	{"per-process-io", no_argument, 0, 'P'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "iowatcher usage:\n"
		"\t-d (--device): device for blktrace to trace\n"
		"\t-D (--blktrace-destination): destination for blktrace\n"
		"\t-t (--trace): trace file name (more than one allowed)\n"
		"\t-F (--fio-trace): fio bandwidth trace (more than one allowed)\n"
		"\t-l (--label): trace label in the graph\n"
		"\t-o (--output): output file name for the SVG image or video\n"
		"\t-p (--prog): run a program while blktrace is run\n"
		"\t-K (--keep-movie-svgs keep svgs generated for movie mode\n"
		"\t-m (--movie [=spindle|rect]): create IO animations\n"
		"\t-C (--codec): ffmpeg codec. Use ffmpeg -codecs to list\n"
		"\t-r (--rolling): number of seconds in the rolling averge\n"
		"\t-T (--title): graph title\n"
		"\t-N (--no-graph): skip a single graph (io, tput, latency, queue-depth, \n"
		"\t\t\tiops, cpu-sys, cpu-io, cpu-irq cpu-soft cpu-user)\n"
		"\t-O (--only-graph): add a single graph to the output\n"
		"\t-h (--height): set the height of each graph\n"
		"\t-w (--width): set the width of each graph\n"
		"\t-c (--columns): numbers of columns in graph output\n"
		"\t-x (--xzoom): limit processed time to min:max\n"
		"\t-y (--yzoom): limit processed sectors to min:max\n"
		"\t-a (--io-plot-action): plot given action (one of Q,D,C) in IO graph\n"
		"\t-P (--per-process-io): distinguish between processes in IO graph\n"
	       );
	exit(1);
}

static int parse_double_range(char *str, double *min, double *max)
{
	char *end;

	/* Empty lower bound - leave original value */
	if (str[0] != ':') {
		*min = strtod(str, &end);
		if (*min == HUGE_VAL || *min == -HUGE_VAL)
			return -ERANGE;
		if (*end != ':')
			return -EINVAL;
	} else
		end = str;
	/* Empty upper bound - leave original value */
	if (end[1]) {
		*max = strtod(end+1, &end);
		if (*max == HUGE_VAL || *max == -HUGE_VAL)
			return -ERANGE;
		if (*end != 0)
			return -EINVAL;
	}
	if (*min > *max)
		return -EINVAL;
	return 0;
}

static int parse_ull_range(char *str, unsigned long long *min,
			   unsigned long long *max)
{
	char *end;

	/* Empty lower bound - leave original value */
	if (str[0] != ':') {
		*min = strtoull(str, &end, 10);
		if (*min == ULLONG_MAX && errno == ERANGE)
			return -ERANGE;
		if (*end != ':')
			return -EINVAL;
	} else
		end = str;
	/* Empty upper bound - leave original value */
	if (end[1]) {
		*max = strtoull(end+1, &end, 10);
		if (*max == ULLONG_MAX && errno == ERANGE)
			return -ERANGE;
		if (*end != 0)
			return -EINVAL;
	}
	if (*min > *max)
		return -EINVAL;
	return 0;
}

static int parse_options(int ac, char **av)
{
	int c;
	int disabled = 0;
	int p_flagged = 0;

	while (1) {
		int option_index = 0;

		c = getopt_long(ac, av, option_string,
				long_options, &option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'T':
			graph_title = strdup(optarg);
			break;
		case 't':
			add_trace_file(optarg);
			set_blktrace_outfile(optarg);
			break;
		case 'F':
			add_fio_trace_file(optarg);
			break;
		case 'o':
			output_filename = strdup(optarg);
			break;
		case 'l':
			set_trace_label(optarg);
			break;
		case 'r':
			set_rolling_avg(atoi(optarg));
			break;
		case 'O':
			if (!disabled) {
				disable_all_graphs();
				disabled = 1;
			}
			enable_one_graph(optarg);
			break;
		case 'N':
			disable_one_graph(optarg);
			break;
		case 'd':
			if (num_blktrace_devices == MAX_DEVICES_PER_TRACE - 1) {
				fprintf(stderr, "Too many blktrace devices provided\n");
				exit(1);
			}
			blktrace_devices[num_blktrace_devices++] = strdup(optarg);
			break;
		case 'D':
			blktrace_dest_dir = strdup(optarg);
			if (!strcmp(blktrace_dest_dir, "")) {
				fprintf(stderr, "Need a directory\n");
				print_usage();
			}
			break;
		case 'p':
			p_flagged = 1;
			break;
		case 'K':
			keep_movie_svgs = 1;
			break;
		case 'm':
			make_movie = 1;
			if (optarg) {
				movie_style = lookup_movie_style(optarg);
				if (movie_style < 0) {
					fprintf(stderr, "Unknown movie style %s\n", optarg);
					print_usage();
				}
			}
			fprintf(stderr, "Using movie style: %s\n",
				movie_styles[movie_style]);
			break;
		case 'C':
			ffmpeg_codec = strdup(optarg);
			break;
		case 'h':
			opt_graph_height = atoi(optarg);
			break;
		case 'w':
			opt_graph_width = atoi(optarg);
			break;
		case 'c':
			columns = atoi(optarg);
			break;
		case 'x':
			if (parse_double_range(optarg, &min_time, &max_time)
			    < 0) {
				fprintf(stderr, "Cannot parse time range %s\n",
					optarg);
				exit(1);
			}
			break;
		case 'y':
			if (parse_ull_range(optarg, &min_mb, &max_mb)
			    < 0) {
				fprintf(stderr,
					"Cannot parse offset range %s\n",
					optarg);
				exit(1);
			}
			if (max_mb > ULLONG_MAX >> 20) {
				fprintf(stderr,
					"Upper range limit too big."
					" Maximum is %llu.\n", ULLONG_MAX >> 20);
				exit(1);
			}
			break;
		case 'a':
			if (strlen(optarg) != 1) {
action_err:
				fprintf(stderr, "Action must be one of Q, D, C.");
				exit(1);
			}
			plot_io_action = action_char_to_num(optarg[0]);
			if (plot_io_action < 0)
				goto action_err;
			break;
		case 'P':
			io_per_process = 1;
			break;
		case '?':
		case HELP_LONG_OPT:
			print_usage();
			break;
		default:
			break;
		}
	}

	if (optind < ac && p_flagged) {
		prog_argv = &av[optind];
		prog_argc = ac - optind;
	} else if (p_flagged) {
		fprintf(stderr, "--prog or -p given but no program specified\n");
		exit(1);
	} else if (optind < ac) {
		fprintf(stderr, "Extra arguments '%s'... (and --prog not specified)\n", av[optind]);
		exit(1);
	}

	return 0;
}

static void dest_mkdir(char *dir)
{
	int ret;

	ret = mkdir(dir, 0777);
	if (ret && errno != EEXIST) {
		fprintf(stderr, "failed to mkdir error %s\n", strerror(errno));
		exit(errno);
	}
}

int main(int ac, char **av)
{
	struct plot *plot;
	unsigned int min_seconds = 0;
	unsigned int max_seconds = 0;
	u64 max_offset = 0;
	u64 min_offset = ~(u64)0;
	struct trace_file *tf;
	int ret;
	int rows, cols;

	init_io_hash_table();
	init_process_hash_table();

	enable_all_graphs();

	parse_options(ac, av);

	last_active_graph = last_graph();
	if (make_movie) {
		set_io_graph_scale(256);
		if (movie_style == MOVIE_SPINDLE)
			set_graph_size(750, 550);
		else
			set_graph_size(700, 400);

		/*
		 * the plots in the movie don't have a seconds
		 * line yet, this makes us skip it
		 */
		last_active_graph = TOTAL_GRAPHS + 1;
	}
	if (opt_graph_height)
		set_graph_height(opt_graph_height);

	if (opt_graph_width)
		set_graph_width(opt_graph_width);

	if (list_empty(&all_traces) && list_empty(&fio_traces)) {
		fprintf(stderr, "No traces found, exiting\n");
		exit(1);
	}

	if (num_blktrace_devices) {
		char *path = join_path(blktrace_dest_dir, blktrace_outfile);
		dest_mkdir(blktrace_dest_dir);
		dest_mkdir(path);

		snprintf(line, line_len, "%s.dump", path);
		unlink(line);

		ret = start_blktrace(blktrace_devices, num_blktrace_devices,
				     blktrace_outfile, blktrace_dest_dir);
		if (ret) {
			perror("Exiting due to blktrace failure");
			exit(ret);
		}

		snprintf(line, line_len, "%s.mpstat", path);
		ret = start_mpstat(line);
		if (ret) {
			perror("Exiting due to mpstat failure");
			exit(ret);
		}

		if (prog_argv && prog_argc) {
			run_program(prog_argc, prog_argv, 1, NULL, NULL);
			wait_for_tracers(SIGINT);
		} else {
			printf("Tracing until interrupted...\n");
			wait_for_tracers(0);
		}
		free(path);
	}

	/* step one, read all the traces */
	read_traces();

	/* step two, find the maxes for time and offset */
	list_for_each_entry(tf, &all_traces, list)
		compare_minmax_tf(tf, &max_seconds, &min_offset, &max_offset);
	list_for_each_entry(tf, &fio_traces, list)
		compare_minmax_tf(tf, &max_seconds, &min_offset, &max_offset);
	min_seconds = min_time;
	if (max_seconds > max_time)
		max_seconds = ceil(max_time);
	if (min_offset < min_mb << 20)
		min_offset = min_mb << 20;
	if (max_offset > max_mb << 20)
		max_offset = max_mb << 20;

	/* push the max we found into all the tfs */
	set_all_minmax_tf(min_seconds, max_seconds, min_offset, max_offset);

	/* alloc graphing structs for all the traces */
	setup_trace_file_graphs();

	/* run through all the traces and read their events */
	read_trace_events();

	pick_line_graph_color();

	plot = alloc_plot();

	if (make_movie) {
		set_legend_width(longest_label + longest_proc_name + 1 + strlen("writes"));
		plot_io_movie(plot);
		exit(0);
	}

	set_plot_output(plot, output_filename);

	if (active_graphs[IO_GRAPH_INDEX] || found_mpstat)
		set_legend_width(longest_label + longest_proc_name + 1 + strlen("writes"));
	else if (num_traces >= 1 || num_fio_traces >= 1)
		set_legend_width(longest_label);
	else
		set_legend_width(0);

	get_graph_size(&cols, &rows);
	if (columns > 1)
		plot->add_xlabel = 1;
	set_plot_title(plot, graph_title);

	check_plot_columns(plot, IO_GRAPH_INDEX);
	plot_io(plot, min_seconds, max_seconds, min_offset, max_offset);
	plot->add_xlabel = 0;

	if (columns > 1) {
		set_graph_size(cols / columns, rows);
		num_xticks /= columns;
		if (num_xticks < 2)
			num_xticks = 2;
	}
	if (rows <= 50)
		num_yticks--;

	check_plot_columns(plot, TPUT_GRAPH_INDEX);
	plot_tput(plot, min_seconds, max_seconds, 1);

	check_plot_columns(plot, FIO_GRAPH_INDEX);
	plot_fio_tput(plot, min_seconds, max_seconds);

	check_plot_columns(plot, CPU_IO_GRAPH_INDEX);
	plot_cpu(plot, max_seconds, "CPU IO Wait Time",
		 CPU_IO_GRAPH_INDEX, MPSTAT_IO);

	check_plot_columns(plot, CPU_SYS_GRAPH_INDEX);
	plot_cpu(plot, max_seconds, "CPU System Time",
		 CPU_SYS_GRAPH_INDEX, MPSTAT_SYS);

	check_plot_columns(plot, CPU_IRQ_GRAPH_INDEX);
	plot_cpu(plot, max_seconds, "CPU IRQ Time",
		 CPU_IRQ_GRAPH_INDEX, MPSTAT_IRQ);

	check_plot_columns(plot, CPU_SOFT_GRAPH_INDEX);
	plot_cpu(plot, max_seconds, "CPU SoftIRQ Time",
		 CPU_SOFT_GRAPH_INDEX, MPSTAT_SOFT);

	check_plot_columns(plot, CPU_USER_GRAPH_INDEX);
	plot_cpu(plot, max_seconds, "CPU User Time",
		 CPU_USER_GRAPH_INDEX, MPSTAT_USER);

	check_plot_columns(plot, LATENCY_GRAPH_INDEX);
	plot_latency(plot, min_seconds, max_seconds);

	check_plot_columns(plot, QUEUE_DEPTH_GRAPH_INDEX);
	plot_queue_depth(plot, min_seconds, max_seconds);

	check_plot_columns(plot, IOPS_GRAPH_INDEX);
	plot_iops(plot, min_seconds, max_seconds);

	/* once for all */
	close_plot(plot);
	close_plot_file(plot);
	return 0;
}
