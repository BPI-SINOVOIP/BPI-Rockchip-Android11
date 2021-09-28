/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "thread_list.h"

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>
#include <vector>

#include "android-base/stringprintf.h"
#include "backtrace/BacktraceMap.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_utf_chars.h"

#include "base/aborting.h"
#include "base/histogram-inl.h"
#include "base/mutex-inl.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/timing_logger.h"
#include "debugger.h"
#include "gc/collector/concurrent_copying.h"
#include "gc/gc_pause_listener.h"
#include "gc/heap.h"
#include "gc/reference_processor.h"
#include "gc_root.h"
#include "jni/jni_internal.h"
#include "lock_word.h"
#include "monitor.h"
#include "native_stack_dump.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "trace.h"
#include "well_known_classes.h"

#if ART_USE_FUTEXES
#include "linux/futex.h"
#include "sys/syscall.h"
#ifndef SYS_futex
#define SYS_futex __NR_futex
#endif
#endif  // ART_USE_FUTEXES

namespace art {

using android::base::StringPrintf;

static constexpr uint64_t kLongThreadSuspendThreshold = MsToNs(5);
// Use 0 since we want to yield to prevent blocking for an unpredictable amount of time.
static constexpr useconds_t kThreadSuspendInitialSleepUs = 0;
static constexpr useconds_t kThreadSuspendMaxYieldUs = 3000;
static constexpr useconds_t kThreadSuspendMaxSleepUs = 5000;

// Whether we should try to dump the native stack of unattached threads. See commit ed8b723 for
// some history.
static constexpr bool kDumpUnattachedThreadNativeStackForSigQuit = true;

ThreadList::ThreadList(uint64_t thread_suspend_timeout_ns)
    : suspend_all_count_(0),
      unregistering_count_(0),
      suspend_all_historam_("suspend all histogram", 16, 64),
      long_suspend_(false),
      shut_down_(false),
      thread_suspend_timeout_ns_(thread_suspend_timeout_ns),
      empty_checkpoint_barrier_(new Barrier(0)) {
  CHECK(Monitor::IsValidLockWord(LockWord::FromThinLockId(kMaxThreadId, 1, 0U)));
}

ThreadList::~ThreadList() {
  CHECK(shut_down_);
}

void ThreadList::ShutDown() {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  // Detach the current thread if necessary. If we failed to start, there might not be any threads.
  // We need to detach the current thread here in case there's another thread waiting to join with
  // us.
  bool contains = false;
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    contains = Contains(self);
  }
  if (contains) {
    Runtime::Current()->DetachCurrentThread();
  }
  WaitForOtherNonDaemonThreadsToExit();
  // Disable GC and wait for GC to complete in case there are still daemon threads doing
  // allocations.
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  heap->DisableGCForShutdown();
  // In case a GC is in progress, wait for it to finish.
  heap->WaitForGcToComplete(gc::kGcCauseBackground, Thread::Current());
  // TODO: there's an unaddressed race here where a thread may attach during shutdown, see
  //       Thread::Init.
  SuspendAllDaemonThreadsForShutdown();

  shut_down_ = true;
}

bool ThreadList::Contains(Thread* thread) {
  return find(list_.begin(), list_.end(), thread) != list_.end();
}

bool ThreadList::Contains(pid_t tid) {
  for (const auto& thread : list_) {
    if (thread->GetTid() == tid) {
      return true;
    }
  }
  return false;
}

pid_t ThreadList::GetLockOwner() {
  return Locks::thread_list_lock_->GetExclusiveOwnerTid();
}

void ThreadList::DumpNativeStacks(std::ostream& os) {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  std::unique_ptr<BacktraceMap> map(BacktraceMap::Create(getpid()));
  for (const auto& thread : list_) {
    os << "DUMPING THREAD " << thread->GetTid() << "\n";
    DumpNativeStack(os, thread->GetTid(), map.get(), "\t");
    os << "\n";
  }
}

void ThreadList::DumpForSigQuit(std::ostream& os) {
  {
    ScopedObjectAccess soa(Thread::Current());
    // Only print if we have samples.
    if (suspend_all_historam_.SampleSize() > 0) {
      Histogram<uint64_t>::CumulativeData data;
      suspend_all_historam_.CreateHistogram(&data);
      suspend_all_historam_.PrintConfidenceIntervals(os, 0.99, data);  // Dump time to suspend.
    }
  }
  bool dump_native_stack = Runtime::Current()->GetDumpNativeStackOnSigQuit();
  Dump(os, dump_native_stack);
  DumpUnattachedThreads(os, dump_native_stack && kDumpUnattachedThreadNativeStackForSigQuit);
}

static void DumpUnattachedThread(std::ostream& os, pid_t tid, bool dump_native_stack)
    NO_THREAD_SAFETY_ANALYSIS {
  // TODO: No thread safety analysis as DumpState with a null thread won't access fields, should
  // refactor DumpState to avoid skipping analysis.
  Thread::DumpState(os, nullptr, tid);
  if (dump_native_stack) {
    DumpNativeStack(os, tid, nullptr, "  native: ");
  }
  os << std::endl;
}

void ThreadList::DumpUnattachedThreads(std::ostream& os, bool dump_native_stack) {
  DIR* d = opendir("/proc/self/task");
  if (!d) {
    return;
  }

  Thread* self = Thread::Current();
  dirent* e;
  while ((e = readdir(d)) != nullptr) {
    char* end;
    pid_t tid = strtol(e->d_name, &end, 10);
    if (!*end) {
      bool contains;
      {
        MutexLock mu(self, *Locks::thread_list_lock_);
        contains = Contains(tid);
      }
      if (!contains) {
        DumpUnattachedThread(os, tid, dump_native_stack);
      }
    }
  }
  closedir(d);
}

// Dump checkpoint timeout in milliseconds. Larger amount on the target, since the device could be
// overloaded with ANR dumps.
static constexpr uint32_t kDumpWaitTimeout = kIsTargetBuild ? 100000 : 20000;

// A closure used by Thread::Dump.
class DumpCheckpoint final : public Closure {
 public:
  DumpCheckpoint(std::ostream* os, bool dump_native_stack)
      : os_(os),
        // Avoid verifying count in case a thread doesn't end up passing through the barrier.
        // This avoids a SIGABRT that would otherwise happen in the destructor.
        barrier_(0, /*verify_count_on_shutdown=*/false),
        backtrace_map_(dump_native_stack ? BacktraceMap::Create(getpid()) : nullptr),
        dump_native_stack_(dump_native_stack) {
    if (backtrace_map_ != nullptr) {
      backtrace_map_->SetSuffixesToIgnore(std::vector<std::string> { "oat", "odex" });
    }
  }

