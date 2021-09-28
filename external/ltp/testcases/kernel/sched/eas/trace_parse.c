/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tst_res_flags.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_macros.h"

#include "trace_parse.h"

int num_trace_records = 0;
struct trace_record *trace = NULL;

static int trace_fd = -1;
static char *trace_buffer = NULL;

static int parse_event_type(char *event_name)
{
	if (!strcmp(event_name, "sched_process_exit"))
		return TRACE_RECORD_SCHED_PROCESS_EXIT;
	if (!strcmp(event_name, "sched_process_fork"))
		return TRACE_RECORD_SCHED_PROCESS_FORK;
	if (!strcmp(event_name, "sched_switch"))
		return TRACE_RECORD_SCHED_SWITCH;
	if (!strcmp(event_name, "sched_wakeup"))
		return TRACE_RECORD_SCHED_WAKEUP;
	if (!strcmp(event_name, "sugov_util_update"))
		return TRACE_RECORD_SUGOV_UTIL_UPDATE;
	if (!strcmp(event_name, "sugov_next_freq"))
		return TRACE_RECORD_SUGOV_NEXT_FREQ;
	if (!strcmp(event_name, "cpu_frequency"))
		return TRACE_RECORD_CPU_FREQUENCY;
	if (!strcmp(event_name, "tracing_mark_write"))
		return TRACE_RECORD_TRACING_MARK_WRITE;
	return -1;
}

void print_trace_record(struct trace_record *tr)
{
	printf("task: %s pid %d cpu %d flags %d irq_context %c preempt_depth %d timestamp %d.%.6d event_type %d ",
	       tr->task, tr->pid, tr->cpu, tr->flags, tr->irq_context,
	       tr->preempt_depth, tr->ts.sec, tr->ts.usec,
	       tr->event_type);
	if (tr->event_type == TRACE_RECORD_SCHED_PROCESS_EXIT)
		printf("(sched_process_exit)\n");
	else if (tr->event_type == TRACE_RECORD_SCHED_PROCESS_FORK)
		printf("(sched_process_fork)\n");
	else if (tr->event_type == TRACE_RECORD_SCHED_SWITCH) {
		struct trace_sched_switch *t = tr->event_data;
		printf("(sched_switch) %s pid=%d prio=%d state=%c => "
		       "%s pid=%d prio=%d\n", t->prev_comm, t->prev_pid,
		       t->prev_prio, t->prev_state, t->next_comm, t->next_pid,
		       t->next_prio);
	} else if (tr->event_type == TRACE_RECORD_SCHED_WAKEUP) {
		struct trace_sched_wakeup *t = tr->event_data;
		printf("(sched_wakeup) %s pid=%d prio=%d cpu=%d\n",
		       t->comm, t->pid, t->prio, t->cpu);
	} else if (tr->event_type == TRACE_RECORD_SUGOV_UTIL_UPDATE) {
		struct trace_sugov_util_update *t = tr->event_data;
		printf("(sugov_util_update) cpu=%d util=%d avg_cap=%d "
		       "max_cap=%d\n", t->cpu, t->util, t->avg_cap,
		       t->max_cap);
	} else if (tr->event_type == TRACE_RECORD_SUGOV_NEXT_FREQ) {
		struct trace_sugov_next_freq *t = tr->event_data;
		printf("(sugov_next_freq) cpu=%d util=%d max=%d freq=%d\n",
		       t->cpu, t->util, t->max, t->freq);
	} else if (tr->event_type == TRACE_RECORD_CPU_FREQUENCY) {
		struct trace_cpu_frequency *t = tr->event_data;
		printf("(cpu_frequency) state=%d cpu=%d\n",
		       t->state, t->cpu);
	} else if (tr->event_type == TRACE_RECORD_TRACING_MARK_WRITE)
		printf("(tracing_mark_write)\n");
	else
		printf("(other)\n");
}

void trace_cleanup(void)
{
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");

}

