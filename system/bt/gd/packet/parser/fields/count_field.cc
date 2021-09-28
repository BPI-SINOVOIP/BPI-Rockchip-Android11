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

#include "fields/count_field.h"
#include "util.h"

const std::string CountField::kFieldType = "CountField";

CountField::CountField(std::string name, int size, ParseLocation loc)
    : ScalarField(name + "_count", size, loc), sized_field_name_(name) {}

const std::string& CountField::GetFieldType() const {
  return CountField::kFieldType;
}

void CountField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << "protected:";
  ScalarField::GenGetter(s, start_offset, end_offset);
  s << "public:\n";
}

bool CountField::GenBuilderParameter(std::ostream&) const {
  // There is no builder parameter for a size field
  return false;
}

bool CountField::HasParameterValidator() const {
  return false;
}

void CountField::GenParameterValidator(std::ostream&) const {
  // There is no builder parameter for a size field
  // TODO: Check if the payload fits in the packet?
}

void CountField::GenInserter(std::ostream&) const {
  ERROR(this) << __func__ << ": This should not be called for count fields";
}

void CountField::GenValidator(std::ostream&) const {
  // Do nothing since the fixed count fields will be handled specially.
}

std::string CountField::GetSizedFieldName() const {
  return sized_field_name_;
}
