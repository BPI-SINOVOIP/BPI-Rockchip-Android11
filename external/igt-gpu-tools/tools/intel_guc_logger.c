/*
 * Copyright © 2014-2018 Intel Corporation
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

#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>

#include "igt.h"

#define MB(x) ((uint64_t)(x) * 1024 * 1024)
#ifndef PAGE_SIZE
  #define PAGE_SIZE 4096
#endif
/* Currently the size of GuC log buffer is 19 pages & so is the size of relay
 * subbuffer. If the size changes in future, then this define also needs to be
 * updated accordingly.
 */
#define SUBBUF_SIZE (19*PAGE_SIZE)
/* Need large buffering from logger side to hide the DISK IO latency, Driver
 * can only store 8 snapshots of GuC log buffer in relay.
 */
#define NUM_SUBBUFS 100

#define RELAY_FILE_NAME  "guc_log"
#define DEFAULT_OUTPUT_FILE_NAME  "guc_log_dump.dat"
#define CONTROL_FILE_NAME "i915_guc_log_control"

char *read_buffer;
char *out_filename;
int poll_timeout = 2; /* by default 2ms timeout */
pthread_mutex_t mutex;
pthread_t flush_thread;
int verbosity_level = 3; /* by default capture logs at max verbosity */
uint32_t produced, consumed;
uint64_t total_bytes_written;
int num_buffers = NUM_SUBBUFS;
int relay_fd, outfile_fd = -1;
uint32_t test_duration, max_filesize;
pthread_cond_t underflow_cond, overflow_cond;
bool stop_logging, discard_oldlogs, capturing_stopped;

static void guc_log_control(bool enable, uint32_t log_level)
{
	int control_fd;
	char data[19];
	uint64_t val;
	int ret;

	igt_assert_lte(log_level, 3);

	control_fd = igt_debugfs_open(-1, CONTROL_FILE_NAME, O_WRONLY);
	igt_assert_f(control_fd >= 0, "couldn't open the guc log control file\n");

	/*
	 * i915 expects GuC log level to be specified as:
	 * 0: disabled
	 * 1: enabled (verbosity level 0 = min)
	 * 2: enabled (verbosity level 1)
	 * 3: enabled (verbosity level 2)
	 * 4: enabled (verbosity level 3 = max)
	 */
	val = enable ? log_level + 1 : 0;

	ret = snprintf(data, sizeof(data), "0x%" PRIx64, val);
	igt_assert(ret > 2 && ret < sizeof(data));

	ret = write(control_fd, data, ret);
	igt_assert_f(ret > 0, "couldn't write to the log control file\n");

	close(control_fd);
}

static void int_sig_handler(int sig)
{
	igt_info("received signal %d\n", sig);

	stop_logging = true;
}

static void pull_leftover_data(void)
{
	unsigned int bytes_read = 0;
	int ret;

	do {
		/* Read the logs from relay buffer */
		ret = read(relay_fd, read_buffer, SUBBUF_SIZE);
		if (!ret)
			break;

		igt_assert_f(ret > 0, "failed to read from the guc log file\n");
		igt_assert_f(ret == SUBBUF_SIZE, "invalid read from relay file\n");

		bytes_read += ret;

		if (outfile_fd >= 0) {
			ret = write(outfile_fd, read_buffer, SUBBUF_SIZE);
			igt_assert_f(ret == SUBBUF_SIZE, "couldn't dump the logs in a file\n");
			total_bytes_written += ret;
		}
	} while(1);

	igt_debug("%u bytes flushed\n", bytes_read);
}

static int num_filled_bufs(void)
{
	return (produced - consumed);
}

