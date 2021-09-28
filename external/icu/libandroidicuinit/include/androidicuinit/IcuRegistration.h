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

#ifndef ICU_REGISTRATION_H_included
#define ICU_REGISTRATION_H_included

#include <memory>
#include <string>

#include <android-base/macros.h>
#include <jni.h>

namespace androidicuinit {
namespace impl {

/*
 * Handles ICU data mapping for a single ICU .dat file.
 * The Create method handles mapping the file into memory and calling
 * udata_setCommonData(). The file is unmapped on object destruction.
 */
class IcuDataMap final {
 public:
  // Maps in ICU data at the path and call udata_setCommonData(), returning
  // null if it failed (prints error to ALOGE).
  static std::unique_ptr<IcuDataMap> Create(const std::string& path);
  // Unmaps the ICU data.
  ~IcuDataMap();

 private:
  IcuDataMap(const std::string& path)
      : path_(path), data_(nullptr), data_length_(0) {}
  bool TryMap();
  bool TryUnmap();

  std::string path_;    // Save for error messages.
  void* data_;          // Save for munmap.
  size_t data_length_;  // Save for munmap.

  DISALLOW_COPY_AND_ASSIGN(IcuDataMap);
};

}  // namespace impl

/*
 * Handles the mapping of all ICU data files into memory for the various files
 * used on Android. All data files are unmapped on object destruction.
 */
class IcuRegistration final {
 public:
  static void Register();
  static void Deregister();

  // Required to be public so it can be destructed by unique_ptr.
  ~IcuRegistration();

 private:
  IcuRegistration();

  static bool pathExists(const std::string& path);
  static std::string getDataTimeZonePath();
  static std::string getTimeZoneModulePath();
  static std::string getI18nModulePath();

  std::unique_ptr<impl::IcuDataMap> icu_datamap_from_data_;
  std::unique_ptr<impl::IcuDataMap> icu_datamap_from_tz_module_;
  std::unique_ptr<impl::IcuDataMap> icu_datamap_from_i18n_module_;

  DISALLOW_COPY_AND_ASSIGN(IcuRegistration);
};

}  // namespace androidicuinit

#endif  // ICU_REGISTRATION_H_included
