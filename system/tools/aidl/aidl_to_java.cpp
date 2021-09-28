/*
 * Copyright (C) 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aidl_to_java.h"
#include "aidl_language.h"
#include "aidl_typenames.h"
#include "logging.h"

#include <android-base/strings.h>

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace android {
namespace aidl {
namespace java {

using android::base::Join;

using std::endl;
using std::function;
using std::map;
using std::string;
using std::vector;

std::string ConstantValueDecorator(const AidlTypeSpecifier& type, const std::string& raw_value) {
  if (type.GetName() == "long" && !type.IsArray()) {
    return raw_value + "L";
  }

  return raw_value;
};

const string& JavaNameOf(const AidlTypeSpecifier& aidl, const AidlTypenames& typenames,
                         bool instantiable = false, bool boxing = false) {
  CHECK(aidl.IsResolved()) << aidl.ToString();

  if (instantiable) {
    // An instantiable type is used in only out type(not even inout type),
    // And instantiable type has to be either the type in List, Map, ParcelFileDescriptor or
    // user-defined type.

    static map<string, string> instantiable_m = {
        {"List", "java.util.ArrayList"},
        {"Map", "java.util.HashMap"},
        {"ParcelFileDescriptor", "android.os.ParcelFileDescriptor"},
    };
    const string& aidl_name = aidl.GetName();

    if (instantiable_m.find(aidl_name) != instantiable_m.end()) {
      return instantiable_m[aidl_name];
    }
  }

  // map from AIDL built-in type name to the corresponding Java type name
  static map<string, string> m = {
      {"void", "void"},
      {"boolean", "boolean"},
      {"byte", "byte"},
      {"char", "char"},
      {"int", "int"},
      {"long", "long"},
      {"float", "float"},
      {"double", "double"},
      {"String", "java.lang.String"},
      {"List", "java.util.List"},
      {"Map", "java.util.Map"},
      {"IBinder", "android.os.IBinder"},
      {"FileDescriptor", "java.io.FileDescriptor"},
      {"CharSequence", "java.lang.CharSequence"},
      {"ParcelFileDescriptor", "android.os.ParcelFileDescriptor"},
  };

  // map from primitive types to the corresponding boxing types
  static map<string, string> boxing_types = {
      {"void", "Void"},   {"boolean", "Boolean"}, {"byte", "Byte"},   {"char", "Character"},
      {"int", "Integer"}, {"long", "Long"},       {"float", "Float"}, {"double", "Double"},
  };

  // Enums in Java are represented by their backing type when
  // referenced in parcelables, methods, etc.
  if (const AidlEnumDeclaration* enum_decl = typenames.GetEnumDeclaration(aidl);
      enum_decl != nullptr) {
    const string& backing_type_name = enum_decl->GetBackingType().GetName();
    CHECK(m.find(backing_type_name) != m.end());
    CHECK(AidlTypenames::IsBuiltinTypename(backing_type_name));
    return m[backing_type_name];
  }

  const string& aidl_name = aidl.GetName();
  if (boxing && AidlTypenames::IsPrimitiveTypename(aidl_name)) {
    // Every primitive type must have the corresponding boxing type
    CHECK(boxing_types.find(aidl_name) != m.end());
    return boxing_types[aidl_name];
  }
  if (m.find(aidl_name) != m.end()) {
    CHECK(AidlTypenames::IsBuiltinTypename(aidl_name));
    return m[aidl_name];
  } else {
    // 'foo.bar.IFoo' in AIDL maps to 'foo.bar.IFoo' in Java
    return aidl_name;
  }
}

namespace {
string JavaSignatureOfInternal(
    const AidlTypeSpecifier& aidl, const AidlTypenames& typenames, bool instantiable,
    bool omit_array, bool boxing = false /* boxing can be true only if it is a type parameter */) {
  string ret = JavaNameOf(aidl, typenames, instantiable, boxing && !aidl.IsArray());
  if (aidl.IsGeneric()) {
    vector<string> arg_names;
    for (const auto& ta : aidl.GetTypeParameters()) {
      arg_names.emplace_back(
          JavaSignatureOfInternal(*ta, typenames, false, false, true /* boxing */));
    }
    ret += "<" + Join(arg_names, ",") + ">";
  }
  if (aidl.IsArray() && !omit_array) {
    ret += "[]";
  }
  return ret;
}

