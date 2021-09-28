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

#include "fields/fixed_enum_field.h"
#include "util.h"

const std::string FixedEnumField::kFieldType = "FixedEnumField";

FixedEnumField::FixedEnumField(EnumDef* enum_def, std::string value, ParseLocation loc)
    : FixedField("fixed_enum", enum_def->size_, loc), enum_(enum_def), value_(value) {}

const std::string& FixedEnumField::GetFieldType() const {
  return FixedEnumField::kFieldType;
}

std::string FixedEnumField::GetDataType() const {
  return enum_->name_;
}

void FixedEnumField::GenValue(std::ostream& s) const {
  s << enum_->name_ << "::" << value_;
}
