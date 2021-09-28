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
#include "DocComment.h"
#include "Lint.h"
#include "LintRegistry.h"

namespace android {

static void unhandledComments(const AST& ast, std::vector<Lint>* errors) {
    for (const DocComment* docComment : ast.getUnhandledComments()) {
        errors->push_back(
                Lint(WARNING, docComment->location())
                << "This comment cannot be processed since it is in an unrecognized "
                << "place in the file. Consider adding it as a multiline comment directly above an "
                << "interface, method, type declaration, enumerator, or field.\n");
    }
}

REGISTER_LINT(unhandledComments);
}  // namespace android
