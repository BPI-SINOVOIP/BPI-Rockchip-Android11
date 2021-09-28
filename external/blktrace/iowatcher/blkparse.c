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
#include <dirent.h>

#include "plot.h"
#include "blkparse.h"
#include "list.h"
#include "tracers.h"

#define IO_HASH_TABLE_BITS  11
#define IO_HASH_TABLE_SIZE (1 << IO_HASH_TABLE_BITS)
static struct list_head io_hash_table[IO_HASH_TABLE_SIZE];
static u64 ios_in_flight = 0;

#define PROCESS_HASH_TABLE_BITS 7
#define PROCESS_HASH_TABLE_SIZE (1 << PROCESS_HASH_TABLE_BITS)
static struct list_head process_hash_table[PROCESS_HASH_TABLE_SIZE];

extern int plot_io_action;
extern int io_per_process;

/*
 * Trace categories
 */
enum {
	BLK_TC_READ	= 1 << 0,	/* reads */
	BLK_TC_WRITE	= 1 << 1,	/* writes */
	BLK_TC_FLUSH	= 1 << 2,	/* flush */
	BLK_TC_SYNC	= 1 << 3,	/* sync */
	BLK_TC_QUEUE	= 1 << 4,	/* queueing/merging */
	BLK_TC_REQUEUE	= 1 << 5,	/* requeueing */
	BLK_TC_ISSUE	= 1 << 6,	/* issue */
	BLK_TC_COMPLETE	= 1 << 7,	/* completions */
	BLK_TC_FS	= 1 << 8,	/* fs requests */
	BLK_TC_PC	= 1 << 9,	/* pc requests */
	BLK_TC_NOTIFY	= 1 << 10,	/* special message */
	BLK_TC_AHEAD	= 1 << 11,	/* readahead */
	BLK_TC_META	= 1 << 12,	/* metadata */
	BLK_TC_DISCARD	= 1 << 13,	/* discard requests */
	BLK_TC_DRV_DATA	= 1 << 14,	/* binary driver data */
	BLK_TC_FUA	= 1 << 15,	/* fua requests */

	BLK_TC_END	= 1 << 15,	/* we've run out of bits! */
};

#define BLK_TC_SHIFT		(16)
#define BLK_TC_ACT(act)		((act) << BLK_TC_SHIFT)
#define BLK_DATADIR(a) (((a) >> BLK_TC_SHIFT) & (BLK_TC_READ | BLK_TC_WRITE))

/*
 * Basic trace actions
 */
enum {
	__BLK_TA_QUEUE = 1,		/* queued */
	__BLK_TA_BACKMERGE,		/* back merged to existing rq */
	__BLK_TA_FRONTMERGE,		/* front merge to existing rq */
	__BLK_TA_GETRQ,			/* allocated new request */
	__BLK_TA_SLEEPRQ,		/* sleeping on rq allocation */
	__BLK_TA_REQUEUE,		/* request requeued */
	__BLK_TA_ISSUE,			/* sent to driver */
	__BLK_TA_COMPLETE,		/* completed by driver */
	__BLK_TA_PLUG,			/* queue was plugged */
	__BLK_TA_UNPLUG_IO,		/* queue was unplugged by io */
	__BLK_TA_UNPLUG_TIMER,		/* queue was unplugged by timer */
	__BLK_TA_INSERT,		/* insert request */
	__BLK_TA_SPLIT,			/* bio was split */
	__BLK_TA_BOUNCE,		/* bio was bounced */
	__BLK_TA_REMAP,			/* bio was remapped */
	__BLK_TA_ABORT,			/* request aborted */
	__BLK_TA_DRV_DATA,		/* binary driver data */
};

#define BLK_TA_MASK ((1 << BLK_TC_SHIFT) - 1)

/*
 * Notify events.
 */
enum blktrace_notify {
	__BLK_TN_PROCESS = 0,		/* establish pid/name mapping */
	__BLK_TN_TIMESTAMP,		/* include system clock */
	__BLK_TN_MESSAGE,               /* Character string message */
};

/*
 * Trace actions in full. Additionally, read or write is masked
 */
