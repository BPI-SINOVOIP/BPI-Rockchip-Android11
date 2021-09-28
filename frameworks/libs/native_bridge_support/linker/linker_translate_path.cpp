/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "linker_translate_path.h"

#include <string>

#include <android/api-level.h>

#include "linker.h"

// Handle dlopen by full path.
//
// 1. Translate original path to native_bridge path.
//
// Native bridge libraries reside in $LIB/$ABI subdirectory. For example:
//   /system/lib/liblog.so -> /system/lib/arm/liblog.so
//
// Native bridge libraries do not use apex. For example:
//   /apex/com.android.i18n/lib/libicuuc.so -> /system/lib/arm/libicuuc.so
//
// 2. Repeat linker workaround to open apex libraries by system path (see http://b/121248172).
//
// For older target SDK versions, linker allows to open apex libraries by system path, so it does:
//   /system/lib/libicuuc.so -> /apex/com.android.art/lib/libicuuc.so
//
// Adding native bridge path translation, we get:
//   /system/lib/libicuuc.so -> /apex/com.android.art/lib/libicuuc.so -> /system/lib/arm/libicuuc.so

#if defined(__arm__)
#define SYSTEM_LIB(name) \
  { "/system/lib/" name, "/system/lib/arm/" name }
#define APEX_LIB(apex, name) \
  { "/apex/" apex "/lib/" name, "/system/lib/arm/" name }
#elif defined(__aarch64__)
#define SYSTEM_LIB(name) \
  { "/system/lib64/" name, "/system/lib64/arm64/" name }
#define APEX_LIB(apex, name) \
  { "/apex/" apex "/lib64/" name, "/system/lib64/arm64/" name }
#else
#error "Unknown guest arch"
#endif

/**
 * Translate /system path or /apex path to native_bridge path
 * Function name is misleading, as it overrides the corresponding function in original linker.
 *
 * param out_name pointing to native_bridge path
 * return true if translation is needed
 */
bool translateSystemPathToApexPath(const char* name, std::string* out_name) {
  static constexpr const char* kPathTranslation[][2] = {
    // Libraries accessible by system path.
    SYSTEM_LIB("libEGL.so"),
    SYSTEM_LIB("libGLESv1_CM.so"),
    SYSTEM_LIB("libGLESv2.so"),
    SYSTEM_LIB("libGLESv3.so"),
    SYSTEM_LIB("libOpenMAXAL.so"),
    SYSTEM_LIB("libOpenSLES.so"),
    SYSTEM_LIB("libRS.so"),
    SYSTEM_LIB("libaaudio.so"),
    SYSTEM_LIB("libamidi.so"),
    SYSTEM_LIB("libandroid.so"),
    SYSTEM_LIB("libbinder_ndk.so"),
    SYSTEM_LIB("libc.so"),
    SYSTEM_LIB("libcamera2ndk.so"),
    SYSTEM_LIB("libdl.so"),
    SYSTEM_LIB("libjnigraphics.so"),
    SYSTEM_LIB("liblog.so"),
    SYSTEM_LIB("libm.so"),
    SYSTEM_LIB("libmediandk.so"),
    SYSTEM_LIB("libnativewindow.so"),
    SYSTEM_LIB("libstdc++.so"),
    SYSTEM_LIB("libsync.so"),
    SYSTEM_LIB("libvulkan.so"),
    SYSTEM_LIB("libwebviewchromium_plat_support.so"),
    SYSTEM_LIB("libz.so"),
    // Apex/system after R.
    APEX_LIB("com.android.i18n", "libandroidicu.so"),
    APEX_LIB("com.android.i18n", "libicui18n.so"),
    APEX_LIB("com.android.i18n", "libicuuc.so"),
    // Apex/system on R (see http://b/161958857).
    APEX_LIB("com.android.art", "libicui18n.so"),
    APEX_LIB("com.android.art", "libicuuc.so"),
    APEX_LIB("com.android.art", "libnativehelper.so"),
    // Apex/system on Q.
    APEX_LIB("com.android.runtime", "libicui18n.so"),
    APEX_LIB("com.android.runtime", "libicuuc.so"),
  };

  static constexpr const char* kPathTranslationQ[][2] = {
    // Apps targeting below Q can open apex libraries by system path.
    SYSTEM_LIB("libicui18n.so"),
    SYSTEM_LIB("libicuuc.so"),
    SYSTEM_LIB("libneuralnetworks.so"),
  };

  static constexpr const char* kPathTranslationN[][2] = {
    // Apps targeting below N can open greylisted libraries.
    SYSTEM_LIB("libandroid_runtime.so"),
    SYSTEM_LIB("libbinder.so"),
    SYSTEM_LIB("libcrypto.so"),
    SYSTEM_LIB("libcutils.so"),
    SYSTEM_LIB("libexpat.so"),
    SYSTEM_LIB("libgui.so"),
    SYSTEM_LIB("libmedia.so"),
    SYSTEM_LIB("libnativehelper.so"),
    SYSTEM_LIB("libssl.so"),
    SYSTEM_LIB("libstagefright.so"),
    SYSTEM_LIB("libsqlite.so"),
    SYSTEM_LIB("libui.so"),
    SYSTEM_LIB("libutils.so"),
    SYSTEM_LIB("libvorbisidec.so"),
  };

  if (name == nullptr) {
    return false;
  }

  auto comparator = [name](auto p) { return strcmp(name, p[0]) == 0; };

  if (auto it = std::find_if(std::begin(kPathTranslation), std::end(kPathTranslation), comparator);
      it != std::end(kPathTranslation)) {
    *out_name = (*it)[1];
    return true;
  }

  if (get_application_target_sdk_version() < __ANDROID_API_Q__) {
    if (auto it =
            std::find_if(std::begin(kPathTranslationQ), std::end(kPathTranslationQ), comparator);
        it != std::end(kPathTranslationQ)) {
      *out_name = (*it)[1];
      return true;
    }
  }

  if (get_application_target_sdk_version() < __ANDROID_API_N__) {
    if (auto it =
            std::find_if(std::begin(kPathTranslationN), std::end(kPathTranslationN), comparator);
        it != std::end(kPathTranslationN)) {
      *out_name = (*it)[1];
      return true;
    }
  }

  return false;
}
