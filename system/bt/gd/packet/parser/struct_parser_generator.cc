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

#include "struct_parser_generator.h"

StructParserGenerator::StructParserGenerator(const Declarations& decls) {
  is_little_endian = decls.is_little_endian;
  for (const auto& s : decls.type_defs_queue_) {
    if (s.second->GetDefinitionType() == TypeDef::Type::STRUCT) {
      const auto* struct_def = dynamic_cast<const StructDef*>(s.second);
      variable_struct_fields_.emplace_back(struct_def);
    }
  }
  for (const auto& node : variable_struct_fields_) {
    if (node.struct_def_->parent_ != nullptr) {
      for (auto& parent : variable_struct_fields_) {
        if (node.struct_def_->parent_->name_ == parent.struct_def_->name_) {
          parent.children_.push_back(&node);
        }
      }
    }
  }
}

void StructParserGenerator::explore_children(const TreeNode& node, std::ostream& s) const {
  auto field = node.packet_field_;
  if (!node.children_.empty()) {
    s << "bool " << field->GetName() << "_child_found = false; /* Greedy match */";
  }
  for (const auto& child : node.children_) {
    s << "if (!" << field->GetName() << "_child_found && ";
    s << child->struct_def_->name_ << "::IsInstance(*" << field->GetName() << "_value.get())) {";
    s << field->GetName() << "_child_found = true;";
    s << "std::unique_ptr<" << child->struct_def_->name_ << "> " << child->packet_field_->GetName() << "_value;";
    s << child->packet_field_->GetName() << "_value.reset(new ";
    s << child->struct_def_->name_ << "(*" << field->GetName() << "_value));";
    if (child->struct_def_->fields_.HasBody()) {
      s << "auto optional_it = ";
      s << child->struct_def_->name_ << "::Parse( " << child->packet_field_->GetName() << "_value.get(), ";
      s << "to_bound, false);";
      s << "if (optional_it) {";
      s << "} else { return " << field->GetName() << "_value;}";
    } else {
      s << child->struct_def_->name_ << "::Parse( " << child->packet_field_->GetName() << "_value.get(), ";
      s << "to_bound, false);";
    }
    explore_children(*child, s);
    s << field->GetName() << "_value = std::move(" << child->packet_field_->GetName() << "_value);";

    s << " }";
  }
}

void StructParserGenerator::Generate(std::ostream& s) const {
  for (const auto& node : variable_struct_fields_) {
    if (node.children_.empty()) {
      continue;
    }
    auto field = node.packet_field_;
    s << "inline std::unique_ptr<" << node.struct_def_->name_ << "> Parse" << node.struct_def_->name_;
    if (is_little_endian) {
      s << "(Iterator<kLittleEndian> to_bound) {";
    } else {
      s << "(Iterator<!kLittleEndian> to_bound) {";
    }
    s << field->GetDataType() << " " << field->GetName() << "_value = ";
    s << "std::make_unique<" << node.struct_def_->name_ << ">();";

    s << "auto " << field->GetName() << "_it = to_bound;";
    s << "auto optional_it = ";
    s << node.struct_def_->name_ << "::Parse( " << field->GetName() << "_value.get(), ";
    s << field->GetName() << "_it";
    if (node.struct_def_->parent_ != nullptr) {
      s << ", true);";
    } else {
      s << ");";
    }
    s << "if (optional_it) {";
    s << field->GetName() << "_it = *optional_it;";
    s << "} else { return nullptr; }";

    explore_children(node, s);
    s << "return " << field->GetName() << "_value; }";
  }
}