#define BLK_TA_QUEUE		(__BLK_TA_QUEUE | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_BACKMERGE	(__BLK_TA_BACKMERGE | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_FRONTMERGE	(__BLK_TA_FRONTMERGE | BLK_TC_ACT(BLK_TC_QUEUE))
#define	BLK_TA_GETRQ		(__BLK_TA_GETRQ | BLK_TC_ACT(BLK_TC_QUEUE))
#define	BLK_TA_SLEEPRQ		(__BLK_TA_SLEEPRQ | BLK_TC_ACT(BLK_TC_QUEUE))
#define	BLK_TA_REQUEUE		(__BLK_TA_REQUEUE | BLK_TC_ACT(BLK_TC_REQUEUE))
#define BLK_TA_ISSUE		(__BLK_TA_ISSUE | BLK_TC_ACT(BLK_TC_ISSUE))
#define BLK_TA_COMPLETE		(__BLK_TA_COMPLETE| BLK_TC_ACT(BLK_TC_COMPLETE))
#define BLK_TA_PLUG		(__BLK_TA_PLUG | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_UNPLUG_IO	(__BLK_TA_UNPLUG_IO | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_UNPLUG_TIMER	(__BLK_TA_UNPLUG_TIMER | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_INSERT		(__BLK_TA_INSERT | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_SPLIT		(__BLK_TA_SPLIT)
#define BLK_TA_BOUNCE		(__BLK_TA_BOUNCE)
#define BLK_TA_REMAP		(__BLK_TA_REMAP | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_ABORT		(__BLK_TA_ABORT | BLK_TC_ACT(BLK_TC_QUEUE))
#define BLK_TA_DRV_DATA		(__BLK_TA_DRV_DATA | BLK_TC_ACT(BLK_TC_DRV_DATA))

#define BLK_TN_PROCESS		(__BLK_TN_PROCESS | BLK_TC_ACT(BLK_TC_NOTIFY))
#define BLK_TN_TIMESTAMP	(__BLK_TN_TIMESTAMP | BLK_TC_ACT(BLK_TC_NOTIFY))
#define BLK_TN_MESSAGE		(__BLK_TN_MESSAGE | BLK_TC_ACT(BLK_TC_NOTIFY))

#define BLK_IO_TRACE_MAGIC	0x65617400
#define BLK_IO_TRACE_VERSION	0x07
/*
 * The trace itself
 */
struct blk_io_trace {
	__u32 magic;		/* MAGIC << 8 | version */
	__u32 sequence;		/* event number */
	__u64 time;		/* in nanoseconds */
	__u64 sector;		/* disk offset */
	__u32 bytes;		/* transfer length */
	__u32 action;		/* what happened */
	__u32 pid;		/* who did it */
	__u32 device;		/* device identifier (dev_t) */
	__u32 cpu;		/* on what cpu did it happen */
	__u16 error;		/* completion error */
	__u16 pdu_len;		/* length of data after this trace */
};

struct pending_io {
	/* sector offset of this IO */
	u64 sector;

	/* dev_t for this IO */
	u32 device;

	/* time this IO was dispatched */
	u64 dispatch_time;
	/* time this IO was finished */
	u64 completion_time;
	struct list_head hash_list;
	/* process which queued this IO */
	u32 pid;
};

struct pid_map {
	struct list_head hash_list;
	u32 pid;
	int index;
	char name[0];
};

u64 get_record_time(struct trace *trace)
{
	return trace->io->time;
}

void init_io_hash_table(void)
{
	int i;
	struct list_head *head;

	for (i = 0; i < IO_HASH_TABLE_SIZE; i++) {
		head = io_hash_table + i;
		INIT_LIST_HEAD(head);
	}
}

/* taken from the kernel hash.h */
static inline u64 hash_sector(u64 val)
{
	u64 hash = val;

	/*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
	u64 n = hash;
	n <<= 18;
	hash -= n;
	n <<= 33;
	hash -= n;
	n <<= 3;
	hash += n;
	n <<= 3;
	hash -= n;
	n <<= 4;
	hash += n;
	n <<= 2;
	hash += n;

	/* High bits are more random, so use them. */
	return hash >> (64 - IO_HASH_TABLE_BITS);
}

static int io_hash_table_insert(struct pending_io *ins_pio)
{
	u64 sector = ins_pio->sector;
	u32 dev = ins_pio->device;
	int slot = hash_sector(sector);
	struct list_head *head;
	struct pending_io *pio;

	head = io_hash_table + slot;
	list_for_each_entry(pio, head, hash_list) {
		if (pio->sector == sector && pio->device == dev)
			return -EEXIST;
	}
	list_add_tail(&ins_pio->hash_list, head);
	return 0;
}

