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

#include <vector>

#include "AST.h"
#include "Lint.h"
#include "LintRegistry.h"

namespace android {
static void importTypes(const AST& ast, std::vector<Lint>* errors) {
    for (const ImportStatement& import : ast.getImportStatements()) {
        const FQName& fqName = import.fqName;
        if (fqName.name() == "types") {
            if (fqName.package() == ast.package().package() &&
                fqName.version() == ast.package().version()) {
                errors->push_back(Lint(ERROR, import.location)
                                  << "Redundant import of types file. "
                                  << "Local types.hal files are imported by default.\n");
            } else {
                errors->push_back(Lint(WARNING, import.location)
                                  << "This imports every type from the file \"" << fqName.string()
                                  << "\". Prefer importing individual types instead.\n");
            }
        }
    }
}

REGISTER_LINT(importTypes);
}  // namespace android