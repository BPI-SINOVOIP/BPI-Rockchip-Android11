// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 CTERA Networks. All Rights Reserved.
 *
 * Started by Amir Goldstein <amir73il@gmail.com>
 * Modified by Matthew Bobrowski <mbobrowski@mbobrowski.org>
 *
 * DESCRIPTION
 *	Test file that has been purposely designed to verify
 *	FAN_REPORT_FID functionality while using newly defined dirent
 *	events.
 */
#define _GNU_SOURCE
#include "config.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/statfs.h>
#include <sys/types.h>

#include "tst_test.h"
#include "fanotify.h"

#if defined(HAVE_SYS_FANOTIFY_H)
#include <sys/fanotify.h>

#define BUF_SIZE 256
#define EVENT_MAX 256

#define MOUNT_POINT "mntpoint"
#define TEST_DIR MOUNT_POINT"/test_dir"
#define DIR1 TEST_DIR"/dir1"
#define DIR2 TEST_DIR"/dir2"
#define FILE1 TEST_DIR"/file1"
#define FILE2 TEST_DIR"/file2"

#if defined(HAVE_NAME_TO_HANDLE_AT)
struct event_t {
	unsigned long long mask;
	__kernel_fsid_t fsid;
	struct file_handle handle;
	char buf[MAX_HANDLE_SZ];
};

static int fanotify_fd;
static char events_buf[BUF_SIZE];
static struct event_t event_set[EVENT_MAX];