static struct pending_io *io_hash_table_search(u64 sector, u32 dev)
{
	int slot = hash_sector(sector);
	struct list_head *head;
	struct pending_io *pio;

	head = io_hash_table + slot;
	list_for_each_entry(pio, head, hash_list) {
		if (pio->sector == sector && pio->device == dev)
			return pio;
	}
	return NULL;
}

static struct pending_io *hash_queued_io(struct blk_io_trace *io)
{
	struct pending_io *pio;
	int ret;

	pio = calloc(1, sizeof(*pio));
	pio->sector = io->sector;
	pio->device = io->device;
	pio->pid = io->pid;

	ret = io_hash_table_insert(pio);
	if (ret < 0) {
		/* crud, the IO is there already */
		free(pio);
		return NULL;
	}
	return pio;
}

static struct pending_io *hash_dispatched_io(struct blk_io_trace *io)
{
	struct pending_io *pio;

	pio = io_hash_table_search(io->sector, io->device);
	if (!pio) {
		pio = hash_queued_io(io);
		if (!pio)
			return NULL;
	}
	pio->dispatch_time = io->time;
	return pio;
}

static struct pending_io *hash_completed_io(struct blk_io_trace *io)
{
	struct pending_io *pio;

	pio = io_hash_table_search(io->sector, io->device);

	if (!pio)
		return NULL;
	return pio;
}

void init_process_hash_table(void)
{
	int i;
	struct list_head *head;

	for (i = 0; i < PROCESS_HASH_TABLE_SIZE; i++) {
		head = process_hash_table + i;
		INIT_LIST_HEAD(head);
	}
}

static u32 hash_pid(u32 pid)
{
	u32 hash = pid;

	hash ^= pid >> 3;
	hash ^= pid >> 3;
	hash ^= pid >> 4;
	hash ^= pid >> 6;
	return (hash & (PROCESS_HASH_TABLE_SIZE - 1));
}

static struct pid_map *process_hash_search(u32 pid)
{
	int slot = hash_pid(pid);
	struct list_head *head;
	struct pid_map *pm;

	head = process_hash_table + slot;
	list_for_each_entry(pm, head, hash_list) {
		if (pm->pid == pid)
			return pm;
	}
	return NULL;
}

static struct pid_map *process_hash_insert(u32 pid, char *name)
{
	int slot = hash_pid(pid);
	struct pid_map *pm;
	int old_index = 0;
	char buf[16];

	pm = process_hash_search(pid);
	if (pm) {
		/* Entry exists and name shouldn't be changed? */
		if (!name || !strcmp(name, pm->name))
			return pm;
		list_del(&pm->hash_list);
		old_index = pm->index;
		free(pm);
	}
	if (!name) {
		sprintf(buf, "[%u]", pid);
		name = buf;
	}
	pm = malloc(sizeof(struct pid_map) + strlen(name) + 1);
	pm->pid = pid;
	pm->index = old_index;
	strcpy(pm->name, name);
	list_add_tail(&pm->hash_list, process_hash_table + slot);

	return pm;
}

static void handle_notify(struct trace *trace)
{
	struct blk_io_trace *io = trace->io;
	void *payload = (char *)io + sizeof(*io);
	u32 two32[2];

	if (io->action == BLK_TN_PROCESS) {
		if (io_per_process)
			process_hash_insert(io->pid, payload);
		return;
	}

	if (io->action != BLK_TN_TIMESTAMP)
		return;

	if (io->pdu_len != sizeof(two32))
		return;

	memcpy(two32, payload, sizeof(two32));
	trace->start_timestamp = io->time;
	trace->abs_start_time.tv_sec = two32[0];
	trace->abs_start_time.tv_nsec = two32[1];
	if (trace->abs_start_time.tv_nsec < 0) {
		trace->abs_start_time.tv_sec--;
		trace->abs_start_time.tv_nsec += 1000000000;
	}
}

int next_record(struct trace *trace)
{
	int skip = trace->io->pdu_len;
	u64 offset;

	trace->cur += sizeof(*trace->io) + skip;
	offset = trace->cur - trace->start;
	if (offset >= trace->len)
		return 1;

	trace->io = (struct blk_io_trace *)trace->cur;
	return 0;
}

