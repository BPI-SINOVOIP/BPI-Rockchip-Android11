/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <paths.h>
#include <pthread.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// crashes if built with -fsanitize=address
void test_crash_malloc() {
  volatile char* heap = malloc(32);
  heap[32] = heap[32];
  printf("(HW)ASAN: Heap Test Failed\n");
}

// crashes if built with -fsanitize=address
void test_crash_stack() {
  volatile char stack[32];
  volatile char* p_stack = stack;
  p_stack[32] = p_stack[32];
  printf("(HW)ASAN: Stack Test Failed\n");
}

void test_crash_pthread_mutex_unlock() {
  volatile char* heap = malloc(32);
  pthread_mutex_unlock((void*)&heap[32]);
  printf("HWASAN: Libc Test Failed\n");
}

int data_asan_exists() {
  int fd = open("/data/asan", O_DIRECTORY | O_PATH | O_CLOEXEC, 0);
  if(fd < 0) {
    printf("ASAN: Missing /data/asan\n");
    return 1;
  }
  close(fd);
  return 0;
}

// crashes if built with -fsanitize=memory
void test_msan_crash_stack() {
  volatile int stack[10];
  stack[5] = 0;
  if (stack[0]) {
    stack[0] = 1;
  }
  printf("MSAN: Stack Test Failed\n");
}

// crashes if built with -fsanitize=integer
void test_integer_overflow() {
  size_t max = (size_t)-1;
  max++;
  printf("UBSAN: Integer Overflow Test Failed\n");
}

// returns 0 if kcov is enabled
int test_kcov() {
  const char* kcov_file = "/sys/kernel/debug/kcov";
  int fd = open(kcov_file, O_RDWR);
  if (fd == -1) {
    printf("KCOV: Could not open %s\n", kcov_file);
    return 1;
  }
  close(fd);
  return 0;
}

// returns 0 if kasan was compiled in
int test_kasan() {
  // rely on the exit status of grep to propagate
  if (system("gzip -d < /proc/config.gz | grep CONFIG_KASAN=y >/dev/null")) {
    printf("KASAN: CONFIG_KASAN not in /proc/config.gz\n");
    return 1;
  }
  return 0;
}

// Number of iterations required to reliably guarantee a GWP-ASan crash.
// GWP-ASan's sample rate is not truly nondeterministic, it initialises a
// thread-local counter at 2*SampleRate, and decrements on each malloc(). Once
// the counter reaches zero, we provide a sampled allocation. GWP-ASan's current
// default sample rate is 1/5000.
#define GWP_ASAN_ITERATIONS_TO_ENSURE_CRASH (0x10000)

// crashes with GWP-ASan
void test_crash_gwp_asan() {
  for (unsigned i = 0; i < GWP_ASAN_ITERATIONS_TO_ENSURE_CRASH; ++i ) {
    volatile char *x = malloc(1);
    free((void*) x);
    *x = 0;
  }
  printf("GWP-ASan: Use after Free Failed\n");
}

// executes a test that is expected to crash
// returns 0 if the test crashes
int test(void (*function)()) {
  fflush(stdout);

  pid_t child = fork();
  int status = 0;

  if (child == -1) {
    perror("fork");
    exit(1);
  }

  if (child == 0) {
    // Silence the ASAN report that is generated
    close(2);

    // Invoke the target function.  If it does not crash, terminate the process.
    function();
    exit(EXIT_SUCCESS);
  }

  // Wait for the child to either crash, or exit cleanly
  while (child == waitpid(child, &status, 0)) {
    if (!WIFEXITED(status))
      continue;
    if (WEXITSTATUS(status) == EXIT_SUCCESS)
      return 1;
    break;
  }
  return 0;
}

int have_option(const char* option, const char** argv, const int argc) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(option, argv[i]))
      return 1;
  return 0;
}

int sanitizer_status(int argc, const char** argv) {
  int test_everything = 0;
  int failures = 0;

  if (argc <= 1)
    test_everything = 1;

  if (test_everything || have_option("asan", argv, argc)) {
    int asan_failures = 0;

#if !defined(ANDROID_SANITIZE_ADDRESS)
    asan_failures += 1;
    printf("ASAN: Compiler flags failed!\n");
#endif

    asan_failures += test(test_crash_malloc);
    asan_failures += test(test_crash_stack);
    asan_failures += data_asan_exists();

    if (!asan_failures)
      printf("ASAN: OK\n");

    failures += asan_failures;
  }

  if (test_everything || have_option("hwasan", argv, argc)) {
    int hwasan_failures = 0;

#if !defined(ANDROID_SANITIZE_HWADDRESS)
    hwasan_failures += 1;
    printf("HWASAN: Compiler flags failed!\n");
#endif

    hwasan_failures += test(test_crash_malloc);
    hwasan_failures += test(test_crash_stack);
    hwasan_failures += test(test_crash_pthread_mutex_unlock);

    if (!hwasan_failures)
      printf("HWASAN: OK\n");

    failures += hwasan_failures;
  }

  if(test_everything || have_option("cov", argv, argc)) {
    int cov_failures = 0;

#ifndef ANDROID_SANITIZE_COVERAGE
    printf("COV: Compiler flags failed!\n");
    cov_failures += 1;
#endif

    if (!cov_failures)
      printf("COV: OK\n");

    failures += cov_failures;
  }

  if (test_everything || have_option("msan", argv, argc)) {
    int msan_failures = 0;

    msan_failures += test(test_msan_crash_stack);

    if (!msan_failures)
      printf("MSAN: OK\n");

    failures += msan_failures;
  }

  if (test_everything || have_option("kasan", argv, argc)) {
    int kasan_failures = 0;

    kasan_failures += test_kasan();

    if(!kasan_failures)
      printf("KASAN: OK\n");

    failures += kasan_failures;
  }

  if (test_everything || have_option("kcov", argv, argc)) {
    int kcov_failures = 0;

    kcov_failures += test_kcov();

    if (!kcov_failures)
      printf("KCOV: OK\n");

    failures += kcov_failures;
  }

  if (test_everything || have_option("ubsan", argv, argc)) {
    int ubsan_failures = 0;

    ubsan_failures += test(test_integer_overflow);

    if (!ubsan_failures)
      printf("UBSAN: OK\n");

    failures += ubsan_failures;
  }

  if (test_everything || have_option("gwp_asan", argv, argc)) {
    int gwp_asan_failures = 0;

    gwp_asan_failures += test(test_crash_gwp_asan);

    if (!gwp_asan_failures)
      printf("GWP-ASan: OK\n");

    failures += gwp_asan_failures;
  }

  return failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
