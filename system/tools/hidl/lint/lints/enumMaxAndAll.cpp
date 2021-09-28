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

#include <android-base/strings.h>
#include <hidl-util/StringHelper.h>
#include <utils/Errors.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "AST.h"
#include "EnumType.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Location.h"
#include "Type.h"

namespace android {
static void enumValueNames(const AST& ast, std::vector<Lint>* errors) {
    std::unordered_set<const Type*> visited;
    ast.getRootScope().recursivePass(
            Type::ParseStage::COMPLETED,
            [&](const Type* t) -> status_t {
                if (!t->isEnum()) return OK;

                const EnumType* enumType = static_cast<const EnumType*>(t);
                if (!Location::inSameFile(ast.getRootScope().location(), enumType->location())) {
                    return OK;
                }
                for (const EnumValue* ev : enumType->values()) {
                    const std::vector<std::string> tokens =
                            base::Split(StringHelper::Uppercase(ev->name()), "_");

                    std::string errorString;
                    for (const std::string& token : tokens) {
                        if (token == "ALL" || token == "COUNT" || token == "MAX") {
                            errorString = "\"" + token + "\"" +
                                          " enum values have been known to become out of date when "
                                          "people add minor version upgrades, extensions to "
                                          "interfaces, or when more functionality is added later. "
                                          "In order to make it easier to maintain interfaces, "
                                          "consider avoiding adding this as part of an enum.\n";
                            break;
                        }
                    }

                    if (!errorString.empty()) {
                        errors->push_back(Lint(WARNING, ev->location(), errorString));
                    }
                }

                return OK;
            },
            &visited);
}

REGISTER_LINT(enumValueNames);
};  // namespace android
