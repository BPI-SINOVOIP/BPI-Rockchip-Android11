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
#include <android-base/strings.h>
#include <hidl-util/FQName.h>
#include <hidl-util/Formatter.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "AST.h"
#include "AidlHelper.h"
#include "Coordinator.h"
#include "DocComment.h"

using namespace android;

static void usage(const char* me) {
    Formatter out(stderr);

    out << "Usage: " << me << " [-o <output path>] ";
    Coordinator::emitOptionsUsageString(out);
    out << " FQNAME...\n\n";

    out << "Converts FQNAME, PACKAGE(.SUBPACKAGE)*@[0-9]+.[0-9]+(::TYPE)? to an aidl "
           "equivalent.\n\n";

    out.indent();
    out.indent();

    out << "-o <output path>: Location to output files.\n";
    out << "-h: Prints this menu.\n";
    Coordinator::emitOptionsDetailString(out);

    out.unindent();
    out.unindent();
}

static const FQName& getNewerFQName(const FQName& lhs, const FQName& rhs) {
    CHECK(lhs.package() == rhs.package());
    CHECK(lhs.name() == rhs.name());

    if (lhs.getPackageMajorVersion() > rhs.getPackageMajorVersion()) return lhs;
    if (lhs.getPackageMajorVersion() < rhs.getPackageMajorVersion()) return rhs;

    if (lhs.getPackageMinorVersion() > rhs.getPackageMinorVersion()) return lhs;
    return rhs;
}

// If similar FQName is not found, the same one is returned
static FQName getLatestMinorVersionFQNameFromList(const FQName& fqName,
                                                  const std::vector<FQName>& list) {
    FQName currentCandidate = fqName;
    bool found = false;
    for (const FQName& current : list) {
        if (current.package() == currentCandidate.package() &&
            current.name() == currentCandidate.name() &&
            current.getPackageMajorVersion() == currentCandidate.getPackageMajorVersion()) {
            // Prioritize elements in the list over the provided fqName
            currentCandidate = found ? getNewerFQName(current, currentCandidate) : current;
            found = true;
        }
    }

    return currentCandidate;
}

static FQName getLatestMinorVersionNamedTypeFromList(const FQName& fqName,
                                                     const std::vector<const NamedType*>& list) {
    FQName currentCandidate = fqName;
    bool found = false;
    for (const NamedType* currentNamedType : list) {
        const FQName& current = currentNamedType->fqName();
        if (current.package() == currentCandidate.package() &&
            current.name() == currentCandidate.name() &&
            current.getPackageMajorVersion() == currentCandidate.getPackageMajorVersion()) {
            // Prioritize elements in the list over the provided fqName
            currentCandidate = found ? getNewerFQName(current, currentCandidate) : current;
            found = true;
        }
    }

    return currentCandidate;
}

static bool packageExists(const Coordinator& coordinator, const FQName& fqName) {
    bool result;
    status_t err = coordinator.packageExists(fqName, &result);
    if (err != OK) {
        std::cerr << "Error trying to find package " << fqName.string() << std::endl;
        exit(1);
    }

    return result;
}

static AST* parse(const Coordinator& coordinator, const FQName& target) {
    AST* ast = coordinator.parse(target);
    if (ast == nullptr) {
        std::cerr << "ERROR: Could not parse " << target.name() << ". Aborting." << std::endl;
        exit(1);
    }

    if (!ast->getUnhandledComments().empty()) {
        AidlHelper::notes()
                << "Unhandled comments from " << target.string()
                << " follow. Consider using hidl-lint to locate these and fixup as many "
                << "as possible.\n";
        for (const DocComment* docComment : ast->getUnhandledComments()) {
            docComment->emit(AidlHelper::notes());
        }
        AidlHelper::notes() << "\n";
    }

    return ast;
}

// hidl is intentionally leaky. Turn off LeakSanitizer by default.
extern "C" const char* __asan_default_options() {
    return "detect_leaks=0";
}

