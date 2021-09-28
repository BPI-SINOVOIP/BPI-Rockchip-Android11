/*
 * Copyright 2019 The Android Open Source Project
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

#include <cstdint>

#define LOG_TAG "bt_shim"

#include "common/message_loop_thread.h"
#include "main/shim/entry.h"
#include "main/shim/shim.h"
#include "osi/include/log.h"
#include "osi/include/properties.h"

static const char* kPropertyKey = "bluetooth.gd.enabled";

static bluetooth::common::MessageLoopThread bt_shim_thread("bt_shim_thread");

static bool gd_shim_enabled_ = false;
static bool gd_shim_property_checked_ = false;
static bool gd_stack_started_up_ = false;

future_t* ShimModuleStartUp() {
  bt_shim_thread.StartUp();
  CHECK(bt_shim_thread.IsRunning())
      << "Unable to start bt shim message loop thread.";
  module_start_up(get_module(GD_SHIM_BTM_MODULE));
  bluetooth::shim::StartGabeldorscheStack();
  gd_stack_started_up_ = true;
  return kReturnImmediate;
}

future_t* ShimModuleShutDown() {
  gd_stack_started_up_ = false;
  bluetooth::shim::StopGabeldorscheStack();
  module_shut_down(get_module(GD_SHIM_BTM_MODULE));
  bt_shim_thread.ShutDown();
  return kReturnImmediate;
}

EXPORT_SYMBOL extern const module_t gd_shim_module = {
    .name = GD_SHIM_MODULE,
    .init = kUnusedModuleApi,
    .start_up = ShimModuleStartUp,
    .shut_down = ShimModuleShutDown,
    .clean_up = kUnusedModuleApi,
    .dependencies = {kUnusedModuleDependencies}};

void bluetooth::shim::Post(base::OnceClosure task) {
  bt_shim_thread.DoInThread(FROM_HERE, std::move(task));
}

bool bluetooth::shim::is_gd_shim_enabled() {
  if (!gd_shim_property_checked_) {
    gd_shim_property_checked_ = true;
    gd_shim_enabled_ = osi_property_get_bool(kPropertyKey, false);
  }
  return gd_shim_enabled_;
}

bool bluetooth::shim::is_gd_stack_started_up() { return gd_stack_started_up_; }
