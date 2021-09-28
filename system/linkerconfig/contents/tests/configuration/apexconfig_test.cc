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

#include "linkerconfig/apexconfig.h"

#include <android-base/file.h>
#include <apex_manifest.pb.h>
#include <gtest/gtest.h>
#include <vector>

#include "configurationtest.h"
#include "linkerconfig/apex.h"
#include "linkerconfig/configwriter.h"
#include "mockenv.h"

using android::linkerconfig::modules::ApexInfo;

namespace {
struct ApexConfigTest : ::testing::Test {
  TemporaryDir tmp_dir;
  std::string apex_root;

  void SetUp() override {
    apex_root = tmp_dir.path + std::string("/");
    MockGenericVariables();
  }

  ApexInfo PrepareApex(std::string apex_name,
                       std::vector<std::string> provided_libs,
                       std::vector<std::string> required_libs) {
    ::apex::proto::ApexManifest manifest;
    manifest.set_name(apex_name);
    for (auto lib : provided_libs) {
      manifest.add_providenativelibs(lib);
    }
    for (auto lib : required_libs) {
      manifest.add_requirenativelibs(lib);
    }
    WriteFile(apex_name + "/apex_manifest.pb", manifest.SerializeAsString());
    return android::linkerconfig::modules::ApexInfo(
        manifest.name(),
        tmp_dir.path,
        {manifest.providenativelibs().begin(),
         manifest.providenativelibs().end()},
        {manifest.requirenativelibs().begin(),
         manifest.requirenativelibs().end()},
        true,
        true);
  }

  void Mkdir(std::string dir_path) {
    if (access(dir_path.c_str(), F_OK) == 0) return;
    Mkdir(android::base::Dirname(dir_path));
    ASSERT_NE(-1, mkdir(dir_path.c_str(), 0755) == -1)
        << "Failed to create a directory: " << dir_path;
  }

  void WriteFile(std::string file, std::string content) {
    std::string file_path = apex_root + file;
    Mkdir(::android::base::Dirname(file_path));
    ASSERT_TRUE(::android::base::WriteStringToFile(content, file_path))
        << "Failed to write a file: " << file_path;
  }
};
}  // namespace

TEST_F(ApexConfigTest, apex_no_dependency) {
  android::linkerconfig::contents::Context ctx;
  auto target_apex = PrepareApex("target", {}, {});
  auto config = android::linkerconfig::contents::CreateApexConfiguration(
      ctx, target_apex);

  android::linkerconfig::modules::ConfigWriter config_writer;
  config.WriteConfig(config_writer);

  VerifyConfiguration(config_writer.ToString());
}

TEST_F(ApexConfigTest, apex_with_required) {
  android::linkerconfig::contents::Context ctx;
  ctx.AddApexModule(PrepareApex("foo", {"a.so"}, {"b.so"}));
  ctx.AddApexModule(PrepareApex("bar", {"b.so"}, {}));
  ctx.AddApexModule(PrepareApex("baz", {"c.so"}, {"a.so"}));
  auto target_apex = PrepareApex("target", {}, {"a.so", "b.so"});
  auto config = android::linkerconfig::contents::CreateApexConfiguration(
      ctx, target_apex);

  android::linkerconfig::modules::ConfigWriter config_writer;
  config.WriteConfig(config_writer);

  VerifyConfiguration(config_writer.ToString());
}