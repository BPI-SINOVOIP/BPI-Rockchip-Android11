// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "package_change_observer.h"
#include "package_manager_remote.h"
#include "manager/event_manager.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

namespace iorap::binder {

PackageChangeObserver::PackageChangeObserver(
    std::shared_ptr<iorap::manager::EventManager> event_manager) :
  event_manager_(event_manager){}

android::binder::Status PackageChangeObserver::onPackageChanged(
  const android::content::pm::PackageChangeEvent& event) {
  LOG(DEBUG) << "Received PackageChangeObserver::onPackageChanged";
  if (event_manager_->OnPackageChanged(event)) {
    return android::binder::Status::ok();
  } else {
    return android::binder::Status::fromStatusT(android::BAD_VALUE);
  }
}
}  // namespace iorap::binder