void first_record(struct trace *trace)
{
	trace->cur = trace->start;
	trace->io = (struct blk_io_trace *)trace->cur;
}

static int is_io_event(struct blk_io_trace *test)
{
	char *message;
	if (!(test->action & BLK_TC_ACT(BLK_TC_NOTIFY)))
		return 1;
	if (test->action == BLK_TN_MESSAGE) {
		int len = test->pdu_len;
		if (len < 3)
			return 0;
		message = (char *)(test + 1);
		if (strncmp(message, "fio ", 4) == 0) {
			return 1;
		}
	}
	return 0;
}

u64 find_last_time(struct trace *trace)
{
	char *p = trace->start + trace->len;
	struct blk_io_trace *test;
	int search_len = 0;
	u64 found = 0;

	if (trace->len < sizeof(*trace->io))
		return 0;
	p -= sizeof(*trace->io);
	while (p >= trace->start) {
		test = (struct blk_io_trace *)p;
		if (CHECK_MAGIC(test) && is_io_event(test)) {
			u64 offset = p - trace->start;
			if (offset + sizeof(*test) + test->pdu_len == trace->len) {
				return test->time;
			}
		}
		p--;
		search_len++;
		if (search_len > 8192) {
			break;
		}
	}

	/* searching backwards didn't work out, we'll have to scan the file */
	first_record(trace);
	while (1) {
		if (is_io_event(trace->io))
			found = trace->io->time;
		if (next_record(trace))
			break;
	}
	first_record(trace);
	return found;
}

static int parse_fio_bank_message(struct trace *trace, u64 *bank_ret, u64 *offset_ret,
			   u64 *num_banks_ret)
{
	char *s;
	char *next;
	char *message;
	struct blk_io_trace *test = trace->io;
	int len = test->pdu_len;
	u64 bank;
	u64 offset;
	u64 num_banks;

	if (!(test->action & BLK_TC_ACT(BLK_TC_NOTIFY)))
		return -1;
	if (test->action != BLK_TN_MESSAGE)
		return -1;

	/* the message is fio rw bank offset num_banks */
	if (len < 3)
		return -1;
	message = (char *)(test + 1);
	if (strncmp(message, "fio r ", 6) != 0)
		return -1;

	message = strndup(message, len);
	s = strchr(message, ' ');
	if (!s)
		goto out;
	s++;
	s = strchr(s, ' ');
	if (!s)
		goto out;

	bank = strtoll(s, &next, 10);
	if (s == next)
		goto out;
	s = next;

	offset = strtoll(s, &next, 10);
	if (s == next)
		goto out;
	s = next;

	num_banks = strtoll(s, &next, 10);
	if (s == next)
		goto out;

	*bank_ret = bank;
	*offset_ret = offset;
	*num_banks_ret = num_banks;

	return 0;
out:
	free(message);
	return -1;
}

static struct dev_info *lookup_dev(struct trace *trace, struct blk_io_trace *io)
{
	u32 dev = io->device;
	int i;
	struct dev_info *di = NULL;

	for (i = 0; i < trace->num_devices; i++) {
		if (trace->devices[i].device == dev) {
			di = trace->devices + i;
			goto found;
		}
	}
	i = trace->num_devices++;
	if (i >= MAX_DEVICES_PER_TRACE) {
		fprintf(stderr, "Trace contains too many devices (%d)\n", i);
		exit(1);
	}
	di = trace->devices + i;
	di->device = dev;
found:
	return di;
}

static void map_devices(struct trace *trace)
{
	struct dev_info *di;
	u64 found;
	u64 map_start = 0;
	int i;

	first_record(trace);
	while (1) {
		if (!(trace->io->action & BLK_TC_ACT(BLK_TC_NOTIFY))) {
			di = lookup_dev(trace, trace->io);
			found = trace->io->sector << 9;
			if (found < di->min)
				di->min = found;

			found += trace->io->bytes;
			if (di->max < found)
				di->max = found;
		}
		if (next_record(trace))
			break;
	}
	first_record(trace);
	for (i = 0; i < trace->num_devices; i++) {
		di = trace->devices + i;
		di->map = map_start;
		map_start += di->max - di->min;
	}
}

static u64 map_io(struct trace *trace, struct blk_io_trace *io)
{
	struct dev_info *di = lookup_dev(trace, io);
	u64 val = trace->io->sector << 9;
	return di->map + val - di->min;
}