// Returns the name of the backing type for the specified type. Note: this
// returns type names as used in AIDL, not a Java signature.
// For enums, this is the enum's backing type.
// For all other types, this is the type itself.
string AidlBackingTypeName(const AidlTypeSpecifier& type, const AidlTypenames& typenames) {
  string type_name;
  if (const AidlEnumDeclaration* enum_decl = typenames.GetEnumDeclaration(type);
      enum_decl != nullptr) {
    type_name = enum_decl->GetBackingType().GetName();
  } else {
    type_name = type.GetName();
  }
  if (type.IsArray()) {
    type_name += "[]";
  }
  return type_name;
}

}  // namespace

string JavaSignatureOf(const AidlTypeSpecifier& aidl, const AidlTypenames& typenames) {
  return JavaSignatureOfInternal(aidl, typenames, false, false);
}

string InstantiableJavaSignatureOf(const AidlTypeSpecifier& aidl, const AidlTypenames& typenames) {
  return JavaSignatureOfInternal(aidl, typenames, true, true);
}

string DefaultJavaValueOf(const AidlTypeSpecifier& aidl, const AidlTypenames& typenames) {
  static map<string, string> m = {
      {"boolean", "false"}, {"byte", "0"},     {"char", R"('\u0000')"}, {"int", "0"},
      {"long", "0L"},       {"float", "0.0f"}, {"double", "0.0d"},
  };

  const string name = AidlBackingTypeName(aidl, typenames);
  CHECK(name != "void");

  if (!aidl.IsArray() && m.find(name) != m.end()) {
    CHECK(AidlTypenames::IsBuiltinTypename(name));
    return m[name];
  } else {
    return "null";
  }
}

static string GetFlagFor(const CodeGeneratorContext& c) {
  if (c.is_return_value) {
    return "android.os.Parcelable.PARCELABLE_WRITE_RETURN_VALUE";
  } else {
    return "0";
  }
}

