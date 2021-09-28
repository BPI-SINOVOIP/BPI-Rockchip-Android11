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

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "linkerconfig/context.h"
#include "linkerconfig/namespace.h"
#include "linkerconfig/namespacebuilder.h"
#include "linkerconfig/sectionbuilder.h"

using namespace android::linkerconfig::contents;
using namespace android::linkerconfig::modules;

namespace {

typedef std::unordered_map<std::string, std::vector<std::string>> fsmap;

std::string Search(const Namespace& ns, std::string_view soname, fsmap fs) {
  // search for current search_paths
  for (auto path : ns.SearchPaths()) {
    auto libs = fs[path];
    if (std::find(std::begin(libs), std::end(libs), soname) != std::end(libs)) {
      return path;
    }
  }
  return "";
}

}  // namespace

TEST(vndk_namespace, vndk_ext) {
  Context vendor_context;
  vendor_context.SetCurrentSection(SectionType::Vendor);
  auto vndk_ns = BuildVndkNamespace(vendor_context, VndkUserPartition::Vendor);

  auto libvndk = "libvndk.so";
  auto libvndksp = "libvndksp.so";

  auto system_lib_path = "/system/${LIB}";
  auto vendor_lib_path = "/vendor/${LIB}";
  auto vendor_vndk_lib_path = "/vendor/${LIB}/vndk";
  auto vendor_vndksp_lib_path = "/vendor/${LIB}/vndk-sp";
  auto apex_vndk_lib_path =
      "/apex/com.android.vndk.v" + Var("VENDOR_VNDK_VERSION") + "/${LIB}";

  auto fs = fsmap{
      {system_lib_path, {libvndk, libvndksp}},
      {vendor_lib_path, {libvndk, libvndksp}},
      {apex_vndk_lib_path, {libvndk, libvndksp}},
  };

  EXPECT_EQ(apex_vndk_lib_path, Search(vndk_ns, libvndk, fs));
  EXPECT_EQ(apex_vndk_lib_path, Search(vndk_ns, libvndksp, fs));

  // vndk-ext can eclipse vndk
  fs[vendor_vndk_lib_path] = {libvndk};
  EXPECT_EQ(vendor_vndk_lib_path, Search(vndk_ns, libvndk, fs));
  EXPECT_EQ(apex_vndk_lib_path, Search(vndk_ns, libvndksp, fs));

  // likewise, vndk-sp-ext can eclipse vndk-sp
  fs[vendor_vndksp_lib_path] = {libvndksp};
  EXPECT_EQ(vendor_vndk_lib_path, Search(vndk_ns, libvndk, fs));
  EXPECT_EQ(vendor_vndksp_lib_path, Search(vndk_ns, libvndksp, fs));
}