void find_extreme_offsets(struct trace *trace, u64 *min_ret, u64 *max_ret, u64 *max_bank_ret,
			  u64 *max_offset_ret)
{
	u64 found = 0;
	u64 max = 0, min = ~(u64)0;
	u64 max_bank = 0;
	u64 max_bank_offset = 0;
	u64 num_banks = 0;

	map_devices(trace);

	first_record(trace);
	while (1) {
		if (!(trace->io->action & BLK_TC_ACT(BLK_TC_NOTIFY))) {
			found = map_io(trace, trace->io);
			if (found < min)
				min = found;

			found += trace->io->bytes;
			if (max < found)
				max = found;
		} else {
			u64 bank;
			u64 offset;
			if (!parse_fio_bank_message(trace, &bank,
						    &offset, &num_banks)) {
				if (bank > max_bank)
					max_bank = bank;
				if (offset > max_bank_offset)
					max_bank_offset = offset;
			}
		}
		if (next_record(trace))
			break;
	}
	first_record(trace);
	*min_ret = min;
	*max_ret = max;
	*max_bank_ret = max_bank;
	*max_offset_ret = max_bank_offset;
}

static void check_io_types(struct trace *trace)
{
	struct blk_io_trace *io = trace->io;
	int action = io->action & BLK_TA_MASK;

	if (!(io->action & BLK_TC_ACT(BLK_TC_NOTIFY))) {
		switch (action) {
		case __BLK_TA_COMPLETE:
			trace->found_completion = 1;
			break;
		case __BLK_TA_ISSUE:
			trace->found_issue = 1;
			break;
		case __BLK_TA_QUEUE:
			trace->found_queue = 1;
			break;
		};
	}
}


int filter_outliers(struct trace *trace, u64 min_offset, u64 max_offset,
		    u64 *yzoom_min, u64 *yzoom_max)
{
	int hits[11];
	u64 max_per_bucket[11];
	u64 min_per_bucket[11];
	u64 bytes_per_bucket = (max_offset - min_offset + 1) / 10;
	int slot;
	int fat_count = 0;

	memset(hits, 0, sizeof(int) * 11);
	memset(max_per_bucket, 0, sizeof(u64) * 11);
	memset(min_per_bucket, 0xff, sizeof(u64) * 11);
	first_record(trace);
	while (1) {
		check_io_types(trace);
		if (!(trace->io->action & BLK_TC_ACT(BLK_TC_NOTIFY)) &&
		    (trace->io->action & BLK_TA_MASK) == __BLK_TA_QUEUE) {
			u64 off = map_io(trace, trace->io) - min_offset;

			slot = (int)(off / bytes_per_bucket);
			hits[slot]++;
			if (off < min_per_bucket[slot])
				min_per_bucket[slot] = off;

			off += trace->io->bytes;
			slot = (int)(off / bytes_per_bucket);
			hits[slot]++;
			if (off > max_per_bucket[slot])
				max_per_bucket[slot] = off;
		}
		if (next_record(trace))
			break;
	}
	first_record(trace);
	for (slot = 0; slot < 11; slot++) {
		if (hits[slot] > fat_count) {
			fat_count = hits[slot];
		}
	}

	*yzoom_max = max_offset;
	for (slot = 10; slot >= 0; slot--) {
		double d = hits[slot];

		if (d >= (double)fat_count * .05) {
			*yzoom_max = max_per_bucket[slot] + min_offset;
			break;
		}
	}

	*yzoom_min = min_offset;
	for (slot = 0; slot < 10; slot++) {
		double d = hits[slot];

		if (d >= (double)fat_count * .05) {
			*yzoom_min = min_per_bucket[slot] + min_offset;
			break;
		}
	}
	return 0;
}

static char footer[] = ".blktrace.0";
static int footer_len = sizeof(footer) - 1;

static int match_trace(char *name, int *len)
{
	int match_len;
	int footer_start;

	match_len = strlen(name);
	if (match_len <= footer_len)
		return 0;

	footer_start = match_len - footer_len;
	if (strcmp(name + footer_start, footer) != 0)
		return 0;

	if (len)
		*len = match_len;
	return 1;
}

struct tracelist {
	struct tracelist *next;
	char *name;
};

static struct tracelist *traces_list(char *dir_name, int *len)
{
	int count = 0;
	struct tracelist *traces = NULL;
	int dlen = strlen(dir_name);
	DIR *dir = opendir(dir_name);
	if (!dir)
		return NULL;

