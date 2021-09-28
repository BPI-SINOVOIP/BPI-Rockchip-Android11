/*
 * Copyright 2019, The Android Open Source Project
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

#include "bridge_builder.h"

#include "bridge.h"
#include "log.h"
#include "result.h"

BridgeBuilder::BridgeBuilder(Bridge& bridge, const char* interfacePrefix) :
    mBridge(bridge),
    mInterfacePrefix(interfacePrefix),
    mPrefixLength(strlen(interfacePrefix)) {
}

void BridgeBuilder::onInterfaceState(unsigned int /*index*/,
                                     const char* name,
                                     InterfaceState state) {
    if (strncmp(name, mInterfacePrefix, mPrefixLength) != 0) {
        // Does not match our prefix, ignore it
        return;
    }

    if (state == InterfaceState::Up) {
        Result res = mBridge.addInterface(name);
        if (!res) {
            LOGE("%s", res.c_str());
        }
    }
}
