/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if __linux__
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "jni.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <backtrace/Backtrace.h>

#include "base/file_utils.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/utils.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "jit/debugger_interface.h"
#include "oat_file.h"
#include "runtime.h"

namespace art {

// For testing debuggerd. We do not have expected-death tests, so can't test this by default.
// Code for this is copied from SignalTest.
static constexpr bool kCauseSegfault = false;
char* go_away_compiler_cfi = nullptr;

static void CauseSegfault() {
#if defined(__arm__) || defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
  // On supported architectures we cause a real SEGV.
  *go_away_compiler_cfi = 'a';
#else
  // On other architectures we simulate SEGV.
  kill(getpid(), SIGSEGV);
#endif
}

extern "C" JNIEXPORT jint JNICALL Java_Main_startSecondaryProcess(JNIEnv*, jclass) {
  printf("Java_Main_startSecondaryProcess\n");
#if __linux__
  // Get our command line so that we can use it to start identical process.
  std::string cmdline;  // null-separated and null-terminated arguments.
  android::base::ReadFileToString("/proc/self/cmdline", &cmdline);
  cmdline = cmdline + "--secondary" + '\0';  // Let the child know it is a helper.

  // Split the string into individual arguments suitable for execv.
  std::vector<char*> argv;
  for (size_t i = 0; i < cmdline.size(); i += strlen(&cmdline[i]) + 1) {
    argv.push_back(&cmdline[i]);
  }
  argv.push_back(nullptr);  // Terminate the list.

  pid_t pid = fork();
  if (pid < 0) {
    LOG(FATAL) << "Fork failed";
  } else if (pid == 0) {
    execv(argv[0], argv.data());
    exit(1);
  }
  return pid;
#else
  return 0;
#endif
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_sigstop(JNIEnv*, jclass) {
  printf("Java_Main_sigstop\n");
#if __linux__
  MutexLock mu(Thread::Current(), *GetNativeDebugInfoLock());  // Avoid races with the JIT thread.
  raise(SIGSTOP);
#endif
  return true;  // Prevent the compiler from tail-call optimizing this method away.
}

// Helper to look for a sequence in the stack trace.
#if __linux__
static bool CheckStack(Backtrace* bt, const std::vector<std::string>& seq) {
  size_t cur_search_index = 0;  // The currently active index in seq.
  CHECK_GT(seq.size(), 0U);

  for (Backtrace::const_iterator it = bt->begin(); it != bt->end(); ++it) {
    if (BacktraceMap::IsValid(it->map)) {
      LOG(INFO) << "Got " << it->func_name << ", looking for " << seq[cur_search_index];
      if (it->func_name.find(seq[cur_search_index]) != std::string::npos) {
        cur_search_index++;
        if (cur_search_index == seq.size()) {
          return true;
        }
      }
    }
  }

  printf("Cannot find %s in backtrace:\n", seq[cur_search_index].c_str());
  for (Backtrace::const_iterator it = bt->begin(); it != bt->end(); ++it) {
    if (BacktraceMap::IsValid(it->map)) {
      printf("  %s\n", Backtrace::FormatFrameData(&*it).c_str());
    }
  }

  return false;
}

static void MoreErrorInfo(pid_t pid, bool sig_quit_on_fail) {
  PrintFileToLog(android::base::StringPrintf("/proc/%d/maps", pid), ::android::base::ERROR);

  if (sig_quit_on_fail) {
    int res = kill(pid, SIGQUIT);
    if (res != 0) {
      PLOG(ERROR) << "Failed to send signal";
    }
  }
}
#endif

extern "C" JNIEXPORT jboolean JNICALL Java_Main_unwindInProcess(JNIEnv*, jclass) {
  printf("Java_Main_unwindInProcess\n");
#if __linux__
  MutexLock mu(Thread::Current(), *GetNativeDebugInfoLock());  // Avoid races with the JIT thread.

  std::unique_ptr<Backtrace> bt(Backtrace::Create(BACKTRACE_CURRENT_PROCESS, GetTid()));
  if (!bt->Unwind(0, nullptr)) {
    printf("Cannot unwind in process.\n");
    return JNI_FALSE;
  } else if (bt->NumFrames() == 0) {
    printf("No frames for unwind in process.\n");
    return JNI_FALSE;
  }

  // We cannot really parse an exact stack, as the optimizing compiler may inline some functions.
  // This is also risky, as deduping might play a trick on us, so the test needs to make sure that
  // only unique functions are being expected.
  // "mini-debug-info" does not include parameters to save space.
  std::vector<std::string> seq = {
      "Java_Main_unwindInProcess",       // This function.
      "java.util.Arrays.binarySearch0",  // Framework method.
      "Base.$noinline$runTest",          // Method in other dex file.
      "Main.main"                        // The Java entry method.
  };

  bool result = CheckStack(bt.get(), seq);
  if (!kCauseSegfault) {
    return result ? JNI_TRUE : JNI_FALSE;
  } else {
    LOG(INFO) << "Result of check-stack: " << result;
  }
#endif

  if (kCauseSegfault) {
    CauseSegfault();
  }

  return JNI_FALSE;
}

#if __linux__
static constexpr int kSleepTimeMicroseconds = 50000;             // 0.05 seconds
static constexpr int kMaxTotalSleepTimeMicroseconds = 10000000;  // 10 seconds

// Wait for a sigstop. This code is copied from libbacktrace.
int wait_for_sigstop(pid_t tid, int* total_sleep_time_usec, bool* detach_failed ATTRIBUTE_UNUSED) {
  for (;;) {
    int status;
    pid_t n = TEMP_FAILURE_RETRY(waitpid(tid, &status, __WALL | WNOHANG));
    if (n == -1) {
      PLOG(WARNING) << "waitpid failed: tid " << tid;
      break;
    } else if (n == tid) {
      if (WIFSTOPPED(status)) {
        return WSTOPSIG(status);
      } else {
        PLOG(ERROR) << "unexpected waitpid response: n=" << n << ", status=" << std::hex << status;
        break;
      }
    }

    if (*total_sleep_time_usec > kMaxTotalSleepTimeMicroseconds) {
      PLOG(WARNING) << "timed out waiting for stop signal: tid=" << tid;
      break;
    }

    usleep(kSleepTimeMicroseconds);
    *total_sleep_time_usec += kSleepTimeMicroseconds;
  }

  return -1;
}
#endif

extern "C" JNIEXPORT jboolean JNICALL Java_Main_unwindOtherProcess(JNIEnv*, jclass, jint pid_int) {
  printf("Java_Main_unwindOtherProcess\n");
#if __linux__
  pid_t pid = static_cast<pid_t>(pid_int);

  // SEIZE is like ATTACH, but it does not stop the process (we let it stop itself).
  if (ptrace(PTRACE_SEIZE, pid, 0, 0)) {
    // Were not able to attach, bad.
    printf("Failed to attach to other process.\n");
    PLOG(ERROR) << "Failed to attach.";
    kill(pid, SIGKILL);
    return JNI_FALSE;
  }

  bool detach_failed = false;
  int total_sleep_time_usec = 0;
  int signal = wait_for_sigstop(pid, &total_sleep_time_usec, &detach_failed);
  if (signal != SIGSTOP) {
    printf("wait_for_sigstop failed.\n");
    return JNI_FALSE;
  }

  std::unique_ptr<Backtrace> bt(Backtrace::Create(pid, BACKTRACE_CURRENT_THREAD));
  bool result = true;
  if (!bt->Unwind(0, nullptr)) {
    printf("Cannot unwind other process.\n");
    result = false;
  } else if (bt->NumFrames() == 0) {
    printf("No frames for unwind of other process.\n");
    result = false;
  }

  if (result) {
    // See comment in unwindInProcess for non-exact stack matching.
    // "mini-debug-info" does not include parameters to save space.
    std::vector<std::string> seq = {
        "Java_Main_sigstop",                // The stop function in the other process.
        "java.util.Arrays.binarySearch0",   // Framework method.
        "Base.$noinline$runTest",           // Method in other dex file.
        "Main.main"                         // The Java entry method.
    };

    result = CheckStack(bt.get(), seq);
  }

  constexpr bool kSigQuitOnFail = true;
  if (!result) {
    printf("Failed to unwind secondary with pid %d\n", pid);
    MoreErrorInfo(pid, kSigQuitOnFail);
  }

  if (ptrace(PTRACE_DETACH, pid, 0, 0) != 0) {
    printf("Detach failed\n");
    PLOG(ERROR) << "Detach failed";
  }

  // If we failed to unwind and induced an ANR dump, give the child some time (20s).
  if (!result && kSigQuitOnFail) {
    sleep(20);
  }

  // Kill the other process once we are done with it.
  kill(pid, SIGKILL);

  return result ? JNI_TRUE : JNI_FALSE;
#else
  printf("Remote unwind supported only on linux\n");
  UNUSED(pid_int);
  return JNI_FALSE;
#endif
}

}  // namespace art
