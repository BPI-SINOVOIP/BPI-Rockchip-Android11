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

#define LOG_TAG "sysprop_java"

#include <android-base/logging.h>
#include <android-base/result.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <getopt.h>

#include "JavaGen.h"
#include "sysprop.pb.h"

using android::base::Result;

namespace {

struct Arguments {
  std::string input_file_path;
  std::string java_output_dir;
  sysprop::Scope scope;
};

[[noreturn]] void PrintUsage(const char* exe_name) {
  std::printf(
      "Usage: %s --scope (internal|public) --java-output-dir dir "
      "sysprop_file\n",
      exe_name);
  std::exit(EXIT_FAILURE);
}

Result<void> ParseArgs(int argc, char* argv[], Arguments* args) {
  for (;;) {
    static struct option long_options[] = {
        {"java-output-dir", required_argument, 0, 'j'},
        {"scope", required_argument, 0, 's'},
    };

    int opt = getopt_long_only(argc, argv, "", long_options, nullptr);
    if (opt == -1) break;

    switch (opt) {
      case 'j':
        args->java_output_dir = optarg;
        break;
      case 's':
        if (strcmp(optarg, "public") == 0) {
          args->scope = sysprop::Scope::Public;
        } else if (strcmp(optarg, "internal") == 0) {
          args->scope = sysprop::Scope::Internal;
        } else {
          return Errorf("Invalid option {} for scope", optarg);
        }
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

  args->input_file_path = argv[optind];
  if (args->java_output_dir.empty()) args->java_output_dir = ".";

  return {};
}

}  // namespace

int main(int argc, char* argv[]) {
  Arguments args;
  std::string err;
  if (auto res = ParseArgs(argc, argv, &args); !res.ok()) {
    LOG(ERROR) << res.error();
    PrintUsage(argv[0]);
  }

  if (auto res = GenerateJavaLibrary(args.input_file_path, args.scope,
                                     args.java_output_dir);
      !res.ok()) {
    LOG(FATAL) << "Error during generating java sysprop from "
               << args.input_file_path << ": " << res.error();
  }
}