static void pull_data(void)
{
	char *ptr;
	int ret;

	pthread_mutex_lock(&mutex);
	while (num_filled_bufs() >= num_buffers) {
		igt_debug("overflow, will wait, produced %u, consumed %u\n", produced, consumed);
		/* Stall the main thread in case of overflow, as there are no
		 * buffers available to store the new logs, otherwise there
		 * could be corruption if both threads work on the same buffer.
		 */
		pthread_cond_wait(&overflow_cond, &mutex);
	};
	pthread_mutex_unlock(&mutex);

	ptr = read_buffer + (produced % num_buffers) * SUBBUF_SIZE;

	/* Read the logs from relay buffer */
	ret = read(relay_fd, ptr, SUBBUF_SIZE);
	igt_assert_f(ret >= 0, "failed to read from the guc log file\n");
	igt_assert_f(!ret || ret == SUBBUF_SIZE, "invalid read from relay file\n");

	if (ret) {
		pthread_mutex_lock(&mutex);
		produced++;
		pthread_cond_signal(&underflow_cond);
		pthread_mutex_unlock(&mutex);
	} else {
		/* Occasionally (very rare) read from the relay file returns no
		 * data, albeit the polling done prior to read call indicated
		 * availability of data.
		 */
		igt_debug("no data read from the relay file\n");
	}
}

static void *flusher(void *arg)
{
	char *ptr;
	int ret;

	igt_debug("execution started of flusher thread\n");

	do {
		pthread_mutex_lock(&mutex);
		while (!num_filled_bufs()) {
			/* Exit only after completing the flush of all the filled
			 * buffers as User would expect that all logs captured up
			 * till the point of interruption/exit are written out to
			 * the disk file.
			 */
			if (capturing_stopped) {
				igt_debug("flusher to exit now\n");
				pthread_mutex_unlock(&mutex);
				return NULL;
			}
			pthread_cond_wait(&underflow_cond, &mutex);
		};
		pthread_mutex_unlock(&mutex);

		ptr = read_buffer + (consumed % num_buffers) * SUBBUF_SIZE;

		ret = write(outfile_fd, ptr, SUBBUF_SIZE);
		igt_assert_f(ret == SUBBUF_SIZE, "couldn't dump the logs in a file\n");

		total_bytes_written += ret;
		if (max_filesize && (total_bytes_written > MB(max_filesize))) {
			igt_debug("reached the target of %" PRIu64 " bytes\n", MB(max_filesize));
			stop_logging = true;
		}

		pthread_mutex_lock(&mutex);
		consumed++;
		pthread_cond_signal(&overflow_cond);
		pthread_mutex_unlock(&mutex);
	} while(1);

	return NULL;
}

static void init_flusher_thread(void)
{
	struct sched_param	thread_sched;
	pthread_attr_t		p_attr;
	int ret;

	pthread_cond_init(&underflow_cond, NULL);
	pthread_cond_init(&overflow_cond, NULL);
	pthread_mutex_init(&mutex, NULL);

	ret = pthread_attr_init(&p_attr);
	igt_assert_f(ret == 0, "error obtaining default thread attributes\n");

	ret = pthread_attr_setinheritsched(&p_attr, PTHREAD_EXPLICIT_SCHED);
	igt_assert_f(ret == 0, "couldn't set inheritsched\n");

	ret = pthread_attr_setschedpolicy(&p_attr, SCHED_RR);
	igt_assert_f(ret == 0, "couldn't set thread scheduling policy\n");

	/* Keep the flusher task also at rt priority, so that it doesn't get
	 * too late in flushing the collected logs in local buffers to the disk,
	 * and so main thread always have spare buffers to collect the logs.
	 */
	thread_sched.sched_priority = 5;
	ret = pthread_attr_setschedparam(&p_attr, &thread_sched);
	igt_assert_f(ret == 0, "couldn't set thread priority\n");

	ret = pthread_create(&flush_thread, &p_attr, flusher, NULL);
	igt_assert_f(ret == 0, "thread creation failed\n");

	ret = pthread_attr_destroy(&p_attr);
	igt_assert_f(ret == 0, "error destroying thread attributes\n");
}

