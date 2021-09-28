/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _LTP_TRACE_PARSE_H_
#define _LTP_TRACE_PARSE_H_

/*
 * It is necessary to define TRACE_EVENTS to communicate the events to trace. */
#define TRACING_DIR "/sys/kernel/debug/tracing/"

enum {
	TRACE_RECORD_OTHER = 0,
	TRACE_RECORD_SCHED_PROCESS_EXIT,
	TRACE_RECORD_SCHED_PROCESS_FORK,
	TRACE_RECORD_SCHED_SWITCH,
	TRACE_RECORD_SCHED_WAKEUP,
	TRACE_RECORD_SUGOV_UTIL_UPDATE,
	TRACE_RECORD_SUGOV_NEXT_FREQ,
	TRACE_RECORD_CPU_FREQUENCY,
	TRACE_RECORD_TRACING_MARK_WRITE,
};

#define IRQ_CONTEXT_NORMAL '.'
#define IRQ_CONTEXT_SOFT 's'
#define IRQ_CONTEXT_HARD 'h'
#define IRQ_CONTEXT_HARD_IN_SOFT 'H'
#define IRQ_CONTEXT_NMI 'z'
#define IRQ_CONTEXT_NMI_IN_HARD 'Z'

struct timestamp {
	unsigned int sec;
	unsigned int usec;
};

struct trace_cpu_frequency {
	unsigned int state;
	unsigned short cpu;
};

struct trace_sched_switch {
	char prev_comm[17];
	unsigned short prev_pid;
	unsigned short prev_prio;
	char prev_state;
	char next_comm[17];
	unsigned short next_pid;
	unsigned short next_prio;
};

struct trace_sched_wakeup {
	char comm[17];
	unsigned short pid;
	unsigned short prio;
	unsigned short cpu;
};

struct trace_sugov_util_update {
	int cpu;
	int util;
	int avg_cap;
	int max_cap;
};

struct trace_sugov_next_freq {
	int cpu;
	int util;
	int max;
	int freq;
};

struct trace_record {
	char task[17];
	unsigned short pid;
	unsigned short cpu;

#define TRACE_RECORD_IRQS_OFF 0x1
#define TRACE_RECORD_TIF_NEED_RESCHED 0x2
#define TRACE_RECORD_PREEMPT_NEED_RESCHED 0x4
	unsigned short flags;
	unsigned char irq_context;
	unsigned short preempt_depth;

	struct timestamp ts;

	unsigned int event_type;
	void *event_data;
};

extern int num_trace_records;
extern struct trace_record *trace;

void trace_cleanup(void);
void print_trace_record(struct trace_record *tr);
struct trace_record *load_trace(void);

#define LOAD_TRACE() \
	if (!load_trace()) \
		tst_brk(TBROK, "Failed to load trace.\n");

#endif /* _LTP_TRACE_PARSE_H_ */
