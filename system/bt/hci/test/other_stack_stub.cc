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

/**
 * Gabeldorsche related legacy-only-stack-side expansion and support code.
 */
#include "base/bind.h"
#include "btcore/include/module.h"  // base::OnceClosure
#include "hci/include/btsnoop.h"
#include "hci/include/hci_layer.h"

const btsnoop_t* btsnoop_get_interface() { return nullptr; }
const packet_fragmenter_t* packet_fragmenter_get_interface() { return nullptr; }
base::MessageLoop* get_main_message_loop() { return nullptr; }

namespace bluetooth {
namespace bqr {

void DumpLmpLlMessage(uint8_t length, uint8_t* p_lmp_ll_message_event) {}
void DumpBtScheduling(unsigned char, unsigned char*) {}

}  // namespace bqr

namespace shim {

bool is_gd_shim_enabled() { return false; }
bool is_gd_stack_started_up() { return false; }
void Post(base::OnceClosure task) {}
const hci_t* hci_layer_get_interface() { return nullptr; }

}  // namespace shim
}  // namespace bluetooth
