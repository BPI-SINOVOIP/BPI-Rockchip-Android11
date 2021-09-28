#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "igt_core.h"
#include "executor.h"
#include "output_strings.h"

static struct {
	int *fds;
	size_t num_dogs;
} watchdogs;

static void __close_watchdog(int fd)
{
	ssize_t ret = write(fd, "V", 1);

	if (ret == -1)
		fprintf(stderr, "Failed to stop a watchdog: %s\n",
			strerror(errno));

	close(fd);
}

static void close_watchdogs(struct settings *settings)
{
	size_t i;

	if (settings && settings->log_level >= LOG_LEVEL_VERBOSE)
		printf("Closing watchdogs\n");

	if (settings == NULL && watchdogs.num_dogs != 0)
		fprintf(stderr, "Closing watchdogs from exit handler!\n");

	for (i = 0; i < watchdogs.num_dogs; i++) {
		__close_watchdog(watchdogs.fds[i]);
	}

	free(watchdogs.fds);
	watchdogs.num_dogs = 0;
	watchdogs.fds = NULL;
}

static void close_watchdogs_atexit(void)
{
	close_watchdogs(NULL);
}

static void init_watchdogs(struct settings *settings)
{
	int i;
	char name[32];
	int fd;

	memset(&watchdogs, 0, sizeof(watchdogs));

	if (!settings->use_watchdog || settings->inactivity_timeout <= 0)
		return;

	if (settings->log_level >= LOG_LEVEL_VERBOSE) {
		printf("Initializing watchdogs\n");
	}

	atexit(close_watchdogs_atexit);

	for (i = 0; ; i++) {
		snprintf(name, sizeof(name), "/dev/watchdog%d", i);
		if ((fd = open(name, O_RDWR | O_CLOEXEC)) < 0)
			break;

		watchdogs.num_dogs++;
		watchdogs.fds = realloc(watchdogs.fds, watchdogs.num_dogs * sizeof(int));
		watchdogs.fds[i] = fd;

		if (settings->log_level >= LOG_LEVEL_VERBOSE)
			printf(" %s\n", name);
	}
}

static int watchdogs_set_timeout(int timeout)
{
	size_t i;
	int orig_timeout = timeout;

	for (i = 0; i < watchdogs.num_dogs; i++) {
		if (ioctl(watchdogs.fds[i], WDIOC_SETTIMEOUT, &timeout)) {
			__close_watchdog(watchdogs.fds[i]);
			watchdogs.fds[i] = -1;
			continue;
		}

		if (timeout < orig_timeout) {
			/*
			 * Timeout of this caliber refused. We want to
			 * use the same timeout for all devices.
			 */
			return watchdogs_set_timeout(timeout);
		}
	}

	return timeout;
}

static void ping_watchdogs(void)
{
	size_t i;
	int ret;

	for (i = 0; i < watchdogs.num_dogs; i++) {
		ret = ioctl(watchdogs.fds[i], WDIOC_KEEPALIVE, NULL);

		if (ret == -1)
			fprintf(stderr, "Failed to ping a watchdog: %s\n",
				strerror(errno));
	}
}

static char *handle_lockdep(void)
{
	const char *header = "Lockdep not active\n\n/proc/lockdep_stats contents:\n";
	int fd = open("/proc/lockdep_stats", O_RDONLY);
	const char *debug_locks_line = " debug_locks:";
	char buf[4096], *p;
	ssize_t bufsize = 0;
	int val;

	if (fd < 0)
		return NULL;

	strcpy(buf, header);

	if ((bufsize = read(fd, buf + strlen(header), sizeof(buf) - strlen(header) - 1)) < 0)
		return NULL;
	bufsize += strlen(header);
	buf[bufsize] = '\0';
	close(fd);

	if ((p = strstr(buf, debug_locks_line)) != NULL &&
	    sscanf(p + strlen(debug_locks_line), "%d", &val) == 1 &&
	    val != 1) {
		return strdup(buf);
	}

	return NULL;
}

/* see Linux's include/linux/kernel.h */
static const struct {
	unsigned long bit;
	const char *explanation;
} abort_taints[] = {
  {(1 << 5), "TAINT_BAD_PAGE: Bad page reference or an unexpected page flags."},
  {(1 << 7), "TAINT_DIE: Kernel has died - BUG/OOPS."},
  {(1 << 9), "TAINT_WARN: WARN_ON has happened."},
  {0, 0}};

static unsigned long tainted(unsigned long *taints)
{
	FILE *f;
	unsigned long bad_taints = 0;

	for (typeof(*abort_taints) *taint = abort_taints; taint->bit; taint++)
		bad_taints |= taint->bit;

	*taints = 0;

	f = fopen("/proc/sys/kernel/tainted", "r");
	if (f) {
		fscanf(f, "%lu", taints);
		fclose(f);
	}

	return *taints & bad_taints;
}

