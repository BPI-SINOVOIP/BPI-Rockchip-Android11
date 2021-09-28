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

#include "declarations.h"
#include "fields/packet_field.h"
#include "parse_location.h"

class StructParserGenerator {
 public:
  explicit StructParserGenerator(const Declarations& declarations);

  void Generate(std::ostream& s) const;

 private:
  class TreeNode {
   public:
    explicit TreeNode(const StructDef* s)
        : struct_def_(s), packet_field_(s->GetNewField(s->name_ + "_parse", ParseLocation())) {}
    const StructDef* struct_def_;
    const PacketField* packet_field_;
    std::list<const TreeNode*> children_;
  };
  std::list<TreeNode> variable_struct_fields_;
  bool is_little_endian;

  void explore_children(const TreeNode& node, std::ostream& s) const;
};