  void Run(Thread* thread) override {
    // Note thread and self may not be equal if thread was already suspended at the point of the
    // request.
    Thread* self = Thread::Current();
    CHECK(self != nullptr);
    std::ostringstream local_os;
    {
      ScopedObjectAccess soa(self);
      thread->Dump(local_os, dump_native_stack_, backtrace_map_.get());
    }
    {
      // Use the logging lock to ensure serialization when writing to the common ostream.
      MutexLock mu(self, *Locks::logging_lock_);
      *os_ << local_os.str() << std::endl;
    }
    barrier_.Pass(self);
  }

  void WaitForThreadsToRunThroughCheckpoint(size_t threads_running_checkpoint) {
    Thread* self = Thread::Current();
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    bool timed_out = barrier_.Increment(self, threads_running_checkpoint, kDumpWaitTimeout);
    if (timed_out) {
      // Avoid a recursive abort.
      LOG((kIsDebugBuild && (gAborting == 0)) ? ::android::base::FATAL : ::android::base::ERROR)
          << "Unexpected time out during dump checkpoint.";
    }
  }

 private:
  // The common stream that will accumulate all the dumps.
  std::ostream* const os_;
  // The barrier to be passed through and for the requestor to wait upon.
  Barrier barrier_;
  // A backtrace map, so that all threads use a shared info and don't reacquire/parse separately.
  std::unique_ptr<BacktraceMap> backtrace_map_;
  // Whether we should dump the native stack.
  const bool dump_native_stack_;
};

void ThreadList::Dump(std::ostream& os, bool dump_native_stack) {
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    os << "DALVIK THREADS (" << list_.size() << "):\n";
  }
  if (self != nullptr) {
    DumpCheckpoint checkpoint(&os, dump_native_stack);
    size_t threads_running_checkpoint;
    {
      // Use SOA to prevent deadlocks if multiple threads are calling Dump() at the same time.
      ScopedObjectAccess soa(self);
      threads_running_checkpoint = RunCheckpoint(&checkpoint);
    }
    if (threads_running_checkpoint != 0) {
      checkpoint.WaitForThreadsToRunThroughCheckpoint(threads_running_checkpoint);
    }
  } else {
    DumpUnattachedThreads(os, dump_native_stack);
  }
}

void ThreadList::AssertThreadsAreSuspended(Thread* self, Thread* ignore1, Thread* ignore2) {
  MutexLock mu(self, *Locks::thread_list_lock_);
  MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
  for (const auto& thread : list_) {
    if (thread != ignore1 && thread != ignore2) {
      CHECK(thread->IsSuspended())
            << "\nUnsuspended thread: <<" << *thread << "\n"
            << "self: <<" << *Thread::Current();
    }
  }
}

#if HAVE_TIMED_RWLOCK
// Attempt to rectify locks so that we dump thread list with required locks before exiting.
NO_RETURN static void UnsafeLogFatalForThreadSuspendAllTimeout() {
  // Increment gAborting before doing the thread list dump since we don't want any failures from
  // AssertThreadSuspensionIsAllowable in cases where thread suspension is not allowed.
  // See b/69044468.
  ++gAborting;
  Runtime* runtime = Runtime::Current();
  std::ostringstream ss;
  ss << "Thread suspend timeout\n";
  Locks::mutator_lock_->Dump(ss);
  ss << "\n";
  runtime->GetThreadList()->Dump(ss);
  --gAborting;
  LOG(FATAL) << ss.str();
  exit(0);
}
#endif

// Unlike suspending all threads where we can wait to acquire the mutator_lock_, suspending an
// individual thread requires polling. delay_us is the requested sleep wait. If delay_us is 0 then
// we use sched_yield instead of calling usleep.
// Although there is the possibility, here and elsewhere, that usleep could return -1 and
// errno = EINTR, there should be no problem if interrupted, so we do not check.
static void ThreadSuspendSleep(useconds_t delay_us) {
  if (delay_us == 0) {
    sched_yield();
  } else {
    usleep(delay_us);
  }
}

size_t ThreadList::RunCheckpoint(Closure* checkpoint_function, Closure* callback) {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotExclusiveHeld(self);
  Locks::thread_list_lock_->AssertNotHeld(self);
  Locks::thread_suspend_count_lock_->AssertNotHeld(self);

  std::vector<Thread*> suspended_count_modified_threads;
  size_t count = 0;
  {
    // Call a checkpoint function for each thread, threads which are suspend get their checkpoint
    // manually called.
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    count = list_.size();
    for (const auto& thread : list_) {
      if (thread != self) {
        bool requested_suspend = false;
        while (true) {
          if (thread->RequestCheckpoint(checkpoint_function)) {
            // This thread will run its checkpoint some time in the near future.
            if (requested_suspend) {
              // The suspend request is now unnecessary.
              bool updated =
                  thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
              DCHECK(updated);
              requested_suspend = false;
            }
            break;
          } else {
            // The thread is probably suspended, try to make sure that it stays suspended.
            if (thread->GetState() == kRunnable) {
              // Spurious fail, try again.
              continue;
            }
            if (!requested_suspend) {
              bool updated =
                  thread->ModifySuspendCount(self, +1, nullptr, SuspendReason::kInternal);
              DCHECK(updated);
              requested_suspend = true;
              if (thread->IsSuspended()) {
                break;
              }
              // The thread raced us to become Runnable. Try to RequestCheckpoint() again.
            } else {
              // The thread previously raced our suspend request to become Runnable but
              // since it is suspended again, it must honor that suspend request now.
              DCHECK(thread->IsSuspended());
              break;
            }
          }
        }
        if (requested_suspend) {
          suspended_count_modified_threads.push_back(thread);
        }
      }
    }
    // Run the callback to be called inside this critical section.
    if (callback != nullptr) {
      callback->Run(self);
    }
  }

  // Run the checkpoint on ourself while we wait for threads to suspend.
  checkpoint_function->Run(self);

  // Run the checkpoint on the suspended threads.
  for (const auto& thread : suspended_count_modified_threads) {
    // We know for sure that the thread is suspended at this point.
    DCHECK(thread->IsSuspended());
    checkpoint_function->Run(thread);
    {
      MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
      bool updated = thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
      DCHECK(updated);
    }
  }

  {
    // Imitate ResumeAll, threads may be waiting on Thread::resume_cond_ since we raised their
    // suspend count. Now the suspend_count_ is lowered so we must do the broadcast.
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    Thread::resume_cond_->Broadcast(self);
  }

  return count;
}

