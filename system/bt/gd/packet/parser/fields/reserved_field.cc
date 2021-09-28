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

#include "fields/reserved_field.h"
#include "util.h"

int ReservedField::unique_id_ = 0;

const std::string ReservedField::kFieldType = "ReservedField";

ReservedField::ReservedField(int size, ParseLocation loc)
    : PacketField("ReservedScalar" + std::to_string(unique_id_++), loc), size_(size) {}

const std::string& ReservedField::GetFieldType() const {
  return ReservedField::kFieldType;
}

Size ReservedField::GetSize() const {
  return size_;
}

std::string ReservedField::GetDataType() const {
  return util::GetTypeForSize(size_);
}

void ReservedField::GenExtractor(std::ostream&, int, bool) const {}

std::string ReservedField::GetGetterFunctionName() const {
  return "";
}

void ReservedField::GenGetter(std::ostream&, Size, Size) const {
  // There is no Getter for a reserved field
}

std::string ReservedField::GetBuilderParameterType() const {
  // There is no builder parameter for a reserved field
  return "";
}

bool ReservedField::HasParameterValidator() const {
  return false;
}

void ReservedField::GenParameterValidator(std::ostream&) const {
  // There is no builder parameter for a reserved field
}

void ReservedField::GenInserter(std::ostream& s) const {
  s << "insert(static_cast<" << util::GetTypeForSize(GetSize().bits()) << ">(0) /* Reserved */, i, " << GetSize().bits()
    << " );\n";
}

void ReservedField::GenValidator(std::ostream&) const {
  // There is no need to validate the value of a reserved field
}