static void do_test(void)
{
	int i, fd, len, count = 0;

	struct file_handle *event_file_handle;
	struct fanotify_event_metadata *metadata;
	struct fanotify_event_info_fid *event_fid;

	if (fanotify_mark(fanotify_fd, FAN_MARK_ADD | FAN_MARK_FILESYSTEM,
				FAN_CREATE | FAN_DELETE | FAN_ATTRIB |
				FAN_MOVED_FROM | FAN_MOVED_TO |
				FAN_DELETE_SELF | FAN_ONDIR,
				AT_FDCWD, TEST_DIR) == -1) {
		if (errno == ENODEV)
			tst_brk(TCONF,
				"FAN_REPORT_FID not supported on %s "
				"filesystem", tst_device->fs_type);
		tst_brk(TBROK | TERRNO,
			"fanotify_mark(%d, FAN_MARK_ADD, FAN_CREATE | "
			"FAN_DELETE | FAN_MOVED_FROM | FAN_MOVED_TO | "
			"FAN_DELETE_SELF | FAN_ONDIR, AT_FDCWD, %s) failed",
			fanotify_fd, TEST_DIR);
	}

	/* Generate a sequence of events */
	event_set[count].mask = FAN_CREATE | FAN_MOVED_FROM | FAN_MOVED_TO | \
				FAN_DELETE;
	event_set[count].handle.handle_bytes = MAX_HANDLE_SZ;
	fanotify_get_fid(TEST_DIR, &event_set[count].fsid,
			 &event_set[count].handle);
	count++;

	fd = SAFE_CREAT(FILE1, 0644);
	SAFE_CLOSE(fd);

	SAFE_RENAME(FILE1, FILE2);

	event_set[count].mask = FAN_ATTRIB | FAN_DELETE_SELF;
	event_set[count].handle.handle_bytes = MAX_HANDLE_SZ;
	fanotify_get_fid(FILE2, &event_set[count].fsid,
			 &event_set[count].handle);
	count++;

	SAFE_UNLINK(FILE2);

	/*
	 * Generate a sequence of events on a directory. Subsequent events
	 * are merged, so it's required that we set FAN_ONDIR once in
	 * order to acknowledge that changes related to a subdirectory
	 * took place. Events on subdirectories are not merged with events
	 * on non-subdirectories.
	 */
	event_set[count].mask = FAN_ONDIR | FAN_CREATE | FAN_MOVED_FROM | \
				FAN_MOVED_TO | FAN_DELETE;
	event_set[count].handle.handle_bytes = MAX_HANDLE_SZ;
	fanotify_get_fid(TEST_DIR, &event_set[count].fsid,
			 &event_set[count].handle);
	count++;

	SAFE_MKDIR(DIR1, 0755);

	SAFE_RENAME(DIR1, DIR2);

	event_set[count].mask = FAN_ONDIR | FAN_DELETE_SELF;
	event_set[count].handle.handle_bytes = MAX_HANDLE_SZ;
	fanotify_get_fid(DIR2, &event_set[count].fsid,
			 &event_set[count].handle);
	count++;

	SAFE_RMDIR(DIR2);

	/* Read events from the event queue */
	len = SAFE_READ(0, fanotify_fd, events_buf, BUF_SIZE);

	/* Process each event in buffer */
	for (i = 0, metadata = (struct fanotify_event_metadata *) events_buf;
		FAN_EVENT_OK(metadata, len);
		metadata = FAN_EVENT_NEXT(metadata,len), i++) {
		event_fid = (struct fanotify_event_info_fid *) (metadata + 1);
		event_file_handle = (struct file_handle *) event_fid->handle;

		if (i >= count) {
			tst_res(TFAIL,
				"got unnecessary event: mask=%llx "
				"pid=%u fd=%d",
				(unsigned long long) metadata->mask,
				metadata->pid,
				metadata->fd);
			metadata->mask = 0;
		} else if (metadata->fd != FAN_NOFD) {
			tst_res(TFAIL,
				"Received unexpected file descriptor %d in "
				"event. Expected to get FAN_NOFD(%d)",
				metadata->fd, FAN_NOFD);
		} else if (metadata->mask != event_set[i].mask) {
			tst_res(TFAIL,
				"Got event: mask=%llx (expected %llx) "
				"pid=%u fd=%d",
				(unsigned long long) metadata->mask,
				event_set[i].mask,
				(unsigned) metadata->pid,
				metadata->fd);
		} else if (metadata->pid != getpid()) {
			tst_res(TFAIL,
				"Got event: mask=%llx pid=%u "
				"(expected %u) fd=%d",
				(unsigned long long) metadata->mask,
				(unsigned) metadata->pid,
				(unsigned) getpid(),
				metadata->fd);
		} else if (event_file_handle->handle_bytes !=
				event_set[i].handle.handle_bytes) {
			tst_res(TFAIL,
				"Got event: handle_bytes (%x) returned in "
				"event does not equal handle_bytes (%x) "
				"retunred in name_to_handle_at(2)",
				event_file_handle->handle_bytes,
				event_set[i].handle.handle_bytes);
		} else if (event_file_handle->handle_type !=
				event_set[i].handle.handle_type) {
			tst_res(TFAIL,
				"handle_type (%x) returned in event does not "
				"equal to handle_type (%x) returned in "
				"name_to_handle_at(2)",
				event_file_handle->handle_type,
				event_set[i].handle.handle_type);
		} else if (memcmp(event_file_handle->f_handle,
					event_set[i].handle.f_handle,
					event_set[i].handle.handle_bytes)
					!= 0) {
			tst_res(TFAIL,
				"event_file_handle->f_handle does not match "
				"handle.f_handle returned in "
				"name_to_handle_at(2)");
		} else if (memcmp(&event_fid->fsid, &event_set[i].fsid,
					sizeof(event_set[i].fsid)) != 0) {
			tst_res(TFAIL,
				"event_fid->fsid != stats.f_fsid that was "
				"obtained via statfs(2)");
		} else {
			tst_res(TPASS,
				"Got event: mask=%llx, pid=%u, "
				"fid=%x.%x.%lx values",
				metadata->mask,
				getpid(),
				FSID_VAL_MEMBER(event_fid->fsid, 0),
				FSID_VAL_MEMBER(event_fid->fsid, 1),
				*(unsigned long *)
				event_file_handle->f_handle);
		}
	}

	for (; i < count; i++)
		tst_res(TFAIL,
			"Didn't receive event: mask=%llx",
			event_set[i].mask);
}

static void do_setup(void)
{
	int fd;

	/* Check kernel for fanotify support */
	fd = SAFE_FANOTIFY_INIT(FAN_CLASS_NOTIF, O_RDONLY);
	SAFE_CLOSE(fd);

	fanotify_fd = fanotify_init(FAN_REPORT_FID, O_RDONLY);
	if (fanotify_fd == -1) {
		if (errno == EINVAL)
			tst_brk(TCONF,
				"FAN_REPORT_FID not supported in kernel");
		tst_brk(TBROK | TERRNO,
			"fanotify_init(FAN_REPORT_FID, O_RDONLY) failed");
	}

	SAFE_MKDIR(TEST_DIR, 0755);
}

static void do_cleanup(void)
{
	if (fanotify_fd > 0)
		SAFE_CLOSE(fanotify_fd);
}

static struct tst_test test = {
	.needs_root = 1,
	.mount_device = 1,
	.mntpoint = MOUNT_POINT,
	.all_filesystems = 1,
	.test_all = do_test,
	.setup = do_setup,
	.cleanup = do_cleanup
};

#else
	TST_TEST_TCONF("System does not have required name_to_handle_at() support");
#endif
#else
	TST_TEST_TCONF("System does not have required fanotify support");
#endif
