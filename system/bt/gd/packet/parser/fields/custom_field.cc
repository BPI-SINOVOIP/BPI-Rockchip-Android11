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

#include "fields/custom_field.h"
#include "util.h"

const std::string CustomField::kFieldType = "CustomField";

CustomField::CustomField(std::string name, std::string type_name, ParseLocation loc)
    : PacketField(name, loc), type_name_(type_name) {}

const std::string& CustomField::GetFieldType() const {
  return CustomField::kFieldType;
}

Size CustomField::GetSize() const {
  return Size();
}

Size CustomField::GetBuilderSize() const {
  std::string ret = "(" + GetName() + "_.size() * 8) ";
  return ret;
}

std::string CustomField::GetDataType() const {
  return type_name_;
}

void CustomField::GenExtractor(std::ostream& s, int, bool) const {
  s << "auto optional_it = ";
  s << GetDataType() << "::Parse( " << GetName() << "_ptr, " << GetName() << "_it);";
  s << "if (optional_it) {";
  s << GetName() << "_it = *optional_it;";
  s << "} else {";
  s << GetName() << "_it = " << GetName() << "_it + " << GetName() << "_it.NumBytesRemaining();";
  s << GetName() << "_ptr = nullptr;";
  s << "}";
}

std::string CustomField::GetGetterFunctionName() const {
  std::stringstream ss;
  ss << "Get" << util::UnderscoreToCamelCase(GetName());
  return ss.str();
}

void CustomField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << "std::unique_ptr<" << GetDataType() << "> " << GetGetterFunctionName() << "() const {";
  s << "ASSERT(was_validated_);";
  s << "size_t end_index = size();";
  s << "auto to_bound = begin();";

  int num_leading_bits = GenBounds(s, start_offset, end_offset, GetSize());
  s << "std::unique_ptr<" << GetDataType() << "> " << GetName() << "_value";
  s << " = std::make_unique<" << GetDataType() << ">();";
  s << GetDataType() << "* " << GetName() << "_ptr = " << GetName() << "_value.get();";
  GenExtractor(s, num_leading_bits, false);
  s << "if (" << GetName() << "_ptr == nullptr) {" << GetName() << "_value.reset(); }";
  s << "return " << GetName() << "_value;";
  s << "}\n";
}

std::string CustomField::GetBuilderParameterType() const {
  return GetDataType();
}

bool CustomField::HasParameterValidator() const {
  return false;
}

void CustomField::GenParameterValidator(std::ostream&) const {
  // Do nothing.
}

void CustomField::GenInserter(std::ostream& s) const {
  s << GetName() << "_.Serialize(i);";
}

void CustomField::GenValidator(std::ostream&) const {
  // Do nothing.
}
