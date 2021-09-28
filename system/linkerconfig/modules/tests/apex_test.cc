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

#include <string>
#include <vector>

#include <android-base/file.h>
#include <apex_manifest.pb.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "apex_testbase.h"
#include "linkerconfig/apex.h"
#include "linkerconfig/basecontext.h"
#include "linkerconfig/configwriter.h"
#include "linkerconfig/namespace.h"
#include "linkerconfig/section.h"

using ::android::base::WriteStringToFile;
using ::android::linkerconfig::modules::ApexInfo;
using ::android::linkerconfig::modules::BaseContext;
using ::android::linkerconfig::modules::ConfigWriter;
using ::android::linkerconfig::modules::InitializeWithApex;
using ::android::linkerconfig::modules::Namespace;
using ::android::linkerconfig::modules::ScanActiveApexes;
using ::android::linkerconfig::modules::Section;
using ::apex::proto::ApexManifest;
using ::testing::Contains;

TEST(apex_namespace, build_namespace) {
  Namespace ns("foo");
  InitializeWithApex(ns,
                     ApexInfo("com.android.foo",
                              "/apex/com.android.foo",
                              {},
                              {},
                              /*has_bin=*/false,
                              /*has_lib=*/true));

  ConfigWriter writer;
  ns.WriteConfig(writer);
  ASSERT_EQ(
      "namespace.foo.isolated = false\n"
      "namespace.foo.search.paths = /apex/com.android.foo/${LIB}\n"
      "namespace.foo.permitted.paths = /apex/com.android.foo/${LIB}\n"
      "namespace.foo.permitted.paths += /system/${LIB}\n"
      "namespace.foo.asan.search.paths = /apex/com.android.foo/${LIB}\n"
      "namespace.foo.asan.permitted.paths = /apex/com.android.foo/${LIB}\n"
      "namespace.foo.asan.permitted.paths += /system/${LIB}\n",
      writer.ToString());
}

TEST(apex_namespace, resolve_between_apex_namespaces) {
  BaseContext ctx;
  Namespace foo("foo"), bar("bar");
  InitializeWithApex(foo,
                     ApexInfo("com.android.foo",
                              "/apex/com.android.foo",
                              {"foo.so"},
                              {"bar.so"},
                              /*has_bin=*/false,
                              /*has_lib=*/true));
  InitializeWithApex(bar,
                     ApexInfo("com.android.bar",
                              "/apex/com.android.bar",
                              {"bar.so"},
                              {},
                              /*has_bin=*/false,
                              /*has_lib=*/true));

  std::vector<Namespace> namespaces;
  namespaces.push_back(std::move(foo));
  namespaces.push_back(std::move(bar));
  Section section("section", std::move(namespaces));

  auto result = section.Resolve(ctx);
  ASSERT_RESULT_OK(result);

  // See if two namespaces are linked correctly
  ASSERT_THAT(section.GetNamespace("foo")->GetLink("bar").GetSharedLibs(),
              Contains("bar.so"));
}

TEST_F(ApexTest, scan_apex_dir) {
  PrepareApex("foo", {}, {"bar.so"});
  WriteFile("/apex/foo/bin/foo", "");
  PrepareApex("bar", {"bar.so"}, {});
  WriteFile("/apex/bar/lib64/bar.so", "");

  auto apexes = ScanActiveApexes(root);
  ASSERT_EQ(2U, apexes.size());

  ASSERT_THAT(apexes["foo"].require_libs, Contains("bar.so"));
  ASSERT_TRUE(apexes["foo"].has_bin);
  ASSERT_FALSE(apexes["foo"].has_lib);

  ASSERT_THAT(apexes["bar"].provide_libs, Contains("bar.so"));
  ASSERT_FALSE(apexes["bar"].has_bin);
  ASSERT_TRUE(apexes["bar"].has_lib);
}