void ThreadList::RunEmptyCheckpoint() {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotExclusiveHeld(self);
  Locks::thread_list_lock_->AssertNotHeld(self);
  Locks::thread_suspend_count_lock_->AssertNotHeld(self);
  std::vector<uint32_t> runnable_thread_ids;
  size_t count = 0;
  Barrier* barrier = empty_checkpoint_barrier_.get();
  barrier->Init(self, 0);
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    for (Thread* thread : list_) {
      if (thread != self) {
        while (true) {
          if (thread->RequestEmptyCheckpoint()) {
            // This thread will run an empty checkpoint (decrement the empty checkpoint barrier)
            // some time in the near future.
            ++count;
            if (kIsDebugBuild) {
              runnable_thread_ids.push_back(thread->GetThreadId());
            }
            break;
          }
          if (thread->GetState() != kRunnable) {
            // It's seen suspended, we are done because it must not be in the middle of a mutator
            // heap access.
            break;
          }
        }
      }
    }
  }

  // Wake up the threads blocking for weak ref access so that they will respond to the empty
  // checkpoint request. Otherwise we will hang as they are blocking in the kRunnable state.
  Runtime::Current()->GetHeap()->GetReferenceProcessor()->BroadcastForSlowPath(self);
  Runtime::Current()->BroadcastForNewSystemWeaks(/*broadcast_for_checkpoint=*/true);
  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    uint64_t total_wait_time = 0;
    bool first_iter = true;
    while (true) {
      // Wake up the runnable threads blocked on the mutexes that another thread, which is blocked
      // on a weak ref access, holds (indirectly blocking for weak ref access through another thread
      // and a mutex.) This needs to be done periodically because the thread may be preempted
      // between the CheckEmptyCheckpointFromMutex call and the subsequent futex wait in
      // Mutex::ExclusiveLock, etc. when the wakeup via WakeupToRespondToEmptyCheckpoint
      // arrives. This could cause a *very rare* deadlock, if not repeated. Most of the cases are
      // handled in the first iteration.
      for (BaseMutex* mutex : Locks::expected_mutexes_on_weak_ref_access_) {
        mutex->WakeupToRespondToEmptyCheckpoint();
      }
      static constexpr uint64_t kEmptyCheckpointPeriodicTimeoutMs = 100;  // 100ms
      static constexpr uint64_t kEmptyCheckpointTotalTimeoutMs = 600 * 1000;  // 10 minutes.
      size_t barrier_count = first_iter ? count : 0;
      first_iter = false;  // Don't add to the barrier count from the second iteration on.
      bool timed_out = barrier->Increment(self, barrier_count, kEmptyCheckpointPeriodicTimeoutMs);
      if (!timed_out) {
        break;  // Success
      }
      // This is a very rare case.
      total_wait_time += kEmptyCheckpointPeriodicTimeoutMs;
      if (kIsDebugBuild && total_wait_time > kEmptyCheckpointTotalTimeoutMs) {
        std::ostringstream ss;
        ss << "Empty checkpoint timeout\n";
        ss << "Barrier count " << barrier->GetCount(self) << "\n";
        ss << "Runnable thread IDs";
        for (uint32_t tid : runnable_thread_ids) {
          ss << " " << tid;
        }
        ss << "\n";
        Locks::mutator_lock_->Dump(ss);
        ss << "\n";
        LOG(FATAL_WITHOUT_ABORT) << ss.str();
        // Some threads in 'runnable_thread_ids' are probably stuck. Try to dump their stacks.
        // Avoid using ThreadList::Dump() initially because it is likely to get stuck as well.
        {
          ScopedObjectAccess soa(self);
          MutexLock mu1(self, *Locks::thread_list_lock_);
          for (Thread* thread : GetList()) {
            uint32_t tid = thread->GetThreadId();
            bool is_in_runnable_thread_ids =
                std::find(runnable_thread_ids.begin(), runnable_thread_ids.end(), tid) !=
                runnable_thread_ids.end();
            if (is_in_runnable_thread_ids &&
                thread->ReadFlag(kEmptyCheckpointRequest)) {
              // Found a runnable thread that hasn't responded to the empty checkpoint request.
              // Assume it's stuck and safe to dump its stack.
              thread->Dump(LOG_STREAM(FATAL_WITHOUT_ABORT),
                           /*dump_native_stack=*/ true,
                           /*backtrace_map=*/ nullptr,
                           /*force_dump_stack=*/ true);
            }
          }
        }
        LOG(FATAL_WITHOUT_ABORT)
            << "Dumped runnable threads that haven't responded to empty checkpoint.";
        // Now use ThreadList::Dump() to dump more threads, noting it may get stuck.
        Dump(LOG_STREAM(FATAL_WITHOUT_ABORT));
        LOG(FATAL) << "Dumped all threads.";
      }
    }
  }
}

// A checkpoint/suspend-all hybrid to switch thread roots from
// from-space to to-space refs. Used to synchronize threads at a point
// to mark the initiation of marking while maintaining the to-space
// invariant.
size_t ThreadList::FlipThreadRoots(Closure* thread_flip_visitor,
                                   Closure* flip_callback,
                                   gc::collector::GarbageCollector* collector,
                                   gc::GcPauseListener* pause_listener) {
  TimingLogger::ScopedTiming split("ThreadListFlip", collector->GetTimings());
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  Locks::thread_list_lock_->AssertNotHeld(self);
  Locks::thread_suspend_count_lock_->AssertNotHeld(self);
  CHECK_NE(self->GetState(), kRunnable);

  collector->GetHeap()->ThreadFlipBegin(self);  // Sync with JNI critical calls.

  // ThreadFlipBegin happens before we suspend all the threads, so it does not count towards the
  // pause.
  const uint64_t suspend_start_time = NanoTime();
  SuspendAllInternal(self, self, nullptr);
  if (pause_listener != nullptr) {
    pause_listener->StartPause();
  }

  // Run the flip callback for the collector.
  Locks::mutator_lock_->ExclusiveLock(self);
  suspend_all_historam_.AdjustAndAddValue(NanoTime() - suspend_start_time);
  flip_callback->Run(self);
  Locks::mutator_lock_->ExclusiveUnlock(self);
  collector->RegisterPause(NanoTime() - suspend_start_time);
  if (pause_listener != nullptr) {
    pause_listener->EndPause();
  }

  // Resume runnable threads.
  size_t runnable_thread_count = 0;
  std::vector<Thread*> other_threads;
  {
    TimingLogger::ScopedTiming split2("ResumeRunnableThreads", collector->GetTimings());
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    --suspend_all_count_;
    for (const auto& thread : list_) {
      // Set the flip function for all threads because Thread::DumpState/DumpJavaStack() (invoked by
      // a checkpoint) may cause the flip function to be run for a runnable/suspended thread before
      // a runnable thread runs it for itself or we run it for a suspended thread below.
      thread->SetFlipFunction(thread_flip_visitor);
      if (thread == self) {
        continue;
      }
      // Resume early the threads that were runnable but are suspended just for this thread flip or
      // about to transition from non-runnable (eg. kNative at the SOA entry in a JNI function) to
      // runnable (both cases waiting inside Thread::TransitionFromSuspendedToRunnable), or waiting
      // for the thread flip to end at the JNI critical section entry (kWaitingForGcThreadFlip),
      ThreadState state = thread->GetState();
      if ((state == kWaitingForGcThreadFlip || thread->IsTransitioningToRunnable()) &&
          thread->GetSuspendCount() == 1) {
        // The thread will resume right after the broadcast.
        bool updated = thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
        DCHECK(updated);
        ++runnable_thread_count;
      } else {
        other_threads.push_back(thread);
      }
    }
    Thread::resume_cond_->Broadcast(self);
  }

  collector->GetHeap()->ThreadFlipEnd(self);

  // Run the closure on the other threads and let them resume.
  {
    TimingLogger::ScopedTiming split3("FlipOtherThreads", collector->GetTimings());
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    for (const auto& thread : other_threads) {
      Closure* flip_func = thread->GetFlipFunction();
      if (flip_func != nullptr) {
        flip_func->Run(thread);
      }
    }
    // Run it for self.
    Closure* flip_func = self->GetFlipFunction();
    if (flip_func != nullptr) {
      flip_func->Run(self);
    }
  }

  // Resume other threads.
  {
    TimingLogger::ScopedTiming split4("ResumeOtherThreads", collector->GetTimings());
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    for (const auto& thread : other_threads) {
      bool updated = thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
      DCHECK(updated);
    }
    Thread::resume_cond_->Broadcast(self);
  }

  return runnable_thread_count + other_threads.size() + 1;  // +1 for self.
}

