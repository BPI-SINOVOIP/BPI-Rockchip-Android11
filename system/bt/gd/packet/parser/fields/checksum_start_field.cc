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

#include "fields/checksum_start_field.h"
#include "util.h"

const std::string ChecksumStartField::kFieldType = "ChecksumStartField";

ChecksumStartField::ChecksumStartField(std::string name, ParseLocation loc)
    : PacketField(name + "_start", loc), started_field_name_(name) {}

const std::string& ChecksumStartField::GetFieldType() const {
  return ChecksumStartField::kFieldType;
}

Size ChecksumStartField::GetSize() const {
  return Size(0);
}

std::string ChecksumStartField::GetDataType() const {
  return "There's no type for Checksum Start fields";
}

void ChecksumStartField::GenExtractor(std::ostream&, int, bool) const {}

std::string ChecksumStartField::GetGetterFunctionName() const {
  return "";
}

void ChecksumStartField::GenGetter(std::ostream&, Size, Size) const {}

std::string ChecksumStartField::GetBuilderParameterType() const {
  return "";
}

bool ChecksumStartField::HasParameterValidator() const {
  return false;
}

void ChecksumStartField::GenParameterValidator(std::ostream&) const {}

void ChecksumStartField::GenInserter(std::ostream&) const {
  ERROR(this) << __func__ << ": This should not be called for checksum start fields";
}

void ChecksumStartField::GenValidator(std::ostream&) const {}

std::string ChecksumStartField::GetStartedFieldName() const {
  return started_field_name_;
}