static char *handle_taint(void)
{
	unsigned long taints;
	char *reason;

	if (!tainted(&taints))
		return NULL;

	asprintf(&reason, "Kernel badly tainted (%#lx) (check dmesg for details):\n",
		 taints);

	for (typeof(*abort_taints) *taint = abort_taints; taint->bit; taint++) {
		if (taint->bit & taints) {
			char *old_reason = reason;
			asprintf(&reason, "%s\t(%#lx) %s\n",
					old_reason,
					taint->bit,
					taint->explanation);
			free(old_reason);
		}
	}

	return reason;
}

static const struct {
	int condition;
	char *(*handler)(void);
} abort_handlers[] = {
	{ ABORT_LOCKDEP, handle_lockdep },
	{ ABORT_TAINT, handle_taint },
	{ 0, 0 },
};

static char *need_to_abort(const struct settings* settings)
{
	typeof(*abort_handlers) *it;

	for (it = abort_handlers; it->condition; it++) {
		char *abort;

		if (!(settings->abort_mask & it->condition))
			continue;

		abort = it->handler();
		if (!abort)
			continue;

		if (settings->log_level >= LOG_LEVEL_NORMAL)
			fprintf(stderr, "Aborting: %s\n", abort);

		return abort;
	}

	return NULL;
}

static void prune_subtest(struct job_list_entry *entry, char *subtest)
{
	char *excl;

	/*
	 * Subtest pruning is done by adding exclusion strings to the
	 * subtest list. The last matching item on the subtest
	 * selection command line flag decides whether to run a
	 * subtest, see igt_core.c for details.  If the list is empty,
	 * the expected subtest set is unknown, so we need to add '*'
	 * first so we can start excluding.
	 */

	if (entry->subtest_count == 0) {
		entry->subtest_count++;
		entry->subtests = realloc(entry->subtests, entry->subtest_count * sizeof(*entry->subtests));
		entry->subtests[0] = strdup("*");
	}

	excl = malloc(strlen(subtest) + 2);
	excl[0] = '!';
	strcpy(excl + 1, subtest);

	entry->subtest_count++;
	entry->subtests = realloc(entry->subtests, entry->subtest_count * sizeof(*entry->subtests));
	entry->subtests[entry->subtest_count - 1] = excl;
}

static bool prune_from_journal(struct job_list_entry *entry, int fd)
{
	char *subtest;
	FILE *f;
	size_t pruned = 0;
	size_t old_count = entry->subtest_count;

	/*
	 * Each journal line is a subtest that has been started, or
	 * the line 'exit:$exitcode (time)', or 'timeout:$exitcode (time)'.
	 */

	f = fdopen(fd, "r");
	if (!f)
		return false;

	while (fscanf(f, "%ms", &subtest) == 1) {
		if (!strncmp(subtest, EXECUTOR_EXIT, strlen(EXECUTOR_EXIT))) {
			/* Fully done. Mark that by making the binary name invalid. */
			fscanf(f, " (%*fs)");
			entry->binary[0] = '\0';
			free(subtest);
			continue;
		}

		if (!strncmp(subtest, EXECUTOR_TIMEOUT, strlen(EXECUTOR_TIMEOUT))) {
			fscanf(f, " (%*fs)");
			free(subtest);
			continue;
		}

		prune_subtest(entry, subtest);

		free(subtest);
		pruned++;
	}

	fclose(f);

	/*
	 * If we know the subtests we originally wanted to run, check
	 * if we got an equal amount already.
	 */
	if (old_count > 0 && pruned >= old_count)
		entry->binary[0] = '\0';

	return pruned > 0;
}

static const char *filenames[_F_LAST] = {
	[_F_JOURNAL] = "journal.txt",
	[_F_OUT] = "out.txt",
	[_F_ERR] = "err.txt",
	[_F_DMESG] = "dmesg.txt",
};

