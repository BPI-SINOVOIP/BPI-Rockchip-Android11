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

#include <vector>

#include "AST.h"
#include "CompoundType.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Location.h"
#include "Type.h"

namespace android {

static void checkSmallStructs(const AST& ast, std::vector<Lint>* errors) {
    // Should lint if it contains a union type at any level
    std::unordered_set<const Type*> visited;
    ast.getRootScope().recursivePass(
            Type::ParseStage::COMPLETED,
            [&](const Type* type) -> status_t {
                if (!type->isCompoundType()) return OK;

                const CompoundType* compoundType = static_cast<const CompoundType*>(type);

                // Will lint in the file that contains it
                if (!Location::inSameFile(compoundType->location(),
                                          ast.getRootScope().location())) {
                    return OK;
                }

                if (compoundType->getReferences().size() == 0) {
                    errors->push_back(
                            Lint(ERROR, compoundType->location())
                            << compoundType->typeName() << " contains no elements.\n"
                            << "Prefer using android.hidl.safe_union@1.0::Monostate instead.\n");
                } else if (compoundType->getReferences().size() == 1) {
                    errors->push_back(Lint(ERROR, compoundType->location())
                                      << compoundType->typeName() << " only contains 1 element.\n"
                                      << "Prefer using the type directly since wrapping it adds "
                                      << "memory and performance overhead.\n");
                }

                return OK;
            },
            &visited);
}

REGISTER_LINT(checkSmallStructs);

}  // namespace android