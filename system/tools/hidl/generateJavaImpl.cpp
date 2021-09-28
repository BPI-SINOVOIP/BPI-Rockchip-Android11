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

#include "AST.h"
#include "Interface.h"
#include "Method.h"

namespace android {
void AST::generateJavaImpl(Formatter& out) const {
    if (!AST::isInterface()) {
        // types.hal does not get a stub header.
        return;
    }

    const Interface* iface = mRootScope.getInterface();
    const std::string baseName = iface->getBaseName();

    out << "// FIXME: your file license if you have one\n\n";
    out << "// FIXME: add package information\n\n";

    out << "import " << mPackage.javaPackage() << "." << iface->definedName() << ";\n\n";

    out << "class " << baseName << " extends " << iface->definedName() << ".Stub"
        << " {\n";

    out.indent([&] {
        const Interface* prevInterface = nullptr;
        for (const auto& tuple : iface->allMethodsFromRoot()) {
            const Method* method = tuple.method();

            if (method->isHidlReserved()) {
                continue;
            }

            const Interface* superInterface = tuple.interface();
            if (prevInterface != superInterface) {
                out << "// Methods from " << superInterface->fullJavaName() << " follow.\n";
                prevInterface = superInterface;
            }

            out << "@Override\npublic ";
            method->emitJavaSignature(out);

            out << "\n";
            out.indent([&] {
                out.indent();
                out << "throws android.os.RemoteException {\n";
                out.unindent();

                out << "// TODO: Implement\n";

                // Return the appropriate value
                const bool returnsValue = !method->results().empty();
                if (returnsValue) {
                    for (const auto& arg : method->results()) {
                        arg->type().emitJavaFieldInitializer(out, arg->name());
                    }

                    const bool needsCallback = method->results().size() > 1;
                    if (needsCallback) {
                        out << "_hidl_cb.onValues(";

                        out.join(method->results().begin(), method->results().end(), ", ",
                                 [&](auto& arg) { out << arg->name(); });

                        out << ");\n";
                    } else {
                        // only 1 value is returned
                        out << "return " << method->results().at(0)->name() << ";\n";
                    }
                }
            });

            out << "}\n\n";
        }
    });

    out << "}\n";
}
}  // namespace android