void ThreadList::SuspendAll(const char* cause, bool long_suspend) {
  Thread* self = Thread::Current();

  if (self != nullptr) {
    VLOG(threads) << *self << " SuspendAll for " << cause << " starting...";
  } else {
    VLOG(threads) << "Thread[null] SuspendAll for " << cause << " starting...";
  }
  {
    ScopedTrace trace("Suspending mutator threads");
    const uint64_t start_time = NanoTime();

    SuspendAllInternal(self, self);
    // All threads are known to have suspended (but a thread may still own the mutator lock)
    // Make sure this thread grabs exclusive access to the mutator lock and its protected data.
#if HAVE_TIMED_RWLOCK
    while (true) {
      if (Locks::mutator_lock_->ExclusiveLockWithTimeout(self,
                                                         NsToMs(thread_suspend_timeout_ns_),
                                                         0)) {
        break;
      } else if (!long_suspend_) {
        // Reading long_suspend without the mutator lock is slightly racy, in some rare cases, this
        // could result in a thread suspend timeout.
        // Timeout if we wait more than thread_suspend_timeout_ns_ nanoseconds.
        UnsafeLogFatalForThreadSuspendAllTimeout();
      }
    }
#else
    Locks::mutator_lock_->ExclusiveLock(self);
#endif

    long_suspend_ = long_suspend;

    const uint64_t end_time = NanoTime();
    const uint64_t suspend_time = end_time - start_time;
    suspend_all_historam_.AdjustAndAddValue(suspend_time);
    if (suspend_time > kLongThreadSuspendThreshold) {
      LOG(WARNING) << "Suspending all threads took: " << PrettyDuration(suspend_time);
    }

    if (kDebugLocking) {
      // Debug check that all threads are suspended.
      AssertThreadsAreSuspended(self, self);
    }
  }
  ATraceBegin((std::string("Mutator threads suspended for ") + cause).c_str());

  if (self != nullptr) {
    VLOG(threads) << *self << " SuspendAll complete";
  } else {
    VLOG(threads) << "Thread[null] SuspendAll complete";
  }
}

// Ensures all threads running Java suspend and that those not running Java don't start.
void ThreadList::SuspendAllInternal(Thread* self,
                                    Thread* ignore1,
                                    Thread* ignore2,
                                    SuspendReason reason) {
  Locks::mutator_lock_->AssertNotExclusiveHeld(self);
  Locks::thread_list_lock_->AssertNotHeld(self);
  Locks::thread_suspend_count_lock_->AssertNotHeld(self);
  if (kDebugLocking && self != nullptr) {
    CHECK_NE(self->GetState(), kRunnable);
  }

  // First request that all threads suspend, then wait for them to suspend before
  // returning. This suspension scheme also relies on other behaviour:
  // 1. Threads cannot be deleted while they are suspended or have a suspend-
  //    request flag set - (see Unregister() below).
  // 2. When threads are created, they are created in a suspended state (actually
  //    kNative) and will never begin executing Java code without first checking
  //    the suspend-request flag.

  // The atomic counter for number of threads that need to pass the barrier.
  AtomicInteger pending_threads;
  uint32_t num_ignored = 0;
  if (ignore1 != nullptr) {
    ++num_ignored;
  }
  if (ignore2 != nullptr && ignore1 != ignore2) {
    ++num_ignored;
  }
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    // Update global suspend all state for attaching threads.
    ++suspend_all_count_;
    pending_threads.store(list_.size() - num_ignored, std::memory_order_relaxed);
    // Increment everybody's suspend count (except those that should be ignored).
    for (const auto& thread : list_) {
      if (thread == ignore1 || thread == ignore2) {
        continue;
      }
      VLOG(threads) << "requesting thread suspend: " << *thread;
      bool updated = thread->ModifySuspendCount(self, +1, &pending_threads, reason);
      DCHECK(updated);

      // Must install the pending_threads counter first, then check thread->IsSuspend() and clear
      // the counter. Otherwise there's a race with Thread::TransitionFromRunnableToSuspended()
      // that can lead a thread to miss a call to PassActiveSuspendBarriers().
      if (thread->IsSuspended()) {
        // Only clear the counter for the current thread.
        thread->ClearSuspendBarrier(&pending_threads);
        pending_threads.fetch_sub(1, std::memory_order_seq_cst);
      }
    }
  }

  // Wait for the barrier to be passed by all runnable threads. This wait
  // is done with a timeout so that we can detect problems.
#if ART_USE_FUTEXES
  timespec wait_timeout;
  InitTimeSpec(false, CLOCK_MONOTONIC, NsToMs(thread_suspend_timeout_ns_), 0, &wait_timeout);
