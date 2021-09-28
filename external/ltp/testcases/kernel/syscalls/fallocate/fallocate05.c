// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017 Cyril Hrubis <chrubis@suse.cz>
 * Copyright (c) 2019 SUSE LLC <mdoucha@suse.cz>
 */

/*
 * Tests that writing to fallocated file works when filesystem is full.
 * Test scenario:
 * - fallocate() some empty blocks
 * - fill the filesystem
 * - write() into the preallocated space
 * - try to fallocate() more blocks until we get ENOSPC
 * - write() into the extra allocated space
 * - deallocate part of the file
 * - write() to the end of file to check that some blocks were freed
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "tst_test.h"
#include "lapi/fallocate.h"

#define MNTPOINT "mntpoint"
#define FALLOCATE_BLOCKS 16
#define DEALLOCATE_BLOCKS 4
#define TESTED_FLAGS "fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)"

static int fd;
static char *buf;
static blksize_t blocksize;
static long bufsize;

static void setup(void)
{
	struct stat statbuf;

	fd = SAFE_OPEN(MNTPOINT "/test_file", O_WRONLY | O_CREAT);

	/*
	 * Use real FS block size, otherwise fallocate() call will test
	 * different things on different platforms
	 */
	SAFE_FSTAT(fd, &statbuf);
	blocksize = statbuf.st_blksize;
	bufsize = FALLOCATE_BLOCKS * blocksize;
	buf = SAFE_MALLOC(bufsize);
}

static void run(void)
{
	long extsize, tmp;

	TEST(fallocate(fd, 0, 0, bufsize));

	if (TST_RET) {
		if (TST_ERR == ENOTSUP)
			tst_brk(TCONF | TTERRNO, "fallocate() not supported");

		tst_brk(TBROK | TTERRNO, "fallocate(fd, 0, 0, %ld)", bufsize);
	}

	tst_fill_fs(MNTPOINT, 1);

	TEST(write(fd, buf, bufsize));

	if (TST_RET < 0)
		tst_res(TFAIL | TTERRNO, "write() failed unexpectedly");
	else if (TST_RET != bufsize)
		tst_res(TFAIL, "Short write(): %ld bytes (expected %zu)",
			TST_RET, bufsize);
	else
		tst_res(TPASS, "write() wrote %ld bytes", TST_RET);

	/*
	 * Some file systems may still have a few extra blocks that can be
	 * allocated.
	 */
	for (TST_RET = 0, extsize = 0; !TST_RET; extsize += blocksize)
		TEST(fallocate(fd, 0, bufsize + extsize, blocksize));

	if (TST_RET != -1) {
		tst_res(TFAIL, "Invalid fallocate() return value %ld", TST_RET);
		return;
	}

	if (TST_ERR != ENOSPC) {
		tst_res(TFAIL | TTERRNO, "fallocate() should fail with ENOSPC");
		return;
	}

	/* The loop above always counts 1 more block than it should. */
	extsize -= blocksize;
	tst_res(TINFO, "fallocate()d %ld extra blocks on full FS",
		extsize / blocksize);

	for (tmp = extsize; tmp > 0; tmp -= TST_RET) {
		TEST(write(fd, buf, MIN(bufsize, tmp)));

		if (TST_RET <= 0) {
			tst_res(TFAIL | TTERRNO, "write() failed unexpectedly");
			return;
		}
	}

	tst_res(TPASS, "fallocate() on full FS");

	/* Btrfs deallocates only complete extents, not individual blocks */
	if (!strcmp(tst_device->fs_type, "btrfs"))
		tmp = bufsize + extsize;
	else
		tmp = DEALLOCATE_BLOCKS * blocksize;

	TEST(fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, tmp));

	if (TST_RET == -1) {
		if (TST_ERR == ENOTSUP)
			tst_brk(TCONF, TESTED_FLAGS);

		tst_brk(TBROK | TTERRNO, TESTED_FLAGS);
	}
	tst_res(TPASS, TESTED_FLAGS);

	TEST(write(fd, buf, 10));
	if (TST_RET == -1)
		tst_res(TFAIL | TTERRNO, "write()");
	else
		tst_res(TPASS, "write()");

	/* TODO: wipe the test device here to allow looping with -i/-I */
}

static void cleanup(void)
{
	if (fd > 0)
		SAFE_CLOSE(fd);

	free(buf);
}

static struct tst_test test = {
	.needs_root = 1,
	.mount_device = 1,
	.dev_min_size = 512,
	.mntpoint = MNTPOINT,
	.all_filesystems = 1,
	.setup = setup,
	.cleanup = cleanup,
	.test_all = run,
};