	while (1) {
		int n = 0;
		struct tracelist *tl;
		struct dirent *d = readdir(dir);
		if (!d)
			break;

		if (!match_trace(d->d_name, &n))
			continue;

		n += dlen + 1; /* dir + '/' + file */
		/* Allocate space for tracelist + filename */
		tl = calloc(1, sizeof(struct tracelist) + (sizeof(char) * (n + 1)));
		if (!tl) {
			closedir(dir);
			return NULL;
		}
		tl->next = traces;
		tl->name = (char *)(tl + 1);
		snprintf(tl->name, n, "%s/%s", dir_name, d->d_name);
		traces = tl;
		count++;
	}

	closedir(dir);

	if (len)
		*len = count;

	return traces;
}

static void traces_free(struct tracelist *traces)
{
	while (traces) {
		struct tracelist *tl = traces;
		traces = traces->next;
		free(tl);
	}
}

static int dump_traces(struct tracelist *traces, int count, char *dumpfile)
{
	struct tracelist *tl;
	char **argv = NULL;
	int argc = 0;
	int i;
	int err = 0;

	argc = count * 2; /* {"-i", trace } */
	argc += 4; /* See below */
	argv = calloc(argc + 1, sizeof(char *));
	if (!argv)
		return -errno;

	i = 0;
	argv[i++] = "blkparse";
	argv[i++] = "-O";
	argv[i++] = "-d";
	argv[i++] = dumpfile;
	for (tl = traces; tl != NULL; tl = tl->next) {
		argv[i++] = "-i";
		argv[i++] = tl->name;
	}

	err = run_program(argc, argv, 1, NULL, NULL);
	if (err)
		fprintf(stderr, "%s exited with %d, expected 0\n", argv[0], err);
	free(argv);
	return err;
}

static char *find_trace_file(char *filename)
{
	int ret;
	struct stat st;
	char *dot;
	int found_dir = 0;
	char *dumpfile;
	int len = strlen(filename);

	/* look for an exact match of whatever they pass in.
	 * If it is a file, assume it is the dump file.
	 * If a directory, remember that it existed so we
	 * can combine traces in that directory later
	 */
	ret = stat(filename, &st);
	if (ret == 0) {
		if (S_ISREG(st.st_mode))
			return strdup(filename);

		if (S_ISDIR(st.st_mode))
			found_dir = 1;
	}

	if (found_dir) {
		int i;
		/* Eat up trailing '/'s */
		for (i = len - 1; filename[i] == '/'; i--)
			filename[i] = '\0';
	}

	/*
	 * try tacking .dump onto the end and see if that already
	 * has been generated
	 */
	ret = asprintf(&dumpfile, "%s.dump", filename);
	if (ret == -1) {
		perror("Error building dump file name");
		return NULL;
	}
	ret = stat(dumpfile, &st);
	if (ret == 0)
		return dumpfile;

	/*
	 * try to generate the .dump from all the traces in
	 * a single dir.
	 */
	if (found_dir) {
		int count;
		struct tracelist *traces = traces_list(filename, &count);
		if (traces) {
			ret = dump_traces(traces, count, dumpfile);
			traces_free(traces);
			if (ret == 0)
				return dumpfile;
		}
	}
	free(dumpfile);

	/*
	 * try to generate the .dump from all the blktrace
	 * files for a named trace
	 */
	dot = strrchr(filename, '.');
	if (!dot || strcmp(".dump", dot) != 0) {
		struct tracelist trace = {0 ,NULL};
		if (dot && dot != filename)
			len = dot - filename;

		ret = asprintf(&trace.name, "%*s.blktrace.0", len, filename);
		if (ret == -1)
			return NULL;
		ret = asprintf(&dumpfile, "%*s.dump", len, filename);
		if (ret == -1) {
			free(trace.name);
			return NULL;
		}

		ret = dump_traces(&trace, 1, dumpfile);
		if (ret == 0) {
			free(trace.name);
			return dumpfile;
		}
		free(trace.name);
		free(dumpfile);
	}
	return NULL;
}
struct trace *open_trace(char *filename)
{
	int fd;
	char *p;
	struct stat st;
	int ret;
	struct trace *trace;
	char *found_filename;

	trace = calloc(1, sizeof(*trace));
	if (!trace) {
		fprintf(stderr, "unable to allocate memory for trace\n");
		return NULL;
	}

