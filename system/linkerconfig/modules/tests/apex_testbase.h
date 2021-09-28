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
#pragma once

#include <android-base/file.h>
#include <apex_manifest.pb.h>
#include <gtest/gtest.h>

#include "linkerconfig/apex.h"

#include <iostream>

struct ApexTest : ::testing::Test {
  TemporaryDir tmp_dir;
  std::string root;

  void SetUp() override {
    root = tmp_dir.path;
  }

  android::linkerconfig::modules::ApexInfo PrepareApex(
      std::string apex_name, std::vector<std::string> provided_libs,
      std::vector<std::string> required_libs) {
    ::apex::proto::ApexManifest manifest;
    manifest.set_name(apex_name);
    for (auto lib : provided_libs) {
      manifest.add_providenativelibs(lib);
    }
    for (auto lib : required_libs) {
      manifest.add_requirenativelibs(lib);
    }
    const auto apex_path = "/apex/" + apex_name;
    WriteFile(apex_path + "/apex_manifest.pb", manifest.SerializeAsString());
    return android::linkerconfig::modules::ApexInfo(
        manifest.name(),
        apex_path,
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
    std::cout << "mkdir(" + dir_path + ")\n";
    ASSERT_NE(-1, mkdir(dir_path.c_str(), 0755) == -1)
        << "Failed to create a directory: " << dir_path;
  }

  void WriteFile(std::string file, std::string content) {
    std::string file_path = root + file;
    Mkdir(::android::base::Dirname(file_path));
    std::cout << "writeFile(" + file_path + ")\n";
    ASSERT_TRUE(::android::base::WriteStringToFile(content, file_path))
        << "Failed to write a file: " << file_path;
  }
};