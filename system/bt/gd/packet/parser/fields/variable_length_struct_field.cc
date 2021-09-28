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

#include "fields/variable_length_struct_field.h"
#include "util.h"

const std::string VariableLengthStructField::kFieldType = "VariableLengthStructField";

VariableLengthStructField::VariableLengthStructField(std::string name, std::string type_name, ParseLocation loc)
    : PacketField(name, loc), type_name_(type_name) {}

const std::string& VariableLengthStructField::GetFieldType() const {
  return VariableLengthStructField::kFieldType;
}

Size VariableLengthStructField::GetSize() const {
  return Size();
}

Size VariableLengthStructField::GetBuilderSize() const {
  std::string ret = "(" + GetName() + "_->size() * 8) ";
  return ret;
}

std::string VariableLengthStructField::GetDataType() const {
  std::string ret = "std::unique_ptr<" + type_name_ + ">";
  return ret;
}

void VariableLengthStructField::GenExtractor(std::ostream& s, int, bool) const {
  s << GetName() << "_ptr = Parse" << type_name_ << "(" << GetName() << "_it);";
  s << "if (" << GetName() << "_ptr != nullptr) {";
  s << GetName() << "_it = " << GetName() << "_it + " << GetName() << "_ptr->size();";
  s << "} else {";
  s << GetName() << "_it = " << GetName() << "_it + " << GetName() << "_it.NumBytesRemaining();";
  s << "}";
}

std::string VariableLengthStructField::GetGetterFunctionName() const {
  std::stringstream ss;
  ss << "Get" << util::UnderscoreToCamelCase(GetName());
  return ss.str();
}

void VariableLengthStructField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << GetDataType() << " " << GetGetterFunctionName() << "() const {";
  s << "ASSERT(was_validated_);";
  s << "size_t end_index = size();";
  s << "auto to_bound = begin();";
  int num_leading_bits = GenBounds(s, start_offset, end_offset, GetSize());
  s << GetDataType() << " " << GetName() << "_ptr;";
  GenExtractor(s, num_leading_bits, false);
  s << "return " << GetName() << "_ptr;";
  s << "}\n";
}

std::string VariableLengthStructField::GetBuilderParameterType() const {
  return GetDataType();
}

bool VariableLengthStructField::BuilderParameterMustBeMoved() const {
  return true;
}

bool VariableLengthStructField::HasParameterValidator() const {
  return false;
}

void VariableLengthStructField::GenParameterValidator(std::ostream&) const {
  // Validated at compile time.
}

void VariableLengthStructField::GenInserter(std::ostream& s) const {
  s << GetName() << "_->Serialize(i);";
}

void VariableLengthStructField::GenValidator(std::ostream&) const {
  // Do nothing
}
