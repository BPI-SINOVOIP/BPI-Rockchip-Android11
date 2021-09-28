// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 Red Hat, Inc.
 *
 * Memory Protection Keys for Userspace (PKU aka PKEYs) is a Skylake-SP
 * server feature that provides a mechanism for enforcing page-based
 * protections, but without requiring modification of the page tables
 * when an application changes protection domains. It works by dedicating
 * 4 previously ignored bits in each page table entry to a "protection key",
 * giving 16 possible keys.
 *
 * Basic method for PKEYs testing:
 *    1. test allocates a pkey(e.g. PKEY_DISABLE_ACCESS) via pkey_alloc()
 *    2. pkey_mprotect() apply this pkey to a piece of memory(buffer)
 *    3. check if access right of the buffer has been changed and take effect
 *    4. remove the access right(pkey) from this buffer via pkey_mprotect()
 *    5. check if buffer area can be read or write after removing pkey
 *    6. pkey_free() releases the pkey after using it
 *
 * Looping around this basic test on diffenrent types of memory.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "pkey.h"

#define TEST_FILE "pkey_testfile"
#define STR "abcdefghijklmnopqrstuvwxyz12345\n"
#define PATH_VM_NRHPS "/proc/sys/vm/nr_hugepages"

static int size;
static int no_hugepage;

static const char * const save_restore[] = {
	"?/proc/sys/vm/nr_hugepages",
	NULL,
};

static struct tcase {
	unsigned long flags;
	unsigned long access_rights;
	char *name;
} tcases[] = {
	{0, PKEY_DISABLE_ACCESS, "PKEY_DISABLE_ACCESS"},
	{0, PKEY_DISABLE_WRITE, "PKEY_DISABLE_WRITE"},
};

static void setup(void)
{
	int i, fd;

	if (access("/sys/kernel/mm/hugepages/", F_OK)) {
		tst_res(TINFO, "Huge page is not supported");
		size = getpagesize();
		no_hugepage = 1;
	} else {
		int val;
		SAFE_FILE_PRINTF(PATH_VM_NRHPS, "%d", 1);
		SAFE_FILE_SCANF(PATH_VM_NRHPS, "%d", &val);
		if (val != 1)
			tst_brk(TBROK, "nr_hugepages = %d, but expect %d",
					val, 1);
		size = SAFE_READ_MEMINFO("Hugepagesize:") * 1024;
	}

	check_pkey_support();

	fd = SAFE_OPEN(TEST_FILE, O_RDWR | O_CREAT, 0664);
	for (i = 0; i < 128; i++)
		SAFE_WRITE(1, fd, STR, strlen(STR));

	SAFE_CLOSE(fd);
}

static struct mmap_param {
	int prot;
	int flags;
	int fd;
} mmap_params[] = {
	{PROT_READ,  MAP_ANONYMOUS | MAP_PRIVATE, -1},
	{PROT_READ,  MAP_ANONYMOUS | MAP_SHARED, -1},
	{PROT_READ,  MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1},
	{PROT_READ,  MAP_ANONYMOUS | MAP_SHARED  | MAP_HUGETLB, -1},
	{PROT_READ,  MAP_PRIVATE, 0},
	{PROT_READ,  MAP_SHARED, 0},

	{PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1},
	{PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1},
	{PROT_WRITE, MAP_PRIVATE, 0},
	{PROT_WRITE, MAP_SHARED, 0},
	{PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1},
	{PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED  | MAP_HUGETLB, -1},

	{PROT_EXEC,  MAP_ANONYMOUS | MAP_PRIVATE, -1},
	{PROT_EXEC,  MAP_ANONYMOUS | MAP_SHARED, -1},
	{PROT_EXEC,  MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1},
	{PROT_EXEC,  MAP_ANONYMOUS | MAP_SHARED  | MAP_HUGETLB, -1},
	{PROT_EXEC,  MAP_PRIVATE, 0},
	{PROT_EXEC,  MAP_SHARED, 0},

	{PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1},
	{PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1},
	{PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1},
	{PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED  | MAP_HUGETLB, -1},
	{PROT_READ | PROT_WRITE, MAP_PRIVATE, 0},
	{PROT_READ | PROT_WRITE, MAP_SHARED, 0},

	{PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1},
	{PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_SHARED, -1},
	{PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1},
	{PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_SHARED  | MAP_HUGETLB, -1},
	{PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, 0},
	{PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, 0},
};

