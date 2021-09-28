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

#include <algorithm>
#include <string>
#include <vector>

#include "AST.h"
#include "DocComment.h"
#include "Interface.h"
#include "Lint.h"
#include "LintRegistry.h"
#include "Method.h"
#include "Reference.h"

namespace android {

// returns true if the line contained a prefix and false otherwise
static bool getFirstWordAfterPrefix(const std::string& str, const std::string& prefix,
                                    std::string* out) {
    std::string line = str;
    if (base::StartsWith(line, prefix)) {
        line = StringHelper::LTrim(line, prefix);

        // check line has other stuff and that there is a space between return and the rest
        if (!base::StartsWith(line, " ")) {
            *out = "";
            return true;
        }

        line = StringHelper::LTrimAll(line, " ");

        std::vector<std::string> words = base::Split(line, " ");
        *out = words.empty() ? "" : words.at(0);
        return true;
    }

    return false;
}

static bool isNameInList(const std::string& name, const std::vector<NamedReference<Type>*>& refs) {
    return std::any_of(refs.begin(), refs.end(), [&](const NamedReference<Type>* namedRef) -> bool {
        return namedRef->name() == name;
    });
}

static bool isSubsequence(const std::vector<NamedReference<Type>*>& refs,
                          const std::vector<std::string>& subsequence) {
    if (subsequence.empty()) return true;

    auto it = subsequence.begin();
    auto subEnd = subsequence.end();
    for (const NamedReference<Type>* namedRef : refs) {
        if (namedRef->name() == *it) it++;

        if (it == subEnd) return true;
    }

    // Should be false here
    return it == subEnd;
}

static void methodDocComments(const AST& ast, std::vector<Lint>* errors) {
    const Interface* iface = ast.getInterface();
    if (iface == nullptr) {
        // no interfaces so no methods
        return;
    }

    for (const Method* method : iface->isIBase() ? iface->methods() : iface->userDefinedMethods()) {
        const DocComment* docComment = method->getDocComment();
        if (docComment == nullptr) continue;

        bool returnRefFound = false;

        std::vector<std::string> dcArgs;
        std::vector<std::string> dcReturns;

        // want a copy so that it can be mutated
        for (const std::string& line : docComment->lines()) {
            std::string returnName;

            if (bool foundPrefix = getFirstWordAfterPrefix(line, "@return", &returnName);
                foundPrefix) {
                if (returnName.empty()) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@return should be followed by a return parameter.\n");
                    continue;
                }

                returnRefFound = true;
                if (!isNameInList(returnName, method->results())) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@return " << returnName
                                      << " is not a return parameter of the method "
                                      << method->name() << ".\n");
                } else {
                    if (std::find(dcReturns.begin(), dcReturns.end(), returnName) !=
                        dcReturns.end()) {
                        errors->push_back(
                                Lint(WARNING, docComment->location())
                                << "@return " << returnName
                                << " was referenced multiple times in the same doc comment.\n");
                    } else {
                        dcReturns.push_back(returnName);
                    }
                }

                continue;
            }

            if (bool foundPrefix = getFirstWordAfterPrefix(line, "@param", &returnName);
                foundPrefix) {
                if (returnName.empty()) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@param should be followed by a parameter name.\n");
                    continue;
                }

                if (returnRefFound) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "Found @param " << returnName
                                      << " after a @return declaration. All @param references "
                                      << "should come before @return references.\n");
                }

                if (!isNameInList(returnName, method->args())) {
                    errors->push_back(Lint(WARNING, docComment->location())
                                      << "@param " << returnName
                                      << " is not an argument to the method " << method->name()
                                      << ".\n");
                } else {
                    if (std::find(dcArgs.begin(), dcArgs.end(), returnName) != dcArgs.end()) {
                        errors->push_back(
                                Lint(WARNING, docComment->location())
                                << "@param " << returnName
                                << " was referenced multiple times in the same doc comment.\n");
                    } else {
                        dcArgs.push_back(returnName);
                    }
                }
            }
        }

        if (!isSubsequence(method->results(), dcReturns)) {
            errors->push_back(Lint(WARNING, docComment->location())
                              << "@return references should be ordered the same way they show up "
                              << "in the return parameter list.\n");
        }
        if (!isSubsequence(method->args(), dcArgs)) {
            errors->push_back(Lint(WARNING, docComment->location())
                              << "@param references should be ordered the same way they show up in "
                              << "the argument list.\n");
        }
    }
}

REGISTER_LINT(methodDocComments);
}  // namespace android