bool WriteToParcelFor(const CodeGeneratorContext& c) {
  static map<string, function<void(const CodeGeneratorContext&)>> method_map{
      {"boolean",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeInt(((" << c.var << ")?(1):(0)));\n";
       }},
      {"boolean[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeBooleanArray(" << c.var << ");\n";
       }},
      {"byte",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeByte(" << c.var << ");\n";
       }},
      {"byte[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeByteArray(" << c.var << ");\n";
       }},
      {"char",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeInt(((int)" << c.var << "));\n";
       }},
      {"char[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeCharArray(" << c.var << ");\n";
       }},
      {"int",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeInt(" << c.var << ");\n";
       }},
      {"int[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeIntArray(" << c.var << ");\n";
       }},
      {"long",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeLong(" << c.var << ");\n";
       }},
      {"long[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeLongArray(" << c.var << ");\n";
       }},
      {"float",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeFloat(" << c.var << ");\n";
       }},
      {"float[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeFloatArray(" << c.var << ");\n";
       }},
      {"double",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeDouble(" << c.var << ");\n";
       }},
      {"double[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeDoubleArray(" << c.var << ");\n";
       }},
      {"String",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeString(" << c.var << ");\n";
       }},
      {"String[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeStringArray(" << c.var << ");\n";
       }},
      {"List",
       [](const CodeGeneratorContext& c) {
         if (c.type.IsGeneric()) {
           const string& contained_type = c.type.GetTypeParameters().at(0)->GetName();
           if (AidlTypenames::IsBuiltinTypename(contained_type)) {
             if (contained_type == "String") {
               c.writer << c.parcel << ".writeStringList(" << c.var << ");\n";
             } else if (contained_type == "IBinder") {
               c.writer << c.parcel << ".writeBinderList(" << c.var << ");\n";
             }
           } else {
             const AidlDefinedType* t = c.typenames.TryGetDefinedType(contained_type);
             CHECK(t != nullptr) << "Unknown type: " << contained_type << endl;
             if (t->AsParcelable() != nullptr) {
               c.writer << c.parcel << ".writeTypedList(" << c.var << ");\n";
             }
           }
         } else {
           c.writer << c.parcel << ".writeList(" << c.var << ");\n";
         }
       }},
      {"Map",
       [](const CodeGeneratorContext& c) {
         if (c.type.IsGeneric()) {
           c.writer << "if (" << c.var << " == null) {\n";
           c.writer.Indent();
           c.writer << c.parcel << ".writeInt(-1);\n";
           c.writer.Dedent();
           c.writer << "} else {\n";
           c.writer.Indent();
           c.writer << c.parcel << ".writeInt(" << c.var << ".size());\n";
           c.writer << c.var << ".forEach((k, v) -> {\n";
           c.writer.Indent();
           c.writer << c.parcel << ".writeString(k);\n";

           CodeGeneratorContext value_context{
               c.writer,
               c.typenames,
               *c.type.GetTypeParameters()[1].get(),
               c.parcel,
               "v",
               c.is_return_value,
               c.is_classloader_created,
               c.filename,
           };
           WriteToParcelFor(value_context);
           c.writer.Dedent();
           c.writer << "});\n";

           c.writer.Dedent();
           c.writer << "}\n";
         } else {
           c.writer << c.parcel << ".writeMap(" << c.var << ");\n";
         }
       }},
      {"IBinder",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeStrongBinder(" << c.var << ");\n";
       }},
      {"IBinder[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeBinderArray(" << c.var << ");\n";
       }},
      {"FileDescriptor",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeRawFileDescriptor(" << c.var << ");\n";
       }},
      {"FileDescriptor[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeRawFileDescriptorArray(" << c.var << ");\n";
       }},
      {"ParcelFileDescriptor",
       [](const CodeGeneratorContext& c) {
         // This is same as writeTypedObject which was introduced with SDK 23.
         // Keeping below code so that the generated code is buildable with older SDK.
         c.writer << "if ((" << c.var << "!=null)) {\n";
         c.writer.Indent();
         c.writer << c.parcel << ".writeInt(1);\n";
         c.writer << c.var << ".writeToParcel(" << c.parcel << ", " << GetFlagFor(c) << ");\n";
         c.writer.Dedent();
         c.writer << "}\n";
         c.writer << "else {\n";
         c.writer.Indent();
         c.writer << c.parcel << ".writeInt(0);\n";
         c.writer.Dedent();
         c.writer << "}\n";
       }},
      {"ParcelFileDescriptor[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".writeTypedArray(" << c.var << ", " << GetFlagFor(c) << ");\n";
       }},
      {"CharSequence",
       [](const CodeGeneratorContext& c) {
         // TextUtils.writeToParcel does not accept null. So, we need to handle
         // the case here.
         c.writer << "if (" << c.var << "!=null) {\n";
         c.writer.Indent();
         c.writer << c.parcel << ".writeInt(1);\n";
         c.writer << "android.text.TextUtils.writeToParcel(" << c.var << ", " << c.parcel << ", "
                  << GetFlagFor(c) << ");\n";
         c.writer.Dedent();
         c.writer << "}\n";
         c.writer << "else {\n";
         c.writer.Indent();
         c.writer << c.parcel << ".writeInt(0);\n";
         c.writer.Dedent();
         c.writer << "}\n";
       }},
  };
  const string type_name = AidlBackingTypeName(c.type, c.typenames);
  const auto found = method_map.find(type_name);
  if (found != method_map.end()) {
    found->second(c);
  } else {
    const AidlDefinedType* t = c.typenames.TryGetDefinedType(c.type.GetName());
    CHECK(t != nullptr) << "Unknown type: " << c.type.GetName() << endl;
    if (t->AsInterface() != nullptr) {
      if (!c.type.IsArray()) {
        // Why don't we use writeStrongInterface which does the exact same thing?
        // Keeping below code just not to break unit tests.
        c.writer << c.parcel << ".writeStrongBinder((((" << c.var << "!=null))?"
                 << "(" << c.var << ".asBinder()):(null)));\n";
      }
    } else if (t->AsParcelable() != nullptr) {
      if (c.type.IsArray()) {
        c.writer << c.parcel << ".writeTypedArray(" << c.var << ", " << GetFlagFor(c) << ");\n";
      } else {
        // This is same as writeTypedObject.
        // Keeping below code just not to break tests.
        c.writer << "if ((" << c.var << "!=null)) {\n";
        c.writer.Indent();
        c.writer << c.parcel << ".writeInt(1);\n";
        c.writer << c.var << ".writeToParcel(" << c.parcel << ", " << GetFlagFor(c) << ");\n";
        c.writer.Dedent();
        c.writer << "}\n";
        c.writer << "else {\n";
        c.writer.Indent();
        c.writer << c.parcel << ".writeInt(0);\n";
        c.writer.Dedent();
        c.writer << "}\n";
      }
    }
  }
  return true;
}