static void open_relay_file(void)
{
	relay_fd = igt_debugfs_open(-1, RELAY_FILE_NAME, O_RDONLY);
	igt_assert_f(relay_fd >= 0, "couldn't open the guc log file\n");

	/* Purge the old/boot-time logs from the relay buffer.
	 * This is more for Val team's requirement, where they have to first
	 * purge the existing logs before starting the tests for which the logs
	 * are actually needed. After this logger will enter into a loop and
	 * wait for the new data, at that point benchmark can be launched from
	 * a different shell.
	 */
	if (discard_oldlogs)
		pull_leftover_data();
}

static void open_output_file(void)
{
	/* Use Direct IO mode for the output file, as the data written is not
	 * supposed to be accessed again, this saves a copy of data from App's
	 * buffer to kernel buffer (Page cache). Due to no buffering on kernel
	 * side, data is flushed out to disk faster and more buffering can be
	 * done on the logger side to hide the disk IO latency.
	 */
	outfile_fd = open(out_filename ? : DEFAULT_OUTPUT_FILE_NAME,
			  O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT,
			  0440);
	igt_assert_f(outfile_fd >= 0, "couldn't open the output file\n");

	free(out_filename);
}

static void init_main_thread(void)
{
	struct sched_param	thread_sched;
	int ret;

	/* Run the main thread at highest priority to ensure that it always
	 * gets woken-up at earliest on arrival of new data and so is always
	 * ready to pull the logs, otherwise there could be loss logs if
	 * GuC firmware is generating logs at a very high rate.
	 */
	thread_sched.sched_priority = 1;
	ret = sched_setscheduler(getpid(), SCHED_FIFO, &thread_sched);
	igt_assert_f(ret == 0, "couldn't set the priority\n");

	if (signal(SIGINT, int_sig_handler) == SIG_ERR)
		igt_assert_f(0, "SIGINT handler registration failed\n");

	if (signal(SIGALRM, int_sig_handler) == SIG_ERR)
		igt_assert_f(0, "SIGALRM handler registration failed\n");

	/* Need an aligned pointer for direct IO */
	ret = posix_memalign((void **)&read_buffer, PAGE_SIZE, num_buffers * SUBBUF_SIZE);
	igt_assert_f(ret == 0, "couldn't allocate the read buffer\n");

	/* Keep the pages locked in RAM, avoid page fault overhead */
	ret = mlock(read_buffer, num_buffers * SUBBUF_SIZE);
	igt_assert_f(ret == 0, "failed to lock memory\n");

	/* Enable the logging, it may not have been enabled from boot and so
	 * the relay file also wouldn't have been created.
	 */
	guc_log_control(true, verbosity_level);

	open_relay_file();
	open_output_file();
}

static int parse_options(int opt, int opt_index, void *data)
{
	igt_debug("opt %c optarg %s\n", opt, optarg);

	switch(opt) {
	case 'v':
		verbosity_level = atoi(optarg);
		igt_assert_f(verbosity_level >= 0 && verbosity_level <= 3, "invalid input for -v option\n");
		igt_debug("verbosity level to be used is %d\n", verbosity_level);
		break;
	case 'o':
		out_filename = strdup(optarg);
		igt_assert_f(out_filename, "Couldn't allocate the o/p filename\n");
		igt_debug("logs to be stored in file %s\n", out_filename);
		break;
	case 'b':
		num_buffers = atoi(optarg);
		igt_assert_f(num_buffers > 0, "invalid input for -b option\n");
		igt_debug("number of buffers to be used is %d\n", num_buffers);
		break;
	case 't':
		test_duration = atoi(optarg);
		igt_assert_f(test_duration > 0, "invalid input for -t option\n");
		igt_debug("logger to run for %d second\n", test_duration);
		break;
	case 'p':
		poll_timeout = atoi(optarg);
		igt_assert_f(poll_timeout != 0, "invalid input for -p option\n");
		if (poll_timeout > 0)
			igt_debug("polling to be done with %d millisecond timeout\n", poll_timeout);
		break;
	case 's':
		max_filesize = atoi(optarg);
		igt_assert_f(max_filesize > 0, "invalid input for -s option\n");
		igt_debug("max allowed size of the output file is %d MB\n", max_filesize);
		break;
	case 'd':
		discard_oldlogs = true;
		igt_debug("old/boot-time logs will be discarded\n");
		break;
	}

	return 0;
}

