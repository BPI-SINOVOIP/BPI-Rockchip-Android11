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
#include <android-base/strings.h>
#include <hidl-util/FQName.h>
#include <hidl-util/Formatter.h>
#include <hidl-util/StringHelper.h>
#include <cstddef>
#include <vector>

#include "AidlHelper.h"
#include "Coordinator.h"
#include "DocComment.h"
#include "FormattingConstants.h"
#include "Interface.h"
#include "Location.h"
#include "Method.h"
#include "NamedType.h"
#include "Reference.h"
#include "Type.h"

namespace android {

static void emitAidlMethodParams(WrappedOutput* wrappedOutput,
                                 const std::vector<NamedReference<Type>*> args,
                                 const std::string& prefix, const std::string& attachToLast,
                                 const Interface& iface) {
    if (args.size() == 0) {
        *wrappedOutput << attachToLast;
        return;
    }

    for (size_t i = 0; i < args.size(); i++) {
        const NamedReference<Type>* arg = args[i];
        std::string out =
                prefix + AidlHelper::getAidlType(*arg->get(), iface.fqName()) + " " + arg->name();
        wrappedOutput->group([&] {
            if (i != 0) wrappedOutput->printUnlessWrapped(" ");
            *wrappedOutput << out;
            if (i == args.size() - 1) {
                if (!attachToLast.empty()) *wrappedOutput << attachToLast;
            } else {
                *wrappedOutput << ",";
            }
        });
    }
}

std::vector<const Method*> AidlHelper::getUserDefinedMethods(const Interface& interface) {
    std::vector<const Method*> methods;
    for (const Interface* iface : interface.typeChain()) {
        const std::vector<Method*> userDefined = iface->userDefinedMethods();
        methods.insert(methods.end(), userDefined.begin(), userDefined.end());
    }

    return methods;
}

struct MethodWithVersion {
    size_t major;
    size_t minor;
    const Method* method;
    std::string name;
};

static void pushVersionedMethodOntoMap(MethodWithVersion versionedMethod,
                                       std::map<std::string, MethodWithVersion>* map,
                                       std::vector<const MethodWithVersion*>* ignored) {
    const Method* method = versionedMethod.method;
    std::string name = method->name();
    size_t underscore = name.find('_');
    if (underscore != std::string::npos) {
        std::string version = name.substr(underscore + 1);  // don't include _
        std::string nameWithoutVersion = name.substr(0, underscore);
        underscore = version.find('_');

        size_t major, minor;
        if (underscore != std::string::npos &&
            base::ParseUint(version.substr(0, underscore), &major) &&
            base::ParseUint(version.substr(underscore + 1), &minor)) {
            // contains major and minor version. consider it's nameWithoutVersion now.
            name = nameWithoutVersion;
            versionedMethod.name = nameWithoutVersion;
        }
    }

    // push name onto map
    auto [it, inserted] = map->emplace(std::move(name), versionedMethod);
    if (!inserted) {
        auto* current = &it->second;

        // Method in the map is more recent
        if ((current->major > versionedMethod.major) ||
            (current->major == versionedMethod.major && current->minor > versionedMethod.minor)) {
            // ignoring versionedMethod
            ignored->push_back(&versionedMethod);
            return;
        }

        // Either current.major < versioned.major OR versioned.minor >= current.minor
        ignored->push_back(current);
        *current = std::move(versionedMethod);
    }
}

struct ResultTransformation {
    enum class TransformType {
        MOVED,    // Moved to the front of the method name
        REMOVED,  // Removed the result
    };

