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

#include "fields/fixed_scalar_field.h"
#include "util.h"

const std::string FixedScalarField::kFieldType = "FixedScalarField";

FixedScalarField::FixedScalarField(int size, int64_t value, ParseLocation loc)
    : FixedField("fixed_scalar", size, loc), value_(value) {}

const std::string& FixedScalarField::GetFieldType() const {
  return FixedScalarField::kFieldType;
}

std::string FixedScalarField::GetDataType() const {
  return util::GetTypeForSize(GetSize().bits());
}

void FixedScalarField::GenValue(std::ostream& s) const {
  s << value_;
}
