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

#include "enum_def.h"

#include <iostream>
#include <map>

#include "fields/enum_field.h"
#include "util.h"

EnumDef::EnumDef(std::string name, int size) : TypeDef(name, size) {}

void EnumDef::AddEntry(std::string name, uint32_t value) {
  if (!util::IsEnumCase(name)) {
    ERROR() << __func__ << ": Enum " << name << "(" << value << ") should be all uppercase with underscores";
  }
  if (value > util::GetMaxValueForBits(size_)) {
    ERROR() << __func__ << ": Value of " << name << "(" << value << ") is greater than the max possible value for enum "
            << name_ << "(" << util::GetMaxValueForBits(size_) << ")\n";
  }

  constants_.insert(std::pair(value, name));
  entries_.insert(name);
}

PacketField* EnumDef::GetNewField(const std::string& name, ParseLocation loc) const {
  return new EnumField(name, *this, "What is this for", loc);
}

bool EnumDef::HasEntry(std::string name) const {
  return entries_.count(name) != 0;
}

TypeDef::Type EnumDef::GetDefinitionType() const {
  return TypeDef::Type::ENUM;
}