#endif
  const uint64_t start_time = NanoTime();
  while (true) {
    int32_t cur_val = pending_threads.load(std::memory_order_relaxed);
    if (LIKELY(cur_val > 0)) {
#if ART_USE_FUTEXES
      if (futex(pending_threads.Address(), FUTEX_WAIT_PRIVATE, cur_val, &wait_timeout, nullptr, 0)
          != 0) {
        if ((errno == EAGAIN) || (errno == EINTR)) {
          // EAGAIN and EINTR both indicate a spurious failure, try again from the beginning.
          continue;
        }
        if (errno == ETIMEDOUT) {
          const uint64_t wait_time = NanoTime() - start_time;
          MutexLock mu(self, *Locks::thread_list_lock_);
          MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
          std::ostringstream oss;
          for (const auto& thread : list_) {
            if (thread == ignore1 || thread == ignore2) {
              continue;
            }
            if (!thread->IsSuspended()) {
              oss << std::endl << "Thread not suspended: " << *thread;
            }
          }
          LOG(kIsDebugBuild ? ::android::base::FATAL : ::android::base::ERROR)
              << "Timed out waiting for threads to suspend, waited for "
              << PrettyDuration(wait_time)
              << oss.str();
        } else {
          PLOG(FATAL) << "futex wait failed for SuspendAllInternal()";
        }
      }  // else re-check pending_threads in the next iteration (this may be a spurious wake-up).
#else
      // Spin wait. This is likely to be slow, but on most architecture ART_USE_FUTEXES is set.
      UNUSED(start_time);
#endif
    } else {
      CHECK_EQ(cur_val, 0);
      break;
    }
  }
}

void ThreadList::ResumeAll() {
  Thread* self = Thread::Current();

  if (self != nullptr) {
    VLOG(threads) << *self << " ResumeAll starting";
  } else {
    VLOG(threads) << "Thread[null] ResumeAll starting";
  }

  ATraceEnd();

  ScopedTrace trace("Resuming mutator threads");

  if (kDebugLocking) {
    // Debug check that all threads are suspended.
    AssertThreadsAreSuspended(self, self);
  }

  long_suspend_ = false;

  Locks::mutator_lock_->ExclusiveUnlock(self);
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    // Update global suspend all state for attaching threads.
    --suspend_all_count_;
    // Decrement the suspend counts for all threads.
    for (const auto& thread : list_) {
      if (thread == self) {
        continue;
      }
      bool updated = thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
      DCHECK(updated);
    }

    // Broadcast a notification to all suspended threads, some or all of
    // which may choose to wake up.  No need to wait for them.
    if (self != nullptr) {
      VLOG(threads) << *self << " ResumeAll waking others";
    } else {
      VLOG(threads) << "Thread[null] ResumeAll waking others";
    }
    Thread::resume_cond_->Broadcast(self);
  }

  if (self != nullptr) {
    VLOG(threads) << *self << " ResumeAll complete";
  } else {
    VLOG(threads) << "Thread[null] ResumeAll complete";
  }
}

bool ThreadList::Resume(Thread* thread, SuspendReason reason) {
  // This assumes there was an ATraceBegin when we suspended the thread.
  ATraceEnd();

  Thread* self = Thread::Current();
  DCHECK_NE(thread, self);
  VLOG(threads) << "Resume(" << reinterpret_cast<void*>(thread) << ") starting..." << reason;

  {
    // To check Contains.
    MutexLock mu(self, *Locks::thread_list_lock_);
    // To check IsSuspended.
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    if (UNLIKELY(!thread->IsSuspended())) {
      LOG(ERROR) << "Resume(" << reinterpret_cast<void*>(thread)
          << ") thread not suspended";
      return false;
    }
    if (!Contains(thread)) {
      // We only expect threads within the thread-list to have been suspended otherwise we can't
      // stop such threads from delete-ing themselves.
      LOG(ERROR) << "Resume(" << reinterpret_cast<void*>(thread)
          << ") thread not within thread list";
      return false;
    }
    if (UNLIKELY(!thread->ModifySuspendCount(self, -1, nullptr, reason))) {
      LOG(ERROR) << "Resume(" << reinterpret_cast<void*>(thread)
                 << ") could not modify suspend count.";
      return false;
    }
  }

  {
    VLOG(threads) << "Resume(" << reinterpret_cast<void*>(thread) << ") waking others";
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    Thread::resume_cond_->Broadcast(self);
  }

  VLOG(threads) << "Resume(" << reinterpret_cast<void*>(thread) << ") complete";
  return true;
}

static void ThreadSuspendByPeerWarning(Thread* self,
                                       LogSeverity severity,
                                       const char* message,
                                       jobject peer) {
  JNIEnvExt* env = self->GetJniEnv();
  ScopedLocalRef<jstring>
      scoped_name_string(env, static_cast<jstring>(env->GetObjectField(
          peer, WellKnownClasses::java_lang_Thread_name)));
  ScopedUtfChars scoped_name_chars(env, scoped_name_string.get());
  if (scoped_name_chars.c_str() == nullptr) {
      LOG(severity) << message << ": " << peer;
      env->ExceptionClear();
  } else {
      LOG(severity) << message << ": " << peer << ":" << scoped_name_chars.c_str();
  }
}

