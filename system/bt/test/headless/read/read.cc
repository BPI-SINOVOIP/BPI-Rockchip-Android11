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

#include "test/headless/read/read.h"
#include "base/logging.h"     // LOG() stdout and android log
#include "osi/include/log.h"  // android log only
#include "test/headless/get_options.h"
#include "test/headless/headless.h"
#include "test/headless/read/name.h"

using namespace bluetooth::test::headless;

Read::Read(const bluetooth::test::headless::GetOpt& options)
    : HeadlessTest<int>(options) {
  test_nodes_.emplace(
      "name", std::make_unique<bluetooth::test::headless::Name>(options));
}
