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

#pragma once

#include <iostream>

#include "custom_field_def.h"
#include "fields/packet_field.h"
#include "parse_location.h"
#include "type_def.h"

class ChecksumDef : public CustomFieldDef {
 public:
  ChecksumDef(std::string name, std::string include, int size);

  virtual PacketField* GetNewField(const std::string& name, ParseLocation loc) const override;

  virtual TypeDef::Type GetDefinitionType() const override;

  void GenChecksumCheck(std::ostream& s) const;

  const std::string include_;
};