static int open_at_end(int dirfd, const char *name)
{
	int fd = openat(dirfd, name, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
	char last;

	if (fd >= 0) {
		if (lseek(fd, -1, SEEK_END) >= 0 &&
		    read(fd, &last, 1) == 1 &&
		    last != '\n') {
			write(fd, "\n", 1);
		}
		lseek(fd, 0, SEEK_END);
	}

	return fd;
}

static int open_for_reading(int dirfd, const char *name)
{
	return openat(dirfd, name, O_RDONLY);
}

bool open_output_files(int dirfd, int *fds, bool write)
{
	int i;
	int (*openfunc)(int, const char*) = write ? open_at_end : open_for_reading;

	for (i = 0; i < _F_LAST; i++) {
		if ((fds[i] = openfunc(dirfd, filenames[i])) < 0) {
			while (--i >= 0)
				close(fds[i]);
			return false;
		}
	}

	return true;
}

void close_outputs(int *fds)
{
	int i;

	for (i = 0; i < _F_LAST; i++) {
		close(fds[i]);
	}
}

static void dump_dmesg(int kmsgfd, int outfd)
{
	/*
	 * Write kernel messages to the log file until we reach
	 * 'now'. Unfortunately, /dev/kmsg doesn't support seeking to
	 * -1 from SEEK_END so we need to use a second fd to read a
	 * message to match against, or stop when we reach EAGAIN.
	 */

	int comparefd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
	unsigned flags;
	unsigned long long seq, cmpseq, usec;
	char cont;
	char buf[2048];
	ssize_t r;

	if (comparefd < 0)
		return;
	lseek(comparefd, 0, SEEK_END);

	if (fcntl(kmsgfd, F_SETFL, O_NONBLOCK)) {
		close(comparefd);
		return;
	}

	while (1) {
		if (comparefd >= 0) {
			r = read(comparefd, buf, sizeof(buf) - 1);
			if (r < 0) {
				if (errno != EAGAIN && errno != EPIPE) {
					close(comparefd);
					return;
				}
			} else {
				buf[r] = '\0';
				if (sscanf(buf, "%u,%llu,%llu,%c;",
					   &flags, &cmpseq, &usec, &cont) == 4) {
					/* Reading comparison record done. */
					close(comparefd);
					comparefd = -1;
				}
			}
		}

		r = read(kmsgfd, buf, sizeof(buf));
		if (r <= 0) {
			if (errno == EPIPE)
				continue;

			/*
			 * If EAGAIN, we're done. If some other error,
			 * we can't do anything anyway.
			 */
			close(comparefd);
			return;
		}

		write(outfd, buf, r);

		if (comparefd < 0 && sscanf(buf, "%u,%llu,%llu,%c;",
					    &flags, &seq, &usec, &cont) == 4) {
			/*
			 * Comparison record has been read, compare
			 * the sequence number to see if we have read
			 * enough.
			 */
			if (seq >= cmpseq)
				return;
		}
	}
}

static bool kill_child(int sig, pid_t child)
{
	/*
	 * Send the signal to the child directly, and to the child's
	 * process group.
	 */
	kill(-child, sig);
	if (kill(child, sig) && errno == ESRCH) {
		fprintf(stderr, "Child process does not exist. This shouldn't happen.\n");
		return false;
	}

	return true;
}

/*
 * Returns:
 *  =0 - Success
 *  <0 - Failure executing
 *  >0 - Timeout happened, need to recreate from journal
 */
static int monitor_output(pid_t child,
			   int outfd, int errfd, int kmsgfd, int sigfd,
			   int *outputs,
			   double *time_spent,
			   struct settings *settings)
{
	fd_set set;
	char buf[2048];
	char *outbuf = NULL;
	size_t outbufsize = 0;
	char current_subtest[256] = {};
	struct signalfd_siginfo siginfo;
	ssize_t s;
	int n, status;
	int nfds = outfd;
	int timeout = settings->inactivity_timeout;
	int timeout_intervals = 1, intervals_left;
	int wd_extra = 10;
	int killed = 0; /* 0 if not killed, signal number otherwise */
	struct timespec time_beg, time_end;
	unsigned long taints = 0;
	bool aborting = false;

	igt_gettime(&time_beg);

	if (errfd > nfds)
		nfds = errfd;
	if (kmsgfd > nfds)
		nfds = kmsgfd;
	if (sigfd > nfds)
		nfds = sigfd;
	nfds++;

	if (timeout > 0) {
		/*
		 * Use original timeout plus some leeway. If we're still
		 * alive, we want to kill the test process instead of cutting
		 * power.
		 */
		int wd_timeout = watchdogs_set_timeout(timeout + wd_extra);

		if (wd_timeout < timeout + wd_extra) {
			/* Watchdog timeout smaller, so ping it more often */
			if (wd_timeout - wd_extra < 0)
				wd_extra = wd_timeout / 2;
			timeout_intervals = timeout / (wd_timeout - wd_extra);
			timeout /= timeout_intervals;

			if (settings->log_level >= LOG_LEVEL_VERBOSE) {
				printf("Watchdog doesn't support the timeout we requested (shortened to %d seconds).\n"
				       "Using %d intervals of %d seconds.\n",
				       wd_timeout, timeout_intervals, timeout);
			}
		}
	}

	intervals_left = timeout_intervals;

	while (outfd >= 0 || errfd >= 0 || sigfd >= 0) {
		struct timeval tv = { .tv_sec = timeout };

		FD_ZERO(&set);
		if (outfd >= 0)
			FD_SET(outfd, &set);
		if (errfd >= 0)
			FD_SET(errfd, &set);
		if (kmsgfd >= 0)
			FD_SET(kmsgfd, &set);
		if (sigfd >= 0)
			FD_SET(sigfd, &set);

		n = select(nfds, &set, NULL, NULL, timeout == 0 ? NULL : &tv);
		if (n < 0) {
			/* TODO */
			return -1;
		}

		if (n == 0) {
			if (--intervals_left)
				continue;

			ping_watchdogs();

			switch (killed) {
			case 0:
				if (settings->log_level >= LOG_LEVEL_NORMAL) {
					printf("Timeout. Killing the current test with SIGQUIT.\n");
					fflush(stdout);
				}

				killed = SIGQUIT;
				if (!kill_child(killed, child))
					return -1;

				/*
				 * Now continue the loop and let the
				 * dying child be handled normally.
				 */
				timeout = 20;
				watchdogs_set_timeout(120);
				intervals_left = timeout_intervals = 1;
				break;
			case SIGQUIT:
				if (settings->log_level >= LOG_LEVEL_NORMAL) {
					printf("Timeout. Killing the current test with SIGKILL.\n");
					fflush(stdout);
				}

				killed = SIGKILL;
				if (!kill_child(killed, child))
					return -1;

				intervals_left = timeout_intervals = 1;
				break;
			case SIGKILL:
				/*
				 * If the child still exists, and the kernel
				 * hasn't oopsed, assume it is still making
				 * forward progress towards exiting (i.e. still
				 * freeing all of its resources).
				 */
				if (kill(child, 0) == 0 && !tainted(&taints)) {
					intervals_left =  1;
					break;
				}

				/* Nothing that can be done, really. Let's tell the caller we want to abort. */
				if (settings->log_level >= LOG_LEVEL_NORMAL) {
					fprintf(stderr, "Child refuses to die, tainted %lx. Aborting.\n",
						taints);
				}
				close_watchdogs(settings);
				free(outbuf);
				close(outfd);
				close(errfd);
				close(kmsgfd);
				return -1;
			}

			continue;
		}

		intervals_left = timeout_intervals;
		ping_watchdogs();

		/* TODO: Refactor these handlers to their own functions */
		if (outfd >= 0 && FD_ISSET(outfd, &set)) {
			char *newline;

			s = read(outfd, buf, sizeof(buf));
			if (s <= 0) {
				if (s < 0) {
					fprintf(stderr, "Error reading test's stdout: %s\n",
						strerror(errno));
				}

				close(outfd);
				outfd = -1;
				goto out_end;
			}

			write(outputs[_F_OUT], buf, s);
			if (settings->sync) {
				fdatasync(outputs[_F_OUT]);
			}

			outbuf = realloc(outbuf, outbufsize + s);
			memcpy(outbuf + outbufsize, buf, s);
			outbufsize += s;

			while ((newline = memchr(outbuf, '\n', outbufsize)) != NULL) {
				size_t linelen = newline - outbuf + 1;

				if (linelen > strlen(STARTING_SUBTEST) &&
				    !memcmp(outbuf, STARTING_SUBTEST, strlen(STARTING_SUBTEST))) {
					write(outputs[_F_JOURNAL], outbuf + strlen(STARTING_SUBTEST),
					      linelen - strlen(STARTING_SUBTEST));
					memcpy(current_subtest, outbuf + strlen(STARTING_SUBTEST),
					       linelen - strlen(STARTING_SUBTEST));
					current_subtest[linelen - strlen(STARTING_SUBTEST)] = '\0';

					if (settings->log_level >= LOG_LEVEL_VERBOSE) {
						fwrite(outbuf, 1, linelen, stdout);
					}
				}
				if (linelen > strlen(SUBTEST_RESULT) &&
				    !memcmp(outbuf, SUBTEST_RESULT, strlen(SUBTEST_RESULT))) {
					char *delim = memchr(outbuf, ':', linelen);

					if (delim != NULL) {
						size_t subtestlen = delim - outbuf - strlen(SUBTEST_RESULT);
						if (memcmp(current_subtest, outbuf + strlen(SUBTEST_RESULT),
							   subtestlen)) {
							/* Result for a test that didn't ever start */
							write(outputs[_F_JOURNAL],
							      outbuf + strlen(SUBTEST_RESULT),
							      subtestlen);
							write(outputs[_F_JOURNAL], "\n", 1);
							if (settings->sync) {
								fdatasync(outputs[_F_JOURNAL]);
							}
							current_subtest[0] = '\0';
						}

						if (settings->log_level >= LOG_LEVEL_VERBOSE) {
							fwrite(outbuf, 1, linelen, stdout);
						}
					}
				}

				memmove(outbuf, newline + 1, outbufsize - linelen);
				outbufsize -= linelen;
			}
		}
	out_end:

		if (errfd >= 0 && FD_ISSET(errfd, &set)) {
			s = read(errfd, buf, sizeof(buf));
			if (s <= 0) {
				if (s < 0) {
					fprintf(stderr, "Error reading test's stderr: %s\n",
						strerror(errno));
				}
				close(errfd);
				errfd = -1;
			} else {
				write(outputs[_F_ERR], buf, s);
				if (settings->sync) {
					fdatasync(outputs[_F_ERR]);
				}
			}
		}

		if (kmsgfd >= 0 && FD_ISSET(kmsgfd, &set)) {
			s = read(kmsgfd, buf, sizeof(buf));
			if (s < 0) {
				if (errno != EPIPE && errno != EINVAL) {
					fprintf(stderr, "Error reading from kmsg, stopping monitoring: %s\n",
						strerror(errno));
					close(kmsgfd);
					kmsgfd = -1;
				} else if (errno == EINVAL) {
					fprintf(stderr, "Warning: Buffer too small for kernel log record, record lost.\n");
				}
			} else {
				write(outputs[_F_DMESG], buf, s);
				if (settings->sync) {
					fdatasync(outputs[_F_DMESG]);
				}
			}
		}

		if (sigfd >= 0 && FD_ISSET(sigfd, &set)) {
			double time;

			s = read(sigfd, &siginfo, sizeof(siginfo));
			if (s < 0) {
				fprintf(stderr, "Error reading from signalfd: %s\n",
					strerror(errno));
				continue;
			} else if (siginfo.ssi_signo == SIGCHLD) {
				if (child != waitpid(child, &status, WNOHANG)) {
					fprintf(stderr, "Failed to reap child\n");
					status = 9999;
				} else if (WIFEXITED(status)) {
					status = WEXITSTATUS(status);
					if (status >= 128) {
						status = 128 - status;
					}
				} else if (WIFSIGNALED(status)) {
					status = -WTERMSIG(status);
				} else {
					status = 9999;
				}
			} else {
				/* We're dying, so we're taking them with us */
				if (settings->log_level >= LOG_LEVEL_NORMAL)
					printf("Abort requested via %s, terminating children\n",
					       strsignal(siginfo.ssi_signo));

				aborting = true;
				timeout = 2;
				killed = SIGQUIT;
				if (!kill_child(killed, child))
					return -1;

				continue;
			}

			igt_gettime(&time_end);

			time = igt_time_elapsed(&time_beg, &time_end);
			if (time < 0.0)
				time = 0.0;

			if (!aborting) {
				dprintf(outputs[_F_JOURNAL], "%s%d (%.3fs)\n",
					killed ? EXECUTOR_TIMEOUT : EXECUTOR_EXIT,
					status, time);
				if (settings->sync) {
					fdatasync(outputs[_F_JOURNAL]);
				}

				if (time_spent)
					*time_spent = time;
			}

			child = 0;
			sigfd = -1; /* we are dying, no signal handling for now */
		}
	}

	dump_dmesg(kmsgfd, outputs[_F_DMESG]);
	if (settings->sync)
		fdatasync(outputs[_F_DMESG]);

	free(outbuf);
	close(outfd);
	close(errfd);
	close(kmsgfd);

	if (aborting)
		return -1;

	return killed;
}

static void __attribute__((noreturn))
execute_test_process(int outfd, int errfd,
		     struct settings *settings,
		     struct job_list_entry *entry)
{
	char *argv[4] = {};
	size_t rootlen;

	dup2(outfd, STDOUT_FILENO);
	dup2(errfd, STDERR_FILENO);

	setpgid(0, 0);

	rootlen = strlen(settings->test_root);
	argv[0] = malloc(rootlen + strlen(entry->binary) + 2);
	strcpy(argv[0], settings->test_root);
	argv[0][rootlen] = '/';
	strcpy(argv[0] + rootlen + 1, entry->binary);

	if (entry->subtest_count) {
		size_t argsize;
		size_t i;

		argv[1] = strdup("--run-subtest");
		argsize = strlen(entry->subtests[0]);
		argv[2] = malloc(argsize + 1);
		strcpy(argv[2], entry->subtests[0]);

		for (i = 1; i < entry->subtest_count; i++) {
			char *sub = entry->subtests[i];
			size_t sublen = strlen(sub);

			argv[2] = realloc(argv[2], argsize + sublen + 2);
			argv[2][argsize] = ',';
			strcpy(argv[2] + argsize + 1, sub);
			argsize += sublen + 1;
		}
	}

	execv(argv[0], argv);
	fprintf(stderr, "Cannot execute %s\n", argv[0]);
	exit(IGT_EXIT_INVALID);
}

static int digits(size_t num)
{
	int ret = 0;
	while (num) {
		num /= 10;
		ret++;
	}

	if (ret == 0) ret++;
	return ret;
}

static void print_time_left(struct execute_state *state,
			    struct settings *settings)
{
	int width;

	if (settings->overall_timeout <= 0)
		return;

	width = digits(settings->overall_timeout);
	printf("(%*.0fs left) ", width, state->time_left);
}

static char *entry_display_name(struct job_list_entry *entry)
{
	size_t size = strlen(entry->binary) + 1;
	char *ret = malloc(size);

	sprintf(ret, "%s", entry->binary);

	if (entry->subtest_count > 0) {
		size_t i;
		const char *delim = "";

		size += 3; /* strlen(" (") + strlen(")") */
		ret = realloc(ret, size);
		strcat(ret, " (");

		for (i = 0; i < entry->subtest_count; i++) {
			size += strlen(delim) + strlen(entry->subtests[i]);
			ret = realloc(ret, size);

			strcat(ret, delim);
			strcat(ret, entry->subtests[i]);

			delim = ", ";
		}
		/* There's already room for this */
		strcat(ret, ")");
	}

	return ret;
}

/*
 * Returns:
 *  =0 - Success
 *  <0 - Failure executing
 *  >0 - Timeout happened, need to recreate from journal
 */
static int execute_next_entry(struct execute_state *state,
			      size_t total,
			      double *time_spent,
			      struct settings *settings,
			      struct job_list_entry *entry,
			      int testdirfd, int resdirfd,
			      int sigfd, sigset_t *sigmask)
{
	int dirfd;
	int outputs[_F_LAST];
	int kmsgfd;
	int outpipe[2] = { -1, -1 };
	int errpipe[2] = { -1, -1 };
	int outfd, errfd;
	char name[32];
	pid_t child;
	int result;
	size_t idx = state->next;

	snprintf(name, sizeof(name), "%zd", idx);
	mkdirat(resdirfd, name, 0777);
	if ((dirfd = openat(resdirfd, name, O_DIRECTORY | O_RDONLY | O_CLOEXEC)) < 0) {
		fprintf(stderr, "Error accessing individual test result directory\n");
		return -1;
	}

	if (!open_output_files(dirfd, outputs, true)) {
		fprintf(stderr, "Error opening output files\n");
		result = -1;
		goto out_dirfd;
	}

	if (settings->sync) {
		fsync(dirfd);
		fsync(resdirfd);
	}

	if (pipe(outpipe) || pipe(errpipe)) {
		fprintf(stderr, "Error creating pipes: %s\n", strerror(errno));
		result = -1;
		goto out_pipe;
	}

	if ((kmsgfd = open("/dev/kmsg", O_RDONLY | O_CLOEXEC)) < 0) {
		fprintf(stderr, "Warning: Cannot open /dev/kmsg\n");
	} else {
		/* TODO: Checking of abort conditions in pre-execute dmesg */
		lseek(kmsgfd, 0, SEEK_END);
	}


	if (settings->log_level >= LOG_LEVEL_NORMAL) {
		char *displayname;
		int width = digits(total);
		printf("[%0*zd/%0*zd] ", width, idx + 1, width, total);

		print_time_left(state, settings);

		displayname = entry_display_name(entry);
		printf("%s", displayname);
		free(displayname);

		printf("\n");
	}

	/*
	 * Flush outputs before forking so our (buffered) output won't
	 * end up in the test outputs.
	 */
	fflush(stdout);
	fflush(stderr);

	child = fork();
	if (child < 0) {
		fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
		result = -1;
		goto out_kmsgfd;
	} else if (child == 0) {
		outfd = outpipe[1];
		errfd = errpipe[1];
		close(outpipe[0]);
		close(errpipe[0]);

		sigprocmask(SIG_UNBLOCK, sigmask, NULL);

		setenv("IGT_SENTINEL_ON_STDERR", "1", 1);

		execute_test_process(outfd, errfd, settings, entry);
		/* unreachable */
	}

	outfd = outpipe[0];
	errfd = errpipe[0];
	close(outpipe[1]);
	close(errpipe[1]);
	outpipe[1] = errpipe[1] = -1;

	result = monitor_output(child, outfd, errfd, kmsgfd, sigfd,
				outputs, time_spent, settings);

out_kmsgfd:
	close(kmsgfd);
out_pipe:
	close_outputs(outputs);
	close(outpipe[0]);
	close(outpipe[1]);
	close(errpipe[0]);
	close(errpipe[1]);
	close_outputs(outputs);
out_dirfd:
	close(dirfd);

	return result;
}

static int remove_file(int dirfd, const char *name)
{
	return unlinkat(dirfd, name, 0) && errno != ENOENT;
}

static bool clear_test_result_directory(int dirfd)
{
	int i;

	for (i = 0; i < _F_LAST; i++) {
		if (remove_file(dirfd, filenames[i])) {
			fprintf(stderr, "Error deleting %s from test result directory: %s\n",
				filenames[i],
				strerror(errno));
			return false;
		}
	}

	return true;
}

static bool clear_old_results(char *path)
{
	int dirfd;
	size_t i;

	if ((dirfd = open(path, O_DIRECTORY | O_RDONLY)) < 0) {
		if (errno == ENOENT) {
			/* Successfully cleared if it doesn't even exist */
			return true;
		}

		fprintf(stderr, "Error clearing old results: %s\n", strerror(errno));
		return false;
	}

	if (remove_file(dirfd, "uname.txt") ||
	    remove_file(dirfd, "starttime.txt") ||
	    remove_file(dirfd, "endtime.txt") ||
	    remove_file(dirfd, "aborted.txt")) {
		close(dirfd);
		fprintf(stderr, "Error clearing old results: %s\n", strerror(errno));
		return false;
	}

	for (i = 0; true; i++) {
		char name[32];
		int resdirfd;

		snprintf(name, sizeof(name), "%zd", i);
		if ((resdirfd = openat(dirfd, name, O_DIRECTORY | O_RDONLY)) < 0)
			break;

		if (!clear_test_result_directory(resdirfd)) {
			close(resdirfd);
			close(dirfd);
			return false;
		}
		close(resdirfd);
		if (unlinkat(dirfd, name, AT_REMOVEDIR)) {
			fprintf(stderr,
				"Warning: Result directory %s contains extra files\n",
				name);
		}
	}

	close(dirfd);

	return true;
}

static double timeofday_double(void)
{
	struct timeval tv;

	if (!gettimeofday(&tv, NULL))
		return tv.tv_sec + tv.tv_usec / 1000000.0;
	return 0.0;
}

static void init_time_left(struct execute_state *state,
			   struct settings *settings)
{
	if (settings->overall_timeout <= 0)
		state->time_left = -1;
	else
		state->time_left = settings->overall_timeout;
}

bool initialize_execute_state_from_resume(int dirfd,
					  struct execute_state *state,
					  struct settings *settings,
					  struct job_list *list)
{
	struct job_list_entry *entry;
	int resdirfd, fd, i;

	free_settings(settings);
	free_job_list(list);
	memset(state, 0, sizeof(*state));
	state->resuming = true;

	if (!read_settings_from_dir(settings, dirfd) ||
	    !read_job_list(list, dirfd)) {
		close(dirfd);
		return false;
	}

	init_time_left(state, settings);

	for (i = list->size; i >= 0; i--) {
		char name[32];

		snprintf(name, sizeof(name), "%d", i);
		if ((resdirfd = openat(dirfd, name, O_DIRECTORY | O_RDONLY)) >= 0)
			break;
	}

	if (i < 0)
		/* Nothing has been executed yet, state is fine as is */
		goto success;

	entry = &list->entries[i];
	state->next = i;
	if ((fd = openat(resdirfd, filenames[_F_JOURNAL], O_RDONLY)) >= 0) {
		if (!prune_from_journal(entry, fd)) {
			/*
			 * The test does not have subtests, or
			 * incompleted before the first subtest
			 * began. Either way, not suitable to
			 * re-run.
			 */
			state->next = i + 1;
		} else if (entry->binary[0] == '\0') {
			/* This test is fully completed */
			state->next = i + 1;
		}

		close(fd);
	}

 success:
	close(resdirfd);
	close(dirfd);

	return true;
}

bool initialize_execute_state(struct execute_state *state,
			      struct settings *settings,
			      struct job_list *job_list)
{
	memset(state, 0, sizeof(*state));

	if (!validate_settings(settings))
		return false;

	if (!serialize_settings(settings) ||
	    !serialize_job_list(job_list, settings))
		return false;

	if (settings->overwrite &&
	    !clear_old_results(settings->results_path))
		return false;

	init_time_left(state, settings);

	state->dry = settings->dry_run;

	return true;
}

static void reduce_time_left(struct settings *settings,
			     struct execute_state *state,
			     double time_spent)
{
	if (state->time_left < 0)
		return;

	if (time_spent > state->time_left)
		state->time_left = 0.0;
	else
		state->time_left -= time_spent;
}

static bool overall_timeout_exceeded(struct execute_state *state)
{
	return state->time_left == 0.0;
}

static void write_abort_file(int resdirfd,
			     const char *reason,
			     const char *testbefore,
			     const char *testafter)
{
	int abortfd;

	if ((abortfd = openat(resdirfd, "aborted.txt", O_CREAT | O_WRONLY | O_EXCL, 0666)) >= 0) {
		/*
		 * Ignore failure to open, there's
		 * already an abort probably (if this
		 * is a resume)
		 */
		dprintf(abortfd, "Aborting.\n");
		dprintf(abortfd, "Previous test: %s\n", testbefore);
		dprintf(abortfd, "Next test: %s\n\n", testafter);
		write(abortfd, reason, strlen(reason));
		close(abortfd);
	}
}

static void oom_immortal(void)
{
	int fd;
	const char never_kill[] = "-1000";

	fd = open("/proc/self/oom_score_adj", O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "Warning: Cannot adjust oom score.\n");
		return;
	}
	if (write(fd, never_kill, sizeof(never_kill)) != sizeof(never_kill))
		fprintf(stderr, "Warning: Adjusting oom score failed.\n");

	close(fd);
}

