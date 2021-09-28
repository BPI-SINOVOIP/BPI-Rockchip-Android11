/*
 * Copyright 2015 David Herrmann <dh.herrmann@gmail.com>
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
 */

/*
 * Testcase: drmGetMagic() and drmAuthMagic()
 */

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include "drm.h"

#ifndef __linux__
# include <pthread.h>
#endif

IGT_TEST_DESCRIPTION("Call drmGetMagic() and drmAuthMagic() and see if it behaves.");

static bool
is_local_tid(pid_t tid)
{
#ifndef __linux__
	return pthread_self() == tid;
#else
	/* On Linux systems, drmGetClient() would return the thread ID
	   instead of the actual process ID */
	return gettid() == tid;
#endif
}


static bool check_auth(int fd)
{
	pid_t client_pid;
	int i, auth, pid, uid;
	unsigned long magic, iocs;
	bool is_authenticated = false;

	client_pid = getpid();
	for (i = 0; !is_authenticated; i++) {
		if (drmGetClient(fd, i, &auth, &pid, &uid, &magic, &iocs) != 0)
			break;
		is_authenticated = auth && (pid == client_pid || is_local_tid(pid));
	}
	return is_authenticated;
}


static int magic_cmp(const void *p1, const void *p2)
{
	return *(const drm_magic_t*)p1 < *(const drm_magic_t*)p2;
}

static void test_many_magics(int master)
{
	drm_magic_t magic, *magics = NULL;
	unsigned int i, j, ns, allocated = 0;
	char path[512];
	int *fds = NULL, slave;

	struct rlimit fd_limit;

	do_or_die(getrlimit(RLIMIT_NOFILE, &fd_limit));
	fd_limit.rlim_cur = 1024;
	do_or_die(setrlimit(RLIMIT_NOFILE, &fd_limit));

	sprintf(path, "/proc/self/fd/%d", master);

	for (i = 0; ; ++i) {
		/* open slave and make sure it's NOT a master */
		slave = open(path, O_RDWR | O_CLOEXEC);
		if (slave < 0) {
			igt_info("Reopening device failed after %d opens\n", i);
			igt_assert(errno == EMFILE);
			break;
		}
		igt_assert(drmSetMaster(slave) < 0);

		/* resize magic-map */
		if (i >= allocated) {
			ns = allocated * 2;
			igt_assert(ns >= allocated);

			if (!ns)
				ns = 128;

			magics = realloc(magics, sizeof(*magics) * ns);
			igt_assert(magics);

			fds = realloc(fds, sizeof(*fds) * ns);
			igt_assert(fds);

			allocated = ns;
		}

		/* insert magic */
		igt_assert(drmGetMagic(slave, &magic) == 0);
		igt_assert(magic > 0);

		magics[i] = magic;
		fds[i] = slave;
	}

	/* make sure we could at least open a reasonable number of files */
	igt_assert(i > 128);

	/*
	 * We cannot open the DRM file anymore. Lets sort the magic-map and
	 * verify no magic was used multiple times.
	 */
	qsort(magics, i, sizeof(*magics), magic_cmp);
	for (j = 1; j < i; ++j)
		igt_assert(magics[j] != magics[j - 1]);

	/* make sure we can authenticate all of them */
	for (j = 0; j < i; ++j)
		igt_assert(drmAuthMagic(master, magics[j]) == 0);

	/* close files again */
	for (j = 0; j < i; ++j)
		close(fds[j]);

	free(fds);
	free(magics);
}

static void test_basic_auth(int master)
{
	drm_magic_t magic, old_magic;
	int slave;

	/* open slave and make sure it's NOT a master */
	slave = drm_open_driver(DRIVER_ANY);
	igt_require(slave >= 0);
	igt_require(drmSetMaster(slave) < 0);

	/* retrieve magic for slave */
	igt_assert(drmGetMagic(slave, &magic) == 0);
	igt_assert(magic > 0);

	/* verify the same magic is returned every time */
	old_magic = magic;
	igt_assert(drmGetMagic(slave, &magic) == 0);
	igt_assert_eq(magic, old_magic);

	/* verify magic can be authorized exactly once, on the master */
	igt_assert(drmAuthMagic(slave, magic) < 0);
	igt_assert(drmAuthMagic(master, magic) == 0);
	igt_assert(drmAuthMagic(master, magic) < 0);

	/* verify that the magic did not change */
	old_magic = magic;
	igt_assert(drmGetMagic(slave, &magic) == 0);
	igt_assert_eq(magic, old_magic);

	close(slave);
}

igt_main
{
	int master;

	/* root (which we run igt as) should always be authenticated */
	igt_subtest("getclient-simple") {
		int fd = drm_open_driver(DRIVER_ANY);

		igt_assert(check_auth(fd) == true);

		close(fd);
	}

	igt_subtest("getclient-master-drop") {
		int fd = drm_open_driver(DRIVER_ANY);
		int fd2 = drm_open_driver(DRIVER_ANY);

		igt_assert(check_auth(fd2) == true);

		close(fd);

		igt_assert(check_auth(fd2) == true);

		close(fd2);
	}

	/* above tests require that no drm fd is open */
	igt_subtest_group {
		igt_fixture
			master = drm_open_driver_master(DRIVER_ANY);

		igt_subtest("basic-auth")
			test_basic_auth(master);

		/* this must be last, we adjust the rlimit */
		igt_subtest("many-magics")
			test_many_magics(master);
	}
}
