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

#include "fields/packet_field.h"

class TypeDef {
 public:
  TypeDef(std::string name) : name_(name) {}

  TypeDef(std::string name, int size) : name_(name), size_(size) {}

  virtual ~TypeDef() = default;

  std::string GetTypeName() const {
    return name_;
  }

  enum class Type {
    INVALID,
    ENUM,
    CHECKSUM,
    CUSTOM,
    PACKET,
    STRUCT,
  };

  virtual Type GetDefinitionType() const = 0;

  virtual PacketField* GetNewField(const std::string& name, ParseLocation loc) const = 0;

  const std::string name_;
  const int size_{-1};
};
