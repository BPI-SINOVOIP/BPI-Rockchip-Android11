/*
 * Copyright © 2007,2014 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 */


#ifndef IGT_CORE_H
#define IGT_CORE_H

#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>

#ifndef IGT_LOG_DOMAIN
#define IGT_LOG_DOMAIN (NULL)
#endif


#ifndef STATIC_ANALYSIS_BUILD
#if defined(__clang_analyzer__) || defined(__COVERITY__) || defined(__KLOCWORK__)
#define STATIC_ANALYSIS_BUILD 1
#else
#define STATIC_ANALYSIS_BUILD 0
#endif
#endif

/**
 * BUILD_BUG_ON_INVALID:
 * @expr: Expression
 *
 * A macro that takes an expression and generates no code. Used for
 * checking at build-time that an expression is valid code.
 */
#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((long)(e))))

/**
 * igt_assume:
 * @expr: Condition to test
 *
 * An assert-like macro to be used for tautologies to give hints to
 * static analysis of code. No-op if STATIC_ANALYSIS_BUILD is not
 * defined, expands to an assert() if it is.
 */
#if STATIC_ANALYSIS_BUILD
#define igt_assume(e) assert(e)
#else
/* Make sure the expression is still parsed even though it generates no code */
#define igt_assume(e) BUILD_BUG_ON_INVALID(e)
#endif

extern const char* __igt_test_description __attribute__((weak));
extern bool __igt_plain_output;
extern char *igt_frame_dump_path;

/**
 * IGT_TEST_DESCRIPTION:
 * @str: description string
 *
 * Defines a description for a test. This is used as the output for the
 * "--help-description" option and is also included in the generated
 * documentation.
 */
#define IGT_TEST_DESCRIPTION(str) const char* __igt_test_description = str

/**
 * IGT_EXIT_SKIP:
 *
 * Exit status indicating the test was skipped.
 */
#define IGT_EXIT_SKIP    77

/**
 * IGT_EXIT_SUCCESS
 *
 * Exit status indicating the test executed successfully.
 */
#define IGT_EXIT_SUCCESS 0

/**
 * IGT_EXIT_INVALID
 *
 * Exit status indicating an invalid option or subtest was specified
 */
#define IGT_EXIT_INVALID 79

/**
 * IGT_EXIT_FAILURE
 *
 * Exit status indicating a test failure
 */
#define IGT_EXIT_FAILURE 98

bool __igt_fixture(void);
void __igt_fixture_complete(void);
void __igt_fixture_end(void) __attribute__((noreturn));
/**
 * igt_fixture:
 *
 * Annotate global test fixture code
 *
 * Testcase with subtests often need to set up a bunch of global state as the
 * common test fixture. To avoid such code interfering with the subtest
 * enumeration (e.g. when enumerating on systems without an intel gpu) such
 * blocks should be annotated with igt_fixture.
 */
#define igt_fixture for (volatile int igt_tokencat(__tmpint,__LINE__) = 0; \
			 igt_tokencat(__tmpint,__LINE__) < 1 && \
			 (STATIC_ANALYSIS_BUILD || \
			 (__igt_fixture() && \
			 (sigsetjmp(igt_subtest_jmpbuf, 1) == 0))); \
			 igt_tokencat(__tmpint,__LINE__) ++, \
			 __igt_fixture_complete())

/* subtest infrastructure */
jmp_buf igt_subtest_jmpbuf;
typedef int (*igt_opt_handler_t)(int opt, int opt_index, void *data);
#define IGT_OPT_HANDLER_SUCCESS 0
#define IGT_OPT_HANDLER_ERROR -2
#ifndef __GTK_DOC_IGNORE__ /* gtkdoc wants to document this forward decl */
struct option;
#endif
int igt_subtest_init_parse_opts(int *argc, char **argv,
				const char *extra_short_opts,
				const struct option *extra_long_opts,
				const char *help_str,
				igt_opt_handler_t extra_opt_handler,
				void *handler_data);


/**
 * igt_subtest_init:
 * @argc: argc from the test's main()
 * @argv: argv from the test's main()
 *
 * This initializes the for tests with subtests without the need for additional
 * command line options. It is just a simplified version of
 * igt_subtest_init_parse_opts().
 *
 * If there's not a reason to the contrary it's less error prone to just use an
 * #igt_main block instead of stitching the test's main() function together
 * manually.
 */
#define igt_subtest_init(argc, argv) \
	igt_subtest_init_parse_opts(&argc, argv, NULL, NULL, NULL, NULL, NULL);

bool __igt_run_subtest(const char *subtest_name, const char *file, const int line);
#define __igt_tokencat2(x, y) x ## y

