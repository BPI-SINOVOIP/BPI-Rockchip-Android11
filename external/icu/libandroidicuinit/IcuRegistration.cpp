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

#include "androidicuinit/IcuRegistration.h"
#include "androidicuinit/android_icu_reg.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <log/log.h>
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/utypes.h>

namespace androidicuinit {
namespace impl {

// Map in ICU data at the path, returning null if it failed (prints error to
// ALOGE).
std::unique_ptr<IcuDataMap> IcuDataMap::Create(const std::string& path) {
  std::unique_ptr<IcuDataMap> map(new IcuDataMap(path));

  if (!map->TryMap()) {
    // madvise or ICU could fail but mmap still succeeds.
    // Destructor will take care of cleaning up a partial init.
    return nullptr;
  }

  return map;
}

// Unmap the ICU data.
IcuDataMap::~IcuDataMap() { TryUnmap(); }

bool IcuDataMap::TryMap() {
  // Open the file and get its length.
  android::base::unique_fd fd(
      TEMP_FAILURE_RETRY(open(path_.c_str(), O_RDONLY)));

  if (fd.get() == -1) {
    ALOGE("Couldn't open '%s': %s", path_.c_str(), strerror(errno));
    return false;
  }

  struct stat sb;
  if (fstat(fd.get(), &sb) == -1) {
    ALOGE("Couldn't stat '%s': %s", path_.c_str(), strerror(errno));
    return false;
  }

  data_length_ = sb.st_size;

  // Map it.
  data_ =
      mmap(NULL, data_length_, PROT_READ, MAP_SHARED, fd.get(), 0 /* offset */);
  if (data_ == MAP_FAILED) {
    ALOGE("Couldn't mmap '%s': %s", path_.c_str(), strerror(errno));
    return false;
  }

  // Tell the kernel that accesses are likely to be random rather than
  // sequential.
  if (madvise(data_, data_length_, MADV_RANDOM) == -1) {
    ALOGE("Couldn't madvise(MADV_RANDOM) '%s': %s", path_.c_str(),
          strerror(errno));
    return false;
  }

  UErrorCode status = U_ZERO_ERROR;

  // Tell ICU to use our memory-mapped data.
  udata_setCommonData(data_, &status);
  if (status != U_ZERO_ERROR) {
    ALOGE("Couldn't initialize ICU (udata_setCommonData): %s (%s)",
          u_errorName(status), path_.c_str());
    return false;
  }

  return true;
}

bool IcuDataMap::TryUnmap() {
  // Don't need to do opposite of udata_setCommonData,
  // u_cleanup (performed in IcuRegistration::~IcuRegistration()) takes care of
  // it.

  // Don't need to opposite of madvise, munmap will take care of it.

  if (data_ != nullptr && data_ != MAP_FAILED) {
    if (munmap(data_, data_length_) == -1) {
      ALOGE("Couldn't munmap '%s': %s", path_.c_str(), strerror(errno));
      return false;
    }
  }

  // Don't need to close the file, it was closed automatically during TryMap.
  return true;
}

}  // namespace impl

// A pointer to the instance used by Register and Deregister. Since this code
// is currently included in a static library this doesn't prevent duplicate
// initialization calls.
static std::unique_ptr<IcuRegistration> gIcuRegistration;

void IcuRegistration::Register() {
  CHECK(gIcuRegistration.get() == nullptr);

  gIcuRegistration.reset(new IcuRegistration());
}

void IcuRegistration::Deregister() {
  gIcuRegistration.reset();
}

// Init ICU, configuring it and loading the data files.
IcuRegistration::IcuRegistration() {
  UErrorCode status = U_ZERO_ERROR;
  // Tell ICU it can *only* use our memory-mapped data.
  udata_setFileAccess(UDATA_NO_FILES, &status);
  if (status != U_ZERO_ERROR) {
    ALOGE("Couldn't initialize ICU (s_setFileAccess): %s", u_errorName(status));
    abort();
  }

  // Note: This logic below should match the logic for ICU4J in
  // TimeZoneDataFiles.java in libcore/ to ensure consistent behavior between
  // ICU4C and ICU4J.

  // Check the timezone /data override file exists from the "Time zone update
  // via APK" feature.
  // https://source.android.com/devices/tech/config/timezone-rules
  // If it does, map it first so we use its data in preference to later ones.
  std::string dataPath = getDataTimeZonePath();
  if (pathExists(dataPath)) {
    ALOGD("Time zone override file found: %s", dataPath.c_str());
    icu_datamap_from_data_ = impl::IcuDataMap::Create(dataPath);
    if (icu_datamap_from_data_ == nullptr) {
      ALOGW(
          "TZ override /data file %s exists but could not be loaded. Skipping.",
          dataPath.c_str());
    }
  } else {
    ALOGV("No timezone override /data file found: %s", dataPath.c_str());
  }

  // Check the timezone override file exists from a mounted APEX file.
  // If it does, map it next so we use its data in preference to later ones.
  std::string tzModulePath = getTimeZoneModulePath();
  if (pathExists(tzModulePath)) {
    ALOGD("Time zone APEX ICU file found: %s", tzModulePath.c_str());
    icu_datamap_from_tz_module_ = impl::IcuDataMap::Create(tzModulePath);
    if (icu_datamap_from_tz_module_ == nullptr) {
      ALOGW(
          "TZ module override file %s exists but could not be loaded. "
          "Skipping.",
          tzModulePath.c_str());
    }
  } else {
    ALOGV("No time zone module override file found: %s", tzModulePath.c_str());
  }

  // Use the ICU data files that shipped with the i18n module for everything
  // else.
  std::string i18nModulePath = getI18nModulePath();
  icu_datamap_from_i18n_module_ = impl::IcuDataMap::Create(i18nModulePath);
  if (icu_datamap_from_i18n_module_ == nullptr) {
    // IcuDataMap::Create() will log on error so there is no need to log here.
    abort();
  }
  ALOGD("I18n APEX ICU file found: %s", i18nModulePath.c_str());

  // Failures to find the ICU data tend to be somewhat obscure because ICU loads
  // its data on first use, which can be anywhere. Force initialization up front
  // so we can report a nice clear error and bail.
  u_init(&status);
  if (status != U_ZERO_ERROR) {
    ALOGE("Couldn't initialize ICU (u_init): %s", u_errorName(status));
    abort();
  }
}

// De-init ICU, unloading the data files. Do the opposite of the above function.
IcuRegistration::~IcuRegistration() {
  // Reset libicu state to before it was loaded.
  u_cleanup();

  // Unmap ICU data files.
  icu_datamap_from_i18n_module_.reset();
  icu_datamap_from_tz_module_.reset();
  icu_datamap_from_data_.reset();

  // We don't need to call udata_setFileAccess because u_cleanup takes care of
  // it.
}

bool IcuRegistration::pathExists(const std::string& path) {
  struct stat sb;
  return stat(path.c_str(), &sb) == 0;
}

// Returns a string containing the expected path of the (optional) /data tz data
// file
std::string IcuRegistration::getDataTimeZonePath() {
  const char* dataPathPrefix = getenv("ANDROID_DATA");
  if (dataPathPrefix == NULL) {
    ALOGE("ANDROID_DATA environment variable not set");
    abort();
  }
  std::string dataPath;
  dataPath = dataPathPrefix;
  dataPath += "/misc/zoneinfo/current/icu/icu_tzdata.dat";

  return dataPath;
}

// Returns a string containing the expected path of the (optional) /apex tz
// module data file
std::string IcuRegistration::getTimeZoneModulePath() {
  const char* tzdataModulePathPrefix = getenv("ANDROID_TZDATA_ROOT");
  if (tzdataModulePathPrefix == NULL) {
    ALOGE("ANDROID_TZDATA_ROOT environment variable not set");
    abort();
  }

  std::string tzdataModulePath;
  tzdataModulePath = tzdataModulePathPrefix;
  tzdataModulePath += "/etc/icu/icu_tzdata.dat";
  return tzdataModulePath;
}

std::string IcuRegistration::getI18nModulePath() {
  const char* i18nModulePathPrefix = getenv("ANDROID_I18N_ROOT");
  if (i18nModulePathPrefix == NULL) {
    ALOGE("ANDROID_I18N_ROOT environment variable not set");
    abort();
  }

  std::string i18nModulePath;
  i18nModulePath = i18nModulePathPrefix;
  i18nModulePath += "/etc/icu/" U_ICUDATA_NAME ".dat";
  return i18nModulePath;
}

}  // namespace androidicuinit

extern "C" {
void android_icu_register() {
  androidicuinit::IcuRegistration::Register();
}


void android_icu_deregister() {
  androidicuinit::IcuRegistration::Deregister();
}
}
