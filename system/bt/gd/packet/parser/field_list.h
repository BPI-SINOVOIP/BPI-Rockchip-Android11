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

#include <map>
#include <set>
#include <vector>

#include "fields/all_fields.h"
#include "fields/packet_field.h"

using FieldListIterator = std::vector<PacketField*>::const_iterator;
using ReverseFieldListIterator = std::vector<PacketField*>::const_reverse_iterator;

class FieldList {
 public:
  FieldList() = default;

  FieldList(std::vector<PacketField*> fields) {
    for (PacketField* field : fields) {
      AppendField(field);
    }
  }

  template <class Iterator>
  FieldList(Iterator begin, Iterator end) {
    while (begin != end) {
      AppendField(*begin);
      begin++;
    }
  }

  PacketField* operator[](int index) const {
    return field_list_[index];
  }

  PacketField* GetField(std::string field_name) const {
    auto it = field_map_.find(field_name);
    if (it == field_map_.end()) {
      return nullptr;
    }

    return it->second;
  }

  void AppendField(PacketField* field) {
    AddField(field);
    field_list_.push_back(field);
  }

  void PrependField(PacketField* field) {
    AddField(field);
    field_list_.insert(field_list_.begin(), field);
  }

  FieldList GetFieldsBeforePayloadOrBody() const {
    FieldList ret;
    for (auto it = begin(); it != end(); it++) {
      const auto& field = *it;
      if (field->GetFieldType() == PayloadField::kFieldType || field->GetFieldType() == BodyField::kFieldType) {
        break;
      }
      ret.AppendField(*it);
    }

    return ret;
  }

  FieldList GetFieldsAfterPayloadOrBody() const {
    FieldListIterator it;
    for (it = begin(); it != end(); it++) {
      const auto& field = *it;
      if (field->GetFieldType() == PayloadField::kFieldType || field->GetFieldType() == BodyField::kFieldType) {
        // Increment it once to get first field after payload/body.
        it++;
        break;
      }
    }

    return FieldList(it, end());
  }

  FieldList GetFieldsWithTypes(std::set<std::string> field_types) const {
    FieldList ret;

    for (const auto& field : field_list_) {
      if (field_types.find(field->GetFieldType()) != field_types.end()) {
        ret.AppendField(field);
      }
    }

    return ret;
  }

  FieldList GetFieldsWithoutTypes(std::set<std::string> field_types) const {
    FieldList ret;

    for (const auto& field : field_list_) {
      if (field_types.find(field->GetFieldType()) == field_types.end()) {
        ret.AppendField(field);
      }
    }

    return ret;
  }

  // Appends header fields of param to header fields of the current and
  // prepends footer fields of the param to footer fields of the current.
  // Ex. Assuming field_list_X has the layout:
  //   field_list_X_header
  //   payload/body
  //   field_list_X_footer
  // The call to field_list_1.Merge(field_list_2) would result in
  //   field_list_1_header
  //   field_list_2_header
  //   payload/body (uses whatever was in field_list_2)
  //   field_list_2_footer
  //   field_list_1_footer
  FieldList Merge(FieldList nested) const {
    FieldList ret;

    for (const auto& field : GetFieldsBeforePayloadOrBody()) {
      ret.AppendField(field);
    }

    for (const auto& field : nested) {
      ret.AppendField(field);
    }

    for (const auto& field : GetFieldsAfterPayloadOrBody()) {
      ret.AppendField(field);
    }

    return ret;
  }

  bool HasPayloadOrBody() const {
    return has_payload_ || has_body_;
  }

  bool HasPayload() const {
    return has_payload_;
  }

  bool HasBody() const {
    return has_body_;
  }

  FieldListIterator begin() const {
    return field_list_.begin();
  }

  FieldListIterator end() const {
    return field_list_.end();
  }

  ReverseFieldListIterator rbegin() const {
    return field_list_.rbegin();
  }

  ReverseFieldListIterator rend() const {
    return field_list_.rend();
  }

  size_t size() const {
    return field_list_.size();
  }

 private:
  void AddField(PacketField* field) {
    if (field_map_.find(field->GetName()) != field_map_.end()) {
      ERROR(field) << "Field with name \"" << field->GetName() << "\" was previously defined.\n";
    }

    if (field->GetFieldType() == PayloadField::kFieldType) {
      if (has_body_) {
        ERROR(field) << "Packet already has a body.";
      }
      has_payload_ = true;
    }

    if (field->GetFieldType() == BodyField::kFieldType) {
      if (has_payload_) {
        ERROR(field) << "Packet already has a payload.";
      }
      has_body_ = true;
    }

    field_map_.insert(std::pair(field->GetName(), field));
  }

  std::vector<PacketField*> field_list_;
  std::map<std::string, PacketField*> field_map_;
  bool has_payload_ = false;
  bool has_body_ = false;
};
