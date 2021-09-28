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

#define LOG_TAG "bt_headless_sdp"

#include <future>

#include "base/logging.h"     // LOG() stdout and android log
#include "osi/include/log.h"  // android log only
#include "stack/include/sdp_api.h"
#include "test/headless/get_options.h"
#include "test/headless/headless.h"
#include "test/headless/nop/nop.h"
#include "types/raw_address.h"

int bluetooth::test::headless::Nop::Run() {
  return RunOnHeadlessStack<int>([this]() {
    fprintf(stdout, "Nop loop:%lu\n", loop_);
    return 0;
  });
}
