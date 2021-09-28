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

#include <android-base/test_utils.h>
#include <gtest/gtest.h>

#include "ApiChecker.h"
#include "Common.h"

namespace {

constexpr const char* kLatestApi =
    R"(
props {
    owner: Platform
    module: "android.all_dep"
    prop {
        api_name: "dep1"
        type: Integer
        scope: Public
        access: ReadWrite
        prop_name: "dep1_int"
        deprecated: true
    }
}
props {
    owner: Platform
    module: "android.platprop"
    prop {
        api_name: "prop1"
        type: Long
        scope: Public
        access: ReadWrite
        prop_name: "prop1"
    }
    prop {
        api_name: "prop2"
        type: String
        scope: Internal
        access: Readonly
        prop_name: "ro.prop2"
    }
    prop {
        api_name: "prop3"
        type: Boolean
        scope: Public
        access: ReadWrite
        prop_name: "ctl.start$prop3"
    }
    prop {
        api_name: "prop4"
        type: String
        scope: Public
        access: Readonly
        prop_name: "ro.prop4"
    }
}
)";

constexpr const char* kCurrentApi =
    R"(
props {
    owner: Platform
    module: "android.platprop"
    prop {
        api_name: "prop1"
        type: Long
        scope: Public
        access: ReadWrite
        prop_name: "prop1"
    }
    prop {
        api_name: "prop2"
        type: Integer
        scope: Public
        access: Writeonce
        prop_name: "ro.public.prop2"
    }
    prop {
        api_name: "prop3"
        type: Boolean
        scope: Public
        access: ReadWrite
        prop_name: "ctl.start$prop3"
    }
    prop {
        api_name: "prop4"
        type: String
        scope: Public
        access: Readonly
        prop_name: "ro.prop4"
        deprecated: true
    }

}
)";

constexpr const char* kInvalidCurrentApi =
    R"(
props {
    owner: Platform
    module: "android.platprop"
    prop {
        api_name: "prop2"
        type: Double
        scope: Public
        access: Readonly
        prop_name: "ro.prop2.a"
    }
    prop {
        api_name: "prop3"
        type: Boolean
        scope: Internal
        access: Readonly
        integer_as_bool: true,
        prop_name: "ctl.start$prop3"
    }
    prop {
        api_name: "prop4"
        type: Boolean
        scope: Internal
        access: ReadWrite
        prop_name: "prop4"
    }
}
)";

}  // namespace

TEST(SyspropTest, ApiCheckerTest) {
  TemporaryFile latest_file;
  close(latest_file.fd);
  latest_file.fd = -1;
  ASSERT_TRUE(android::base::WriteStringToFile(kLatestApi, latest_file.path));

  std::string err;
  auto latest_api = ParseApiFile(latest_file.path);
  ASSERT_RESULT_OK(latest_api);

  TemporaryFile current_file;
  close(current_file.fd);
  current_file.fd = -1;
  ASSERT_TRUE(android::base::WriteStringToFile(kCurrentApi, current_file.path));

  auto current_api = ParseApiFile(current_file.path);
  ASSERT_RESULT_OK(current_api);
  EXPECT_RESULT_OK(CompareApis(*latest_api, *current_api));

  TemporaryFile invalid_current_file;
  close(invalid_current_file.fd);
  invalid_current_file.fd = -1;
  ASSERT_TRUE(android::base::WriteStringToFile(kInvalidCurrentApi,
                                               invalid_current_file.path));

  auto invalid_current_api = ParseApiFile(invalid_current_file.path);
  ASSERT_RESULT_OK(invalid_current_api);

  auto res = CompareApis(*latest_api, *invalid_current_api);
  EXPECT_FALSE(res.ok());

  EXPECT_EQ(res.error().message(),
            "Prop prop1 has been removed\n"
            "Accessibility of prop prop3 has become more restrictive\n"
            "Scope of prop prop3 has become more restrictive\n"
            "Integer-as-bool of prop prop3 has been changed\n"
            "Type of prop prop4 has been changed\n"
            "Scope of prop prop4 has become more restrictive\n"
            "Underlying property of prop prop4 has been changed\n");
}
