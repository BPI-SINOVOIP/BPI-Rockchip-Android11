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

#include "checksum_def.h"
#include "checksum_type_checker.h"
#include "fields/checksum_field.h"
#include "util.h"

ChecksumDef::ChecksumDef(std::string name, std::string include, int size) : CustomFieldDef(name, include, size) {}

PacketField* ChecksumDef::GetNewField(const std::string& name, ParseLocation loc) const {
  return new ChecksumField(name, name_, size_, loc);
}

TypeDef::Type ChecksumDef::GetDefinitionType() const {
  return TypeDef::Type::CHECKSUM;
}

void ChecksumDef::GenChecksumCheck(std::ostream& s) const {
  s << "static_assert(ChecksumTypeChecker<" << name_ << "," << util::GetTypeForSize(size_) << ">::value, \"";
  s << name_ << " is not a valid checksum type. Please see README for more details.\");";
}