static void process_command_line(int argc, char **argv)
{
	static struct option long_options[] = {
		{"verbosity", required_argument, 0, 'v'},
		{"outputfile", required_argument, 0, 'o'},
		{"buffers", required_argument, 0, 'b'},
		{"testduration", required_argument, 0, 't'},
		{"polltimeout", required_argument, 0, 'p'},
		{"size", required_argument, 0, 's'},
		{"discard", no_argument, 0, 'd'},
		{ 0, 0, 0, 0 }
	};

	const char *help =
		"  -v --verbosity=level   verbosity level of GuC logging (0-3)\n"
		"  -o --outputfile=name   name of the output file, including the location, where logs will be stored\n"
		"  -b --buffers=num       number of buffers to be maintained on logger side for storing logs\n"
		"  -t --testduration=sec  max duration in seconds for which the logger should run\n"
		"  -p --polltimeout=ms    polling timeout in ms, -1 == indefinite wait for the new data\n"
		"  -s --size=MB           max size of output file in MBs after which logging will be stopped\n"
		"  -d --discard           discard the old/boot-time logs before entering into the capture loop\n";

	igt_simple_init_parse_opts(&argc, argv, "v:o:b:t:p:s:d", long_options,
				   help, parse_options, NULL);
}

int main(int argc, char **argv)
{
	struct pollfd relay_poll_fd;
	int nfds;
	int ret;

	process_command_line(argc, argv);

	init_main_thread();

	/* Use a separate thread for flushing the logs to a file on disk.
	 * Main thread will buffer the data from relay file in its pool of
	 * buffers and other thread will flush the data to disk in background.
	 * This is needed, albeit by default data is written out to disk in
	 * async mode, as when there are too many dirty pages in the RAM,
	 * (/proc/sys/vm/dirty_ratio), kernel starts blocking the processes
	 * doing the file writes.
	 */
	init_flusher_thread();

	relay_poll_fd.fd = relay_fd;
	relay_poll_fd.events = POLLIN;
	relay_poll_fd.revents = 0;

	nfds = 1; /* only one fd to poll */

	alarm(test_duration); /* Start the alarm */

	do {
		/* Wait/poll for the new data to be available, relay doesn't
		 * provide a blocking read.
		 * On older kernels need to do polling with a timeout instead of
		 * indefinite wait to avoid relying on relay for the wakeup, as
		 * relay used to do the wakeup in a deferred manner on jiffies
		 * granularity by scheduling a timer and moreover that timer was
		 * re-scheduled on every newly produced buffer and so was pushed
		 * out if there were multiple flush interrupts in a very quick
		 * succession (less than a jiffy gap between 2 flush interrupts)
		 * causing relay to run out of sub buffers to store new logs.
		 */
		ret = poll(&relay_poll_fd, nfds, poll_timeout);
		if (ret < 0) {
			if (errno == EINTR)
				break;
			igt_assert_f(0, "poll call failed\n");
		}

		/* No data available yet, poll again, hopefully new data is round the corner */
		if (!relay_poll_fd.revents)
			continue;

		pull_data();
	} while (!stop_logging);

	/* Pause logging on the GuC side */
	guc_log_control(false, 0);

	/* Signal flusher thread to make an exit */
	capturing_stopped = 1;
	pthread_cond_signal(&underflow_cond);
	pthread_join(flush_thread, NULL);

	pull_leftover_data();
	igt_info("total bytes written %" PRIu64 "\n", total_bytes_written);

	free(read_buffer);
	close(relay_fd);
	close(outfile_fd);
	igt_exit();
}
