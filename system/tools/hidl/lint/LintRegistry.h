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

#pragma once

#include <functional>
#include <vector>

namespace android {
struct AST;
struct Lint;

using LintFunction = std::function<void(const AST&, std::vector<Lint>*)>;

class LintRegistry {
    std::vector<LintFunction> kLintFunctions;

  public:
    static LintRegistry* get();

    void registerLintFunction(const LintFunction& lintFunction) {
        kLintFunctions.push_back(lintFunction);
    }

    const std::vector<LintFunction>& getLintFunctions() { return kLintFunctions; }

    void runAllLintFunctions(const AST& ast, std::vector<Lint>* errors);
};

struct LintPass {
    LintPass(const LintFunction& lintFunction);
};

#define REGISTER_LINT(FUNCTION) static LintPass LINT(FUNCTION)

}  // namespace android
