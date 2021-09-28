/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_SIGNAL_CATCHER_H_
#define ART_RUNTIME_SIGNAL_CATCHER_H_

#include "android-base/unique_fd.h"
#include "base/mutex.h"

namespace art {

class Runtime;
class SignalSet;
class Thread;

/*
 * A daemon thread that catches signals and does something useful. For
 * example, when a SIGQUIT (Ctrl-\) arrives, we suspend and dump the
 * status of all threads.
 */
class SignalCatcher {
 public:
  SignalCatcher();
  ~SignalCatcher();

  void HandleSigQuit() REQUIRES(!Locks::mutator_lock_, !Locks::thread_list_lock_,
                                !Locks::thread_suspend_count_lock_);


 private:
  // NO_THREAD_SAFETY_ANALYSIS for static function calling into member function with excludes lock.
  static void* Run(void* arg) NO_THREAD_SAFETY_ANALYSIS;

  void HandleSigUsr1();
  void Output(const std::string& s);
  void SetHaltFlag(bool new_value) REQUIRES(!lock_);
  bool ShouldHalt() REQUIRES(!lock_);
  int WaitForSignal(Thread* self, SignalSet& signals) REQUIRES(!lock_);

  mutable Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable cond_ GUARDED_BY(lock_);
  bool halt_ GUARDED_BY(lock_);
  pthread_t pthread_ GUARDED_BY(lock_);
  Thread* thread_ GUARDED_BY(lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_SIGNAL_CATCHER_H_
