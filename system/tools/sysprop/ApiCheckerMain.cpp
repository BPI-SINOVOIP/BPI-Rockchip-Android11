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

#define LOG_TAG "sysprop_api_checker_main"

#include <android-base/logging.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <getopt.h>

#include "ApiChecker.h"
#include "Common.h"

namespace {

[[noreturn]] void PrintUsage(const char* exe_name) {
  std::printf("Usage: %s latest-file current-file\n", exe_name);
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::fprintf(stderr, "%s needs 2 arguments\n", argv[0]);
    PrintUsage(argv[0]);
  }

  sysprop::SyspropLibraryApis latest, current;

  if (auto res = ParseApiFile(argv[1]); res.ok()) {
    latest = std::move(*res);
  } else {
    LOG(FATAL) << "parsing sysprop_library API file " << argv[1]
               << " failed: " << res.error();
  }

  if (auto res = ParseApiFile(argv[2]); res.ok()) {
    current = std::move(*res);
  } else {
    LOG(FATAL) << "parsing sysprop_library API file " << argv[2]
               << " failed: " << res.error();
  }

  if (auto res = CompareApis(latest, current); !res.ok()) {
    LOG(ERROR) << "sysprop_library API check failed:\n" << res.error();
    return EXIT_FAILURE;
  }
}