static void *parse_event_data(unsigned int event_type, char *data)
{
	if (event_type == TRACE_RECORD_TRACING_MARK_WRITE) {
		char *buf = SAFE_MALLOC(strlen(data) + 1);
		strcpy(buf, data);
		return buf;
	}
	if (event_type == TRACE_RECORD_CPU_FREQUENCY) {
		struct trace_cpu_frequency *t;
		t = SAFE_MALLOC(sizeof(struct trace_cpu_frequency));
		if (sscanf(data, "state=%d cpu_id=%hd", &t->state, &t->cpu)
		    != 2) {
			printf("Error parsing cpu_frequency event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		return t;
	}
	if (event_type == TRACE_RECORD_SUGOV_UTIL_UPDATE) {
		struct trace_sugov_util_update *t;
		t = SAFE_MALLOC(sizeof(struct trace_sugov_util_update));
		if (sscanf(data, "cpu=%d util=%d avg_cap=%d max_cap=%d",
			   &t->cpu, &t->util, &t->avg_cap, &t->max_cap) != 4) {
			printf("Error parsing sugov_util_update event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		return t;
	}
	if (event_type == TRACE_RECORD_SUGOV_NEXT_FREQ) {
		struct trace_sugov_next_freq *t;
		t = SAFE_MALLOC(sizeof(struct trace_sugov_next_freq));
		if (sscanf(data, "cpu=%d util=%d max=%d freq=%d",
			   &t->cpu, &t->util, &t->max, &t->freq) != 4) {
			printf("Error parsing sugov_next_freq event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		return t;
	}
	if (event_type == TRACE_RECORD_SCHED_SWITCH) {
		struct trace_sched_switch *t;
		char *tmp, *tmp2;
		t = SAFE_MALLOC(sizeof(struct trace_sched_switch));
		/*
		 * The prev_comm could have spaces in it. Find start of
		 * "prev_pid=" as that is just after end of prev_comm.
		 */
		if (strstr(data, "prev_comm=") != data) {
			printf("Malformatted sched_switch event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		tmp = strstr(data, " prev_pid=");
		if (!tmp) {
			printf("Malformatted sched_switch event, "
			       "no prev_pid:\n%s\n", data);
			free(t);
			return NULL;
		}
		*tmp = 0;
		snprintf(t->prev_comm, sizeof(t->prev_comm), "%s", data + 10);
		*tmp = ' ';
		tmp++;
		if (sscanf(tmp, "prev_pid=%hd prev_prio=%hd "
			   "prev_state=%c ==>\n",
			   &t->prev_pid, &t->prev_prio, &t->prev_state) != 3) {
			printf("Malformatted sched_switch event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		tmp = strstr(tmp, "==> next_comm=");
		if (!tmp) {
			printf("Malformatted sched_switch event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		tmp += 14;
		tmp2 = strstr(tmp, " next_pid=");
		if (!tmp2) {
			printf("Malformatted sched_switch event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		*tmp2 = 0;
		if (strlen(tmp) > 16) {
			printf("next_comm %s is greater than 16!\n",
			       tmp);
			t->next_comm[0] = 0;
		}
		snprintf(t->next_comm, sizeof(t->next_comm), "%s", tmp);
		*tmp2 = ' ';
		tmp2++;
		if (sscanf(tmp2, "next_pid=%hd next_prio=%hd", &t->next_pid,
			   &t->next_prio) != 2) {
			printf("Malformatted sched_switch event:\n%s\n",
			       data);
			free(t);
			return NULL;
		}
		return t;
	}
	if (event_type == TRACE_RECORD_SCHED_WAKEUP) {
		struct trace_sched_wakeup *t;
		char *tmp;
		t = SAFE_MALLOC(sizeof(struct trace_sched_wakeup));
		tmp = strstr(data, " pid=");
		if (!tmp) {
			printf("Malformatted sched_wakeup event:\n%s\n", data);
			free(t);
			return NULL;
		}
		*tmp = 0;
		snprintf(t->comm, sizeof(t->comm), "%s", data + 5);
		*tmp = ' ';
		if (sscanf(tmp, " pid=%hd prio=%hd target_cpu=%hd", &t->pid,
			   &t->prio, &t->cpu) != 3) {
			printf("Malformatted sched_wakeup event:\n%s\n", data);
			free(t);
			return NULL;
		}
		return t;
	}
	return NULL;
}

/*
 * Given a line of text from a trace containing a trace record, parse the trace
 * record into a struct trace_record.
 * First 16 chars are the currently running thread name. Drop leading spaces.
 * Next char is a dash
 * Next 5 chars are PID. Drop trailing spaces.
 * Next char is a space.
 * Next five chars are the CPU, i.e. [001].
 * Next char is a space.
 * Next letter is whether IRQs are off.
 * Next letter is if NEED_RESCHED is set.
 * Next letter is if this is in hard or soft IRQ context.
 * Next letter is the preempt disable depth.
 * Next char is a space.
 * Next twelve letters are the timestamp. Drop leading spaces.
 * Next char is colon.
 * Next char is space.
 * Next twelve letters are the event name.
 * Next char is colon.
 * Rest of line is string specific to event.
 */
static int parse_trace_record(struct trace_record *tr, char *line) {
	unsigned int idx = 0;
	char *found;

	/* Skip leading spaces in the task name. */
	while (idx < 16 && line[idx] == ' ')
		idx++;
	if (idx == 16) {
		printf("Malformatted trace record, no task name:\n");
		printf("%s", line);
		return -1;
	}
	memcpy(tr->task, &line[idx], 16-idx);
	if (line[16] != '-') {
		printf("Malformatted trace record, no dash after task "
		       "name:\n");
		printf("%s", line);
		return -1;
	}
	idx = 17;
	if (line[22] != ' ') {
		printf("Malformatted trace record, no space between"
		       "pid and CPU:\n");
		printf("%s", line);
		return -1;
	}
	line[22] = 0;
	if (sscanf(&line[17], "%hd", &tr->pid) != 1) {
		printf("Malformatted trace record, error parsing"
		       "pid:\n");
		printf("%s", line);
		return -1;
	}
	if (line[28] != ' ') {
		printf("Malformatted trace record, no space between"
		       "CPU and flags:\n");
		printf("%s", line);
		return -1;
	}
	line[28] = 0;
	if (sscanf(&line[23], "[%hd]", &tr->cpu) != 1) {
		printf("Malformatted trace record, error parsing CPU:\n");
		printf("%s", line);
		return -1;
	}
	if (line[29] == 'd') {
		tr->flags |= TRACE_RECORD_IRQS_OFF;
	} else if (line[29] != '.') {
		printf("Malformatted trace record, error parsing irqs-off:\n");
		printf("%s", line);
		return -1;
	}
	if (line[30] == 'N') {
		tr->flags |= TRACE_RECORD_TIF_NEED_RESCHED;
		tr->flags |= TRACE_RECORD_PREEMPT_NEED_RESCHED;
	} else if (line[30] == 'n') {
		tr->flags |= TRACE_RECORD_TIF_NEED_RESCHED;
	} else if (line[30] == 'p') {
		tr->flags |= TRACE_RECORD_PREEMPT_NEED_RESCHED;
	} else if (line[30] != '.') {
		printf("Malformatted trace record, error parsing "
		       "need-resched:\n");
		printf("%s", line);
		return -1;
	}

	if (line[31] != IRQ_CONTEXT_NORMAL && line[31] != IRQ_CONTEXT_SOFT &&
	    line[31] != IRQ_CONTEXT_HARD &&
	    line[31] != IRQ_CONTEXT_HARD_IN_SOFT &&
	    line[31] != IRQ_CONTEXT_NMI && line[31] != IRQ_CONTEXT_NMI_IN_HARD) {
		printf("Malformatted trace record, error parsing irq "
		       "context:\n");
		printf("%s", line);
		return -1;
	}
	tr->irq_context = line[31];

	if (line[33] != ' ') {
		printf("Malformatted trace record, no space between"
		       "flags and timestamp:\n");
		printf("%s", line);
		return -1;
	}
	line[33] = 0;
	if (line[32] == '.') {
		tr->preempt_depth = 0;
	} else if (sscanf(&line[32], "%hx", &tr->preempt_depth) != 1) {
		printf("Malformatted trace record, error parsing "
		       "preempt-depth:\n");
		printf("%s", line);
		return -1;
	}

	/* timestamp starts as early as line[34], skip leading spaces */
	idx = 34;
	while (idx < 38 && line[idx] == ' ')
		idx++;
	if (sscanf(&line[idx], "%d.%d: ", &tr->ts.sec,
		   &tr->ts.usec) != 2) {
		printf("Malformatted trace record, error parsing "
		       "timestamp:\n");
		printf("%s", line);
		return -1;
	}
	/* we know a space appears in the string because sscanf parsed it */
	found = strchr(&line[idx], ' ');
	idx = (found - line + 1);
	found = strchr(&line[idx], ':');
	if (!found) {
		printf("Malformatted trace record, error parsing "
		       "event name:\n");
		printf("%s", line);
		return -1;
	}
	*found = 0;
	tr->event_type = parse_event_type(&line[idx]);

	/*
	 * there is a space between the ':' after the event name and the event
	 * data
	 */
	if (tr->event_type > 0)
		tr->event_data = parse_event_data(tr->event_type, found + 2);

	return 0;
}

#define TRACE_BUFFER_SIZE 8192
static int refill_buffer(char *buffer, char *idx)
{
	int bytes_in_buffer;
	int bytes_to_read;
	int bytes_read = 0;
	int rv;

	bytes_in_buffer = TRACE_BUFFER_SIZE - (idx - buffer) - 1;
	bytes_to_read = TRACE_BUFFER_SIZE - bytes_in_buffer - 1;

	if (trace_fd == -1) {
		trace_fd = open(TRACING_DIR "trace", O_RDONLY);
		if (trace_fd == -1) {
			printf("Could not open trace file!\n");
			return 0;
		}
	}

	/* Shift existing bytes in buffer to front. */
	memmove(buffer, idx, bytes_in_buffer);
	idx = buffer + bytes_in_buffer;

	while(bytes_to_read) {
		rv = read(trace_fd, idx, bytes_to_read);
		if (rv == -1) {
			printf("Could not read trace file! (%d)\n", errno);
			return -1;
		}
		if (rv == 0)
			break;
		idx += rv;
		bytes_read += rv;
		bytes_to_read -= rv;
	}

	return bytes_read;
}

/*
 * Returns a pointer to a null (not newline) terminated line
 * of the trace.
 */
static char *read_trace_line(void)
{
	static char *idx;
	char *line, *newline;
	int rv;

	if (!trace_buffer) {
		trace_buffer = SAFE_MALLOC(TRACE_BUFFER_SIZE);
		idx = trace_buffer + TRACE_BUFFER_SIZE - 1;
		*idx = 0;
	}

	line = idx;
	newline = strchr(idx, '\n');
	if (!newline) {
		rv = refill_buffer(trace_buffer, idx);
		if (rv == -1 || rv == 0)
				return NULL;
		idx = trace_buffer;
		line = idx;
		newline = strchr(idx, '\n');
	}
	if (newline) {
		*newline = 0;
		idx = newline + 1;
		return line;
	}
	return NULL;
}

struct trace_record *load_trace(void)
{
	int buflines, unused;
	int tmp_num_trace_records = 0;
	char *line;
	enum parse_state_type {
		PARSE_STATE_SEEK_NUM_ENTRIES = 0,
		PARSE_STATE_REMAINING_COMMENTS,
		PARSE_STATE_TRACE_ENTRIES,
	};
	int parse_state = PARSE_STATE_SEEK_NUM_ENTRIES;

	num_trace_records = 0;

#ifdef PRINT_PARSING_UPDATES
	printf("\n");
#endif
	while((line = read_trace_line())) {
		if (line[0] == '#') {
			if (parse_state == PARSE_STATE_TRACE_ENTRIES) {
				printf("Malformatted trace output, comment"
				       "after start of trace entries.\n");
				if (trace)
					free(trace);
				return NULL;
			}
			if (parse_state == PARSE_STATE_REMAINING_COMMENTS)
				continue;
			if (sscanf(line,
				   "# entries-in-buffer/entries-written: "
				   "%d/%d",
				   &buflines, &unused) != 2) {
				continue;
			}

			trace = SAFE_MALLOC(sizeof(struct trace_record) *
					    buflines);
			parse_state = PARSE_STATE_REMAINING_COMMENTS;
		} else {
			if (parse_state == PARSE_STATE_SEEK_NUM_ENTRIES) {
				printf("Malformatted trace output, non-comment "
				       "before number of entries.\n");
				if (trace)
					free(trace);
				return NULL;
			}

			if (parse_state == PARSE_STATE_REMAINING_COMMENTS)
				parse_state = PARSE_STATE_TRACE_ENTRIES;

			if (parse_trace_record(&trace[tmp_num_trace_records++],
					       line)) {
				printf("Malformatted trace record entry:\n");
				printf("%s\n", line);
				if (trace)
					free(trace);
				return NULL;
			}
#ifdef PRINT_PARSING_UPDATES
			if (tmp_num_trace_records%1000 == 0) {
				printf("\r%d/%d records processed",
				       tmp_num_trace_records, buflines);
				fflush(stdout);
			}
#endif
		}
	}
#ifdef PRINT_PARSING_UPDATES
	printf("\n");
#endif
	num_trace_records = tmp_num_trace_records;
	if (trace_fd >= 0) {
		close(trace_fd);
		trace_fd = -1;
	}
	if (trace_buffer) {
		free(trace_buffer);
		trace_buffer = NULL;
	}
	return trace;
}
