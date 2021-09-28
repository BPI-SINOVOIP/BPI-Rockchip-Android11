#ifndef RUNNER_OUTPUT_STRINGS_H
#define RUNNER_OUTPUT_STRINGS_H

/*
 * Output when a subtest has begun. Is followed by the subtest name.
 *
 * Example:
 * Starting subtest: subtestname
 */
static const char STARTING_SUBTEST[] = "Starting subtest: ";

/*
 * Output when a subtest has ended. Is followed by the subtest name
 * and optionally its runtime.
 *
 * Examples:
 * Subtest subtestname: SKIP
 * Subtest subtestname: PASS (0.003s)
 */
static const char SUBTEST_RESULT[] = "Subtest ";

/*
 * Output in dmesg when a subtest has begin. Is followed by the subtest name.
 *
 * Example:
 * [IGT] test-binary-name: starting subtest subtestname
 */
static const char STARTING_SUBTEST_DMESG[] = ": starting subtest ";

/*
 * Output when a test process is executed.
 *
 * Example:
 * IGT-Version: 1.22-gde9af343 (x86_64) (Linux: 4.12.0-1-amd64 x86_64)
 */
static const char IGT_VERSIONSTRING[] = "IGT-Version: ";

/*
 * Output by the executor to mark the test's exit code.
 *
 * Example:
 * exit:77 (0.003s)
 */
static const char EXECUTOR_EXIT[] = "exit:";

/*
 * Output by the executor to mark the test as timeouted, with an exit
 * code.
 *
 * Example:
 * timeout:-15 (360.000s)
 */
static const char EXECUTOR_TIMEOUT[] = "timeout:";

#endif
