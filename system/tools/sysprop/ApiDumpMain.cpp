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

#define LOG_TAG "sysprop_api_dump_main"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <google/protobuf/text_format.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "Common.h"

namespace {

[[noreturn]] void PrintUsage(const char* exe_name) {
  std::printf("Usage: %s output_file sysprop_files...\n", exe_name);
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::fprintf(stderr, "%s needs at least 2 arguments\n", argv[0]);
    PrintUsage(argv[0]);
  }

  sysprop::SyspropLibraryApis api;
  std::map<std::string, sysprop::Properties> modules;

  for (int i = 2; i < argc; ++i) {
    if (auto res = ParseProps(argv[i]); res.ok()) {
      if (!modules.emplace(res->module(), *res).second) {
        LOG(FATAL) << "duplicated module name " << res->module();
      }
    } else {
      LOG(FATAL) << "parsing sysprop file " << argv[i]
                 << " failed: " << res.error();
    }
  }

  for (auto& [name, props] : modules) {
    // Sort properties to normalize
    std::sort(props.mutable_prop()->begin(), props.mutable_prop()->end(),
              [](auto& a, auto& b) { return a.api_name() < b.api_name(); });
    *api.add_props() = std::move(props);
  }

  std::string res;
  if (!google::protobuf::TextFormat::PrintToString(api, &res)) {
    LOG(FATAL) << "dumping API failed";
  }

  if (!android::base::WriteStringToFile(res, argv[1])) {
    PLOG(FATAL) << "writing API file to " << argv[1] << " failed";
  }
}
