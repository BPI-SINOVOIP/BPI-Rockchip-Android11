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

#include <android-base/logging.h>

#include <iostream>
#include <vector>

#include "AST.h"
#include "Interface.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Method.h"

namespace android {

enum InterfaceMethodType { NONE = 0, ONEWAY = 1, TWOWAY = 2, MIXED = ONEWAY | TWOWAY };

// To use InterfaceMethodType as a bitfield
static inline InterfaceMethodType operator|(InterfaceMethodType lhs, InterfaceMethodType rhs) {
    using T = std::underlying_type_t<InterfaceMethodType>;
    return static_cast<InterfaceMethodType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

// This function returns what kind of methods the interface contains
static InterfaceMethodType getInterfaceOnewayType(const Interface& iface,
                                                  bool includeParentMethods) {
    InterfaceMethodType onewayType = NONE;

    const std::vector<Method*>& methods = iface.userDefinedMethods();
    if (methods.empty()) {
        return (iface.superType() != nullptr && includeParentMethods)
                       ? getInterfaceOnewayType(*iface.superType(), true)
                       : NONE;
    }

    for (auto method : methods) {
        if (method->isOneway()) {
            onewayType = onewayType | ONEWAY;
        } else {
            onewayType = onewayType | TWOWAY;
        }

        if (onewayType == MIXED) {
            return onewayType;
        }
    }

    if (includeParentMethods) {
        onewayType = onewayType | getInterfaceOnewayType(*iface.superType(), true);
    }

    CHECK(onewayType != NONE) << "Functions are neither oneway nor non-oneway?: "
                              << iface.location();

    return onewayType;
}

static void onewayLint(const AST& ast, std::vector<Lint>* errors) {
    const Interface* iface = ast.getInterface();
    if (iface == nullptr) {
        // No interfaces so no oneway methods.
        return;
    }

    InterfaceMethodType ifaceType = getInterfaceOnewayType(*iface, false);
    if (ifaceType == NONE) {
        // Can occur for empty interfaces
        return;
    }

    std::string lintExplanation =
            "Since a function being oneway/non-oneway has large implications on the threading "
            "model and how client code needs to call an interface, it can be confusing/problematic "
            "when similar looking calls to the same interface result in wildly different "
            "behavior.\n";
    if (ifaceType == MIXED) {
        // This interface in itself is MIXED. Flag to user.
        errors->push_back(Lint(WARNING, iface->location())
                          << iface->typeName() << " has both oneway and non-oneway methods. "
                          << "It should only contain one of the two.\n"
                          << lintExplanation);
        return;
    }

    InterfaceMethodType parentType = getInterfaceOnewayType(*iface->superType(), true);
    if (parentType == NONE || parentType == MIXED) {
        // parents are mixed or don't have type, while this interface has only one type of
        // method. parentType => MIXED would have generated a lint on the parent interface.
        return;
    }

    if (parentType != ifaceType) {
        // type mismatch raise warning.
        errors->push_back(Lint(WARNING, iface->location())
                          << iface->typeName() << " should only have "
                          << (parentType == ONEWAY ? "oneway" : "non-oneway")
                          << " methods like its parent.\n"
                          << lintExplanation);
    }
}

REGISTER_LINT(onewayLint);

}  // namespace android
