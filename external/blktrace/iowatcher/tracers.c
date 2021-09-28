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
#include <signal.h>
#include <sys/wait.h>
#include <spawn.h>

#include "plot.h"
#include "blkparse.h"
#include "list.h"
#include "tracers.h"

extern char **environ;

static pid_t blktrace_pid = 0;
static pid_t mpstat_pid = 0;

static void sig_handler_for_quit(int val)
{
	fprintf(stderr, "Received signal %d. Terminating tracers.\n", val);
	wait_for_tracers(SIGTERM);
}

int start_blktrace(char **devices, int num_devices, char *trace_name, char *dest)
{
	int ret;
	int i;
	char *argv[15 + MAX_DEVICES_PER_TRACE * 2];
	int argc = 0;

	if (!trace_name)
		trace_name = "trace";
	if (!dest)
		dest = ".";

	argv[argc++] = "blktrace";
	argv[argc++] = "-b";
	argv[argc++] = "8192";
	argv[argc++] = "-a";
	argv[argc++] = "queue";
	argv[argc++] = "-a";
	argv[argc++] = "complete";
	argv[argc++] = "-a";
	argv[argc++] = "issue";
	argv[argc++] = "-a";
	argv[argc++] = "notify";

	if (num_devices == 1) {
		argv[argc++] = "-o";
		argv[argc++] = trace_name;
	} else {
		/* Multiple devices output to a directory named trace_name */
		dest = trace_name;
	}
	argv[argc++] = "-D";
	argv[argc++] = dest;

	for (i = 0; i < num_devices; i++) {
		argv[argc++] = "-d";
		argv[argc++] = devices[i];
	}
	argv[argc] = NULL;
	signal(SIGTERM, sig_handler_for_quit);
	signal(SIGINT, sig_handler_for_quit);
	ret = run_program(argc, argv, 0, &blktrace_pid, NULL);
	return ret;
}

int wait_program(pid_t pid, const char *pname, int sig)
{
	int status;
	int ret = 0;

	if (sig) {
		ret = kill(pid, sig);
		if (ret) {
			fprintf(stderr, "Failed to send signal %d to %s (%lu): %s\n",
			        sig, pname, (long)pid, strerror(errno));
			return ret;
		}
		fprintf(stderr, "Kill (%d): %s (%ld)\n", sig, pname, (long)pid);
	}

	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		ret = WEXITSTATUS(status);
		if (ret == 127) /* spawnp failed after forking */
			fprintf(stderr, "Failed to run '%s'\n", pname);
		else
			fprintf(stderr, "Exit (%d): %s\n", ret, pname);
	} else if (WIFSIGNALED(status) && sig && WTERMSIG(status) != sig) {
		fprintf(stderr, "'%s' killed by signal %d\n", pname, WTERMSIG(status));
		ret = -1;
	}
	return ret;
}

int run_program(int argc, char **argv, int wait, pid_t *pid, char *outpath)
{
	int i;
	int err;
	pid_t _pid;
	posix_spawn_file_actions_t facts;
	posix_spawn_file_actions_t *factp = NULL;

	if (outpath != NULL) {
		posix_spawn_file_actions_init(&facts);
		posix_spawn_file_actions_addopen(&facts, 1, outpath, O_WRONLY|O_CREAT|O_TRUNC, 0600);
		factp = &facts;
	}

	fprintf(stderr, "Start");
	for (i = 0; i < argc; i++)
		fprintf(stderr, " %s", argv[i]);
	fprintf(stderr, "\n");

	err = posix_spawnp(&_pid, argv[0], factp, NULL, argv, environ);
	if (err != 0) {
		fprintf(stderr, "Could not run '%s': %s\n", argv[0], strerror(err));
	} else if (wait) {
		err = wait_program(_pid, argv[0], 0);
	} else if (!pid) {
		fprintf(stderr, "Warning: %s (%ld): Not saving pid and not waiting for it.\n",
		        argv[0], (long)_pid);
	} else {
		*pid = _pid;
	}
	return err;
}

int wait_for_tracers(int sig)
{
	int err;
	if (blktrace_pid != 0) {
		err = wait_program(blktrace_pid, "blktrace", sig);
		if (err)
			exit(1);
		blktrace_pid = 0;
	}
	if (mpstat_pid != 0) {
		err = wait_program(mpstat_pid, "mpstat", sig);
		if (err)
			exit(1);
		mpstat_pid = 0;
	}
	return 0;
}

int start_mpstat(char *path)
{
	int ret;
	int argc = 4;
	char *argv[] = {
		"mpstat",
		"-P", "ALL", "1",
		NULL,
	};

	ret = run_program(argc, argv, 0, &mpstat_pid, path);
	return ret;
}
