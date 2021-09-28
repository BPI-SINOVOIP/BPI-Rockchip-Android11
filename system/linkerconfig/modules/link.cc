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

#include "linkerconfig/link.h"

#include "linkerconfig/log.h"

namespace android {
namespace linkerconfig {
namespace modules {

void Link::AddSharedLib(const std::vector<std::string>& lib_names) {
  if (!allow_all_shared_libs_) {
    shared_libs_.insert(shared_libs_.end(), lib_names.begin(), lib_names.end());
  }
}

void Link::AllowAllSharedLibs() {
  if (!allow_all_shared_libs_) {
    shared_libs_.clear();
    allow_all_shared_libs_ = true;
  }
}

void Link::WriteConfig(ConfigWriter& writer) const {
  const auto prefix =
      "namespace." + origin_namespace_ + ".link." + target_namespace_ + ".";
  if (allow_all_shared_libs_) {
    writer.WriteLine(prefix + "allow_all_shared_libs = true");
  } else if (!shared_libs_.empty()) {
    writer.WriteVars(prefix + "shared_libs", shared_libs_);
  } else {
    LOG(WARNING) << "Ignored empty shared libs link from " << origin_namespace_
                 << " to " << target_namespace_;
  }
}

}  // namespace modules
}  // namespace linkerconfig
}  // namespace android
