/*
 * Copyright 2020 The Android Open Source Project
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

#include <unordered_map>

#include <unistd.h>

#include "base/logging.h"  // LOG() stdout and android log
#include "test/headless/get_options.h"

namespace bluetooth {
namespace test {
namespace headless {

namespace {

template <typename T>
using ExecutionUnit = std::function<T()>;

constexpr char kHeadlessStartSentinel[] =
    " START HEADLESS HEADLESS HEADLESS HEADLESS HEADLESS HEADLESS HEADLESS "
    "HEADLESS";
constexpr char kHeadlessStopSentinel[] =
    " STOP HEADLESS HEADLESS HEADLESS HEADLESS HEADLESS HEADLESS HEADLESS "
    "HEADLESS";

}  // namespace

class HeadlessStack {
 protected:
  HeadlessStack() = default;
  virtual ~HeadlessStack() = default;

  void SetUp();
  void TearDown();
};

class HeadlessRun : public HeadlessStack {
 protected:
  const bluetooth::test::headless::GetOpt& options_;
  unsigned long loop_{0};

  HeadlessRun(const bluetooth::test::headless::GetOpt& options)
      : options_(options) {}

  template <typename T>
  T RunOnHeadlessStack(ExecutionUnit<T> func) {
    SetUp();
    LOG(INFO) << kHeadlessStartSentinel;

    T rc;
    for (loop_ = 0; loop_ < options_.loop_; loop_++) {
      rc = func();
      if (options_.msec_ != 0) {
        usleep(options_.msec_ * 1000);
      }
      if (rc) {
        break;
      }
    }
    if (rc) {
      LOG(ERROR) << "FAIL:" << rc << " loop/loops:" << loop_ << "/"
                 << options_.loop_;
    } else {
      LOG(INFO) << "PASS:" << rc << " loop/loops:" << loop_ << "/"
                << options_.loop_;
    }

    LOG(INFO) << kHeadlessStopSentinel;
    TearDown();
    return rc;
  }
  virtual ~HeadlessRun() = default;
};

template <typename T>
class HeadlessTest : public HeadlessRun {
 public:
  virtual T Run() {
    if (options_.non_options_.size() == 0) {
      fprintf(stdout, "Must supply at least one subtest name\n");
      return -1;
    }

    std::string subtest = options_.GetNextSubTest();
    if (test_nodes_.find(subtest) == test_nodes_.end()) {
      fprintf(stdout, "Unknown subtest module:%s\n", subtest.c_str());
      return -1;
    }
    return test_nodes_.at(subtest)->Run();
  }

  virtual ~HeadlessTest() = default;

 protected:
  HeadlessTest(const bluetooth::test::headless::GetOpt& options)
      : HeadlessRun(options) {}

  std::unordered_map<std::string, std::unique_ptr<HeadlessTest<T>>> test_nodes_;
};

}  // namespace headless
}  // namespace test
}  // namespace bluetooth
