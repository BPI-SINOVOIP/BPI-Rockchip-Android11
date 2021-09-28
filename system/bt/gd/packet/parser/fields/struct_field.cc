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

#include "fields/struct_field.h"
#include "util.h"

const std::string StructField::kFieldType = "StructField";

StructField::StructField(std::string name, std::string type_name, Size size, ParseLocation loc)
    : PacketField(name, loc), type_name_(type_name), size_(size) {}

const std::string& StructField::GetFieldType() const {
  return StructField::kFieldType;
}

Size StructField::GetSize() const {
  return size_;
}

Size StructField::GetBuilderSize() const {
  std::string ret = "(" + GetName() + "_.size() * 8)";
  return ret;
}

std::string StructField::GetDataType() const {
  return type_name_;
}

void StructField::GenExtractor(std::ostream& s, int, bool) const {
  s << GetName() << "_it = ";
  s << GetDataType() << "::Parse(" << GetName() << "_ptr, " << GetName() << "_it);";
}

std::string StructField::GetGetterFunctionName() const {
  std::stringstream ss;
  ss << "Get" << util::UnderscoreToCamelCase(GetName());
  return ss.str();
}

void StructField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << GetDataType() << " " << GetGetterFunctionName() << "() const {";
  s << "ASSERT(was_validated_);";
  s << "size_t end_index = size();";
  s << "auto to_bound = begin();";
  int num_leading_bits = GenBounds(s, start_offset, end_offset, GetSize());
  s << GetDataType() << " " << GetName() << "_value;";
  s << GetDataType() << "* " << GetName() << "_ptr = &" << GetName() << "_value;";
  GenExtractor(s, num_leading_bits, false);

  s << "return " << GetName() << "_value;";
  s << "}\n";
}

std::string StructField::GetBuilderParameterType() const {
  return GetDataType();
}

bool StructField::HasParameterValidator() const {
  return false;
}

void StructField::GenParameterValidator(std::ostream&) const {
  // Validated at compile time.
}

void StructField::GenInserter(std::ostream& s) const {
  s << GetName() << "_.Serialize(i);";
}

void StructField::GenValidator(std::ostream&) const {
  // Do nothing
}