Thread* ThreadList::SuspendThreadByPeer(jobject peer,
                                        bool request_suspension,
                                        SuspendReason reason,
                                        bool* timed_out) {
  const uint64_t start_time = NanoTime();
  useconds_t sleep_us = kThreadSuspendInitialSleepUs;
  *timed_out = false;
  Thread* const self = Thread::Current();
  Thread* suspended_thread = nullptr;
  VLOG(threads) << "SuspendThreadByPeer starting";
  while (true) {
    Thread* thread;
    {
      // Note: this will transition to runnable and potentially suspend. We ensure only one thread
      // is requesting another suspend, to avoid deadlock, by requiring this function be called
      // holding Locks::thread_list_suspend_thread_lock_. Its important this thread suspend rather
      // than request thread suspension, to avoid potential cycles in threads requesting each other
      // suspend.
      ScopedObjectAccess soa(self);
      MutexLock thread_list_mu(self, *Locks::thread_list_lock_);
      thread = Thread::FromManagedThread(soa, peer);
      if (thread == nullptr) {
        if (suspended_thread != nullptr) {
          MutexLock suspend_count_mu(self, *Locks::thread_suspend_count_lock_);
          // If we incremented the suspend count but the thread reset its peer, we need to
          // re-decrement it since it is shutting down and may deadlock the runtime in
          // ThreadList::WaitForOtherNonDaemonThreadsToExit.
          bool updated = suspended_thread->ModifySuspendCount(soa.Self(),
                                                              -1,
                                                              nullptr,
                                                              reason);
          DCHECK(updated);
        }
        ThreadSuspendByPeerWarning(self,
                                   ::android::base::WARNING,
                                    "No such thread for suspend",
                                    peer);
        return nullptr;
      }
      if (!Contains(thread)) {
        CHECK(suspended_thread == nullptr);
        VLOG(threads) << "SuspendThreadByPeer failed for unattached thread: "
            << reinterpret_cast<void*>(thread);
        return nullptr;
      }
      VLOG(threads) << "SuspendThreadByPeer found thread: " << *thread;
      {
        MutexLock suspend_count_mu(self, *Locks::thread_suspend_count_lock_);
        if (request_suspension) {
          if (self->GetSuspendCount() > 0) {
            // We hold the suspend count lock but another thread is trying to suspend us. Its not
            // safe to try to suspend another thread in case we get a cycle. Start the loop again
            // which will allow this thread to be suspended.
            continue;
          }
          CHECK(suspended_thread == nullptr);
          suspended_thread = thread;
          bool updated = suspended_thread->ModifySuspendCount(self, +1, nullptr, reason);
          DCHECK(updated);
          request_suspension = false;
        } else {
          // If the caller isn't requesting suspension, a suspension should have already occurred.
          CHECK_GT(thread->GetSuspendCount(), 0);
        }
        // IsSuspended on the current thread will fail as the current thread is changed into
        // Runnable above. As the suspend count is now raised if this is the current thread
        // it will self suspend on transition to Runnable, making it hard to work with. It's simpler
        // to just explicitly handle the current thread in the callers to this code.
        CHECK_NE(thread, self) << "Attempt to suspend the current thread for the debugger";
        // If thread is suspended (perhaps it was already not Runnable but didn't have a suspend
        // count, or else we've waited and it has self suspended) or is the current thread, we're
        // done.
        if (thread->IsSuspended()) {
          VLOG(threads) << "SuspendThreadByPeer thread suspended: " << *thread;
          if (ATraceEnabled()) {
            std::string name;
            thread->GetThreadName(name);
            ATraceBegin(StringPrintf("SuspendThreadByPeer suspended %s for peer=%p", name.c_str(),
                                      peer).c_str());
          }
          return thread;
        }
        const uint64_t total_delay = NanoTime() - start_time;
        if (total_delay >= thread_suspend_timeout_ns_) {
          ThreadSuspendByPeerWarning(self,
                                     ::android::base::FATAL,
                                     "Thread suspension timed out",
                                     peer);
          if (suspended_thread != nullptr) {
            CHECK_EQ(suspended_thread, thread);
            bool updated = suspended_thread->ModifySuspendCount(soa.Self(),
                                                                -1,
                                                                nullptr,
                                                                reason);
            DCHECK(updated);
          }
          *timed_out = true;
          return nullptr;
        } else if (sleep_us == 0 &&
            total_delay > static_cast<uint64_t>(kThreadSuspendMaxYieldUs) * 1000) {
          // We have spun for kThreadSuspendMaxYieldUs time, switch to sleeps to prevent
          // excessive CPU usage.
          sleep_us = kThreadSuspendMaxYieldUs / 2;
        }
      }
      // Release locks and come out of runnable state.
    }
    VLOG(threads) << "SuspendThreadByPeer waiting to allow thread chance to suspend";
    ThreadSuspendSleep(sleep_us);
    // This may stay at 0 if sleep_us == 0, but this is WAI since we want to avoid using usleep at
    // all if possible. This shouldn't be an issue since time to suspend should always be small.
    sleep_us = std::min(sleep_us * 2, kThreadSuspendMaxSleepUs);
  }
}

static void ThreadSuspendByThreadIdWarning(LogSeverity severity,
                                           const char* message,
                                           uint32_t thread_id) {
  LOG(severity) << StringPrintf("%s: %d", message, thread_id);
}

Thread* ThreadList::SuspendThreadByThreadId(uint32_t thread_id,
                                            SuspendReason reason,
                                            bool* timed_out) {
  const uint64_t start_time = NanoTime();
  useconds_t sleep_us = kThreadSuspendInitialSleepUs;
  *timed_out = false;
  Thread* suspended_thread = nullptr;
  Thread* const self = Thread::Current();
  CHECK_NE(thread_id, kInvalidThreadId);
  VLOG(threads) << "SuspendThreadByThreadId starting";
  while (true) {
    {
      // Note: this will transition to runnable and potentially suspend. We ensure only one thread
      // is requesting another suspend, to avoid deadlock, by requiring this function be called
      // holding Locks::thread_list_suspend_thread_lock_. Its important this thread suspend rather
      // than request thread suspension, to avoid potential cycles in threads requesting each other
      // suspend.
      ScopedObjectAccess soa(self);
      MutexLock thread_list_mu(self, *Locks::thread_list_lock_);
      Thread* thread = nullptr;
      for (const auto& it : list_) {
        if (it->GetThreadId() == thread_id) {
          thread = it;
          break;
        }
      }
      if (thread == nullptr) {
        CHECK(suspended_thread == nullptr) << "Suspended thread " << suspended_thread
            << " no longer in thread list";
        // There's a race in inflating a lock and the owner giving up ownership and then dying.
        ThreadSuspendByThreadIdWarning(::android::base::WARNING,
                                       "No such thread id for suspend",
                                       thread_id);
        return nullptr;
      }
      VLOG(threads) << "SuspendThreadByThreadId found thread: " << *thread;
      DCHECK(Contains(thread));
      {
        MutexLock suspend_count_mu(self, *Locks::thread_suspend_count_lock_);
        if (suspended_thread == nullptr) {
          if (self->GetSuspendCount() > 0) {
            // We hold the suspend count lock but another thread is trying to suspend us. Its not
            // safe to try to suspend another thread in case we get a cycle. Start the loop again
            // which will allow this thread to be suspended.
            continue;
          }
          bool updated = thread->ModifySuspendCount(self, +1, nullptr, reason);
          DCHECK(updated);
          suspended_thread = thread;
        } else {
          CHECK_EQ(suspended_thread, thread);
          // If the caller isn't requesting suspension, a suspension should have already occurred.
          CHECK_GT(thread->GetSuspendCount(), 0);
        }
        // IsSuspended on the current thread will fail as the current thread is changed into
        // Runnable above. As the suspend count is now raised if this is the current thread
        // it will self suspend on transition to Runnable, making it hard to work with. It's simpler
        // to just explicitly handle the current thread in the callers to this code.
        CHECK_NE(thread, self) << "Attempt to suspend the current thread for the debugger";
        // If thread is suspended (perhaps it was already not Runnable but didn't have a suspend
        // count, or else we've waited and it has self suspended) or is the current thread, we're
        // done.
        if (thread->IsSuspended()) {
          if (ATraceEnabled()) {
            std::string name;
            thread->GetThreadName(name);
            ATraceBegin(StringPrintf("SuspendThreadByThreadId suspended %s id=%d",
                                      name.c_str(), thread_id).c_str());
          }
          VLOG(threads) << "SuspendThreadByThreadId thread suspended: " << *thread;
          return thread;
        }
        const uint64_t total_delay = NanoTime() - start_time;
        if (total_delay >= thread_suspend_timeout_ns_) {
          ThreadSuspendByThreadIdWarning(::android::base::WARNING,
                                         "Thread suspension timed out",
                                         thread_id);
          if (suspended_thread != nullptr) {
            bool updated = thread->ModifySuspendCount(soa.Self(), -1, nullptr, reason);
            DCHECK(updated);
          }
          *timed_out = true;
          return nullptr;
        } else if (sleep_us == 0 &&
            total_delay > static_cast<uint64_t>(kThreadSuspendMaxYieldUs) * 1000) {
          // We have spun for kThreadSuspendMaxYieldUs time, switch to sleeps to prevent
          // excessive CPU usage.
          sleep_us = kThreadSuspendMaxYieldUs / 2;
        }
      }
      // Release locks and come out of runnable state.
    }
    VLOG(threads) << "SuspendThreadByThreadId waiting to allow thread chance to suspend";
    ThreadSuspendSleep(sleep_us);
    sleep_us = std::min(sleep_us * 2, kThreadSuspendMaxSleepUs);
  }
}

