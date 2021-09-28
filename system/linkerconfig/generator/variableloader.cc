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

#include "linkerconfig/variableloader.h"

#include <android-base/result.h>
#include <android-base/strings.h>

#include <climits>
#include <cstdlib>
#include <cstring>

#include "linkerconfig/environment.h"
#include "linkerconfig/librarylistloader.h"
#include "linkerconfig/log.h"
#include "linkerconfig/stringutil.h"
#include "linkerconfig/variables.h"

using android::base::Result;
using android::linkerconfig::modules::GetProductVndkVersion;
using android::linkerconfig::modules::GetVendorVndkVersion;
using android::linkerconfig::modules::TrimPrefix;
using android::linkerconfig::modules::Variables;

namespace {
using namespace android::linkerconfig::generator;

// Stub libraries are list of libraries which has stub interface and installed
// in system image so other partition and APEX modules can link to it.
// TODO(b/147210213) : Generate this list on build and read from the file
std::vector<std::string> stub_libraries = {
    "libEGL.so",
    "libGLESv1_CM.so",
    "libGLESv2.so",
    "libGLESv3.so",
    "libRS.so",
    "libaaudio.so",
    "libadbd_auth.so",
    "libadbd_fs.so",
    "libandroid.so",
    "libandroid_net.so",
    "libbinder_ndk.so",
    "libc.so",
    "libcgrouprc.so",
    "libclang_rt.asan-arm-android.so",
    "libclang_rt.asan-i686-android.so",
    "libclang_rt.asan-x86_64-android.so",
    "libdl.so",
    "libdl_android.so",
    "libft2.so",
    "libincident.so",
    "liblog.so",
    "libm.so",
    "libmediametrics.so",
    "libmediandk.so",
    "libnativewindow.so",
    "libneuralnetworks_packageinfo.so",
    "libsync.so",
    "libvndksupport.so",
    "libvulkan.so",
    "libselinux.so",
};

void LoadVndkVersionVariable() {
  Variables::AddValue("VENDOR_VNDK_VERSION", GetVendorVndkVersion());
  Variables::AddValue("PRODUCT_VNDK_VERSION", GetProductVndkVersion());
}

Result<std::string> GetRealPath(std::string target_path) {
  char resolved_path[PATH_MAX];
  if (realpath(target_path.c_str(), resolved_path) != nullptr) {
    return resolved_path;
  }

  return ErrnoErrorf("Failed to get realpath from {}", target_path);
}

void LoadVariableFromPartitionPath(const std::string& root,
                                   std::string variable_name,
                                   std::string partition) {
  auto real_path = GetRealPath(root + partition);

  if (real_path.ok()) {
    Variables::AddValue(variable_name, TrimPrefix(*real_path, root));
  } else {
    LOG(WARNING) << real_path.error();
    Variables::AddValue(variable_name, partition);
  }
}

void LoadPartitionPathVariables(const std::string& root) {
  // TODO(b/141714913): generalize path handling
  LoadVariableFromPartitionPath(root, "PRODUCT", "/product");
  LoadVariableFromPartitionPath(root, "SYSTEM_EXT", "/system_ext");
}

void LoadVndkLibraryListVariables(const std::string& root,
                                  const std::string& vndk_version,
                                  const std::string& partition) {
  if (vndk_version == "") {
    return;
  }
  const std::string vndk_path = root + "/apex/com.android.vndk.v" + vndk_version;
  // Skip loading if VNDK APEX is not available
  if (::access(vndk_path.c_str(), F_OK) != 0) {
    return;
  }
  const std::string llndk_libraries_path =
      vndk_path + "/etc/llndk.libraries." + vndk_version + ".txt";
  const std::string vndksp_libraries_path =
      vndk_path + "/etc/vndksp.libraries." + vndk_version + ".txt";
  const std::string vndkcore_libraries_path =
      vndk_path + "/etc/vndkcore.libraries." + vndk_version + ".txt";
  const std::string vndkprivate_libraries_path =
      vndk_path + "/etc/vndkprivate.libraries." + vndk_version + ".txt";
  const std::string sanitizer_library_path =
      root + "/system/etc/sanitizer.libraries.txt";

  Variables::AddValue("LLNDK_LIBRARIES_" + partition,
                      GetPublicLibrariesString(llndk_libraries_path,
                                               vndkprivate_libraries_path));

  Variables::AddValue("PRIVATE_LLNDK_LIBRARIES_" + partition,
                      GetPrivateLibrariesString(llndk_libraries_path,
                                                vndkprivate_libraries_path));

  Variables::AddValue("VNDK_SAMEPROCESS_LIBRARIES_" + partition,
                      GetPublicLibrariesString(vndksp_libraries_path,
                                               vndkprivate_libraries_path));

  Variables::AddValue("VNDK_CORE_LIBRARIES_" + partition,
                      GetPublicLibrariesString(vndkcore_libraries_path,
                                               vndkprivate_libraries_path));

  Variables::AddValue("SANITIZER_DEFAULT_" + partition,
                      GetPublicLibrariesString(sanitizer_library_path,
                                               vndkcore_libraries_path));

  if (partition == "VENDOR") {
    auto vndkcorevariant_library_path =
        root + "/system/etc/vndkcorevariant.libraries.txt";
    Variables::AddValue("VNDK_USING_CORE_VARIANT_LIBRARIES",
                        GetPublicLibrariesString(vndkcorevariant_library_path,
                                                 vndkprivate_libraries_path));
  }
}

void LoadLibraryListVariables(const std::string& root) {
  LoadVndkLibraryListVariables(root, GetVendorVndkVersion(), "VENDOR");
  LoadVndkLibraryListVariables(root, GetProductVndkVersion(), "PRODUCT");

  auto sanitizer_library_path = root + "/system/etc/sanitizer.libraries.txt";
  Variables::AddValue("SANITIZER_RUNTIME_LIBRARIES",
                      GetLibrariesString(sanitizer_library_path));

  Variables::AddValue("STUB_LIBRARIES",
                      android::base::Join(stub_libraries, ':'));
}
}  // namespace

namespace android {
namespace linkerconfig {
namespace generator {

void LoadVariables(const std::string& root) {
  LoadVndkVersionVariable();
  LoadPartitionPathVariables(root);
  LoadLibraryListVariables(root);
}

}  // namespace generator
}  // namespace linkerconfig
}  // namespace android