static bool should_die_because_signal(int sigfd)
{
	struct signalfd_siginfo siginfo;
	int ret;
	struct pollfd sigpoll = { .fd = sigfd, .events = POLLIN | POLLRDBAND };

	ret = poll(&sigpoll, 1, 0);

	if (ret != 0) {
		if (ret == -1) {
			fprintf(stderr, "Poll on signalfd failed with %s\n", strerror(errno));
			return true; /* something is wrong, let's die */
		}

		ret = read(sigfd, &siginfo, sizeof(siginfo));

		if (ret == -1) {
			fprintf(stderr, "Error reading from signalfd: %s\n", strerror(errno));
			return false; /* we may want to retry later */
		}

		if (siginfo.ssi_signo == SIGCHLD) {
			fprintf(stderr, "Runner got stray SIGCHLD while not executing any tests.\n");
		} else {
			fprintf(stderr, "Runner is being killed by %s\n",
				strsignal(siginfo.ssi_signo));
			return true;
		}

	}

	return false;
}

bool execute(struct execute_state *state,
	     struct settings *settings,
	     struct job_list *job_list)
{
	struct utsname unamebuf;
	int resdirfd, testdirfd, unamefd, timefd;
	sigset_t sigmask;
	int sigfd;
	double time_spent = 0.0;
	bool status = true;

