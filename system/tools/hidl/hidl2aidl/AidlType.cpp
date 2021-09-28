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

#include <hidl-util/FQName.h>
#include <string>

#include "AidlHelper.h"
#include "FmqType.h"
#include "NamedType.h"
#include "Type.h"
#include "VectorType.h"

namespace android {

static std::string getPlaceholderType(const std::string& type) {
    return "IBinder /* FIXME: " + type + " */";
}

std::string AidlHelper::getAidlType(const Type& type, const FQName& relativeTo) {
    if (type.isVector()) {
        const VectorType& vec = static_cast<const VectorType&>(type);
        const Type* elementType = vec.getElementType();

        // Aidl doesn't support List<*> for C++ and NDK backends
        return getAidlType(*elementType, relativeTo) + "[]";
    } else if (type.isNamedType()) {
        const NamedType& namedType = static_cast<const NamedType&>(type);
        if (getAidlPackage(relativeTo) == getAidlPackage(namedType.fqName())) {
            return getAidlName(namedType.fqName());
        } else {
            return getAidlFQName(namedType.fqName());
        }
    } else if (type.isMemory()) {
        return getPlaceholderType("memory");
    } else if (type.isFmq()) {
        const FmqType& fmq = static_cast<const FmqType&>(type);
        return getPlaceholderType(fmq.templatedTypeName() + "<" +
                                  getAidlType(*fmq.getElementType(), relativeTo) + ">");
    } else if (type.isPointer()) {
        return getPlaceholderType("pointer");
    } else if (type.isEnum()) {
        // enum type goes to the primitive java type in HIDL, but AIDL should use
        // the enum type name itself
        return type.definedName();
    } else {
        return type.getJavaType();
    }
}

}  // namespace android
