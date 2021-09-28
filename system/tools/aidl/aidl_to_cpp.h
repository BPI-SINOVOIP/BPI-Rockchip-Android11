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
#pragma once

#include "aidl_language.h"

namespace android {
namespace aidl {
namespace cpp {

// This header provides functions that translate AIDL things to cpp things.

std::string ConstantValueDecorator(const AidlTypeSpecifier& type, const std::string& raw_value);

struct CodeGeneratorContext {
  CodeWriter& writer;

  const AidlTypenames& types;
  const AidlTypeSpecifier& type;  // an argument or return type to generate code for
  const string name;              // name of the variable for the argument or the return value
  const bool isPointer;           // whether the variable 'name' is a pointer or not
  const string log;               // name of the variable of type Json::Value to write the log into
};

std::string GetTransactionIdFor(const AidlMethod& method);

std::string CppNameOf(const AidlTypeSpecifier& type, const AidlTypenames& typenames);

bool IsNonCopyableType(const AidlTypeSpecifier& type, const AidlTypenames& typenames);

// Returns the name of the Parcel method suitable for reading data of the
// given type.
std::string ParcelReadMethodOf(const AidlTypeSpecifier& type, const AidlTypenames& typenames);

// Returns the potentially-cast representation of the provided variable name,
// suitable for being passed to a method from ParcelReadMethodOf.
std::string ParcelReadCastOf(const AidlTypeSpecifier& type, const AidlTypenames& typenames,
                             const std::string& variable_name);

// Returns the name of the Parcel method suitable for writing data of the
// given type.
std::string ParcelWriteMethodOf(const AidlTypeSpecifier& type, const AidlTypenames& typenames);

// Returns the potentially-cast representation of the provided variable name,
// suitable for being passed to a method from ParcelWriteMethodOf.
std::string ParcelWriteCastOf(const AidlTypeSpecifier& type, const AidlTypenames& typenames,
                              const std::string& variable_name);

void AddHeaders(const AidlTypeSpecifier& type, const AidlTypenames& typenames,
                std::set<std::string>& headers);

void AddHeaders(const AidlDefinedType& parcelable, std::set<std::string>& headers);
}  // namespace cpp
}  // namespace aidl
}  // namespace android