/**
 * igt_tokencat:
 * @x: first variable
 * @y: second variable
 *
 * C preprocessor helper to concatenate two variables while properly expanding
 * them.
 */
#define igt_tokencat(x, y) __igt_tokencat2(x, y)

/**
 * igt_subtest:
 * @name: name of the subtest
 *
 * This is a magic control flow block which denotes a subtest code block. Within
 * that code block igt_skip|success will only bail out of the subtest. The _f
 * variant accepts a printf format string, which is useful for constructing
 * combinatorial tests.
 *
 * This is a simpler version of igt_subtest_f()
 */
#define igt_subtest(name) for (; __igt_run_subtest((name), __FILE__, __LINE__) && \
				   (sigsetjmp(igt_subtest_jmpbuf, 1) == 0); \
				   igt_success())
#define __igt_subtest_f(tmp, format...) \
	for (char tmp [256]; \
	     snprintf( tmp , sizeof( tmp ), \
		      format), \
	     __igt_run_subtest(tmp, __FILE__, __LINE__) && \
	     (sigsetjmp(igt_subtest_jmpbuf, 1) == 0); \
	     igt_success())

/**
 * igt_subtest_f:
 * @...: format string and optional arguments
 *
 * This is a magic control flow block which denotes a subtest code block. Within
 * that code block igt_skip|success will only bail out of the subtest. The _f
 * variant accepts a printf format string, which is useful for constructing
 * combinatorial tests.
 *
 * Like igt_subtest(), but also accepts a printf format string instead of a
 * static string.
 */
#define igt_subtest_f(f...) \
	__igt_subtest_f(igt_tokencat(__tmpchar, __LINE__), f)

const char *igt_subtest_name(void);
bool igt_only_list_subtests(void);

void __igt_subtest_group_save(int *, int *);
void __igt_subtest_group_restore(int, int);
/**
 * igt_subtest_group:
 *
 * Group a set of subtests together with their common setup code
 *
 * Testcase with subtests often need to set up a bunch of shared state as the
 * common test fixture. But if there are multiple with different requirements
 * the commont setup code can't be extracted, since a test condition failure in
 * e.g. igt_require() would result in all subsequent tests skipping. Even those
 * from a different group.
 *
 * This macro allows to group together a set of #igt_fixture and #igt_subtest
 * clauses. If any common setup in a fixture fails, only the subtests in this
 * group will fail or skip. Subtest groups can be arbitrarily nested.
 */
#define igt_subtest_group for (int igt_tokencat(__tmpint,__LINE__) = 0, \
			       igt_tokencat(__save,__LINE__) = 0, \
			       igt_tokencat(__desc,__LINE__) = 0; \
			       igt_tokencat(__tmpint,__LINE__) < 1 && \
			       (__igt_subtest_group_save(& igt_tokencat(__save,__LINE__), \
							 & igt_tokencat(__desc,__LINE__) ), true); \
			       igt_tokencat(__tmpint,__LINE__) ++, \
			       __igt_subtest_group_restore(igt_tokencat(__save,__LINE__), \
							   igt_tokencat(__desc,__LINE__)))

/**
 * igt_main_args:
 * @extra_short_opts: getopt_long() compliant list with additional short options
 * @extra_long_opts: getopt_long() compliant list with additional long options
 * @help_str: help string for the additional options
 * @extra_opt_handler: handler for the additional options
 * @handler_data: user data given to @extra_opt_handler when invoked
 *
 * This is a magic control flow block used instead of a main()
 * function for tests with subtests, along with custom command line
 * arguments. The macro parameters are passed directly to
 * #igt_subtest_init_parse_opts.
 */
#define igt_main_args(short_opts, long_opts, help_str, opt_handler, handler_data) \
	static void igt_tokencat(__real_main, __LINE__)(void); \
	int main(int argc, char **argv) { \
		igt_subtest_init_parse_opts(&argc, argv, \
					    short_opts, long_opts, help_str, \
					    opt_handler, handler_data); \
		igt_tokencat(__real_main, __LINE__)(); \
		igt_exit(); \
	} \
	static void igt_tokencat(__real_main, __LINE__)(void) \


/**
 * igt_main:
 *
 * This is a magic control flow block used instead of a main() function for
 * tests with subtests. Open-coding the main() function is not recommended.
 */
#define igt_main igt_main_args(NULL, NULL, NULL, NULL, NULL)

const char *igt_test_name(void);
void igt_simple_init_parse_opts(int *argc, char **argv,
				const char *extra_short_opts,
				const struct option *extra_long_opts,
				const char *help_str,
				igt_opt_handler_t extra_opt_handler,
				void *handler_data);