Thread* ThreadList::FindThreadByThreadId(uint32_t thread_id) {
  for (const auto& thread : list_) {
    if (thread->GetThreadId() == thread_id) {
      return thread;
    }
  }
  return nullptr;
}

void ThreadList::WaitForOtherNonDaemonThreadsToExit(bool check_no_birth) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  while (true) {
    Locks::runtime_shutdown_lock_->Lock(self);
    if (check_no_birth) {
      // No more threads can be born after we start to shutdown.
      CHECK(Runtime::Current()->IsShuttingDownLocked());
      CHECK_EQ(Runtime::Current()->NumberOfThreadsBeingBorn(), 0U);
    } else {
      if (Runtime::Current()->NumberOfThreadsBeingBorn() != 0U) {
        // Awkward. Shutdown_cond_ is private, but the only live thread may not be registered yet.
        // Fortunately, this is used mostly for testing, and not performance-critical.
        Locks::runtime_shutdown_lock_->Unlock(self);
        usleep(1000);
        continue;
      }
    }
    MutexLock mu(self, *Locks::thread_list_lock_);
    Locks::runtime_shutdown_lock_->Unlock(self);
    // Also wait for any threads that are unregistering to finish. This is required so that no
    // threads access the thread list after it is deleted. TODO: This may not work for user daemon
    // threads since they could unregister at the wrong time.
    bool done = unregistering_count_ == 0;
    if (done) {
      for (const auto& thread : list_) {
        if (thread != self && !thread->IsDaemon()) {
          done = false;
          break;
        }
      }
    }
    if (done) {
      break;
    }
    // Wait for another thread to exit before re-checking.
    Locks::thread_exit_cond_->Wait(self);
  }
}

void ThreadList::SuspendAllDaemonThreadsForShutdown() {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  Thread* self = Thread::Current();
  size_t daemons_left = 0;
  {
    // Tell all the daemons it's time to suspend.
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    for (const auto& thread : list_) {
      // This is only run after all non-daemon threads have exited, so the remainder should all be
      // daemons.
      CHECK(thread->IsDaemon()) << *thread;
      if (thread != self) {
        bool updated = thread->ModifySuspendCount(self, +1, nullptr, SuspendReason::kInternal);
        DCHECK(updated);
        ++daemons_left;
      }
      // We are shutting down the runtime, set the JNI functions of all the JNIEnvs to be
      // the sleep forever one.
      thread->GetJniEnv()->SetFunctionsToRuntimeShutdownFunctions();
    }
  }
  if (daemons_left == 0) {
    // No threads left; safe to shut down.
    return;
  }
  // There is not a clean way to shut down if we have daemons left. We have no mechanism for
  // killing them and reclaiming thread stacks. We also have no mechanism for waiting until they
  // have truly finished touching the memory we are about to deallocate. We do the best we can with
  // timeouts.
  //
  // If we have any daemons left, wait until they are (a) suspended and (b) they are not stuck
  // in a place where they are about to access runtime state and are not in a runnable state.
  // We attempt to do the latter by just waiting long enough for things to
  // quiesce. Examples: Monitor code or waking up from a condition variable.
  //
  // Give the threads a chance to suspend, complaining if they're slow. (a)
  bool have_complained = false;
  static constexpr size_t kTimeoutMicroseconds = 2000 * 1000;
  static constexpr size_t kSleepMicroseconds = 1000;
  bool all_suspended = false;
  for (size_t i = 0; !all_suspended && i < kTimeoutMicroseconds / kSleepMicroseconds; ++i) {
    bool found_running = false;
    {
      MutexLock mu(self, *Locks::thread_list_lock_);
      for (const auto& thread : list_) {
        if (thread != self && thread->GetState() == kRunnable) {
          if (!have_complained) {
            LOG(WARNING) << "daemon thread not yet suspended: " << *thread;
            have_complained = true;
          }
          found_running = true;
        }
      }
    }
    if (found_running) {
      // Sleep briefly before checking again. Max total sleep time is kTimeoutMicroseconds.
      usleep(kSleepMicroseconds);
    } else {
      all_suspended = true;
    }
  }
  if (!all_suspended) {
    // We can get here if a daemon thread executed a fastnative native call, so that it
    // remained in runnable state, and then made a JNI call after we called
    // SetFunctionsToRuntimeShutdownFunctions(), causing it to permanently stay in a harmless
    // but runnable state. See b/147804269 .
    LOG(WARNING) << "timed out suspending all daemon threads";
  }
  // Assume all threads are either suspended or somehow wedged.
  // Wait again for all the now "suspended" threads to actually quiesce. (b)
  static constexpr size_t kDaemonSleepTime = 200 * 1000;
  usleep(kDaemonSleepTime);
  std::list<Thread*> list_copy;
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    // Half-way through the wait, set the "runtime deleted" flag, causing any newly awoken
    // threads to immediately go back to sleep without touching memory. This prevents us from
    // touching deallocated memory, but it also prevents mutexes from getting released. Thus we
    // only do this once we're reasonably sure that no system mutexes are still held.
    for (const auto& thread : list_) {
      DCHECK(thread == self || !all_suspended || thread->GetState() != kRunnable);
      // In the !all_suspended case, the target is probably sleeping.
      thread->GetJniEnv()->SetRuntimeDeleted();
      // Possibly contended Mutex acquisitions are unsafe after this.
      // Releasing thread_list_lock_ is OK, since it can't block.
    }
  }
  // Finally wait for any threads woken before we set the "runtime deleted" flags to finish
  // touching memory.
  usleep(kDaemonSleepTime);
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(hwaddress_sanitizer)
  // Sleep a bit longer with -fsanitize=address, since everything is slower.
  usleep(2 * kDaemonSleepTime);
#endif
#endif
  // At this point no threads should be touching our data structures anymore.
}

