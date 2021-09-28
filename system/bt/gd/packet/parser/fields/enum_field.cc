/*
 * Copyright 2019 The Android Open Source Project
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

#include "fields/enum_field.h"

#include "util.h"

const std::string EnumField::kFieldType = "EnumField";

EnumField::EnumField(std::string name, EnumDef enum_def, std::string value, ParseLocation loc)
    : ScalarField(name, enum_def.size_, loc), enum_def_(enum_def), value_(value) {}

const std::string& EnumField::GetFieldType() const {
  return EnumField::kFieldType;
}

EnumDef EnumField::GetEnumDef() {
  return enum_def_;
}

std::string EnumField::GetDataType() const {
  return enum_def_.name_;
}

bool EnumField::HasParameterValidator() const {
  return false;
}

void EnumField::GenParameterValidator(std::ostream&) const {
  // Validated at compile time.
}

void EnumField::GenInserter(std::ostream& s) const {
  s << "insert(static_cast<" << util::GetTypeForSize(GetSize().bits()) << ">(";
  s << GetName() << "_), i, " << GetSize().bits() << ");";
}

void EnumField::GenValidator(std::ostream&) const {
  // Do nothing
}
