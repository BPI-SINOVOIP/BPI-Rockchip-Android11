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
#include <hidl-util/Formatter.h>

#include "AST.h"

namespace android {

void AST::generateFormattedHidl(Formatter& out) const {
    if (mHeader != nullptr) {
        mHeader->emit(out, CommentType::MULTILINE);
        out << "\n";
    }

    out << "package " << mPackage.string() << ";\n\n";

    out.join(mImportStatements.begin(), mImportStatements.end(), "\n", [&](const auto& import) {
        if (import.fqName.name().empty()) {
            out << "import " << import.fqName.string() << ";";
        } else {
            out << "import " << import.fqName.getRelativeFQName(mPackage) << ";";
        }
    });
    if (!mImportStatements.empty()) out << "\n\n";

    mRootScope.emitHidlDefinition(out);
}

}  // namespace android
