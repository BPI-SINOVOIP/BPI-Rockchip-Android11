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

#pragma once

/**
 * Gabeldorsche related legacy-only-stack-side expansion and support code.
 */
#include "base/bind.h"
#include "btcore/include/module.h"
#include "main/shim/entry.h"
#include "osi/include/future.h"

static const char GD_SHIM_MODULE[] = "gd_shim_module";
static const char GD_SHIM_BTM_MODULE[] = "gd_shim_btm_module";

constexpr future_t* kReturnImmediate = nullptr;
constexpr module_lifecycle_fn kUnusedModuleApi = nullptr;
constexpr char* kUnusedModuleDependencies = nullptr;

namespace bluetooth {
namespace shim {

/**
 * Checks if the bluetooth stack is running in legacy or gd mode.
 *
 * This check is used throughout the legacy stack to determine which
 * methods, classes or functions to invoke.  The default (false) mode
 * is the legacy mode which runs the original legacy bluetooth stack.
 * When enabled (true) the core portion of the gd stack is invoked
 * at key points to execute equivalent functionality using the
 * gd core components.
 *
 * @return true if using gd shim core, false if using legacy.
 */
bool is_gd_shim_enabled();

/**
 * Checks if the bluetooth gd stack has been started up.
 *
 * @return true if bluetooth gd stack is started, false otherwise.
 */
bool is_gd_stack_started_up();

/**
 * Posts a task on the shim message queue.
 *
 * @param task Task to be posted onto the message queue.
 */
void Post(base::OnceClosure task);

}  // namespace shim
}  // namespace bluetooth