/**
 * igt_simple_init:
 * @argc: argc from the test's main()
 * @argv: argv from the test's main()
 *
 * This initializes a simple test without any support for subtests.
 *
 * If there's not a reason to the contrary it's less error prone to just use an
 * #igt_simple_main block instead of stitching the test's main() function together
 * manually.
 */
#define igt_simple_init(argc, argv) \
	igt_simple_init_parse_opts(&argc, argv, NULL, NULL, NULL, NULL, NULL);


/**
 * igt_simple_main_args:
 * @extra_short_opts: getopt_long() compliant list with additional short options
 * @extra_long_opts: getopt_long() compliant list with additional long options
 * @help_str: help string for the additional options
 * @extra_opt_handler: handler for the additional options
 * @handler_data: user data given to @extra_opt_handler when invoked
 *
 * This is a magic control flow block used instead of a main()
 * function for simple tests with custom command line arguments. The
 * macro parameters are passed directly to
 * #igt_simple_init_parse_opts.
 */
#define igt_simple_main_args(short_opts, long_opts, help_str, opt_handler, handler_data) \
	static void igt_tokencat(__real_main, __LINE__)(void); \
	int main(int argc, char **argv) { \
		igt_simple_init_parse_opts(&argc, argv, \
					   short_opts, long_opts, help_str, \
					   opt_handler, handler_data);	\
		igt_tokencat(__real_main, __LINE__)(); \
		igt_exit(); \
	} \
	static void igt_tokencat(__real_main, __LINE__)(void) \


/**
 * igt_simple_main:
 *
 * This is a magic control flow block used instead of a main() function for
 * simple tests. Open-coding the main() function is not recommended.
 */
#define igt_simple_main igt_simple_main_args(NULL, NULL, NULL, NULL, NULL)

/**
 * igt_constructor:
 *
 * Convenience macro to run the provided code block when igt first starts,
 * before any tests have been run. This should be used for parts of the igt
 * library that require initialization of objects with global context.
 *
 * This code block will be executed exactly once.
 */
#define igt_constructor \
	__attribute__((constructor)) \
	static void igt_tokencat(__igt_constructor_l, __LINE__)(void)

__attribute__((format(printf, 1, 2)))
void igt_skip(const char *f, ...) __attribute__((noreturn));
__attribute__((format(printf, 5, 6)))
void __igt_skip_check(const char *file, const int line,
		      const char *func, const char *check,
		      const char *format, ...) __attribute__((noreturn));
#define igt_skip_check(E, F...) \
	__igt_skip_check(__FILE__, __LINE__, __func__, E, F)
void igt_success(void);

bool igt_can_fail(void);

void igt_fail(int exitcode) __attribute__((noreturn));
__attribute__((format(printf, 6, 7)))
void __igt_fail_assert(const char *domain, const char *file,
		       const int line, const char *func, const char *assertion,
		       const char *format, ...)
	__attribute__((noreturn));
void igt_exit(void) __attribute__((noreturn));
void igt_fatal_error(void) __attribute__((noreturn));

/**
 * igt_ignore_warn:
 * @expr: condition to ignore
 *
 *
 * Stops the compiler warning about an unused return value.
 */
static inline void igt_ignore_warn(bool value)
{
}

__attribute__((format(printf, 1, 2)))
void igt_describe_f(const char *fmt, ...);