void ThreadList::Register(Thread* self) {
  DCHECK_EQ(self, Thread::Current());
  CHECK(!shut_down_);

  if (VLOG_IS_ON(threads)) {
    std::ostringstream oss;
    self->ShortDump(oss);  // We don't hold the mutator_lock_ yet and so cannot call Dump.
    LOG(INFO) << "ThreadList::Register() " << *self  << "\n" << oss.str();
  }

  // Atomically add self to the thread list and make its thread_suspend_count_ reflect ongoing
  // SuspendAll requests.
  MutexLock mu(self, *Locks::thread_list_lock_);
  MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
  // Modify suspend count in increments of 1 to maintain invariants in ModifySuspendCount. While
  // this isn't particularly efficient the suspend counts are most commonly 0 or 1.
  for (int delta = suspend_all_count_; delta > 0; delta--) {
    bool updated = self->ModifySuspendCount(self, +1, nullptr, SuspendReason::kInternal);
    DCHECK(updated);
  }
  CHECK(!Contains(self));
  list_.push_back(self);
  if (kUseReadBarrier) {
    gc::collector::ConcurrentCopying* const cc =
        Runtime::Current()->GetHeap()->ConcurrentCopyingCollector();
    // Initialize according to the state of the CC collector.
    self->SetIsGcMarkingAndUpdateEntrypoints(cc->IsMarking());
    if (cc->IsUsingReadBarrierEntrypoints()) {
      self->SetReadBarrierEntrypoints();
    }
    self->SetWeakRefAccessEnabled(cc->IsWeakRefAccessEnabled());
  }
  self->NotifyInTheadList();
}

void ThreadList::Unregister(Thread* self) {
  DCHECK_EQ(self, Thread::Current());
  CHECK_NE(self->GetState(), kRunnable);
  Locks::mutator_lock_->AssertNotHeld(self);

  VLOG(threads) << "ThreadList::Unregister() " << *self;

  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    ++unregistering_count_;
  }

  // Any time-consuming destruction, plus anything that can call back into managed code or
  // suspend and so on, must happen at this point, and not in ~Thread. The self->Destroy is what
  // causes the threads to join. It is important to do this after incrementing unregistering_count_
  // since we want the runtime to wait for the daemon threads to exit before deleting the thread
  // list.
  self->Destroy();

  // If tracing, remember thread id and name before thread exits.
  Trace::StoreExitingThreadInfo(self);

  uint32_t thin_lock_id = self->GetThreadId();
  while (true) {
    // Remove and delete the Thread* while holding the thread_list_lock_ and
    // thread_suspend_count_lock_ so that the unregistering thread cannot be suspended.
    // Note: deliberately not using MutexLock that could hold a stale self pointer.
    {
      MutexLock mu(self, *Locks::thread_list_lock_);
      if (!Contains(self)) {
        std::string thread_name;
        self->GetThreadName(thread_name);
        std::ostringstream os;
        DumpNativeStack(os, GetTid(), nullptr, "  native: ", nullptr);
        LOG(ERROR) << "Request to unregister unattached thread " << thread_name << "\n" << os.str();
        break;
      } else {
        MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
        if (!self->IsSuspended()) {
          list_.remove(self);
          break;
        }
      }
    }
    // In the case where we are not suspended yet, sleep to leave other threads time to execute.
    // This is important if there are realtime threads. b/111277984
    usleep(1);
    // We failed to remove the thread due to a suspend request, loop and try again.
  }
  delete self;

  // Release the thread ID after the thread is finished and deleted to avoid cases where we can
  // temporarily have multiple threads with the same thread id. When this occurs, it causes
  // problems in FindThreadByThreadId / SuspendThreadByThreadId.
  ReleaseThreadId(nullptr, thin_lock_id);

  // Clear the TLS data, so that the underlying native thread is recognizably detached.
  // (It may wish to reattach later.)
#ifdef __BIONIC__
  __get_tls()[TLS_SLOT_ART_THREAD_SELF] = nullptr;
#else
  CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, nullptr), "detach self");
  Thread::self_tls_ = nullptr;
#endif

  // Signal that a thread just detached.
  MutexLock mu(nullptr, *Locks::thread_list_lock_);
  --unregistering_count_;
  Locks::thread_exit_cond_->Broadcast(nullptr);
}

void ThreadList::ForEach(void (*callback)(Thread*, void*), void* context) {
  for (const auto& thread : list_) {
    callback(thread, context);
  }
}

void ThreadList::VisitRootsForSuspendedThreads(RootVisitor* visitor) {
  Thread* const self = Thread::Current();
  std::vector<Thread*> threads_to_visit;

  // Tell threads to suspend and copy them into list.
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    for (Thread* thread : list_) {
      bool suspended = thread->ModifySuspendCount(self, +1, nullptr, SuspendReason::kInternal);
      DCHECK(suspended);
      if (thread == self || thread->IsSuspended()) {
        threads_to_visit.push_back(thread);
      } else {
        bool resumed = thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
        DCHECK(resumed);
      }
    }
  }

  // Visit roots without holding thread_list_lock_ and thread_suspend_count_lock_ to prevent lock
  // order violations.
  for (Thread* thread : threads_to_visit) {
    thread->VisitRoots(visitor, kVisitRootFlagAllRoots);
  }

  // Restore suspend counts.
  {
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    for (Thread* thread : threads_to_visit) {
      bool updated = thread->ModifySuspendCount(self, -1, nullptr, SuspendReason::kInternal);
      DCHECK(updated);
    }
  }
}

void ThreadList::VisitRoots(RootVisitor* visitor, VisitRootFlags flags) const {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  for (const auto& thread : list_) {
    thread->VisitRoots(visitor, flags);
  }
}

void ThreadList::SweepInterpreterCaches(IsMarkedVisitor* visitor) const {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  for (const auto& thread : list_) {
    thread->SweepInterpreterCache(visitor);
  }
}

void ThreadList::VisitReflectiveTargets(ReflectiveValueVisitor *visitor) const {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  for (const auto& thread : list_) {
    thread->VisitReflectiveTargets(visitor);
  }
}

uint32_t ThreadList::AllocThreadId(Thread* self) {
  MutexLock mu(self, *Locks::allocated_thread_ids_lock_);
  for (size_t i = 0; i < allocated_ids_.size(); ++i) {
    if (!allocated_ids_[i]) {
      allocated_ids_.set(i);
      return i + 1;  // Zero is reserved to mean "invalid".
    }
  }
  LOG(FATAL) << "Out of internal thread ids";
  UNREACHABLE();
}

void ThreadList::ReleaseThreadId(Thread* self, uint32_t id) {
  MutexLock mu(self, *Locks::allocated_thread_ids_lock_);
  --id;  // Zero is reserved to mean "invalid".
  DCHECK(allocated_ids_[id]) << id;
  allocated_ids_.reset(id);
}

ScopedSuspendAll::ScopedSuspendAll(const char* cause, bool long_suspend) {
  Runtime::Current()->GetThreadList()->SuspendAll(cause, long_suspend);
}

ScopedSuspendAll::~ScopedSuspendAll() {
  Runtime::Current()->GetThreadList()->ResumeAll();
}

}  // namespace art