int main(int argc, char** argv) {
    const char* me = argv[0];
    if (argc == 1) {
        usage(me);
        std::cerr << "ERROR: no fqname specified." << std::endl;
        exit(1);
    }

    Coordinator coordinator;
    std::string outputPath;
    coordinator.parseOptions(argc, argv, "ho:", [&](int res, char* arg) {
        switch (res) {
            case 'o': {
                if (!outputPath.empty()) {
                    fprintf(stderr, "ERROR: -o <output path> can only be specified once.\n");
                    exit(1);
                }
                outputPath = arg;
                break;
            }
            case 'h':
            case '?':
            default: {
                usage(me);
                exit(1);
                break;
            }
        }
    });

    if (!outputPath.empty() && outputPath.back() != '/') {
        outputPath += "/";
    }
    coordinator.setOutputPath(outputPath);

    argc -= optind;
    argv += optind;

    if (argc == 0) {
        usage(me);
        std::cerr << "ERROR: no fqname specified." << std::endl;
        exit(1);
    }

    for (int i = 0; i < argc; ++i) {
        const char* arg = argv[i];

        FQName fqName;
        if (!FQName::parse(arg, &fqName)) {
            std::cerr << "ERROR: Invalid fully-qualified name as argument: " << arg << "."
                      << std::endl;
            exit(1);
        }

        if (!packageExists(coordinator, fqName)) {
            std::cerr << "ERROR: Could not get sources for: " << arg << "." << std::endl;
            exit(1);
        }

        FQName currentFqName(fqName);
        while (currentFqName.getPackageMinorVersion() != 0) {
            if (!packageExists(coordinator, currentFqName.downRev())) break;

            currentFqName = currentFqName.downRev();
        }

        std::vector<FQName> targets;
        while (packageExists(coordinator, currentFqName)) {
            std::vector<FQName> newTargets;
            status_t err = coordinator.appendPackageInterfacesToVector(currentFqName, &newTargets);
            if (err != OK) break;

            targets.insert(targets.end(), newTargets.begin(), newTargets.end());

            currentFqName = currentFqName.upRev();
        }

        // targets should not contain duplicates since appendPackageInterfaces is only called once
        // per version. now remove all the elements that are not the "newest"
        const auto& newEnd =
                std::remove_if(targets.begin(), targets.end(), [&](const FQName& fqName) -> bool {
                    if (fqName.name() == "types") return false;

                    return getLatestMinorVersionFQNameFromList(fqName, targets) != fqName;
                });
        targets.erase(newEnd, targets.end());

        if (fqName.isFullyQualified()) {
            // Ensure that this fqName exists in the list.
            // If not then there is a more recent version
            if (std::find(targets.begin(), targets.end(), fqName) == targets.end()) {
                // Not found. Error.
                std::cerr << "ERROR: A newer minor version of " << fqName.string()
                          << " exists. Compile that instead." << std::endl;
                exit(1);
            } else {
                targets.clear();
                targets.push_back(fqName);
            }
        }

        // Set up AIDL conversion log
        std::string aidlPackage = AidlHelper::getAidlPackage(fqName);
        std::string aidlName = AidlHelper::getAidlName(fqName);
        Formatter err = coordinator.getFormatter(
                fqName, Coordinator::Location::DIRECT,
                base::Join(base::Split(aidlPackage, "."), "/") + "/" +
                        (aidlName.empty() ? "" : (aidlName + "-")) + "conversion.log");
        AidlHelper::setNotes(&err);

        std::vector<const NamedType*> namedTypesInPackage;
        for (const FQName& target : targets) {
            if (target.name() != "types") continue;

            AST* ast = parse(coordinator, target);

            CHECK(!ast->isInterface());

            std::vector<const NamedType*> types = ast->getRootScope().getSortedDefinedTypes();
            namedTypesInPackage.insert(namedTypesInPackage.end(), types.begin(), types.end());
        }

        const auto& endNamedTypes = std::remove_if(
                namedTypesInPackage.begin(), namedTypesInPackage.end(),
                [&](const NamedType* namedType) -> bool {
                    return getLatestMinorVersionNamedTypeFromList(
                                   namedType->fqName(), namedTypesInPackage) != namedType->fqName();
                });
        namedTypesInPackage.erase(endNamedTypes, namedTypesInPackage.end());

        for (const NamedType* namedType : namedTypesInPackage) {
            AidlHelper::emitAidl(*namedType, coordinator);
        }

        for (const FQName& target : targets) {
            if (target.name() == "types") continue;

            AST* ast = parse(coordinator, target);

            const Interface* iface = ast->getInterface();
            CHECK(iface);

            AidlHelper::emitAidl(*iface, coordinator);
        }
    }

    return 0;
}