/**
 * igt_describe:
 * @dsc: string containing description
 *
 * Attach a description to the following #igt_subtest or #igt_subtest_group
 * block.
 *
 * The description should complement the test/subtest name and provide more
 * context on what is being tested. It should explain the idea of the test and
 * do not mention implementation details, so that it never goes out of date.
 *
 * DO:
 *  * focus on the userspace's perspective
 *  * try to capture the reason for the test's existence
 *  * be brief
 *
 * DON'T:
 *  * try to translate the code into English
 *  * explain all the checks the test does
 *  * delve on the implementation
 *
 * Good examples:
 *  * "make sure that legacy cursor updates do not stall atomic commits"
 *  * "check that atomic updates of many planes are indeed atomic and take
 *     effect immediately after the commit"
 *  * "make sure that the meta-data exposed by the kernel to the userspace
 *     is correct and matches the used EDID"
 *
 * Bad examples:
 *  * "spawn 10 threads, each pinning cpu core with a busy loop..."
 *  * "randomly generate holes in a primary plane then try to cover each hole
 *    with a plane and make sure that CRC matches, do 25 gazillion rounds of
 *    that..."
 *
 *
 * Resulting #igt_subtest documentation is a concatenation of its own
 * description and all the parenting #igt_subtest_group descriptions, starting
 * from the outermost one. Example:
 *
 * |[<!-- language="C" -->
 * #include "igt.h"
 *
 * IGT_TEST_DESCRIPTION("Global description of the whole binary");
 * igt_main
 * {
 * 	igt_describe("Desc of the subgroup with A and B");
 * 	igt_subtest_group {
 * 		igt_describe("Desc of the subtest A");
 * 		igt_subtest("subtest-a") {
 * 			...
 * 		}
 *
 * 		igt_describe("Desc of the subtest B");
 * 		igt_subtest("subtest-b") {
 * 			...
 * 		}
 * 	}
 *
 * 	igt_describe("Desc of the subtest C");
 * 	igt_subtest("subtest-c") {
 * 		...
 * 	}
 * }
 * ]|
 *
 * It's will accessible via --describe command line switch:
 *
 * |[
 * $ test --describe
 * Global description of the whole binary
 *
 * SUB subtest-a test.c:5:
 *   Desc of the subgroup with A and B
 *
 *   Desc of the subtest A
 *
 * SUB subtest-b test.c:10:
 *   Desc of the subgroup with A and B
 *
 *   Desc of the subtest B
 *
 * SUB subtest-c test.c:15:
 *   Desc of the subtest C
 * ]|
 *
 * Every single #igt_subtest does not have to be preceded with a #igt_describe
 * as long as it has good-enough explanation provided on the #igt_subtest_group
 * level.
 *
 * Example:
 *
 * |[<!-- language="C" -->
 * #include "igt.h"
 *
 * igt_main
 * {
 * 	igt_describe("check xyz with different tilings");
 * 	igt_subtest_group {
 * 		// no need for extra description, group is enough and tiling is
 * 		// obvious from the test name
 * 		igt_subtest("foo-tiling-x") {
 * 			...
 * 		}
 *
 * 		igt_subtest("foo-tiling-y") {
 * 			...
 * 		}
 * 	}
 * }
 * ]|
 *
 */
#define igt_describe(dsc) \
	igt_describe_f("%s", dsc)

/**
 * igt_assert:
 * @expr: condition to test
 *
 * Fails (sub-)test if the condition is not met.
 *
 * Should be used everywhere where a test checks results.
 */
#define igt_assert(expr) \
	do { if (!(expr)) \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, #expr , NULL); \
	} while (0)

/**
 * igt_assert_f:
 * @expr: condition to test
 * @...: format string and optional arguments
 *
 * Fails (sub-)test if the condition is not met.
 *
 * Should be used everywhere where a test checks results.
 *
 * In addition to the plain igt_assert() helper this allows to print additional
 * information to help debugging test failures.
 */
#define igt_assert_f(expr, f...) \
	do { if (!(expr)) \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, #expr , f); \
	} while (0)

/**
 * igt_fail_on:
 * @expr: condition to test
 *
 * Fails (sub-)test if the condition is met.
 *
 * Should be used everywhere where a test checks results.
 */
#define igt_fail_on(expr) igt_assert(!(expr))

/**
 * igt_fail_on_f:
 * @expr: condition to test
 * @...: format string and optional arguments
 *
 * Fails (sub-)test if the condition is met.
 *
 * Should be used everywhere where a test checks results.
 *
 * In addition to the plain igt_assert() helper this allows to print additional
 * information to help debugging test failures.
 */
#define igt_fail_on_f(expr, f...) igt_assert_f(!(expr), f)

/**
 * igt_assert_cmpint:
 * @n1: first value
 * @cmp: compare operator
 * @ncmp: negated version of @cmp
 * @n2: second value
 *
 * Fails (sub-)test if the condition is not met
 *
 * Should be used everywhere where a test compares two integer values.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_cmpint(n1, cmp, ncmp, n2) \
	do { \
		int __n1 = (n1), __n2 = (n2); \
		if (__n1 cmp __n2) ; else \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, \
				  #n1 " " #cmp " " #n2, \
				  "error: %d " #ncmp " %d\n", __n1, __n2); \
	} while (0)

/**
 * igt_assert_cmpuint:
 * @n1: first value
 * @cmp: compare operator
 * @ncmp: negated version of @cmp
 * @n2: second value
 *
 * Like igt_assert_cmpint(), but for unsigned ints.
 */
#define igt_assert_cmpuint(n1, cmp, ncmp, n2) \
	do { \
		uint32_t __n1 = (n1), __n2 = (n2); \
		if (__n1 cmp __n2) ; else \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, \
				  #n1 " " #cmp " " #n2, \
				  "error: %#x " #ncmp " %#x\n", __n1, __n2); \
	} while (0)

/**
 * igt_assert_cmps64:
 * @n1: first value
 * @cmp: compare operator
 * @ncmp: negated version of @cmp
 * @n2: second value
 *
 * Like igt_assert_cmpuint(), but for larger signed ints.
 */
