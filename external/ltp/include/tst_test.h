// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015-2016 Cyril Hrubis <chrubis@suse.cz>
 * Copyright (c) Linux Test Project, 2016-2019
 */

#ifndef TST_TEST_H__
#define TST_TEST_H__

#ifdef __TEST_H__
# error Oldlib test.h already included
#endif /* __TEST_H__ */

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "tst_common.h"
#include "tst_res_flags.h"
#include "tst_checkpoint.h"
#include "tst_device.h"
#include "tst_mkfs.h"
#include "tst_fs.h"
#include "tst_pid.h"
#include "tst_cmd.h"
#include "tst_cpu.h"
#include "tst_process_state.h"
#include "tst_atomic.h"
#include "tst_kvercmp.h"
#include "tst_clone.h"
#include "tst_kernel.h"
#include "tst_minmax.h"
#include "tst_get_bad_addr.h"
#include "tst_path_has_mnt_flags.h"
#include "tst_sys_conf.h"
#include "tst_coredump.h"
#include "tst_buffers.h"
#include "tst_capability.h"

/*
 * Reports testcase result.
 */
void tst_res_(const char *file, const int lineno, int ttype,
              const char *fmt, ...)
              __attribute__ ((format (printf, 4, 5)));

#define tst_res(ttype, arg_fmt, ...) \
	tst_res_(__FILE__, __LINE__, (ttype), (arg_fmt), ##__VA_ARGS__)

void tst_resm_hexd_(const char *file, const int lineno, int ttype,
	const void *buf, size_t size, const char *arg_fmt, ...)
	__attribute__ ((format (printf, 6, 7)));

#define tst_res_hexd(ttype, buf, size, arg_fmt, ...) \
	tst_resm_hexd_(__FILE__, __LINE__, (ttype), (buf), (size), \
			(arg_fmt), ##__VA_ARGS__)

/*
 * Reports result and exits a test.
 */
void tst_brk_(const char *file, const int lineno, int ttype,
              const char *fmt, ...)
              __attribute__ ((format (printf, 4, 5)));

#define tst_brk(ttype, arg_fmt, ...)						\
	({									\
		TST_BRK_SUPPORTS_ONLY_TCONF_TBROK(!((ttype) &			\
			(TBROK | TCONF | TFAIL))); 				\
		tst_brk_(__FILE__, __LINE__, (ttype), (arg_fmt), ##__VA_ARGS__);\
	})

/* flush stderr and stdout */
void tst_flush(void);

pid_t safe_fork(const char *filename, unsigned int lineno);
#define SAFE_FORK() \
	safe_fork(__FILE__, __LINE__)

#define TST_TRACE(expr)	                                            \
	({int ret = expr;                                           \
	  ret != 0 ? tst_res(TINFO, #expr " failed"), ret : ret; }) \

#include "tst_safe_macros.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_net.h"

/*
 * Wait for all children and exit with TBROK if
 * any of them returned a non-zero exit status.
 */
void tst_reap_children(void);

struct tst_option {
	char *optstr;
	char **arg;
	char *help;
};

/*
 * Options parsing helpers.
 *
 * If str is NULL these are No-op.
 *
 * On failure non-zero (errno) is returned.
 */
int tst_parse_int(const char *str, int *val, int min, int max);
int tst_parse_long(const char *str, long *val, long min, long max);
int tst_parse_float(const char *str, float *val, float min, float max);

struct tst_tag {
	const char *name;
	const char *value;
};

extern unsigned int tst_variant;

struct tst_test {
	/* number of tests available in test() function */
	unsigned int tcnt;

	struct tst_option *options;

	const char *min_kver;

	/* If set the test is compiled out */
	const char *tconf_msg;

	int needs_tmpdir:1;
	int needs_root:1;
	int forks_child:1;
	int needs_device:1;
	int needs_checkpoints:1;
	int needs_overlay:1;
	int format_device:1;
	int mount_device:1;
	int needs_rofs:1;
	int child_needs_reinit:1;
	int needs_devfs:1;
	int restore_wallclock:1;
	/*
	 * If set the test function will be executed for all available
	 * filesystems and the current filesytem type would be set in the
	 * tst_device->fs_type.
	 *
	 * The test setup and cleanup are executed before/after __EACH__ call
	 * to the test function.
	 */
	int all_filesystems:1;

	/*
	 * If set non-zero denotes number of test variant, the test is executed
	 * variants times each time with tst_variant set to different number.
	 *
	 * This allows us to run the same test for different settings. The
	 * intended use is to test different syscall wrappers/variants but the
	 * API is generic and does not limit the usage in any way.
	 */
	unsigned int test_variants;

	/* Minimal device size in megabytes */
	unsigned int dev_min_size;

	/* Device filesystem type override NULL == default */
	const char *dev_fs_type;
	/* Flags to be passed to tst_get_supported_fs_types() */
	int dev_fs_flags;

	/* Options passed to SAFE_MKFS() when format_device is set */
	const char *const *dev_fs_opts;
	const char *const *dev_extra_opts;

	/* Device mount options, used if mount_device is set */
	const char *mntpoint;
	unsigned int mnt_flags;
	void *mnt_data;

	/* override default timeout per test run, disabled == -1 */
	int timeout;

	void (*setup)(void);
	void (*cleanup)(void);

	void (*test)(unsigned int test_nr);
	void (*test_all)(void);

	/* Syscall name used by the timer measurement library */
	const char *scall;

	/* Sampling function for timer measurement testcases */
	int (*sample)(int clk_id, long long usec);

	/* NULL terminated array of resource file names */
	const char *const *resource_files;

	/* NULL terminated array of needed kernel drivers */
	const char * const *needs_drivers;

	/*
	 * NULL terminated array of (/proc, /sys) files to save
	 * before setup and restore after cleanup
	 */
	const char * const *save_restore;

	/*
	 * NULL terminated array of kernel config options required for the
	 * test.
	 */
	const char *const *needs_kconfigs;

	/*
	 * NULL-terminated array to be allocated buffers.
	 */
	struct tst_buffers *bufs;

	/*
	 * NULL-terminated array of capability settings
	 */
	struct tst_cap *caps;

	/*
	 * {NULL, NULL} terminated array of tags.
	 */
	const struct tst_tag *tags;
};

/*
 * Runs tests.
 */
void tst_run_tcases(int argc, char *argv[], struct tst_test *self)
                    __attribute__ ((noreturn));

/*
 * Does library initialization for child processes started by exec()
 *
 * The LTP_IPC_PATH variable must be passed to the program environment.
 */
void tst_reinit(void);

//TODO Clean?
#define TEST(SCALL) \
	do { \
		errno = 0; \
		TST_RET = SCALL; \
		TST_ERR = errno; \
	} while (0)

#define TEST_VOID(SCALL) \
	do { \
		errno = 0; \
		SCALL; \
		TST_ERR = errno; \
	} while (0)

extern long TST_RET;
extern int TST_ERR;

extern void *TST_RET_PTR;

#define TESTPTR(SCALL) \
	do { \
		errno = 0; \
		TST_RET_PTR = (void*)SCALL; \
		TST_ERR = errno; \
	} while (0)

/*
 * Functions to convert ERRNO to its name and SIGNAL to its name.
 */
const char *tst_strerrno(int err);
const char *tst_strsig(int sig);
/*
 * Returns string describing status as returned by wait().
 *
 * BEWARE: Not thread safe.
 */
const char *tst_strstatus(int status);

unsigned int tst_timeout_remaining(void);
unsigned int tst_multiply_timeout(unsigned int timeout);
void tst_set_timeout(int timeout);


/*
 * Returns path to the test temporary directory in a newly allocated buffer.
 */
char *tst_get_tmpdir(void);

#ifndef TST_NO_DEFAULT_MAIN

static struct tst_test test;

int main(int argc, char *argv[])
{
	tst_run_tcases(argc, argv, &test);
}

#endif /* TST_NO_DEFAULT_MAIN */

#define TST_TEST_TCONF(message)                                 \
        static struct tst_test test = { .tconf_msg = message  } \
/*
 * This is a hack to make the testcases link without defining TCID
 */
const char *TCID;

#endif	/* TST_TEST_H__ */
