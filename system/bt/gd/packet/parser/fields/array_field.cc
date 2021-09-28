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

#include "fields/array_field.h"

#include "fields/custom_field.h"
#include "fields/scalar_field.h"
#include "util.h"

const std::string ArrayField::kFieldType = "ArrayField";

ArrayField::ArrayField(std::string name, int element_size, int array_size, ParseLocation loc)
    : PacketField(name, loc), element_field_(new ScalarField("val", element_size, loc)), element_size_(element_size),
      array_size_(array_size) {
  if (element_size > 64 || element_size < 0)
    ERROR(this) << __func__ << ": Not implemented for element size = " << element_size;
  if (element_size % 8 != 0) {
    ERROR(this) << "Can only have arrays with elements that are byte aligned (" << element_size << ")";
  }
}

ArrayField::ArrayField(std::string name, TypeDef* type_def, int array_size, ParseLocation loc)
    : PacketField(name, loc), element_field_(type_def->GetNewField("val", loc)),
      element_size_(element_field_->GetSize()), array_size_(array_size) {
  if (!element_size_.empty() && element_size_.bits() % 8 != 0) {
    ERROR(this) << "Can only have arrays with elements that are byte aligned (" << element_size_ << ")";
  }
}

const std::string& ArrayField::GetFieldType() const {
  return ArrayField::kFieldType;
}

Size ArrayField::GetSize() const {
  if (!element_size_.empty() && !element_size_.has_dynamic()) {
    return Size(array_size_ * element_size_.bits());
  }
  return Size();
}

Size ArrayField::GetBuilderSize() const {
  if (!element_size_.empty() && !element_size_.has_dynamic()) {
    return GetSize();
  } else if (element_field_->BuilderParameterMustBeMoved()) {
    std::string ret = "[this](){ size_t length = 0; for (const auto& elem : " + GetName() +
                      "_) { length += elem->size() * 8; } return length; }()";
    return ret;
  } else {
    std::string ret = "[this](){ size_t length = 0; for (const auto& elem : " + GetName() +
                      "_) { length += elem.size() * 8; } return length; }()";
    return ret;
  }
}

Size ArrayField::GetStructSize() const {
  if (!element_size_.empty() && !element_size_.has_dynamic()) {
    return GetSize();
  } else if (element_field_->BuilderParameterMustBeMoved()) {
    std::string ret = "[this](){ size_t length = 0; for (const auto& elem : to_fill->" + GetName() +
                      "_) { length += elem->size() * 8; } return length; }()";
    return ret;
  } else {
    std::string ret = "[this](){ size_t length = 0; for (const auto& elem : to_fill->" + GetName() +
                      "_) { length += elem.size() * 8; } return length; }()";
    return ret;
  }
}

std::string ArrayField::GetDataType() const {
  return "std::array<" + element_field_->GetDataType() + "," + std::to_string(array_size_) + ">";
}

void ArrayField::GenExtractor(std::ostream& s, int num_leading_bits, bool for_struct) const {
  s << GetDataType() << "::iterator ret_it = " << GetName() << "_ptr->begin();";
  s << "auto " << element_field_->GetName() << "_it = " << GetName() << "_it;";
  if (!element_size_.empty()) {
    s << "while (" << element_field_->GetName() << "_it.NumBytesRemaining() >= " << element_size_.bytes();
    s << " && ret_it < " << GetName() << "_ptr->end()) {";
  } else {
    s << "while (" << element_field_->GetName() << "_it.NumBytesRemaining() > 0 ";
    s << " && ret_it < " << GetName() << "_ptr->end()) {";
  }
  if (element_field_->BuilderParameterMustBeMoved()) {
    s << element_field_->GetDataType() << " " << element_field_->GetName() << "_ptr;";
  } else {
    s << element_field_->GetDataType() << "* " << element_field_->GetName() << "_ptr = ret_it;";
  }
  element_field_->GenExtractor(s, num_leading_bits, for_struct);
  if (element_field_->BuilderParameterMustBeMoved()) {
    s << "*ret_it = std::move(" << element_field_->GetName() << "_ptr);";
  }
  s << "ret_it++;";
  s << "}";
}

std::string ArrayField::GetGetterFunctionName() const {
  std::stringstream ss;
  ss << "Get" << util::UnderscoreToCamelCase(GetName());
  return ss.str();
}

void ArrayField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << GetDataType() << " " << GetGetterFunctionName() << "() {";
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

std::string ArrayField::GetBuilderParameterType() const {
  std::stringstream ss;
  if (element_field_->BuilderParameterMustBeMoved()) {
    ss << "std::array<" << element_field_->GetDataType() << "," << array_size_ << ">";
  } else {
    ss << "const std::array<" << element_field_->GetDataType() << "," << array_size_ << ">&";
  }
  return ss.str();
}

bool ArrayField::BuilderParameterMustBeMoved() const {
  return element_field_->BuilderParameterMustBeMoved();
}

bool ArrayField::GenBuilderMember(std::ostream& s) const {
  s << "std::array<" << element_field_->GetDataType() << "," << array_size_ << "> " << GetName();
  return true;
}

bool ArrayField::HasParameterValidator() const {
  return false;
}

void ArrayField::GenParameterValidator(std::ostream&) const {
  // Array length is validated by the compiler
}

void ArrayField::GenInserter(std::ostream& s) const {
  s << "for (const auto& val_ : " << GetName() << "_) {";
  element_field_->GenInserter(s);
  s << "}\n";
}

void ArrayField::GenValidator(std::ostream&) const {
  // NOTE: We could check if the element size divides cleanly into the array size, but we decided to forgo that
  // in favor of just returning as many elements as possible in a best effort style.
  //
  // Other than that there is nothing that arrays need to be validated on other than length so nothing needs to
  // be done here.
}

bool ArrayField::IsContainerField() const {
  return true;
}

const PacketField* ArrayField::GetElementField() const {
  return element_field_;
}
