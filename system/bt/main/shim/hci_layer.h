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

/**
 * Gd shim layer to legacy hci layer stack
 */
#pragma once

#include "hci/include/hci_layer.h"

static const char GD_HCI_MODULE[] = "gd_hci_module";

namespace bluetooth {
namespace shim {

const hci_t* hci_layer_get_interface();

}  // namespace shim
}  // namespace bluetooth