	if (state->dry) {
		printf("Dry run, not executing. Invoke igt_resume if you want to execute.\n");
		return true;
	}

	if ((resdirfd = open(settings->results_path, O_DIRECTORY | O_RDONLY)) < 0) {
		/* Initialize state should have done this */
		fprintf(stderr, "Error: Failure opening results path %s\n",
			settings->results_path);
		return false;
	}

	if ((testdirfd = open(settings->test_root, O_DIRECTORY | O_RDONLY)) < 0) {
		fprintf(stderr, "Error: Failure opening test root %s\n",
			settings->test_root);
		close(resdirfd);
		return false;
	}

	/* TODO: On resume, don't rewrite, verify that content matches current instead */
	if ((unamefd = openat(resdirfd, "uname.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666)) < 0) {
		fprintf(stderr, "Error: Failure opening uname.txt: %s\n",
			strerror(errno));
		close(testdirfd);
		close(resdirfd);
		return false;
	}

	if ((timefd = openat(resdirfd, "starttime.txt", O_CREAT | O_WRONLY | O_EXCL, 0666)) >= 0) {
		/*
		 * Ignore failure to open. If this is a resume, we
		 * don't want to overwrite. For other errors, we
		 * ignore the start time.
		 */
		dprintf(timefd, "%f\n", timeofday_double());
		close(timefd);
	}

	oom_immortal();

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGCHLD);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGQUIT);
	sigaddset(&sigmask, SIGHUP);
	sigfd = signalfd(-1, &sigmask, O_CLOEXEC);
	sigprocmask(SIG_BLOCK, &sigmask, NULL);

