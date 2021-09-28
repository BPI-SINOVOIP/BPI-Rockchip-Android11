/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "AST.h"

#include <android-base/logging.h>
#include <hidl-util/Formatter.h>
#include <json/json.h>
#include <string>
#include <vector>

#include "Interface.h"

namespace android {

void AST::generateInheritanceHierarchy(Formatter& out) const {
    const Interface* callingIface = mRootScope.getInterface();
    CHECK(callingIface != nullptr);

    Json::Value root;
    root["interface"] = callingIface->fqName().string();
    if (callingIface->superType()) {
        Json::Value inheritedInterfaces(Json::arrayValue);
        for (const Interface* iface : mRootScope.getInterface()->superTypeChain()) {
            if (iface->isIBase()) break;
            inheritedInterfaces.append(iface->fqName().string());
        }
        root["inheritedInterfaces"] = inheritedInterfaces;
    }
    Json::StyledWriter writer;
    out << writer.write(root);
}

}  // namespace android
