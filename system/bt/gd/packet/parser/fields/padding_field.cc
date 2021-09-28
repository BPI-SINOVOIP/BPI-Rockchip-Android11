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

#include "fields/padding_field.h"
#include "util.h"

const std::string PaddingField::kFieldType = "PaddingField";

PaddingField::PaddingField(int size, ParseLocation loc)
    : PacketField("padding_" + std::to_string(size * 8), loc), size_(size * 8) {}

const std::string& PaddingField::GetFieldType() const {
  return PaddingField::kFieldType;
}

Size PaddingField::GetSize() const {
  return size_;
}

Size PaddingField::GetBuilderSize() const {
  return 0;
}

std::string PaddingField::GetDataType() const {
  return "There's no type for Padding fields";
}

void PaddingField::GenExtractor(std::ostream&, int, bool) const {}

std::string PaddingField::GetGetterFunctionName() const {
  return "";
}

void PaddingField::GenGetter(std::ostream&, Size, Size) const {}

std::string PaddingField::GetBuilderParameterType() const {
  return "";
}

bool PaddingField::HasParameterValidator() const {
  return false;
}

void PaddingField::GenParameterValidator(std::ostream&) const {}

void PaddingField::GenInserter(std::ostream&) const {
  ERROR(this) << __func__ << ": This should not be called for padding fields";
}

void PaddingField::GenValidator(std::ostream&) const {}