	if (sigfd < 0) {
		/* TODO: Handle better */
		fprintf(stderr, "Cannot mask signals\n");
		status = false;
		goto end;
	}

	init_watchdogs(settings);

	if (!uname(&unamebuf)) {
		dprintf(unamefd, "%s %s %s %s %s\n",
			unamebuf.sysname,
			unamebuf.nodename,
			unamebuf.release,
			unamebuf.version,
			unamebuf.machine);
	} else {
		dprintf(unamefd, "uname() failed\n");
	}
	close(unamefd);

	/* Check if we're already in abort-state at bootup */
	if (!state->resuming) {
		char *reason;

		if ((reason = need_to_abort(settings)) != NULL) {
			char *nexttest = entry_display_name(&job_list->entries[state->next]);
			write_abort_file(resdirfd, reason, "nothing", nexttest);
			free(reason);
			free(nexttest);

			status = false;

			goto end;
		}
	}

	for (; state->next < job_list->size;
	     state->next++) {
		char *reason;
		int result;

		if (should_die_because_signal(sigfd)) {
			status = false;
			goto end;
		}

		result = execute_next_entry(state,
					    job_list->size,
					    &time_spent,
					    settings,
					    &job_list->entries[state->next],
					    testdirfd, resdirfd,
					    sigfd, &sigmask);

		if (result < 0) {
			status = false;
			break;
		}

		reduce_time_left(settings, state, time_spent);

		if (overall_timeout_exceeded(state)) {
			if (settings->log_level >= LOG_LEVEL_NORMAL) {
				printf("Overall timeout time exceeded, stopping.\n");
			}

			break;
		}

		if ((reason = need_to_abort(settings)) != NULL) {
			char *prev = entry_display_name(&job_list->entries[state->next]);
			char *next = (state->next + 1 < job_list->size ?
				      entry_display_name(&job_list->entries[state->next + 1]) :
				      strdup("nothing"));
			write_abort_file(resdirfd, reason, prev, next);
			free(prev);
			free(next);
			free(reason);
			status = false;
			break;
		}

		if (result > 0) {
			double time_left = state->time_left;

			close_watchdogs(settings);
			sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
			/* make sure that we do not leave any signals unhandled */
			if (should_die_because_signal(sigfd)) {
				status = false;
				goto end_post_signal_restore;
			}
			close(sigfd);
			close(testdirfd);
			initialize_execute_state_from_resume(resdirfd, state, settings, job_list);
			state->time_left = time_left;
			return execute(state, settings, job_list);
		}
	}

	if ((timefd = openat(resdirfd, "endtime.txt", O_CREAT | O_WRONLY | O_EXCL, 0666)) >= 0) {
		dprintf(timefd, "%f\n", timeofday_double());
		close(timefd);
	}

 end:
	close_watchdogs(settings);
	sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
	/* make sure that we do not leave any signals unhandled */
	if (should_die_because_signal(sigfd))
		status = false;
 end_post_signal_restore:
	close(sigfd);
	close(testdirfd);
	close(resdirfd);
	return status;
}
