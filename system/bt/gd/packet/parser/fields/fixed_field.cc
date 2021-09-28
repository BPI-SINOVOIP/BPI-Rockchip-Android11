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

#include "fields/fixed_field.h"
#include "util.h"

int FixedField::unique_id_ = 0;

FixedField::FixedField(std::string name, int size, ParseLocation loc)
    : ScalarField(name + std::to_string(unique_id_++), size, loc) {}

void FixedField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << "protected:";
  ScalarField::GenGetter(s, start_offset, end_offset);
  s << "public:\n";
}

std::string FixedField::GetBuilderParameterType() const {
  return "";
}

bool FixedField::GenBuilderParameter(std::ostream&) const {
  // No parameter needed for a fixed field.
  return false;
}

bool FixedField::HasParameterValidator() const {
  return false;
}

void FixedField::GenParameterValidator(std::ostream&) const {
  // No parameter validator needed for a fixed field.
}

void FixedField::GenInserter(std::ostream& s) const {
  s << "insert(";
  GenValue(s);
  s << ", i , " << GetSize().bits() << ");";
}

void FixedField::GenValidator(std::ostream& s) const {
  s << "if (Get" << util::UnderscoreToCamelCase(GetName()) << "() != ";
  GenValue(s);
  s << ") return false;";
}
