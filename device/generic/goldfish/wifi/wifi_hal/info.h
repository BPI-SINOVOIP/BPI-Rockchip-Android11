/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <functional>
#include <vector>
#include <hardware_legacy/wifi_hal.h>
#include "interface.h"
#include "netlink.h"

class Info {
public:
    using StopHandler = std::function<void ()>;
    Info();

    bool init();
    void eventLoop();
    void stop(StopHandler stopHandler);
    wifi_error getInterfaces(int* num, wifi_interface_handle** interfaces);

private:
    Netlink mNetlink;
    std::vector<wifi_interface_handle> mInterfaceHandles;
    std::vector<Interface> mInterfaces;
};