	found_filename = find_trace_file(filename);
	if (!found_filename) {
		fprintf(stderr, "Unable to find trace file %s\n", filename);
		goto fail;
	}
	filename = found_filename;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open trace file %s err %s\n", filename, strerror(errno));
		goto fail;
	}
	ret = fstat(fd, &st);
	if (ret < 0) {
		fprintf(stderr, "stat failed on %s err %s\n", filename, strerror(errno));
		goto fail_fd;
	}
	p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED) {
		fprintf(stderr, "Unable to mmap trace file %s, err %s\n", filename, strerror(errno));
		goto fail_fd;
	}
	trace->fd = fd;
	trace->len = st.st_size;
	trace->start = p;
	trace->cur = p;
	trace->io = (struct blk_io_trace *)p;
	return trace;

fail_fd:
	close(fd);
fail:
	free(trace);
	return NULL;
}
static inline int tput_event(struct trace *trace)
{
	if (trace->found_completion)
		return __BLK_TA_COMPLETE;
	if (trace->found_issue)
		return __BLK_TA_ISSUE;
	if (trace->found_queue)
		return __BLK_TA_QUEUE;

	return __BLK_TA_COMPLETE;
}

int action_char_to_num(char action)
{
	switch (action) {
	case 'Q':
		return __BLK_TA_QUEUE;
	case 'D':
		return __BLK_TA_ISSUE;
	case 'C':
		return __BLK_TA_COMPLETE;
	}
	return -1;
}

static inline int io_event(struct trace *trace)
{
	if (plot_io_action)
		return plot_io_action;
	if (trace->found_queue)
		return __BLK_TA_QUEUE;
	if (trace->found_issue)
		return __BLK_TA_ISSUE;
	if (trace->found_completion)
		return __BLK_TA_COMPLETE;

	return __BLK_TA_COMPLETE;
}

void add_tput(struct trace *trace, struct graph_line_data *writes_gld,
	      struct graph_line_data *reads_gld)
{
	struct blk_io_trace *io = trace->io;
	struct graph_line_data *gld;
	int action = io->action & BLK_TA_MASK;
	int seconds;

	if (io->action & BLK_TC_ACT(BLK_TC_NOTIFY))
		return;

	if (action != tput_event(trace))
		return;

	if (BLK_DATADIR(io->action) & BLK_TC_READ)
		gld = reads_gld;
	else
		gld = writes_gld;

	seconds = SECONDS(io->time);
	gld->data[seconds].sum += io->bytes;

	gld->data[seconds].count = 1;
	if (gld->data[seconds].sum > gld->max)
		gld->max = gld->data[seconds].sum;
}

#define GDD_PTR_ALLOC_STEP 16

static struct pid_map *get_pid_map(struct trace_file *tf, u32 pid)
{
	struct pid_map *pm;

	if (!io_per_process) {
		if (!tf->io_plots)
			tf->io_plots = 1;
		return NULL;
	}

	pm = process_hash_insert(pid, NULL);
	/* New entry? */
	if (!pm->index) {
		if (tf->io_plots == tf->io_plots_allocated) {
			tf->io_plots_allocated += GDD_PTR_ALLOC_STEP;
			tf->gdd_reads = realloc(tf->gdd_reads, tf->io_plots_allocated * sizeof(struct graph_dot_data *));
			if (!tf->gdd_reads)
				abort();
			tf->gdd_writes = realloc(tf->gdd_writes, tf->io_plots_allocated * sizeof(struct graph_dot_data *));
			if (!tf->gdd_writes)
				abort();
			memset(tf->gdd_reads + tf->io_plots_allocated - GDD_PTR_ALLOC_STEP,
			       0, GDD_PTR_ALLOC_STEP * sizeof(struct graph_dot_data *));
			memset(tf->gdd_writes + tf->io_plots_allocated - GDD_PTR_ALLOC_STEP,
			       0, GDD_PTR_ALLOC_STEP * sizeof(struct graph_dot_data *));
		}
		pm->index = tf->io_plots++;

		return pm;
	}
	return pm;
}

