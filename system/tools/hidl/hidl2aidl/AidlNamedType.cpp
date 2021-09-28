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

#include "AidlHelper.h"
#include "CompoundType.h"
#include "Coordinator.h"
#include "EnumType.h"
#include "NamedType.h"
#include "TypeDef.h"

namespace android {

static void emitConversionNotes(Formatter& out, const NamedType& namedType) {
    out << "// This is the HIDL definition of " << namedType.fqName().string() << "\n";
    out.pushLinePrefix("// ");
    namedType.emitHidlDefinition(out);
    out.popLinePrefix();
    out << "\n";
}

static void emitTypeDefAidlDefinition(Formatter& out, const TypeDef& typeDef) {
    out << "// Cannot convert typedef " << typeDef.referencedType()->definedName() << " "
        << typeDef.fqName().string() << " since AIDL does not support typedefs.\n";
    emitConversionNotes(out, typeDef);
}

static void emitEnumAidlDefinition(Formatter& out, const EnumType& enumType) {
    const ScalarType* scalar = enumType.storageType()->resolveToScalarType();
    CHECK(scalar != nullptr) << enumType.typeName();

    enumType.emitDocComment(out);
    out << "@Backing(type=\"" << AidlHelper::getAidlType(*scalar, enumType.fqName()) << "\")\n";
    out << "enum " << enumType.fqName().name() << " ";
    out.block([&] {
        enumType.forEachValueFromRoot([&](const EnumValue* value) {
            value->emitDocComment(out);
            out << value->name();
            if (!value->isAutoFill()) {
                out << " = " << value->constExpr()->expression();
            }
            out << ",\n";
        });
    });
}

static void emitCompoundTypeAidlDefinition(Formatter& out, const CompoundType& compoundType,
                                           const Coordinator& coordinator) {
    for (const NamedType* namedType : compoundType.getSubTypes()) {
        AidlHelper::emitAidl(*namedType, coordinator);
    }

    compoundType.emitDocComment(out);
    out << "parcelable " << AidlHelper::getAidlName(compoundType.fqName()) << " ";
    if (compoundType.style() == CompoundType::STYLE_STRUCT) {
        out.block([&] {
            for (const NamedReference<Type>* field : compoundType.getFields()) {
                field->emitDocComment(out);
                out << AidlHelper::getAidlType(*field->get(), compoundType.fqName()) << " "
                    << field->name() << ";\n";
            }
        });
    } else {
        out << "{}\n";
        out << "// Cannot convert unions/safe_unions since AIDL does not support them.\n";
        emitConversionNotes(out, compoundType);
    }
    out << "\n\n";
}

// TODO: Enum/Typedef should just emit to hidl-error.log or similar
void AidlHelper::emitAidl(const NamedType& namedType, const Coordinator& coordinator) {
    Formatter out = getFileWithHeader(namedType, coordinator);
    if (namedType.isTypeDef()) {
        const TypeDef& typeDef = static_cast<const TypeDef&>(namedType);
        emitTypeDefAidlDefinition(out, typeDef);
    } else if (namedType.isCompoundType()) {
        const CompoundType& compoundType = static_cast<const CompoundType&>(namedType);
        emitCompoundTypeAidlDefinition(out, compoundType, coordinator);
    } else if (namedType.isEnum()) {
        const EnumType& enumType = static_cast<const EnumType&>(namedType);
        emitEnumAidlDefinition(out, enumType);
    } else {
        out << "// TODO: Fix this " << namedType.definedName() << "\n";
    }
}

}  // namespace android
