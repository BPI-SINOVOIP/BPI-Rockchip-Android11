/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "binder/iiorap_impl.h"
#include "common/debug.h"
#include "common/loggers.h"
#include "db/models.h"
#include "manager/event_manager.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <binder/IPCThreadState.h>
#include <utils/Trace.h>

#include <stdio.h>

static constexpr const char* kServiceName = iorap::binder::IIorapImpl::getServiceName();

int main(int /*argc*/, char** argv) {
  if (android::base::GetBoolProperty("iorapd.log.verbose", iorap::kIsDebugBuild)) {
    // Show verbose logs if the property is enabled or if we are a debug build.
    setenv("ANDROID_LOG_TAGS", "*:v", /*overwrite*/ 1);
  }

  // Logs go to system logcat.
  android::base::InitLogging(argv, iorap::common::StderrAndLogdLogger{android::base::SYSTEM});

  LOG(INFO) << kServiceName << " (the prefetchening) firing up";
  {
    android::ScopedTrace trace_db_init{ATRACE_TAG_ACTIVITY_MANAGER, "IorapNativeService::db_init"};
    iorap::db::SchemaModel db_schema =
        iorap::db::SchemaModel::GetOrCreate(
            android::base::GetProperty("iorapd.db.location",
                                       "/data/misc/iorapd/sqlite.db"));
    db_schema.MarkSingleton();
  }

  std::shared_ptr<iorap::manager::EventManager> event_manager;
  {
    android::ScopedTrace trace_start{ATRACE_TAG_ACTIVITY_MANAGER, "IorapNativeService::start"};

    // TODO: use fruit for this DI.
    event_manager =
        iorap::manager::EventManager::Create();
    if (!iorap::binder::IIorapImpl::Start(event_manager)) {
      LOG(ERROR) << "Unable to start IorapNativeService";
      exit(1);
    }
  }

  // This must be logged after all other initialization has finished.
  LOG(INFO) << kServiceName << " (the prefetchening) readied up";

  event_manager->Join();  // TODO: shutdown somewhere?
  // Block until something else shuts down the binder service.
  android::IPCThreadState::self()->joinThreadPool();
  LOG(INFO) << kServiceName << " shutting down";

  return 0;
}