void add_io(struct trace *trace, struct trace_file *tf)
{
	struct blk_io_trace *io = trace->io;
	int action = io->action & BLK_TA_MASK;
	u64 offset;
	int index;
	char *label;
	struct pid_map *pm;

	if (io->action & BLK_TC_ACT(BLK_TC_NOTIFY))
		return;

	if (action != io_event(trace))
		return;

	offset = map_io(trace, io);

	pm = get_pid_map(tf, io->pid);
	if (!pm) {
		index = 0;
		label = "";
	} else {
		index = pm->index;
		label = pm->name;
	}
	if (BLK_DATADIR(io->action) & BLK_TC_READ) {
		if (!tf->gdd_reads[index])
			tf->gdd_reads[index] = alloc_dot_data(tf->min_seconds, tf->max_seconds, tf->min_offset, tf->max_offset, tf->stop_seconds, pick_color(), strdup(label));
		set_gdd_bit(tf->gdd_reads[index], offset, io->bytes, io->time);
	} else if (BLK_DATADIR(io->action) & BLK_TC_WRITE) {
		if (!tf->gdd_writes[index])
			tf->gdd_writes[index] = alloc_dot_data(tf->min_seconds, tf->max_seconds, tf->min_offset, tf->max_offset, tf->stop_seconds, pick_color(), strdup(label));
		set_gdd_bit(tf->gdd_writes[index], offset, io->bytes, io->time);
	}
}

void add_pending_io(struct trace *trace, struct graph_line_data *gld)
{
	unsigned int seconds;
	struct blk_io_trace *io = trace->io;
	int action = io->action & BLK_TA_MASK;
	double avg;
	struct pending_io *pio;

	if (io->action & BLK_TC_ACT(BLK_TC_NOTIFY))
		return;

	if (action == __BLK_TA_QUEUE) {
		if (io->sector == 0)
			return;
		if (trace->found_issue || trace->found_completion) {
			pio = hash_queued_io(trace->io);
			/*
			 * When there are no ISSUE events count depth and
			 * latency at least from queue events
			 */
			if (pio && !trace->found_issue) {
				pio->dispatch_time = io->time;
				goto account_io;
			}
		}
		return;
	}
	if (action == __BLK_TA_REQUEUE) {
		if (ios_in_flight > 0)
			ios_in_flight--;
		return;
	}
	if (action != __BLK_TA_ISSUE)
		return;

	pio = hash_dispatched_io(trace->io);
	if (!pio)
		return;

	if (!trace->found_completion) {
		list_del(&pio->hash_list);
		free(pio);
	}

account_io:
	ios_in_flight++;

	seconds = SECONDS(io->time);
	gld->data[seconds].sum += ios_in_flight;
	gld->data[seconds].count++;

	avg = (double)gld->data[seconds].sum / gld->data[seconds].count;
	if (gld->max < (u64)avg) {
		gld->max = avg;
	}
}

void add_completed_io(struct trace *trace,
		      struct graph_line_data *latency_gld)
{
	struct blk_io_trace *io = trace->io;
	int seconds;
	int action = io->action & BLK_TA_MASK;
	struct pending_io *pio;
	double avg;
	u64 latency;

	if (io->action & BLK_TC_ACT(BLK_TC_NOTIFY))
		return;

	if (action != __BLK_TA_COMPLETE)
		return;

	seconds = SECONDS(io->time);

	pio = hash_completed_io(trace->io);
	if (!pio)
		return;

	if (ios_in_flight > 0)
		ios_in_flight--;
	if (io->time >= pio->dispatch_time) {
		latency = io->time - pio->dispatch_time;
		latency_gld->data[seconds].sum += latency;
		latency_gld->data[seconds].count++;
	}

	list_del(&pio->hash_list);
	free(pio);

	avg = (double)latency_gld->data[seconds].sum /
		latency_gld->data[seconds].count;
	if (latency_gld->max < (u64)avg) {
		latency_gld->max = avg;
	}
}

void add_iop(struct trace *trace, struct graph_line_data *gld)
{
	struct blk_io_trace *io = trace->io;
	int action = io->action & BLK_TA_MASK;
	int seconds;

	if (io->action & BLK_TC_ACT(BLK_TC_NOTIFY))
		return;

	/* iops and tput use the same events */
	if (action != tput_event(trace))
		return;

	seconds = SECONDS(io->time);
	gld->data[seconds].sum += 1;
	gld->data[seconds].count = 1;
	if (gld->data[seconds].sum > gld->max)
		gld->max = gld->data[seconds].sum;
}

void check_record(struct trace *trace)
{
	handle_notify(trace);
}
