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

#ifndef IORAP_SRC_PACKAGE_CHANGE_OBSERVER_H_
#define IORAP_SRC_PACKAGE_CHANGE_OBSERVER_H_

#include "binder/iiorap_def.h"

#include <android/content/pm/BnPackageChangeObserver.h>
#include <android/content/pm/PackageChangeEvent.h>
#include <android/content/pm/IPackageManagerNative.h>

namespace iorap::manager {
  class EventManager;
};

namespace iorap::binder {

class PackageChangeObserver : public android::content::pm::BnPackageChangeObserver  {
 public:
  PackageChangeObserver(std::shared_ptr<iorap::manager::EventManager> event_manager);

  // Callback when the package is changed.
  android::binder::Status onPackageChanged(
      const ::android::content::pm::PackageChangeEvent& event) override;
 private:
  std::shared_ptr<iorap::manager::EventManager> event_manager_;
};
}  // namespace iorap::binder

#endif  // IORAP_SRC_PACKAGE_CHANGE_OBSERVER_H_
