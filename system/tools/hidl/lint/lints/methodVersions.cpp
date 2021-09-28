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
#include <android-base/parseint.h>
#include <hidl-util/FQName.h>
#include <hidl-util/StringHelper.h>

#include <algorithm>
#include <string>
#include <vector>

#include "AST.h"
#include "Interface.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Method.h"

using namespace std::string_literals;

namespace android {
static std::string getSanitizedMethodName(const Method& method) {
    size_t underscore = method.name().find('_');
    return (underscore == std::string::npos) ? method.name() : method.name().substr(0, underscore);
}

static bool checkMethodVersion(const Method& method, const FQName& fqName, std::string* error) {
    size_t underscore = method.name().find('_');
    CHECK(underscore != std::string::npos);  // only called on versionedMethods

    std::string version = method.name().substr(underscore + 1);  // don't include _
    std::string nameWithoutVersion = method.name().substr(0, underscore);
    underscore = version.find('_');

    std::string camelCaseMessage =
            "Methods should follow the camelCase naming convention.\n"
            "Underscores are only allowed in method names when defining a new version of a method. "
            "Use the methodName_MAJOR_MINOR naming convention if that was the intended use. MAJOR, "
            "MINOR must be integers representing the current package version.";

    if (nameWithoutVersion != StringHelper::ToCamelCase(nameWithoutVersion)) {
        *error = camelCaseMessage;
        return false;
    }

    // does not contain both major and minor versions
    if (underscore == std::string::npos) {
        *error = camelCaseMessage;
        return false;
    }

    size_t major;
    if (!base::ParseUint(version.substr(0, underscore), &major)) {
        // could not parse a major rev
        *error = camelCaseMessage;
        return false;
    }

    size_t minor;
    if (!base::ParseUint(version.substr(underscore + 1), &minor)) {
        // could not parse a minor rev
        *error = camelCaseMessage;
        return false;
    }

    if ((major == fqName.getPackageMajorVersion()) && (minor == fqName.getPackageMinorVersion())) {
        return true;
    }

    *error = method.name() + " looks like version "s + std::to_string(major) + "."s +
             std::to_string(minor) + " of "s + getSanitizedMethodName(method) +
             ", but the interface is in package version "s + fqName.version();
    return false;
}

static void methodVersions(const AST& ast, std::vector<Lint>* errors) {
    const Interface* iface = ast.getInterface();
    if (iface == nullptr) {
        // No interfaces so no methods.
        return;
    }

    for (Method* method : iface->userDefinedMethods()) {
        if (method->name().find('_') == std::string::npos) {
            if (method->name() != StringHelper::ToCamelCase(method->name())) {
                errors->push_back(Lint(WARNING, method->location(),
                                       "Methods should follow the camelCase naming convention.\n"));
            }

            continue;
        }

        // the method has been versioned
        std::string errorString;
        if (checkMethodVersion(*method, ast.package(), &errorString)) {
            // Ensure that a supertype contains the method
            std::string methodName = getSanitizedMethodName(*method);

            const std::vector<const Interface*>& superTypeChain = iface->superTypeChain();
            bool foundMethod = std::any_of(
                    superTypeChain.begin(), superTypeChain.end(),
                    [&](const Interface* superType) -> bool {
                        const std::vector<Method*>& superMethods = superType->userDefinedMethods();
                        return std::any_of(superMethods.begin(), superMethods.end(),
                                           [&](Method* superMethod) -> bool {
                                               return getSanitizedMethodName(*superMethod) ==
                                                      methodName;
                                           });
                    });

            if (!foundMethod) {
                errors->push_back(Lint(WARNING, method->location())
                                  << "Could not find method " << methodName
                                  << " in any of the super types.\n"
                                  << "Should only use the method_X_Y naming convention when the "
                                  << "method is replacing an older version of the same method.\n");
            }
        } else {
            errors->push_back(Lint(WARNING, method->location()) << errorString << "\n");
        }
    }
}

REGISTER_LINT(methodVersions);
}  // namespace android