// Ensures that a variable is initialized to refer to the classloader
// of the current object and returns the name of the variable.
static string EnsureAndGetClassloader(CodeGeneratorContext& c) {
  CHECK(c.is_classloader_created != nullptr);
  if (!*(c.is_classloader_created)) {
    c.writer << "java.lang.ClassLoader cl = "
             << "(java.lang.ClassLoader)this.getClass().getClassLoader();\n";
    *(c.is_classloader_created) = true;
  }
  return "cl";
}

bool CreateFromParcelFor(const CodeGeneratorContext& c) {
  static map<string, function<void(const CodeGeneratorContext&)>> method_map{
      {"boolean",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = (0!=" << c.parcel << ".readInt());\n";
       }},
      {"boolean[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createBooleanArray();\n";
       }},
      {"byte",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readByte();\n";
       }},
      {"byte[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createByteArray();\n";
       }},
      {"char",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = (char)" << c.parcel << ".readInt();\n";
       }},
      {"char[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createCharArray();\n";
       }},
      {"int",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readInt();\n";
       }},
      {"int[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createIntArray();\n";
       }},
      {"long",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readLong();\n";
       }},
      {"long[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createLongArray();\n";
       }},
      {"float",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readFloat();\n";
       }},
      {"float[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createFloatArray();\n";
       }},
      {"double",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readDouble();\n";
       }},
      {"double[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createDoubleArray();\n";
       }},
      {"String",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readString();\n";
       }},
      {"String[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createStringArray();\n";
       }},
      {"List",
       [](const CodeGeneratorContext& c) {
         if (c.type.IsGeneric()) {
           const string& contained_type = c.type.GetTypeParameters().at(0)->GetName();
           if (AidlTypenames::IsBuiltinTypename(contained_type)) {
             if (contained_type == "String") {
               c.writer << c.var << " = " << c.parcel << ".createStringArrayList();\n";
             } else if (contained_type == "IBinder") {
               c.writer << c.var << " = " << c.parcel << ".createBinderArrayList();\n";
             }
           } else {
             const AidlDefinedType* t = c.typenames.TryGetDefinedType(contained_type);
             CHECK(t != nullptr) << "Unknown type: " << contained_type << endl;
             if (t->AsParcelable() != nullptr) {
               c.writer << c.var << " = " << c.parcel << ".createTypedArrayList("
                        << JavaNameOf(*(c.type.GetTypeParameters().at(0)), c.typenames)
                        << ".CREATOR);\n";
             }
           }
         } else {
           const string classloader = EnsureAndGetClassloader(const_cast<CodeGeneratorContext&>(c));
           c.writer << c.var << " = " << c.parcel << ".readArrayList(" << classloader << ");\n";
         }
       }},
      {"Map",
       [](const CodeGeneratorContext& c) {
         if (c.type.IsGeneric()) {
           c.writer << "{\n";
           c.writer.Indent();
           c.writer << "int N = " << c.parcel << ".readInt();\n";
           c.writer << c.var << " = N < 0 ? null : new java.util.HashMap<>();\n";

           auto creator = JavaNameOf(*(c.type.GetTypeParameters().at(1)), c.typenames) + ".CREATOR";
           c.writer << "java.util.stream.IntStream.range(0, N).forEach(i -> {\n";
           c.writer.Indent();
           c.writer << "String k = " << c.parcel << ".readString();\n";
           c.writer << JavaNameOf(*(c.type.GetTypeParameters().at(1)), c.typenames) << " v;\n";
           CodeGeneratorContext value_context{
               c.writer,
               c.typenames,
               *c.type.GetTypeParameters()[1].get(),
               c.parcel,
               "v",
               c.is_return_value,
               c.is_classloader_created,
               c.filename,
           };
           CreateFromParcelFor(value_context);
           c.writer << c.var << ".put(k, v);\n";

           c.writer.Dedent();
           c.writer << "});\n";

           c.writer.Dedent();
           c.writer << "}\n";
         } else {
           const string classloader = EnsureAndGetClassloader(const_cast<CodeGeneratorContext&>(c));
           c.writer << c.var << " = " << c.parcel << ".readHashMap(" << classloader << ");\n";
         }
       }},
      {"IBinder",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readStrongBinder();\n";
       }},
      {"IBinder[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createBinderArray();\n";
       }},
      {"FileDescriptor",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".readRawFileDescriptor();\n";
       }},
      {"FileDescriptor[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createRawFileDescriptorArray();\n";
       }},
      {"ParcelFileDescriptor",
       [](const CodeGeneratorContext& c) {
         // This is same as readTypedObject which was introduced with SDK 23.
         // Keeping below code so that the generated code is buildable with older SDK.
         c.writer << "if ((0!=" << c.parcel << ".readInt())) {\n";
         c.writer.Indent();
         c.writer << c.var << " = " << "android.os.ParcelFileDescriptor.CREATOR.createFromParcel(" << c.parcel
                  << ");\n";
         c.writer.Dedent();
         c.writer << "}\n";
         c.writer << "else {\n";
         c.writer.Indent();
         c.writer << c.var << " = null;\n";
         c.writer.Dedent();
         c.writer << "}\n";
       }},
      {"ParcelFileDescriptor[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel
                  << ".createTypedArray(android.os.ParcelFileDescriptor.CREATOR);\n";
       }},
      {"CharSequence",
       [](const CodeGeneratorContext& c) {
         // We have written 0 for null CharSequence.
         c.writer << "if (0!=" << c.parcel << ".readInt()) {\n";
         c.writer.Indent();
         c.writer << c.var << " = android.text.TextUtils.CHAR_SEQUENCE_CREATOR.createFromParcel("
                  << c.parcel << ");\n";
         c.writer.Dedent();
         c.writer << "}\n";
         c.writer << "else {\n";
         c.writer.Indent();
         c.writer << c.var << " = null;\n";
         c.writer.Dedent();
         c.writer << "}\n";
       }},
  };
  const auto found = method_map.find(AidlBackingTypeName(c.type, c.typenames));
  if (found != method_map.end()) {
    found->second(c);
  } else {
    const AidlDefinedType* t = c.typenames.TryGetDefinedType(c.type.GetName());
    CHECK(t != nullptr) << "Unknown type: " << c.type.GetName() << endl;
    if (t->AsInterface() != nullptr) {
      if (!c.type.IsArray()) {
        c.writer << c.var << " = " << c.type.GetName() << ".Stub.asInterface(" << c.parcel
                 << ".readStrongBinder());\n";
      }
    } else if (t->AsParcelable() != nullptr || t->AsStructuredParcelable() != nullptr) {
      if (c.type.IsArray()) {
        c.writer << c.var << " = " << c.parcel << ".createTypedArray("
                 << JavaNameOf(c.type, c.typenames) << ".CREATOR);\n";
      } else {
        // This is same as readTypedObject.
        // Keeping below code just not to break unit tests.
        c.writer << "if ((0!=" << c.parcel << ".readInt())) {\n";
        c.writer.Indent();
        c.writer << c.var << " = " << c.type.GetName() << ".CREATOR.createFromParcel(" << c.parcel
                 << ");\n";
        c.writer.Dedent();
        c.writer << "}\n";
        c.writer << "else {\n";
        c.writer.Indent();
        c.writer << c.var << " = null;\n";
        c.writer.Dedent();
        c.writer << "}\n";
      }
    }
  }
  return true;
}

