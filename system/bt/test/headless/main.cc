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

#define LOG_TAG "bt_headless"

#include <unordered_map>

#include "base/logging.h"     // LOG() stdout and android log
#include "osi/include/log.h"  // android log only
#include "test/headless/get_options.h"
#include "test/headless/headless.h"
#include "test/headless/nop/nop.h"
#include "test/headless/pairing/pairing.h"
#include "test/headless/read/read.h"
#include "test/headless/sdp/sdp.h"

using namespace bluetooth::test::headless;

namespace {

class Main : public HeadlessTest<int> {
 public:
  Main(const bluetooth::test::headless::GetOpt& options)
      : HeadlessTest<int>(options) {
    test_nodes_.emplace(
        "nop", std::make_unique<bluetooth::test::headless::Nop>(options));
    test_nodes_.emplace(
        "pairing",
        std::make_unique<bluetooth::test::headless::Pairing>(options));
    test_nodes_.emplace(
        "read", std::make_unique<bluetooth::test::headless::Read>(options));
    test_nodes_.emplace(
        "sdp", std::make_unique<bluetooth::test::headless::Sdp>(options));
  }

  int Run() override {
    if (options_.close_stderr_) {
      fclose(stderr);
    }
    return HeadlessTest<int>::Run();
  }
};

}  // namespace

int main(int argc, char** argv) {
  fflush(nullptr);
  setvbuf(stdout, nullptr, _IOLBF, 0);

  bluetooth::test::headless::GetOpt options(argc, argv);
  if (!options.IsValid()) {
    return -1;
  }

  Main main(options);
  return main.Run();
}
