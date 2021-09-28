/*
 * Copyright (C) 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aidl.h"
#include "aidl_checkapi.h"
#include "io_delegate.h"
#include "logging.h"
#include "options.h"

#include <iostream>

#ifdef AIDL_CPP_BUILD
constexpr Options::Language kDefaultLang = Options::Language::CPP;
#else
constexpr Options::Language kDefaultLang = Options::Language::JAVA;
#endif

using android::aidl::Options;

int process_options(const Options& options) {
  android::aidl::IoDelegate io_delegate;
  switch (options.GetTask()) {
    case Options::Task::COMPILE:
      return android::aidl::compile_aidl(options, io_delegate);
    case Options::Task::PREPROCESS:
      return android::aidl::preprocess_aidl(options, io_delegate) ? 0 : 1;
    case Options::Task::DUMP_API:
      return android::aidl::dump_api(options, io_delegate) ? 0 : 1;
    case Options::Task::CHECK_API:
      return android::aidl::check_api(options, io_delegate) ? 0 : 1;
    case Options::Task::DUMP_MAPPINGS:
      return android::aidl::dump_mappings(options, io_delegate) ? 0 : 1;
    default:
      LOG(FATAL) << "aidl: internal error" << std::endl;
      return 1;
  }
}

int main(int argc, char* argv[]) {
  android::base::InitLogging(argv);
  LOG(DEBUG) << "aidl starting";

  Options options(argc, argv, kDefaultLang);
  if (!options.Ok()) {
    std::cerr << options.GetErrorMessage();
    std::cerr << options.GetUsage();
    return 1;
  }

  int ret = process_options(options);

  // compiler invariants

  // once AIDL_ERROR/AIDL_FATAL are used everywhere instead of std::cerr/LOG, we
  // can make this assertion in both directions.
  if (ret == 0) {
    AIDL_FATAL_IF(AidlError::hadError(), "Compiler success, but error emitted");
  }

  return ret;
}