#define igt_assert_cmps64(n1, cmp, ncmp, n2) \
	do { \
		int64_t __n1 = (n1), __n2 = (n2); \
		if (__n1 cmp __n2) ; else \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, \
				  #n1 " " #cmp " " #n2, \
				  "error: %lld " #ncmp " %lld\n", (long long)__n1, (long long)__n2); \
	} while (0)

/**
 * igt_assert_cmpu64:
 * @n1: first value
 * @cmp: compare operator
 * @ncmp: negated version of @cmp
 * @n2: second value
 *
 * Like igt_assert_cmpuint(), but for larger ints.
 */
#define igt_assert_cmpu64(n1, cmp, ncmp, n2) \
	do { \
		uint64_t __n1 = (n1), __n2 = (n2); \
		if (__n1 cmp __n2) ; else \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, \
				  #n1 " " #cmp " " #n2, \
				  "error: %#llx " #ncmp " %#llx\n", (long long)__n1, (long long)__n2); \
	} while (0)

/**
 * igt_assert_cmpdouble:
 * @n1: first value
 * @cmp: compare operator
 * @ncmp: negated version of @cmp
 * @n2: second value
 *
 * Like igt_assert_cmpint(), but for doubles.
 */
#define igt_assert_cmpdouble(n1, cmp, ncmp, n2) \
	do { \
		double __n1 = (n1), __n2 = (n2); \
		if (__n1 cmp __n2) ; else \
		__igt_fail_assert(IGT_LOG_DOMAIN, __FILE__, __LINE__, __func__, \
				  #n1 " " #cmp " " #n2, \
				  "error: %#lf " #ncmp " %#lf\n", __n1, __n2); \
	} while (0)

/**
 * igt_assert_eq:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the two integers are not equal. Beware that for now this
 * only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_eq(n1, n2) igt_assert_cmpint(n1, ==, !=, n2)

/**
 * igt_assert_eq_u32:
 * @n1: first integer
 * @n2: second integer
 *
 * Like igt_assert_eq(), but for uint32_t.
 */
#define igt_assert_eq_u32(n1, n2) igt_assert_cmpuint(n1, ==, !=, n2)

/**
 * igt_assert_eq_s64:
 * @n1: first integer
 * @n2: second integer
 *
 * Like igt_assert_eq_u32(), but for int64_t.
 */
#define igt_assert_eq_s64(n1, n2) igt_assert_cmps64(n1, ==, !=, n2)

/**
 * igt_assert_eq_u64:
 * @n1: first integer
 * @n2: second integer
 *
 * Like igt_assert_eq_u32(), but for uint64_t.
 */
#define igt_assert_eq_u64(n1, n2) igt_assert_cmpu64(n1, ==, !=, n2)

/**
 * igt_assert_eq_double:
 * @n1: first double
 * @n2: second double
 *
 * Like igt_assert_eq(), but for doubles.
 */
#define igt_assert_eq_double(n1, n2) igt_assert_cmpdouble(n1, ==, !=, n2)

/**
 * igt_assert_neq:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the two integers are equal. Beware that for now this
 * only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_neq(n1, n2) igt_assert_cmpint(n1, !=, ==, n2)

/**
 * igt_assert_neq_u32:
 * @n1: first integer
 * @n2: second integer
 *
 * Like igt_assert_neq(), but for uint32_t.
 */
#define igt_assert_neq_u32(n1, n2) igt_assert_cmpuint(n1, !=, ==, n2)

/**
 * igt_assert_neq_u64:
 * @n1: first integer
 * @n2: second integer
 *
 * Like igt_assert_neq_u32(), but for uint64_t.
 */
#define igt_assert_neq_u64(n1, n2) igt_assert_cmpu64(n1, !=, ==, n2)

/**
 * igt_assert_neq_double:
 * @n1: first double
 * @n2: second double
 *
 * Like igt_assert_neq(), but for doubles.
 */
#define igt_assert_neq_double(n1, n2) igt_assert_cmpdouble(n1, !=, ==, n2)

/**
 * igt_assert_lte:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the second integer is strictly smaller than the first.
 * Beware that for now this only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_lte(n1, n2) igt_assert_cmpint(n1, <=, >, n2)

/**
 * igt_assert_lte_u64:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the second integer is strictly smaller than the first.
 * Beware that for now this only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_lte_u64(n1, n2) igt_assert_cmpu64(n1, <=, >, n2)

/**
 * igt_assert_lte_s64:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the second integer is strictly smaller than the first.
 * Beware that for now this only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_lte_s64(n1, n2) igt_assert_cmps64(n1, <=, >, n2)

/**
 * igt_assert_lt:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the second integer is smaller than or equal to the first.
 * Beware that for now this only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_lt(n1, n2) igt_assert_cmpint(n1, <, >=, n2)

/**
 * igt_assert_lt_u64:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the second integer is smaller than or equal to the first.
 * Beware that for now this only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_lt_u64(n1, n2) igt_assert_cmpu64(n1, <, >=, n2)

/**
 * igt_assert_lt_s64:
 * @n1: first integer
 * @n2: second integer
 *
 * Fails (sub-)test if the second integer is smaller than or equal to the first.
 * Beware that for now this only works on integers.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_lt_s64(n1, n2) igt_assert_cmps64(n1, <, >=, n2)

/**
 * igt_assert_fd:
 * @fd: file descriptor
 *
 * Fails (sub-) test if the given file descriptor is invalid.
 *
 * Like igt_assert(), but displays the values being compared on failure instead
 * of simply printing the stringified expression.
 */
#define igt_assert_fd(fd) \
	igt_assert_f(fd >= 0, "file descriptor " #fd " failed\n");

/**
 * igt_require:
 * @expr: condition to test
 *
 * Skip a (sub-)test if a condition is not met.
 *
 * Should be used everywhere where a test checks results to decide about
 * skipping. This is useful to streamline the skip logic since it allows for a more flat
 * code control flow, similar to igt_assert()
 */
#define igt_require(expr) do { \
	if (!(expr)) igt_skip_check(#expr , NULL); \
	else igt_debug("Test requirement passed: %s\n", #expr); \
} while (0)

/**
 * igt_skip_on:
 * @expr: condition to test
 *
 * Skip a (sub-)test if a condition is met.
 *
 * Should be used everywhere where a test checks results to decide about
 * skipping. This is useful to streamline the skip logic since it allows for a more flat
 * code control flow, similar to igt_assert()
 */
#define igt_skip_on(expr) do { \
	if ((expr)) igt_skip_check("!(" #expr ")" , NULL); \
	else igt_debug("Test requirement passed: !(%s)\n", #expr); \
} while (0)

/**
 * igt_require_f:
 * @expr: condition to test
 * @...: format string and optional arguments
 *
 * Skip a (sub-)test if a condition is not met.
 *
 * Should be used everywhere where a test checks results to decide about
 * skipping. This is useful to streamline the skip logic since it allows for a more flat
 * code control flow, similar to igt_assert()
 *
 * In addition to the plain igt_require() helper this allows to print additional
 * information to help debugging test failures.
 */
#define igt_require_f(expr, f...) do { \
	if (!(expr)) igt_skip_check(#expr , f); \
	else igt_debug("Test requirement passed: %s\n", #expr); \
} while (0)

/**
 * igt_skip_on_f:
 * @expr: condition to test
 * @...: format string and optional arguments
 *
 * Skip a (sub-)test if a condition is met.
 *
 * Should be used everywhere where a test checks results to decide about
 * skipping. This is useful to streamline the skip logic since it allows for a more flat
 * code control flow, similar to igt_assert()
 *
 * In addition to the plain igt_skip_on() helper this allows to print additional
 * information to help debugging test failures.
 */
#define igt_skip_on_f(expr, f...) do { \
	if ((expr)) igt_skip_check("!("#expr")", f); \
	else igt_debug("Test requirement passed: !(%s)\n", #expr); \
} while (0)

/* fork support code */
bool __igt_fork(void);

/**
 * igt_fork:
 * @child: name of the int variable with the child number
 * @num_children: number of children to fork
 *
 * This is a magic control flow block which spawns parallel test threads with
 * fork().
 *
 * The test children execute in parallel to the main test thread. Joining all
 * test threads should be done with igt_waitchildren to ensure that the exit
 * codes of all children are properly reflected in the test status.
 *
 * Note that igt_skip() will not be forwarded, feature tests need to be done
 * before spawning threads with igt_fork().
 */
#define igt_fork(child, num_children) \
	for (int child = 0; child < (num_children); child++) \
		for (; __igt_fork(); exit(0))
int __igt_waitchildren(void);
void igt_waitchildren(void);
void igt_waitchildren_timeout(int seconds, const char *reason);

/**
 * igt_helper_process:
 * @running: indicates whether the process is currently running
 * @use_SIGKILL: whether the helper should be terminated with SIGKILL or SIGTERM
 * @pid: pid of the helper if @running is true
 * @id: internal id
 *
 * Tracking structure for helper processes. Users of the i-g-t library should
 * only set @use_SIGKILL directly.
 */
struct igt_helper_process {
	bool running;
	bool use_SIGKILL;
	pid_t pid;
	int id;
};
bool __igt_fork_helper(struct igt_helper_process *proc);

/**
 * igt_fork_helper:
 * @proc: #igt_helper_process structure
 *
 * This is a magic control flow block which denotes an asynchronous helper
 * process block. The difference compared to igt_fork() is that failures from
 * the child process will not be forwarded, making this construct more suitable
 * for background processes. Common use cases are regular interference of the
 * main test thread through e.g. sending signals or evicting objects through
 * debugfs. Through the explicit #igt_helper_process they can also be controlled
 * in a more fine-grained way than test children spawned through igt_fork().
 *
 * For tests with subtest helper process can be started outside of a
 * #igt_subtest block.
 *
 * Calling igt_wait_helper() joins a helper process and igt_stop_helper()
 * forcefully terminates it.
 */
#define igt_fork_helper(proc) \
	for (; __igt_fork_helper(proc); exit(0))
int igt_wait_helper(struct igt_helper_process *proc);
void igt_stop_helper(struct igt_helper_process *proc);

/* exit handler code */

/**
 * igt_exit_handler_t:
 * @sig: Signal number which caused the exit or 0.
 *
 * Exit handler type used by igt_install_exit_handler(). Note that exit handlers
 * can potentially be run from signal handling contexts, the @sig parameter can
 * be used to figure this out and act accordingly.
 */
typedef void (*igt_exit_handler_t)(int sig);

/* reliable atexit helpers, also work when killed by a signal (if possible) */
void igt_install_exit_handler(igt_exit_handler_t fn);

/* helpers to automatically reduce test runtime in simulation */
bool igt_run_in_simulation(void);
/**
 * SLOW_QUICK:
 * @slow: value in simulation mode
 * @quick: value in normal mode
 *
 * Simple macro to select between two values (e.g. number of test rounds or test
 * buffer size) depending upon whether i-g-t is run in simulation mode or not.
 */
#define SLOW_QUICK(slow,quick) (igt_run_in_simulation() ? (quick) : (slow))

void igt_skip_on_simulation(void);

extern const char *igt_interactive_debug;
extern bool igt_skip_crc_compare;

/**
 * igt_log_level:
 * @IGT_LOG_DEBUG: debug information, not printed by default
 * @IGT_LOG_INFO: informational message, printed by default
 * @IGT_LOG_WARN: non-fatal warnings which should be treated as test failures
 * @IGT_LOG_CRITICAL: critical errors which lead to immediate termination of tests
 * @IGT_LOG_NONE: unused
 *
 * Log levels used by functions like igt_log().
 */
enum igt_log_level {
	IGT_LOG_DEBUG,
	IGT_LOG_INFO,
	IGT_LOG_WARN,
	IGT_LOG_CRITICAL,
	IGT_LOG_NONE,
};
__attribute__((format(printf, 3, 4)))
void igt_log(const char *domain, enum igt_log_level level, const char *format, ...);
__attribute__((format(printf, 3, 0)))
void igt_vlog(const char *domain, enum igt_log_level level, const char *format, va_list args);

/**
 * igt_debug:
 * @...: format string and optional arguments
 *
 * Wrapper for igt_log() for message at the IGT_LOG_DEBUG level.
 */
#define igt_debug(f...) igt_log(IGT_LOG_DOMAIN, IGT_LOG_DEBUG, f)

/**
 * igt_info:
 * @...: format string and optional arguments
 *
 * Wrapper for igt_log() for message at the IGT_LOG_INFO level.
 */
#define igt_info(f...) igt_log(IGT_LOG_DOMAIN, IGT_LOG_INFO, f)

/**
 * igt_warn:
 * @...: format string and optional arguments
 *
 * Wrapper for igt_log() for message at the IGT_LOG_WARN level.
 */
#define igt_warn(f...) igt_log(IGT_LOG_DOMAIN, IGT_LOG_WARN, f)

/**
 * igt_critical:
 * @...: format string and optional arguments
 *
 * Wrapper for igt_log() for message at the IGT_LOG_CRITICAL level.
 */
#define igt_critical(f...) igt_log(IGT_LOG_DOMAIN, IGT_LOG_CRITICAL, f)

typedef bool (*igt_buffer_log_handler_t)(const char *line, void *data);
void igt_log_buffer_inspect(igt_buffer_log_handler_t check, void *data);

extern enum igt_log_level igt_log_level;

/**
 * igt_warn_on:
 * @condition: condition to test
 *
 * Print a IGT_LOG_WARN level message if a condition is not met.
 *
 * Should be used everywhere where a test checks results to decide about
 * printing warnings. This is useful to streamline the test logic since it
 * allows for a more flat code control flow, similar to igt_assert()
 *
 * This macro also returns the value of @condition.
 */
#define igt_warn_on(condition) ({ \
		typeof(condition) ret__ = (condition); \
		if (ret__) \
			igt_warn("Warning on condition %s in function %s, file %s:%i\n", \
				 #condition, __func__, __FILE__, __LINE__); \
		ret__; \
	})

/**
 * igt_warn_on_f:
 * @condition: condition to test
 * @...: format string and optional arguments
 *
 * Skip a (sub-)test if a condition is not met.
 *
 * Print a IGT_LOG_WARN level message if a condition is not met.
 *
 * Should be used everywhere where a test checks results to decide about
 * printing warnings. This is useful to streamline the test logic since it
 * allows for a more flat code control flow, similar to igt_assert()
 *
 * In addition to the plain igt_warn_on_f() helper this allows to print
 * additional information (again as warnings) to help debugging test failures.
 *
 * It also returns the value of @condition.
 */
#define igt_warn_on_f(condition, f...) ({ \
		typeof(condition) ret__ = (condition); \
		if (ret__) {\
			igt_warn("Warning on condition %s in function %s, file %s:%i\n", \
				 #condition, __func__, __FILE__, __LINE__); \
			igt_warn(f); \
		} \
		ret__; \
	})

void igt_set_timeout(unsigned int seconds,
		     const char *op);

/**
 * igt_gettime:
 * @ts: current monotonic clock reading
 *
 * Reports the current time in the monotonic clock.
 * Returns: 0 on success, -errno on failure.
 */
int igt_gettime(struct timespec *ts);

/**
 * igt_time_elapsed:
 * @then: Earlier timestamp
 * @now: Later timestamp
 *
 * Returns: Time between two timestamps in seconds, as a floating
 * point number.
 */
double igt_time_elapsed(struct timespec *then,
			struct timespec *now);

/**
 * igt_nsec_elapsed:
 * @start: measure from this point in time
 *
 * Reports the difference in the monotonic clock from the start time
 * in nanoseconds. On the first invocation, start should be zeroed and will
 * be set by the call.
 *
 * Typical use would be:
 *
 * igt_subtest("test") {
 * 	struct timespec start = {};
 * 	while (igt_nsec_elapsed(&start) < test_timeout_ns)
 *	 	do_test();
 * }
 *
 * A handy approximation is to use nsec >> 30 to convert to seconds,
 * nsec >> 20 to convert to milliseconds - the error is about 8%, acceptable
 * for test run times.
 */
uint64_t igt_nsec_elapsed(struct timespec *start);

/**
 * igt_seconds_elapsed:
 * @start: measure from this point in time
 *
 * A wrapper around igt_nsec_elapsed that reports the approximate (8% error)
 * number of seconds since the start point.
 */
static inline uint32_t igt_seconds_elapsed(struct timespec *start)
{
	return igt_nsec_elapsed(start) >> 30;
}

void igt_reset_timeout(void);

FILE *__igt_fopen_data(const char* igt_srcdir, const char* igt_datadir,
		       const char* filename);
/**
 * igt_fopen_data:
 * @filename: filename to open.
 *
 * Open a datafile for test, first try from installation directory,
 * then from build directory, and finally from current directory.
 */
#define igt_fopen_data(filename) \
	__igt_fopen_data(IGT_SRCDIR, IGT_DATADIR, filename)

int igt_system(const char *command);
int igt_system_quiet(const char *command);
#define igt_system_cmd(status, format...) \
	do { \
		char *buf = 0; \
		igt_assert(asprintf(&buf, format) != -1); \
	        status = igt_system(buf); \
		free(buf); \
	} while (0)

/**
 * igt_kmsg:
 * @format: printf-style format string with optional args
 *
 * Writes a message into the kernel log file (/dev/kmsg).
 */
__attribute__((format(printf, 1, 2)))
void igt_kmsg(const char *format, ...);
#define KMSG_EMER	"<0>[IGT] "
#define KMSG_ALERT	"<1>[IGT] "
#define KMSG_CRIT	"<2>[IGT] "
#define KMSG_ERR	"<3>[IGT] "
#define KMSG_WARNING	"<4>[IGT] "
#define KMSG_NOTICE	"<5>[IGT] "
#define KMSG_INFO	"<6>[IGT] "
#define KMSG_DEBUG	"<7>[IGT] "

#define READ_ONCE(x) (*(volatile typeof(x) *)(&(x)))

#define MSEC_PER_SEC (1000)
#define USEC_PER_SEC (1000*MSEC_PER_SEC)
#define NSEC_PER_SEC (1000*USEC_PER_SEC)

#endif /* IGT_CORE_H */
