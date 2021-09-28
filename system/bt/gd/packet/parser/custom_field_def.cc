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

#include "custom_field_def.h"

#include "util.h"

CustomFieldDef::CustomFieldDef(std::string name, std::string include) : TypeDef(name), include_(include) {}

CustomFieldDef::CustomFieldDef(std::string name, std::string include, int size)
    : TypeDef(name, size), include_(include) {
  if (size % 8 != 0) {
    ERROR() << "Custom fields must be byte aligned.";
  }
}

PacketField* CustomFieldDef::GetNewField(const std::string& name, ParseLocation loc) const {
  if (size_ == -1) {
    return new CustomField(name, name_, loc);
  } else {
    return new CustomFieldFixedSize(name, name_, size_, loc);
  }
}

TypeDef::Type CustomFieldDef::GetDefinitionType() const {
  return TypeDef::Type::CUSTOM;
}

void CustomFieldDef::GenInclude(std::ostream& s) const {
  s << "#include \"" << include_ << util::CamelCaseToUnderScore(GetTypeName()) << ".h\"\n";
}

void CustomFieldDef::GenPyBind11Include(std::ostream& s) const {
  s << "#include \"" << include_ << util::CamelCaseToUnderScore(GetTypeName()) << "_pybind11_type_caster.h\"\n";
}

void CustomFieldDef::GenUsing(std::ostream& s) const {
  s << "using ::bluetooth::";
  for (const auto& c : include_) {
    switch (c) {
      case '/':
        s << "::";
        break;
      default:
        s << c;
    }
  }
  s << GetTypeName() << ";";
}

void CustomFieldDef::GenCustomFieldCheck(std::ostream& s, bool little_endian) const {
  s << "static_assert(CustomTypeChecker<" << name_ << ", ";
  s << (little_endian ? "" : "!") << "kLittleEndian>::value, \"";
  s << name_ << " is not a valid custom field type. Please see README for more details.\");";
}
