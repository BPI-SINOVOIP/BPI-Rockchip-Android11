/*
 * Copyright © 2017 Intel Corporation
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

#include <stdlib.h>
#include <signal.h>
#include "igt.h"

/*
 * The purpose of this test is to test the CI system that we have
 * for running the tests. The test should generate all possible
 * exit states for igt tests.
 *
 * Possible exit-states of igt tests:
 * 1. pass - subtest: pass-result
 * 2. fail - subtest: fail-result
 * 3. dmesg warn - subtest: dmesg-pass
 *               - subtest: dmesg-warn
 *     The purpose is to check that certain kernel log activity
 *     gets correctly reported in the test result, and that normal
 *     activity doesn't.
 * 4. crash - subtest: user-crash
 * 5. piglit timeout - subtest: piglit-timeout
 * 6. incomplete - subtest: generate-panic
 *      NOTE: inorder for this to generate the incomplete state
 *      the kernel must be configured to reboot on panic.
 *      NOTE: if the tested CI system have features such as
 *      PSTORE and/or kexec/kdump enabled. This test could be
 *      used to make sure that the CI system stores the generated
 *      log/dumps as expected.
 * 7. incomplete - where user hang is not caught by piglit timeout.
 *      This would be caught by a user-side softdog daemon,
 *      such as owatch by ezbench. However, I don't know
 *      how to trigger this state, so it will not be tested.
 * 8. incomplete - system requires hard reboot :
 *      This state could be triggered by calling an evil kernel
 *      module that was developed hang the system. Such
 *      a module will not be developed for this purpose,
 *      so this "exit state" will not be tested.
 *
 * TODO: If this test was deployed on a CI system that
 * was able to pick up testing again after reboot,
 * such as ezbench, a post-analyze test should be added
 * that collected and analyzed the result of the tests
 * run before reboot.
 */

__attribute__((format(printf, 1, 2)))
static void kmsg(const char *format, ...)
#define KERN_EMER	"<0>"
#define KERN_ALERT	"<1>"
#define KERN_CRIT	"<2>"
#define KERN_ERR	"<3>"
#define KERN_WARNING	"<4>"
#define KERN_NOTICE	"<5>"
#define KERN_INFO	"<6>"
#define KERN_DEBUG	"<7>"
{
	va_list ap;
	FILE *file;

	file = fopen("/dev/kmsg", "w");
	if (file == NULL)
		return;

	va_start(ap, format);
	vfprintf(file, format, ap);
	va_end(ap);
	fclose(file);
}

static void test_result(bool result)
{
	igt_assert_eq(result, true);
}

static void test_dmesg(bool pass)
{
	if (pass)
		kmsg(KERN_DEBUG "[drm: IGT inserted string.");
	else
		kmsg(KERN_WARNING "[drm: IGT inserted string.");
}

static void test_user_crash(void)
{
	raise(SIGSEGV);
}

static void test_piglit_timeout(void)
{
	sleep(605);
}

static void test_panic(void)
{
	system("echo c > /proc/sysrq-trigger");
}

igt_main
{

	igt_fixture {
		igt_skip_on_f(!getenv("IGT_CI_META_TEST"),
			      "Only for meta-testing of CI systems");
	}

	igt_subtest("pass-result")
		test_result(true);

	igt_subtest("warn") {
		igt_warn("This is a test that should fail with a warning\n");

		test_result(true);
	}

	igt_subtest("fail-result")
		test_result(false);

	igt_subtest("dmesg-pass")
		test_dmesg(true);

	igt_subtest("dmesg-warn")
		test_dmesg(false);

	igt_subtest("user-crash")
		test_user_crash();

	igt_subtest("piglit-timeout")
		test_piglit_timeout();

	igt_subtest("generate-panic")
		test_panic();
}

