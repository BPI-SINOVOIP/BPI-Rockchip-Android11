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

#include <hidl-util/StringHelper.h>
#include <unordered_set>
#include <vector>

#include "AST.h"
#include "CompoundType.h"
#include "EnumType.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Location.h"
#include "NamedType.h"
#include "Reference.h"
#include "Type.h"

namespace android {

static void namingConventions(const AST& ast, std::vector<Lint>* errors) {
    std::unordered_set<const Type*> visited;
    ast.getRootScope().recursivePass(
            Type::ParseStage::COMPLETED,
            [&](const Type* type) -> status_t {
                // Do not consider root scope
                if (type->parent() == nullptr) return OK;
                if (!type->isNamedType()) return OK;

                const NamedType* namedType = static_cast<const NamedType*>(type);
                if (!Location::inSameFile(ast.getRootScope().location(), namedType->location())) {
                    return OK;
                }

                std::string definedName = namedType->definedName();
                // remove the "I" from the beginning
                if (namedType->isInterface()) definedName = definedName.substr(1);

                std::string desiredName = StringHelper::ToPascalCase(definedName);

                if (desiredName != definedName) {
                    if (namedType->isInterface()) desiredName = "I" + desiredName;

                    errors->push_back(Lint(WARNING, namedType->location())
                                      << "type \"" << namedType->definedName()
                                      << "\" should be named \"" << desiredName
                                      << "\" following the PascalCase (UpperCamelCase) "
                                      << "naming convention.\n");
                }

                if (namedType->isCompoundType()) {
                    const CompoundType* compoundType = static_cast<const CompoundType*>(namedType);
                    for (const NamedReference<Type>* ref : compoundType->getFields()) {
                        std::string memberName = (*ref).name();
                        if (StringHelper::ToCamelCase(memberName) != memberName) {
                            errors->push_back(Lint(WARNING, (*ref).location())
                                              << "member \"" << memberName << "\" of type \""
                                              << compoundType->definedName()
                                              << "\" should be named \""
                                              << StringHelper::ToCamelCase(memberName)
                                              << "\" following the camelCase naming convention.\n");
                        }
                    }
                } else if (namedType->isEnum()) {
                    const EnumType* enumType = static_cast<const EnumType*>(namedType);
                    for (const EnumValue* ev : enumType->values()) {
                        if (StringHelper::ToUpperSnakeCase(ev->name()) != ev->name()) {
                            errors->push_back(Lint(WARNING, ev->location())
                                              << "enumeration \"" << ev->name() << "\" of enum \""
                                              << enumType->definedName() << "\" should be named \""
                                              << StringHelper::ToUpperSnakeCase(ev->name())
                                              << "\" following the UPPER_SNAKE_CASE naming "
                                              << "convention.\n");
                        }
                    }
                }

                return OK;
            },
            &visited);
}

REGISTER_LINT(namingConventions);

}  // namespace android
