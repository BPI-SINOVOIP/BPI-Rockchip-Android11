// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 FUJITSU LIMITED. All rights reserved.
 * Author: Yang Xu <xuyang2018.jy@cn.fujitsu.com>
 *
 * This testcase checks the basic flag of quotactl(2) for project quota on
 * non-XFS filesystems.
 *
 * 1) quotactl(2) succeeds to turn on quota with Q_QUOTAON flag for project.
 * 2) quotactl(2) succeeds to set disk quota limits with Q_SETQUOTA flag
 *    for project.
 * 3) quotactl(2) succeeds to get disk quota limits with Q_GETQUOTA flag
 *    for project.
 * 4) quotactl(2) succeeds to set information about quotafile with Q_SETINFO
 *    flag for project.
 * 5) quotactl(2) succeeds to get information about quotafile with Q_GETINFO
 *    flag for project.
 * 6) quotactl(2) succeeds to get quota format with Q_GETFMT flag for project.
 * 7) quotactl(2) succeeds to get disk quota limit greater than or equal to
 *    ID with Q_GETNEXTQUOTA flag for project.
 * 8) quotactl(2) succeeds to turn off quota with Q_QUOTAOFF flag for project.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include "config.h"
#include "lapi/quotactl.h"
#include "tst_test.h"

#ifndef QFMT_VFS_V1
# define QFMT_VFS_V1 4
#endif

#define FMTID QFMT_VFS_V1
#define MNTPOINT	"mntpoint"
static int32_t fmt_id = FMTID;
static int test_id, mount_flag;
static struct dqblk set_dq = {
	.dqb_bsoftlimit = 100,
	.dqb_valid = QIF_BLIMITS
};
static struct dqblk res_dq;
static struct dqinfo set_qf = {
	.dqi_bgrace = 80,
	.dqi_valid = IIF_BGRACE
};

static struct dqinfo res_qf;
static int32_t fmt_buf;

static struct if_nextdqblk res_ndq;

static struct tcase {
	int cmd;
	int *id;
	void *addr;
	void *set_data;
	void *res_data;
	int sz;
	char *des;
	char *tname;
} tcases[] = {
	{QCMD(Q_QUOTAON, PRJQUOTA), &fmt_id, NULL,
	NULL, NULL, 0, "turn on quota for project",
	"QCMD(Q_QUOTAON, PRJQUOTA)"},

	{QCMD(Q_SETQUOTA, PRJQUOTA), &test_id, &set_dq,
	NULL, NULL, 0, "set disk quota limit for project",
	"QCMD(Q_SETQUOTA, PRJQUOTA)"},

	{QCMD(Q_GETQUOTA, PRJQUOTA), &test_id, &res_dq,
	&set_dq.dqb_bsoftlimit, &res_dq.dqb_bsoftlimit,
	sizeof(res_dq.dqb_bsoftlimit), "get disk quota limit for project",
	"QCMD(Q_GETQUOTA, PRJQUOTA)"},

	{QCMD(Q_SETINFO, PRJQUOTA), &test_id, &set_qf,
	NULL, NULL, 0, "set information about quotafile for project",
	"QCMD(Q_SETINFO, PRJQUOTA"},

	{QCMD(Q_GETINFO, PRJQUOTA), &test_id, &res_qf,
	&set_qf.dqi_bgrace, &res_qf.dqi_bgrace, sizeof(res_qf.dqi_bgrace),
	"get information about quotafile for project",
	"QCMD(Q_GETINFO, PRJQUOTA"},

	{QCMD(Q_GETFMT, PRJQUOTA), &test_id, &fmt_buf,
	&fmt_id, &fmt_buf, sizeof(fmt_buf),
	"get quota format for project", "QCMD(Q_GETFMT, PRJQUOTA)"},

	{QCMD(Q_GETNEXTQUOTA, PRJQUOTA), &test_id, &res_ndq,
	&test_id, &res_ndq.dqb_id, sizeof(res_ndq.dqb_id),
	"get next disk quota limit for project",
	"QCMD(Q_GETNEXTQUOTA, PRJQUOTA)"},

	{QCMD(Q_QUOTAOFF, PRJQUOTA), &test_id, NULL,
	NULL, NULL, 0, "turn off quota for project",
	"QCMD(Q_QUOTAOFF, PRJQUOTA)"},

};

static void setup(void)
{
	const char *const extra_opts[] = {"-O quota,project", NULL};

	test_id = geteuid();
	SAFE_MKFS(tst_device->dev, tst_device->fs_type, NULL, extra_opts);
	SAFE_MOUNT(tst_device->dev, MNTPOINT, tst_device->fs_type, 0, "quota");
	mount_flag = 1;
}

static void cleanup(void)
{
	if (mount_flag && tst_umount(MNTPOINT))
		tst_res(TWARN | TERRNO, "umount(%s)", MNTPOINT);
}

static void verify_quota(unsigned int n)
{
	struct tcase *tc = &tcases[n];

	res_dq.dqb_bsoftlimit = 0;
	res_qf.dqi_igrace = 0;
	fmt_buf = 0;

	tst_res(TINFO, "Test #%d: %s", n, tc->tname);

	TEST(quotactl(tc->cmd, tst_device->dev, *tc->id, tc->addr));
	if (TST_RET == -1) {
		tst_res(TFAIL | TTERRNO, "quotactl failed to %s", tc->des);
		return;
	}

	if (memcmp(tc->res_data, tc->set_data, tc->sz)) {
		tst_res(TFAIL, "quotactl failed to %s", tc->des);
		tst_res_hexd(TINFO, tc->res_data, tc->sz, "retval:   ");
		tst_res_hexd(TINFO, tc->set_data, tc->sz, "expected: ");
		return;
	}

	tst_res(TPASS, "quotactl succeeded to %s", tc->des);
}

static const char *kconfigs[] = {
	"CONFIG_QFMT_V2",
	NULL
};

static struct tst_test test = {
	.needs_root = 1,
	.needs_kconfigs = kconfigs,
	.min_kver = "4.10", /* commit 689c958cbe6b (ext4: add project quota support) */
	.test = verify_quota,
	.tcnt = ARRAY_SIZE(tcases),
	.setup = setup,
	.cleanup = cleanup,
	.needs_device = 1,
	.dev_fs_type = "ext4",
	.mntpoint = MNTPOINT,
};