bool ReadFromParcelFor(const CodeGeneratorContext& c) {
  static map<string, function<void(const CodeGeneratorContext&)>> method_map{
      {"boolean[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readBooleanArray(" << c.var << ");\n";
       }},
      {"byte[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readByteArray(" << c.var << ");\n";
       }},
      {"char[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readCharArray(" << c.var << ");\n";
       }},
      {"int[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readIntArray(" << c.var << ");\n";
       }},
      {"long[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readLongArray(" << c.var << ");\n";
       }},
      {"float[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readFloatArray(" << c.var << ");\n";
       }},
      {"double[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readDoubleArray(" << c.var << ");\n";
       }},
      {"String[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readStringArray(" << c.var << ");\n";
       }},
      {"List",
       [](const CodeGeneratorContext& c) {
         if (c.type.IsGeneric()) {
           const string& contained_type = c.type.GetTypeParameters().at(0)->GetName();
           if (AidlTypenames::IsBuiltinTypename(contained_type)) {
             if (contained_type == "String") {
               c.writer << c.parcel << ".readStringList(" << c.var << ");\n";
             } else if (contained_type == "IBinder") {
               c.writer << c.parcel << ".readBinderList(" << c.var << ");\n";
             }
           } else {
             const AidlDefinedType* t = c.typenames.TryGetDefinedType(contained_type);
             CHECK(t != nullptr) << "Unknown type: " << contained_type << endl;
             if (t->AsParcelable() != nullptr) {
               c.writer << c.parcel << ".readTypedList(" << c.var << ", "
                        << JavaNameOf(*(c.type.GetTypeParameters().at(0)), c.typenames)
                        << ".CREATOR);\n";
             }
           }
         } else {
           const string classloader = EnsureAndGetClassloader(const_cast<CodeGeneratorContext&>(c));
           c.writer << c.parcel << ".readList(" << c.var << ", " << classloader << ");\n";
         }
       }},
      {"Map",
       [](const CodeGeneratorContext& c) {
         if (c.type.IsGeneric()) {
           c.writer << "if (" << c.var << " != null) " << c.var << ".clear();\n";
           c.writer << "java.util.stream.IntStream.range(0, " << c.parcel
                    << ".readInt()).forEach(i -> {\n";
           c.writer.Indent();
           c.writer << "String k = " << c.parcel << ".readString();\n";
           c.writer << JavaNameOf(*(c.type.GetTypeParameters().at(1)), c.typenames) << " v;\n";
           CodeGeneratorContext value_context{
               c.writer,
               c.typenames,
               *c.type.GetTypeParameters()[1].get(),
               c.parcel,
               "v",
               c.is_return_value,
               c.is_classloader_created,
               c.filename,
           };
           CreateFromParcelFor(value_context);
           c.writer << c.var << ".put(k, v);\n";

           c.writer.Dedent();
           c.writer << "});\n";

           c.writer.Dedent();
           c.writer << "}\n";
         } else {
           const string classloader = EnsureAndGetClassloader(const_cast<CodeGeneratorContext&>(c));
           c.writer << c.var << " = " << c.parcel << ".readHashMap(" << classloader << ");\n";
         }
       }},
      {"IBinder[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createBinderArray();\n";
       }},
      {"FileDescriptor[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.var << " = " << c.parcel << ".createRawFileDescriptorArray();\n";
       }},
      {"ParcelFileDescriptor",
       [](const CodeGeneratorContext& c) {
         c.writer << "if ((0!=" << c.parcel << ".readInt())) {\n";
         c.writer.Indent();
         c.writer << c.var << " = " << "android.os.ParcelFileDescriptor.CREATOR.createFromParcel(" << c.parcel << ");\n";
         c.writer.Dedent();
         c.writer << "}\n";
       }},
      {"ParcelFileDescriptor[]",
       [](const CodeGeneratorContext& c) {
         c.writer << c.parcel << ".readTypedArray(" << c.var
                  << ", android.os.ParcelFileDescriptor.CREATOR);\n";
       }},
  };
  const auto& found = method_map.find(AidlBackingTypeName(c.type, c.typenames));
  if (found != method_map.end()) {
    found->second(c);
  } else {
    const AidlDefinedType* t = c.typenames.TryGetDefinedType(c.type.GetName());
    CHECK(t != nullptr) << "Unknown type: " << c.type.GetName() << endl;
    if (t->AsParcelable() != nullptr) {
      if (c.type.IsArray()) {
        c.writer << c.parcel << ".readTypedArray(" << c.var << ", " << c.type.GetName()
                 << ".CREATOR);\n";
      } else {
        c.writer << "if ((0!=" << c.parcel << ".readInt())) {\n";
        c.writer.Indent();
        c.writer << c.var << ".readFromParcel(" << c.parcel << ");\n";
        c.writer.Dedent();
        c.writer << "}\n";
      }
    }
  }
  return true;
}

}  // namespace java
}  // namespace aidl
}  // namespace android
