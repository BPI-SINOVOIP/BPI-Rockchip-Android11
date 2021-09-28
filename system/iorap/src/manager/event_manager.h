/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef IORAP_MANAGER_EVENT_MANAGER_H_
#define IORAP_MANAGER_EVENT_MANAGER_H_

#include "binder/app_launch_event.h"
#include "binder/dexopt_event.h"
#include "binder/job_scheduled_event.h"
#include "binder/request_id.h"
#include "binder/task_result.h"

#include <android/content/pm/PackageChangeEvent.h>

#include <memory>

namespace android {
class Printer;
}  // namespace android

namespace iorap::perfetto {
struct RxProducerFactory;
}  // namespace iorap::perfetto

namespace iorap::manager {

// These callbacks are invoked by the EventManager to provide asynchronous notification for the status
// of an event handler.
//
// Calling 'On_Event' in EventManager should be considered merely to start the task.
// Calling 'OnComplete' here is considered to terminate the request (either with a success or error).
// OnProgress is optional, but if it is called it must be called prior to 'OnComplete'.
//
// All callbacks for the same request-id are sequentially consistent.
class TaskResultCallbacks {
 public:
  virtual void OnProgress(iorap::binder::RequestId request_id, iorap::binder::TaskResult task_result) {}
  virtual void OnComplete(iorap::binder::RequestId request_id, iorap::binder::TaskResult task_result) {}

  virtual ~TaskResultCallbacks() {}
};

class EventManager {
 public:
  static std::shared_ptr<EventManager> Create();
  static std::shared_ptr<EventManager> Create(
      /*borrow*/perfetto::RxProducerFactory& perfetto_factory);
  void SetTaskResultCallbacks(std::shared_ptr<TaskResultCallbacks> callbacks);

  // Joins any background threads created by EventManager.
  void Join();

  // Handles an AppLaunchEvent:
  //
  // * Intent starts and app launch starts are treated critically
  //   and will be handled immediately. This means the caller
  //   (e.g. the binder pool thread) could be starved in the name
  //   of low latency.
  //
  // * Other types are handled in a separate thread.
  bool OnAppLaunchEvent(binder::RequestId request_id,
                        const binder::AppLaunchEvent& event);

  // Handles a DexOptEvent:
  //
  // Clean up the invalidate traces after package is updated by dexopt.
  bool OnDexOptEvent(binder::RequestId request_id,
                     const binder::DexOptEvent& event);


  // Handles a JobScheduledEvent:
  //
  // * Start/stop background jobs (typically for idle maintenance).
  // * For example, this could kick off a background compiler.
  bool OnJobScheduledEvent(binder::RequestId request_id,
                           const binder::JobScheduledEvent& event);

  // Handles a PackageChangeEvent:
  //
  // * The package manager service send this event for package install
  //   update or delete.
  bool OnPackageChanged(const android::content::pm::PackageChangeEvent& event);

  // Print to adb shell dumpsys (for bugreport info).
  void Dump(/*borrow*/::android::Printer& printer);

  // A dumpsys --refresh-properties command signaling that we should
  // refresh our system properties.
  void RefreshSystemProperties(::android::Printer& printer);

  // A dumpsys --purge-package <name> command signaling
  // that all db rows and files associated with a package should be deleted.
  bool PurgePackage(::android::Printer& printer, const std::string& package_name);

  // A dumpsys --compile-package <name> command signaling
  // that a package should be recompiled.
  bool CompilePackage(::android::Printer& printer, const std::string& package_name);

  class Impl;
 private:
  std::unique_ptr<Impl> impl_;

  EventManager(perfetto::RxProducerFactory& perfetto_factory);
};

}  // namespace iorap::manager

#endif  // IORAP_MANAGER_EVENT_MANAGER_H_
