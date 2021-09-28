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

#include <unordered_set>
#include <vector>

#include "AST.h"
#include "CompoundType.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Location.h"
#include "Scope.h"
#include "Type.h"

namespace android {

// if this pattern of separate execution for getDefinedTypes and getReferences is common, it should
// be abstacted into a recursivePass type function
static void lintUnionRecursively(const Scope* scope, std::unordered_set<const Type*>* visited,
                                 std::vector<Lint>* errors) {
    CHECK(scope->getParseStage() == Type::ParseStage::COMPLETED)
            << "type parsing is not yet complete";

    if (visited->find(scope) != visited->end()) return;
    visited->insert(scope);

    std::string lintExplanation =
            "Union types should not be used since they are not supported in Java and are not type "
            "safe. Prefer using safe_union instead.";
    for (const auto* nextType : scope->getDefinedTypes()) {
        if (!nextType->isCompoundType()) {
            // if the type is not compound then it cannot be a union type, but can contain one.
            lintUnionRecursively(static_cast<const Scope*>(nextType), visited, errors);
            continue;
        }

        const CompoundType* compoundType = static_cast<const CompoundType*>(nextType);
        if (compoundType->style() == CompoundType::Style::STYLE_UNION) {
            errors->push_back(Lint(ERROR, compoundType->location())
                              << compoundType->typeName() << " is defined as a Union type.\n"
                              << lintExplanation << "\n");
            continue;
        }

        // Not a union type, so must be struct or safe_union
        // Definitely still in the same file.
        lintUnionRecursively(compoundType, visited, errors);
    }

    for (const auto* nextRef : scope->getReferences()) {
        if (!nextRef->get()->isCompoundType()) continue;

        const CompoundType* compoundType = static_cast<const CompoundType*>(nextRef->get());
        if (compoundType->style() == CompoundType::Style::STYLE_UNION) {
            // The reference was not from this scope
            if (!Location::inSameFile(scope->location(), nextRef->location())) continue;

            // The type is defined in the same file. Will lint there.
            if (Location::inSameFile(scope->location(), compoundType->location())) continue;

            errors->push_back(Lint(ERROR, nextRef->location())
                              << "Reference to union type: " << compoundType->typeName()
                              << " located in " << compoundType->location().begin().filename()
                              << "\n"
                              << lintExplanation << "\n");
            continue;
        }

        const status_t FOUND_UNION = 1;
        status_t result = compoundType->recursivePass(
                Type::ParseStage::COMPLETED,
                [&](const Type* type) -> status_t {
                    if (!type->isCompoundType()) return OK;

                    const CompoundType* compoundType = static_cast<const CompoundType*>(type);
                    if (compoundType->style() == CompoundType::Style::STYLE_UNION) {
                        return FOUND_UNION;
                    }

                    // some other type of compound type, like struct/safeunion
                    return OK;
                },
                visited);

        if (result == FOUND_UNION) {
            // The struct contains a reference to a union somewhere
            errors->push_back(Lint(ERROR, nextRef->location())
                              << "Reference to struct: " << compoundType->typeName()
                              << " located in " << compoundType->location().begin().filename()
                              << " contains a union type.\n"
                              << lintExplanation << "\n");
        }
    }
}

static void safeunionLint(const AST& ast, std::vector<Lint>* errors) {
    // Should lint if it contains a union type at any level
    std::unordered_set<const Type*> visited;

    lintUnionRecursively(&ast.getRootScope(), &visited, errors);
}

REGISTER_LINT(safeunionLint);

}  // namespace android