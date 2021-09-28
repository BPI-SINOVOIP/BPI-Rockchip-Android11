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

#include "fields/payload_field.h"
#include "util.h"

const std::string PayloadField::kFieldType = "PayloadField";

PayloadField::PayloadField(std::string modifier, ParseLocation loc)
    : PacketField("payload", loc), size_field_(nullptr), size_modifier_(modifier) {}

void PayloadField::SetSizeField(const SizeField* size_field) {
  if (size_field_ != nullptr) {
    ERROR(this, size_field_, size_field) << "The size field for the payload has already been assigned.";
  }

  size_field_ = size_field;
}

const std::string& PayloadField::GetFieldType() const {
  return PayloadField::kFieldType;
}

Size PayloadField::GetSize() const {
  if (size_field_ == nullptr) {
    if (!size_modifier_.empty()) {
      ERROR(this) << "Missing size field for payload with size modifier.";
    }
    return Size();
  }

  std::string dynamic_size = "(Get" + util::UnderscoreToCamelCase(size_field_->GetName()) + "() * 8)";
  if (!size_modifier_.empty()) {
    dynamic_size += "- (" + size_modifier_ + ")";
  }

  return dynamic_size;
}

std::string PayloadField::GetDataType() const {
  return "PacketView";
}

void PayloadField::GenExtractor(std::ostream&, int, bool) const {
  ERROR(this) << __func__ << " should never be called. ";
}

std::string PayloadField::GetGetterFunctionName() const {
  return "GetPayload";
}

void PayloadField::GenGetter(std::ostream& s, Size start_offset, Size end_offset) const {
  s << "PacketView<kLittleEndian> " << GetGetterFunctionName() << "() const {";
  s << "ASSERT(was_validated_);";
  s << "size_t end_index = size();";
  s << "auto to_bound = begin();";
  GenBounds(s, start_offset, end_offset, GetSize());
  s << "return GetLittleEndianSubview(field_begin, field_end);";
  s << "}\n\n";

  s << "PacketView<!kLittleEndian> " << GetGetterFunctionName() << "BigEndian() const {";
  s << "ASSERT(was_validated_);";
  s << "size_t end_index = size();";
  s << "auto to_bound = begin();";
  GenBounds(s, start_offset, end_offset, GetSize());
  s << "return GetBigEndianSubview(field_begin, field_end);";
  s << "}\n";
}

std::string PayloadField::GetBuilderParameterType() const {
  return "std::unique_ptr<BasePacketBuilder>";
}

bool PayloadField::BuilderParameterMustBeMoved() const {
  return true;
}

void PayloadField::GenBuilderParameterFromView(std::ostream& s) const {
  s << "std::make_unique<RawBuilder>(std::vector<uint8_t>(view.GetPayload().begin(), view.GetPayload().end()))";
}

bool PayloadField::HasParameterValidator() const {
  return false;
}

void PayloadField::GenParameterValidator(std::ostream&) const {
  // There is no validation needed for a payload
}

void PayloadField::GenInserter(std::ostream&) const {
  ERROR() << __func__ << " Should never be called.";
}

void PayloadField::GenValidator(std::ostream&) const {
  // Do nothing
}