static char *flag_to_str(int flags)
{
	switch (flags) {
	case MAP_PRIVATE:
		return "MAP_PRIVATE";
	case MAP_SHARED:
		return "MAP_SHARED";
	case MAP_ANONYMOUS | MAP_PRIVATE:
		return "MAP_ANONYMOUS|MAP_PRIVATE";
	case MAP_ANONYMOUS | MAP_SHARED:
		return "MAP_ANONYMOUS|MAP_SHARED";
	case MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB:
		return "MAP_ANONYMOUS|MAP_PRIVATE|MAP_HUGETLB";
	case MAP_ANONYMOUS | MAP_SHARED  | MAP_HUGETLB:
		return "MAP_ANONYMOUS|MAP_SHARED|MAP_HUGETLB";
	default:
		return "UNKNOWN FLAGS";
	}
}

static void pkey_test(struct tcase *tc, struct mmap_param *mpa)
{
	pid_t pid;
	char *buffer;
	int pkey, status;
	int fd = mpa->fd;

	if (no_hugepage && (mpa->flags & MAP_HUGETLB)) {
		tst_res(TINFO, "Skip test on (%s) buffer", flag_to_str(mpa->flags));
		return;
	}

	if (fd == 0)
		fd = SAFE_OPEN(TEST_FILE, O_RDWR | O_CREAT, 0664);

	buffer = SAFE_MMAP(NULL, size, mpa->prot, mpa->flags, fd, 0);

	pkey = ltp_pkey_alloc(tc->flags, tc->access_rights);
	if (pkey == -1)
		tst_brk(TBROK | TERRNO, "pkey_alloc failed");

	tst_res(TINFO, "Set %s on (%s) buffer", tc->name, flag_to_str(mpa->flags));
	if (ltp_pkey_mprotect(buffer, size, mpa->prot, pkey) == -1)
		tst_brk(TBROK | TERRNO, "pkey_mprotect failed");

	pid = SAFE_FORK();
	if (pid == 0) {
		tst_no_corefile(0);

		switch (tc->access_rights) {
		case PKEY_DISABLE_ACCESS:
			tst_res(TFAIL | TERRNO,
				"Read buffer success, buffer[0] = %d", *buffer);
		break;
		case PKEY_DISABLE_WRITE:
			*buffer = 'a';
		break;
		}
		exit(0);
	}

	SAFE_WAITPID(pid, &status, 0);

        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV)
		tst_res(TPASS, "Child ended by %s as expected", tst_strsig(SIGSEGV));
        else
                tst_res(TFAIL, "Child: %s", tst_strstatus(status));

	tst_res(TINFO, "Remove %s from the buffer", tc->name);
	if (ltp_pkey_mprotect(buffer, size, mpa->prot, 0x0) == -1)
		tst_brk(TBROK | TERRNO, "pkey_mprotect failed");

	switch (mpa->prot) {
	case PROT_READ:
		tst_res(TPASS, "Read buffer success, buffer[0] = %d", *buffer);
	break;
	case PROT_WRITE:
		*buffer = 'a';
	break;
	case PROT_READ | PROT_WRITE:
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		*buffer = 'a';
		tst_res(TPASS, "Read & Write buffer success, buffer[0] = %d", *buffer);
	break;
	}

	if (fd >= 0)
		SAFE_CLOSE(fd);

	SAFE_MUNMAP(buffer, size);

	if (ltp_pkey_free(pkey) == -1)
		tst_brk(TBROK | TERRNO, "pkey_free failed");
}

static void verify_pkey(unsigned int i)
{
	long unsigned int j;
	struct mmap_param *mpa;

	struct tcase *tc = &tcases[i];

	for (j = 0; j < ARRAY_SIZE(mmap_params); j++) {
		mpa = &mmap_params[j];

		pkey_test(tc, mpa);
	}
}

static struct tst_test test = {
	.tcnt = ARRAY_SIZE(tcases),
	.needs_root = 1,
	.needs_tmpdir = 1,
	.forks_child = 1,
	.test = verify_pkey,
	.setup = setup,
	.save_restore = save_restore,
};
