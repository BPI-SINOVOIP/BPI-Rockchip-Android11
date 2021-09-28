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

#include <deque>
#include <map>
#include <optional>

#include "checksum_def.h"
#include "custom_field_def.h"
#include "enum_def.h"
#include "enum_gen.h"
#include "packet_def.h"
#include "struct_def.h"

class Declarations {
 public:
  void AddTypeDef(std::string name, TypeDef* def) {
    auto it = type_defs_.find(name);
    if (it != type_defs_.end()) {
      ERROR() << "Redefinition of Type " << name;
    }
    type_defs_.insert(std::pair(name, def));
    type_defs_queue_.push_back(std::pair(name, def));
  }

  TypeDef* GetTypeDef(const std::string& name) {
    auto it = type_defs_.find(name);
    if (it == type_defs_.end()) {
      return nullptr;
    }

    return it->second;
  }

  void AddPacketDef(std::string name, PacketDef def) {
    auto it = packet_defs_.find(name);
    if (it != packet_defs_.end()) {
      ERROR() << "Redefinition of Packet " << name;
    }
    packet_defs_.insert(std::pair(name, def));
    packet_defs_queue_.push_back(std::pair(name, def));
  }

  PacketDef* GetPacketDef(const std::string& name) {
    auto it = packet_defs_.find(name);
    if (it == packet_defs_.end()) {
      return nullptr;
    }

    return &(it->second);
  }

  void AddGroupDef(std::string name, FieldList* group_def) {
    auto it = group_defs_.find(name);
    if (it != group_defs_.end()) {
      ERROR() << "Redefinition of group " << name;
    }
    group_defs_.insert(std::pair(name, group_def));
  }

  FieldList* GetGroupDef(std::string name) {
    if (group_defs_.find(name) == group_defs_.end()) {
      return nullptr;
    }

    return group_defs_.at(name);
  }

  std::map<std::string, FieldList*> group_defs_;

  std::map<std::string, TypeDef*> type_defs_;
  std::deque<std::pair<std::string, TypeDef*>> type_defs_queue_;
  std::map<std::string, PacketDef> packet_defs_;
  std::deque<std::pair<std::string, PacketDef>> packet_defs_queue_;
  bool is_little_endian;
};