    std::string resultName;
    TransformType type;
};

static bool shouldWarnStatusType(const std::string& typeName) {
    static const std::vector<std::string> kUppercaseIgnoreStatusTypes = {"ERROR", "STATUS"};

    const std::string uppercase = StringHelper::Uppercase(typeName);
    for (const std::string& ignore : kUppercaseIgnoreStatusTypes) {
        if (uppercase.find(ignore) != std::string::npos) return true;
    }
    return false;
}

void AidlHelper::emitAidl(const Interface& interface, const Coordinator& coordinator) {
    for (const NamedType* type : interface.getSubTypes()) {
        emitAidl(*type, coordinator);
    }

    Formatter out = getFileWithHeader(interface, coordinator);

    interface.emitDocComment(out);
    if (interface.superType() && interface.superType()->fqName() != gIBaseFqName) {
        out << "// Interface inherits from " << interface.superType()->fqName().string()
            << " but AIDL does not support interface inheritance.\n";
    }

    out << "interface " << getAidlName(interface.fqName()) << " ";
    out.block([&] {
        for (const NamedType* type : interface.getSubTypes()) {
            emitAidl(*type, coordinator);
        }

        std::map<std::string, MethodWithVersion> methodMap;
        std::vector<const MethodWithVersion*> ignoredMethods;
        std::vector<std::string> methodNames;
        std::vector<const Interface*> typeChain = interface.typeChain();
        for (auto iface = typeChain.rbegin(); iface != typeChain.rend(); ++iface) {
            for (const Method* method : (*iface)->userDefinedMethods()) {
                pushVersionedMethodOntoMap(
                        {(*iface)->fqName().getPackageMajorVersion(),
                         (*iface)->fqName().getPackageMinorVersion(), method, method->name()},
                        &methodMap, &ignoredMethods);
                methodNames.push_back(method->name());
            }
        }

        std::set<std::string> ignoredMethodNames;
        out.join(ignoredMethods.begin(), ignoredMethods.end(), "\n",
                 [&](const MethodWithVersion* versionedMethod) {
                     out << "// Ignoring method " << versionedMethod->method->name() << " from "
                         << versionedMethod->major << "." << versionedMethod->minor
                         << "::" << getAidlName(interface.fqName())
                         << " since a newer alternative is available.";
                     ignoredMethodNames.insert(versionedMethod->method->name());
                 });
        if (!ignoredMethods.empty()) out << "\n\n";

        out.join(methodNames.begin(), methodNames.end(), "\n", [&](const std::string& name) {
            const Method* method = methodMap[name].method;
            if (method == nullptr) {
                CHECK(ignoredMethodNames.count(name) == 1);
                return;
            }

            std::vector<NamedReference<Type>*> results;
            std::vector<ResultTransformation> transformations;
            for (NamedReference<Type>* res : method->results()) {
                const std::string aidlType = getAidlType(*res->get(), interface.fqName());

                if (shouldWarnStatusType(aidlType)) {
                    out << "// FIXME: AIDL has built-in status types. Do we need the status type "
                           "here?\n";
                }
                results.push_back(res);
            }

            if (method->name() != name) {
                out << "// Changing method name from " << method->name() << " to " << name << "\n";
            }

            std::string returnType = "void";
            if (results.size() == 1) {
                returnType = getAidlType(*results[0]->get(), interface.fqName());

                out << "// Adding return type to method instead of out param " << returnType << " "
                    << results[0]->name() << " since there is only one return value.\n";
                transformations.emplace_back(ResultTransformation{
                        results[0]->name(), ResultTransformation::TransformType::MOVED});
                results.clear();
            }

            if (method->getDocComment() != nullptr) {
                std::vector<std::string> modifiedDocComment;
                for (const std::string& line : method->getDocComment()->lines()) {
                    std::vector<std::string> tokens = base::Split(line, " ");
                    if (tokens.size() <= 1 || tokens[0] != "@return") {
                        // unimportant line
                        modifiedDocComment.emplace_back(line);
                        continue;
                    }

                    const std::string& res = tokens[1];
                    bool transformed = false;
                    for (const ResultTransformation& transform : transformations) {
                        if (transform.resultName != res) continue;

                        // Some transform was done to it
                        if (transform.type == ResultTransformation::TransformType::MOVED) {
                            // remove the name
                            tokens.erase(++tokens.begin());
                            transformed = true;
                        } else {
                            CHECK(transform.type == ResultTransformation::TransformType::REMOVED);
                            tokens.insert(tokens.begin(),
                                          "FIXME: The following return was removed\n");
                            transformed = true;
                        }
                    }

                    if (!transformed) {
                        tokens.erase(tokens.begin());
                        tokens.insert(tokens.begin(), "@param out");
                    }

                    modifiedDocComment.emplace_back(base::Join(tokens, " "));
                }

                DocComment(modifiedDocComment, HIDL_LOCATION_HERE).emit(out);
            }

            WrappedOutput wrappedOutput(MAX_LINE_LENGTH);

            if (method->isOneway()) wrappedOutput << "oneway ";
            wrappedOutput << returnType << " " << name << "(";

            if (results.empty()) {
                emitAidlMethodParams(&wrappedOutput, method->args(), /* prefix */ "in ",
                                     /* attachToLast */ ");\n", interface);
            } else {
                if (!method->args().empty()) {
                    emitAidlMethodParams(&wrappedOutput, method->args(), /* prefix */ "in ",
                                         /* attachToLast */ ",", interface);
                    wrappedOutput.printUnlessWrapped(" ");
                }

                // TODO: Emit warning if a primitive is given as a out param.
                emitAidlMethodParams(&wrappedOutput, results, /* prefix */ "out ",
                                     /* attachToLast */ ");\n", interface);
            }

            out << wrappedOutput;
        });
    });
}

}  // namespace android
