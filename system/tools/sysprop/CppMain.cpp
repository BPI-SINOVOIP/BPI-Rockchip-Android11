/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "sysprop_cpp"

#include <android-base/logging.h>
#include <android-base/result.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <getopt.h>

#include "CppGen.h"

using android::base::Result;

namespace {

struct Arguments {
  std::string input_file_path;
  std::string header_dir;
  std::string public_header_dir;
  std::string source_dir;
  std::string include_name;
};

[[noreturn]] void PrintUsage(const char* exe_name) {
  std::printf(
      "Usage: %s --header-dir dir --source-dir dir "
      "--include-name name --public-header-dir dir "
      "sysprop_file\n",
      exe_name);
  std::exit(EXIT_FAILURE);
}

Result<Arguments> ParseArgs(int argc, char* argv[]) {
  Arguments ret;
  for (;;) {
    static struct option long_options[] = {
        {"header-dir", required_argument, 0, 'h'},
        {"public-header-dir", required_argument, 0, 'p'},
        {"source-dir", required_argument, 0, 'c'},
        {"include-name", required_argument, 0, 'n'},
    };

    int opt = getopt_long_only(argc, argv, "", long_options, nullptr);
    if (opt == -1) break;

    switch (opt) {
      case 'h':
        ret.header_dir = optarg;
        break;
      case 'p':
        ret.public_header_dir = optarg;
        break;
      case 'c':
        ret.source_dir = optarg;
        break;
      case 'n':
        ret.include_name = optarg;
        break;
      default:
        PrintUsage(argv[0]);
    }
  }

  if (optind >= argc) {
    return Errorf("No input file specified");
  }

  if (optind + 1 < argc) {
    return Errorf("More than one input file");
  }

  if (ret.header_dir.empty() || ret.public_header_dir.empty() ||
      ret.source_dir.empty() || ret.include_name.empty()) {
    PrintUsage(argv[0]);
  }

  ret.input_file_path = argv[optind];

  return ret;
}

}  // namespace

int main(int argc, char* argv[]) {
  Arguments args;
  if (auto res = ParseArgs(argc, argv); res.ok()) {
    args = std::move(*res);
  } else {
    LOG(ERROR) << argv[0] << ": " << res.error();
    PrintUsage(argv[0]);
  }

  if (auto res = GenerateCppFiles(args.input_file_path, args.header_dir,
                                  args.public_header_dir, args.source_dir,
                                  args.include_name);
      !res.ok()) {
    LOG(FATAL) << "Error during generating cpp sysprop from "
               << args.input_file_path << ": " << res.error();
  }
}
