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

#include <gtest/gtest.h>

#include "linkerconfig/apex.h"
#include "linkerconfig/baseconfig.h"
#include "linkerconfig/variables.h"
#include "testbase.h"

using android::linkerconfig::contents::Context;
using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::AsanPath;

struct linkerconfig_vndklite_backward_compatibility : ::testing::Test {
  void SetUp() override {
    MockVariables();
    MockVnkdLite();
    ApexInfo vndk_apex;
    vndk_apex.name = "com.android.vndk.vQ";
    ctx.AddApexModule(vndk_apex);
  }
  Context ctx;
};

TEST_F(linkerconfig_vndklite_backward_compatibility, system_section) {
  auto config = android::linkerconfig::contents::CreateBaseConfiguration(ctx);

  auto system_section = config.GetSection("system");
  ASSERT_TRUE(system_section);

  auto default_namespace = system_section->GetNamespace("default");
  ASSERT_TRUE(default_namespace);

  EXPECT_TRUE(default_namespace->ContainsSearchPath("/vendor/${LIB}",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/odm/${LIB}",
                                                    AsanPath::WITH_DATA_ASAN));

  auto sphal_namespace = system_section->GetNamespace("sphal");
  ASSERT_TRUE(sphal_namespace);

  EXPECT_TRUE(sphal_namespace->ContainsSearchPath("/odm/${LIB}",
                                                  AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(sphal_namespace->ContainsSearchPath("/vendor/${LIB}",
                                                  AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(
      sphal_namespace->ContainsSearchPath("/vendor/${LIB}/hw", AsanPath::NONE));

  EXPECT_TRUE(sphal_namespace->ContainsPermittedPath("/odm/${LIB}",
                                                     AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(sphal_namespace->ContainsPermittedPath("/vendor/${LIB}",
                                                     AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(sphal_namespace->ContainsPermittedPath("/system/vendor/${LIB}",
                                                     AsanPath::NONE));

  auto rs_namespace = system_section->GetNamespace("rs");
  ASSERT_TRUE(rs_namespace);

  EXPECT_TRUE(rs_namespace->ContainsSearchPath("/odm/${LIB}/vndk-sp",
                                               AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(rs_namespace->ContainsSearchPath("/vendor/${LIB}/vndk-sp",
                                               AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(rs_namespace->ContainsSearchPath("/odm/${LIB}",
                                               AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(rs_namespace->ContainsSearchPath("/vendor/${LIB}",
                                               AsanPath::WITH_DATA_ASAN));

  EXPECT_TRUE(rs_namespace->ContainsPermittedPath("/odm/${LIB}",
                                                  AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(rs_namespace->ContainsPermittedPath("/vendor/${LIB}",
                                                  AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(rs_namespace->ContainsPermittedPath("/system/vendor/${LIB}",
                                                  AsanPath::NONE));

  auto vndk_namespace = system_section->GetNamespace("vndk");
  ASSERT_TRUE(vndk_namespace);

  EXPECT_TRUE(vndk_namespace->ContainsSearchPath("/odm/${LIB}/vndk-sp",
                                                 AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(vndk_namespace->ContainsSearchPath("/vendor/${LIB}/vndk-sp",
                                                 AsanPath::WITH_DATA_ASAN));

  EXPECT_TRUE(vndk_namespace->ContainsPermittedPath("/odm/${LIB}/hw",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(vndk_namespace->ContainsPermittedPath("/odm/${LIB}/egl",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(vndk_namespace->ContainsPermittedPath("/vendor/${LIB}/hw",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(vndk_namespace->ContainsPermittedPath("/vendor/${LIB}/egl",
                                                    AsanPath::WITH_DATA_ASAN));
}

TEST_F(linkerconfig_vndklite_backward_compatibility, vendor_section) {
  auto config = android::linkerconfig::contents::CreateBaseConfiguration(ctx);

  auto vendor_section = config.GetSection("vendor");
  ASSERT_TRUE(vendor_section);

  auto default_namespace = vendor_section->GetNamespace("default");
  ASSERT_TRUE(default_namespace);

  EXPECT_TRUE(default_namespace->ContainsSearchPath("/odm/${LIB}",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/odm/${LIB}/vndk",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/odm/${LIB}/vndk-sp",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/vendor/${LIB}",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/vendor/${LIB}/vndk",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/vendor/${LIB}/vndk-sp",
                                                    AsanPath::WITH_DATA_ASAN));
}

TEST_F(linkerconfig_vndklite_backward_compatibility, unrestricted_section) {
  auto config = android::linkerconfig::contents::CreateBaseConfiguration(ctx);

  auto unrestricted_section = config.GetSection("unrestricted");
  ASSERT_TRUE(unrestricted_section);

  auto default_namespace = unrestricted_section->GetNamespace("default");
  ASSERT_TRUE(default_namespace);

  EXPECT_TRUE(default_namespace->ContainsSearchPath("/odm/${LIB}",
                                                    AsanPath::WITH_DATA_ASAN));
  EXPECT_TRUE(default_namespace->ContainsSearchPath("/vendor/${LIB}",
                                                    AsanPath::WITH_DATA_ASAN));
}