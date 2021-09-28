/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "utils/flatbuffers.h"

#include <vector>

#include "utils/strings/numbers.h"
#include "utils/variant.h"
#include "flatbuffers/reflection_generated.h"

namespace libtextclassifier3 {
namespace {

// Gets the field information for a field name, returns nullptr if the
// field was not defined.
const reflection::Field* GetFieldOrNull(const reflection::Object* type,
                                        const StringPiece field_name) {
  TC3_CHECK(type != nullptr && type->fields() != nullptr);
  return type->fields()->LookupByKey(field_name.data());
}

const reflection::Field* GetFieldOrNull(const reflection::Object* type,
                                        const int field_offset) {
  if (type->fields() == nullptr) {
    return nullptr;
  }
  for (const reflection::Field* field : *type->fields()) {
    if (field->offset() == field_offset) {
      return field;
    }
  }
  return nullptr;
}

const reflection::Field* GetFieldOrNull(const reflection::Object* type,
                                        const StringPiece field_name,
                                        const int field_offset) {
  // Lookup by name might be faster as the fields are sorted by name in the
  // schema data, so try that first.
  if (!field_name.empty()) {
    return GetFieldOrNull(type, field_name.data());
  }
  return GetFieldOrNull(type, field_offset);
}

const reflection::Field* GetFieldOrNull(const reflection::Object* type,
                                        const FlatbufferField* field) {
  TC3_CHECK(type != nullptr && field != nullptr);
  if (field->field_name() == nullptr) {
    return GetFieldOrNull(type, field->field_offset());
  }
  return GetFieldOrNull(
      type,
      StringPiece(field->field_name()->data(), field->field_name()->size()),
      field->field_offset());
}

const reflection::Field* GetFieldOrNull(const reflection::Object* type,
                                        const FlatbufferFieldT* field) {
  TC3_CHECK(type != nullptr && field != nullptr);
  return GetFieldOrNull(type, field->field_name, field->field_offset);
}

bool Parse(const std::string& str_value, float* value) {
  double double_value;
  if (!ParseDouble(str_value.data(), &double_value)) {
    return false;
  }
  *value = static_cast<float>(double_value);
  return true;
}

bool Parse(const std::string& str_value, double* value) {
  return ParseDouble(str_value.data(), value);
}

bool Parse(const std::string& str_value, int64* value) {
  return ParseInt64(str_value.data(), value);
}

bool Parse(const std::string& str_value, int32* value) {
  return ParseInt32(str_value.data(), value);
}

bool Parse(const std::string& str_value, std::string* value) {
  *value = str_value;
  return true;
}

template <typename T>
bool ParseAndSetField(const reflection::Field* field,
                      const std::string& str_value,
                      ReflectiveFlatbuffer* buffer) {
  T value;
  if (!Parse(str_value, &value)) {
    TC3_LOG(ERROR) << "Could not parse '" << str_value << "'";
    return false;
  }
  if (field->type()->base_type() == reflection::Vector) {
    buffer->Repeated(field)->Add(value);
    return true;
  } else {
    return buffer->Set<T>(field, value);
  }
}

}  // namespace

template <>
const char* FlatbufferFileIdentifier<Model>() {
  return ModelIdentifier();
}

std::unique_ptr<ReflectiveFlatbuffer> ReflectiveFlatbufferBuilder::NewRoot()
    const {
  if (!schema_->root_table()) {
    TC3_LOG(ERROR) << "No root table specified.";
    return nullptr;
  }
  return std::unique_ptr<ReflectiveFlatbuffer>(
      new ReflectiveFlatbuffer(schema_, schema_->root_table()));
}

std::unique_ptr<ReflectiveFlatbuffer> ReflectiveFlatbufferBuilder::NewTable(
    StringPiece table_name) const {
  for (const reflection::Object* object : *schema_->objects()) {
    if (table_name.Equals(object->name()->str())) {
      return std::unique_ptr<ReflectiveFlatbuffer>(
          new ReflectiveFlatbuffer(schema_, object));
    }
  }
  return nullptr;
}

const reflection::Field* ReflectiveFlatbuffer::GetFieldOrNull(
    const StringPiece field_name) const {
  return libtextclassifier3::GetFieldOrNull(type_, field_name);
}

const reflection::Field* ReflectiveFlatbuffer::GetFieldOrNull(
    const FlatbufferField* field) const {
  return libtextclassifier3::GetFieldOrNull(type_, field);
}

bool ReflectiveFlatbuffer::GetFieldWithParent(
    const FlatbufferFieldPath* field_path, ReflectiveFlatbuffer** parent,
    reflection::Field const** field) {
  const auto* path = field_path->field();
  if (path == nullptr || path->size() == 0) {
    return false;
  }

  for (int i = 0; i < path->size(); i++) {
    *parent = (i == 0 ? this : (*parent)->Mutable(*field));
    if (*parent == nullptr) {
      return false;
    }
    *field = (*parent)->GetFieldOrNull(path->Get(i));
    if (*field == nullptr) {
      return false;
    }
  }

  return true;
}

const reflection::Field* ReflectiveFlatbuffer::GetFieldOrNull(
    const int field_offset) const {
  return libtextclassifier3::GetFieldOrNull(type_, field_offset);
}

bool ReflectiveFlatbuffer::ParseAndSet(const reflection::Field* field,
                                       const std::string& value) {
  switch (field->type()->base_type() == reflection::Vector
              ? field->type()->element()
              : field->type()->base_type()) {
    case reflection::String:
      return ParseAndSetField<std::string>(field, value, this);
    case reflection::Int:
      return ParseAndSetField<int32>(field, value, this);
    case reflection::Long:
      return ParseAndSetField<int64>(field, value, this);
    case reflection::Float:
      return ParseAndSetField<float>(field, value, this);
    case reflection::Double:
      return ParseAndSetField<double>(field, value, this);
    default:
      TC3_LOG(ERROR) << "Unhandled field type: " << field->type()->base_type();
      return false;
  }
}

bool ReflectiveFlatbuffer::ParseAndSet(const FlatbufferFieldPath* path,
                                       const std::string& value) {
  ReflectiveFlatbuffer* parent;
  const reflection::Field* field;
  if (!GetFieldWithParent(path, &parent, &field)) {
    return false;
  }
  return parent->ParseAndSet(field, value);
}

ReflectiveFlatbuffer* ReflectiveFlatbuffer::Add(StringPiece field_name) {
  const reflection::Field* field = GetFieldOrNull(field_name);
  if (field == nullptr) {
    return nullptr;
  }

  if (field->type()->base_type() != reflection::BaseType::Vector) {
    return nullptr;
  }

  return Add(field);
}

ReflectiveFlatbuffer* ReflectiveFlatbuffer::Add(
    const reflection::Field* field) {
  if (field == nullptr) {
    return nullptr;
  }
  return Repeated(field)->Add();
}

ReflectiveFlatbuffer* ReflectiveFlatbuffer::Mutable(
    const StringPiece field_name) {
  if (const reflection::Field* field = GetFieldOrNull(field_name)) {
    return Mutable(field);
  }
  TC3_LOG(ERROR) << "Unknown field: " << field_name.ToString();
  return nullptr;
}

ReflectiveFlatbuffer* ReflectiveFlatbuffer::Mutable(
    const reflection::Field* field) {
  if (field->type()->base_type() != reflection::Obj) {
    TC3_LOG(ERROR) << "Field is not of type Object.";
    return nullptr;
  }
  const auto entry = children_.find(field);
  if (entry != children_.end()) {
    return entry->second.get();
  }
  const auto it = children_.insert(
      /*hint=*/entry,
      std::make_pair(
          field,
          std::unique_ptr<ReflectiveFlatbuffer>(new ReflectiveFlatbuffer(
              schema_, schema_->objects()->Get(field->type()->index())))));
  return it->second.get();
}

RepeatedField* ReflectiveFlatbuffer::Repeated(StringPiece field_name) {
  if (const reflection::Field* field = GetFieldOrNull(field_name)) {
    return Repeated(field);
  }
  TC3_LOG(ERROR) << "Unknown field: " << field_name.ToString();
  return nullptr;
}

RepeatedField* ReflectiveFlatbuffer::Repeated(const reflection::Field* field) {
  if (field->type()->base_type() != reflection::Vector) {
    TC3_LOG(ERROR) << "Field is not of type Vector.";
    return nullptr;
  }

  // If the repeated field was already set, return its instance.
  const auto entry = repeated_fields_.find(field);
  if (entry != repeated_fields_.end()) {
    return entry->second.get();
  }

  // Otherwise, create a new instance and store it.
  std::unique_ptr<RepeatedField> repeated_field(
      new RepeatedField(schema_, field));
  const auto it = repeated_fields_.insert(
      /*hint=*/entry, std::make_pair(field, std::move(repeated_field)));
  return it->second.get();
}

flatbuffers::uoffset_t ReflectiveFlatbuffer::Serialize(
    flatbuffers::FlatBufferBuilder* builder) const {
  // Build all children before we can start with this table.
  std::vector<
      std::pair</* field vtable offset */ int,
                /* field data offset in buffer */ flatbuffers::uoffset_t>>
      offsets;
  offsets.reserve(children_.size() + repeated_fields_.size());
  for (const auto& it : children_) {
    offsets.push_back({it.first->offset(), it.second->Serialize(builder)});
  }

  // Create strings.
  for (const auto& it : fields_) {
    if (it.second.Has<std::string>()) {
      offsets.push_back(
          {it.first->offset(),
           builder->CreateString(it.second.ConstRefValue<std::string>()).o});
    }
  }

  // Build the repeated fields.
  for (const auto& it : repeated_fields_) {
    offsets.push_back({it.first->offset(), it.second->Serialize(builder)});
  }

  // Build the table now.
  const flatbuffers::uoffset_t table_start = builder->StartTable();

  // Add scalar fields.
  for (const auto& it : fields_) {
    switch (it.second.GetType()) {
      case Variant::TYPE_BOOL_VALUE:
        builder->AddElement<uint8_t>(
            it.first->offset(), static_cast<uint8_t>(it.second.Value<bool>()),
            static_cast<uint8_t>(it.first->default_integer()));
        continue;
      case Variant::TYPE_INT8_VALUE:
        builder->AddElement<int8_t>(
            it.first->offset(), static_cast<int8_t>(it.second.Value<int8>()),
            static_cast<int8_t>(it.first->default_integer()));
        continue;
      case Variant::TYPE_UINT8_VALUE:
        builder->AddElement<uint8_t>(
            it.first->offset(), static_cast<uint8_t>(it.second.Value<uint8>()),
            static_cast<uint8_t>(it.first->default_integer()));
        continue;
      case Variant::TYPE_INT_VALUE:
        builder->AddElement<int32>(
            it.first->offset(), it.second.Value<int>(),
            static_cast<int32>(it.first->default_integer()));
        continue;
      case Variant::TYPE_UINT_VALUE:
        builder->AddElement<uint32>(
            it.first->offset(), it.second.Value<uint>(),
            static_cast<uint32>(it.first->default_integer()));
        continue;
      case Variant::TYPE_INT64_VALUE:
        builder->AddElement<int64>(it.first->offset(), it.second.Value<int64>(),
                                   it.first->default_integer());
        continue;
      case Variant::TYPE_UINT64_VALUE:
        builder->AddElement<uint64>(it.first->offset(),
                                    it.second.Value<uint64>(),
                                    it.first->default_integer());
        continue;
      case Variant::TYPE_FLOAT_VALUE:
        builder->AddElement<float>(
            it.first->offset(), it.second.Value<float>(),
            static_cast<float>(it.first->default_real()));
        continue;
      case Variant::TYPE_DOUBLE_VALUE:
        builder->AddElement<double>(it.first->offset(),
                                    it.second.Value<double>(),
                                    it.first->default_real());
        continue;
      default:
        continue;
    }
  }

  // Add strings, subtables and repeated fields.
  for (const auto& it : offsets) {
    builder->AddOffset(it.first, flatbuffers::Offset<void>(it.second));
  }

  return builder->EndTable(table_start);
}

std::string ReflectiveFlatbuffer::Serialize() const {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(flatbuffers::Offset<void>(Serialize(&builder)));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

template <>
bool ReflectiveFlatbuffer::AppendFromVector<std::string>(
    const flatbuffers::Table* from, const reflection::Field* field) {
  auto* from_vector = from->GetPointer<
      const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(
      field->offset());
  if (from_vector == nullptr) {
    return false;
  }

  RepeatedField* to_repeated = Repeated(field);
  for (const flatbuffers::String* element : *from_vector) {
    to_repeated->Add(element->str());
  }
  return true;
}

template <>
bool ReflectiveFlatbuffer::AppendFromVector<ReflectiveFlatbuffer>(
    const flatbuffers::Table* from, const reflection::Field* field) {
  auto* from_vector = from->GetPointer<const flatbuffers::Vector<
      flatbuffers::Offset<const flatbuffers::Table>>*>(field->offset());
  if (from_vector == nullptr) {
    return false;
  }

  RepeatedField* to_repeated = Repeated(field);
  for (const flatbuffers::Table* const from_element : *from_vector) {
    ReflectiveFlatbuffer* to_element = to_repeated->Add();
    if (to_element == nullptr) {
      return false;
    }
    to_element->MergeFrom(from_element);
  }
  return true;
}

bool ReflectiveFlatbuffer::MergeFrom(const flatbuffers::Table* from) {
  // No fields to set.
  if (type_->fields() == nullptr) {
    return true;
  }

  for (const reflection::Field* field : *type_->fields()) {
    // Skip fields that are not explicitly set.
    if (!from->CheckField(field->offset())) {
      continue;
    }
    const reflection::BaseType type = field->type()->base_type();
    switch (type) {
      case reflection::Bool:
        Set<bool>(field, from->GetField<uint8_t>(field->offset(),
                                                 field->default_integer()));
        break;
      case reflection::Byte:
        Set<int8_t>(field, from->GetField<int8_t>(field->offset(),
                                                  field->default_integer()));
        break;
      case reflection::UByte:
        Set<uint8_t>(field, from->GetField<uint8_t>(field->offset(),
                                                    field->default_integer()));
        break;
      case reflection::Int:
        Set<int32>(field, from->GetField<int32>(field->offset(),
                                                field->default_integer()));
        break;
      case reflection::UInt:
        Set<uint32>(field, from->GetField<uint32>(field->offset(),
                                                  field->default_integer()));
        break;
      case reflection::Long:
        Set<int64>(field, from->GetField<int64>(field->offset(),
                                                field->default_integer()));
        break;
      case reflection::ULong:
        Set<uint64>(field, from->GetField<uint64>(field->offset(),
                                                  field->default_integer()));
        break;
      case reflection::Float:
        Set<float>(field, from->GetField<float>(field->offset(),
                                                field->default_real()));
        break;
      case reflection::Double:
        Set<double>(field, from->GetField<double>(field->offset(),
                                                  field->default_real()));
        break;
      case reflection::String:
        Set<std::string>(
            field, from->GetPointer<const flatbuffers::String*>(field->offset())
                       ->str());
        break;
      case reflection::Obj:
        if (ReflectiveFlatbuffer* nested_field = Mutable(field);
            nested_field == nullptr ||
            !nested_field->MergeFrom(
                from->GetPointer<const flatbuffers::Table* const>(
                    field->offset()))) {
          return false;
        }
        break;
      case reflection::Vector:
        switch (field->type()->element()) {
          case reflection::Int:
            AppendFromVector<int32>(from, field);
            break;
          case reflection::UInt:
            AppendFromVector<uint>(from, field);
            break;
          case reflection::Long:
            AppendFromVector<int64>(from, field);
            break;
          case reflection::ULong:
            AppendFromVector<uint64>(from, field);
            break;
          case reflection::Byte:
            AppendFromVector<int8_t>(from, field);
            break;
          case reflection::UByte:
            AppendFromVector<uint8_t>(from, field);
            break;
          case reflection::String:
            AppendFromVector<std::string>(from, field);
            break;
          case reflection::Obj:
            AppendFromVector<ReflectiveFlatbuffer>(from, field);
            break;
          case reflection::Double:
            AppendFromVector<double>(from, field);
            break;
          case reflection::Float:
            AppendFromVector<float>(from, field);
            break;
          default:
            TC3_LOG(ERROR) << "Repeated unsupported type: "
                           << field->type()->element()
                           << " for field: " << field->name()->str();
            return false;
            break;
        }
        break;
      default:
        TC3_LOG(ERROR) << "Unsupported type: " << type
                       << " for field: " << field->name()->str();
        return false;
    }
  }
  return true;
}

bool ReflectiveFlatbuffer::MergeFromSerializedFlatbuffer(StringPiece from) {
  return MergeFrom(flatbuffers::GetAnyRoot(
      reinterpret_cast<const unsigned char*>(from.data())));
}

void ReflectiveFlatbuffer::AsFlatMap(
    const std::string& key_separator, const std::string& key_prefix,
    std::map<std::string, Variant>* result) const {
  // Add direct fields.
  for (const auto& it : fields_) {
    (*result)[key_prefix + it.first->name()->str()] = it.second;
  }

  // Add nested messages.
  for (const auto& it : children_) {
    it.second->AsFlatMap(key_separator,
                         key_prefix + it.first->name()->str() + key_separator,
                         result);
  }
}

std::string ReflectiveFlatbuffer::ToTextProto() const {
  std::string result;
  std::string current_field_separator;
  // Add direct fields.
  for (const auto& field_value_pair : fields_) {
    const std::string field_name = field_value_pair.first->name()->str();
    const Variant& value = field_value_pair.second;
    std::string quotes;
    if (value.GetType() == Variant::TYPE_STRING_VALUE) {
      quotes = "'";
    }
    result.append(current_field_separator + field_name + ": " + quotes +
                  value.ToString() + quotes);
    current_field_separator = ", ";
  }

  // Add nested messages.
  for (const auto& field_flatbuffer_pair : children_) {
    const std::string field_name = field_flatbuffer_pair.first->name()->str();
    result.append(current_field_separator + field_name + " {" +
                  field_flatbuffer_pair.second->ToTextProto() + "}");
    current_field_separator = ", ";
  }

  return result;
}

bool SwapFieldNamesForOffsetsInPath(const reflection::Schema* schema,
                                    FlatbufferFieldPathT* path) {
  if (schema == nullptr || !schema->root_table()) {
    TC3_LOG(ERROR) << "Empty schema provided.";
    return false;
  }

  reflection::Object const* type = schema->root_table();
  for (int i = 0; i < path->field.size(); i++) {
    const reflection::Field* field = GetFieldOrNull(type, path->field[i].get());
    if (field == nullptr) {
      TC3_LOG(ERROR) << "Could not find field: " << path->field[i]->field_name;
      return false;
    }
    path->field[i]->field_name.clear();
    path->field[i]->field_offset = field->offset();

    // Descend.
    if (i < path->field.size() - 1) {
      if (field->type()->base_type() != reflection::Obj) {
        TC3_LOG(ERROR) << "Field: " << field->name()->str()
                       << " is not of type `Object`.";
        return false;
      }
      type = schema->objects()->Get(field->type()->index());
    }
  }
  return true;
}

//
// Repeated field methods.
//

ReflectiveFlatbuffer* RepeatedField::Add() {
  if (is_primitive_) {
    TC3_LOG(ERROR) << "Trying to add sub-message on a primitive-typed field.";
    return nullptr;
  }

  object_items_.emplace_back(new ReflectiveFlatbuffer(
      schema_, schema_->objects()->Get(field_->type()->index())));
  return object_items_.back().get();
}

namespace {

template <typename T>
flatbuffers::uoffset_t TypedSerialize(const std::vector<Variant>& values,
                                      flatbuffers::FlatBufferBuilder* builder) {
  std::vector<T> typed_values;
  typed_values.reserve(values.size());
  for (const Variant& item : values) {
    typed_values.push_back(item.Value<T>());
  }
  return builder->CreateVector(typed_values).o;
}

}  // namespace

flatbuffers::uoffset_t RepeatedField::Serialize(
    flatbuffers::FlatBufferBuilder* builder) const {
  switch (field_->type()->element()) {
    case reflection::String:
      return SerializeString(builder);
      break;
    case reflection::Obj:
      return SerializeObject(builder);
      break;
    case reflection::Bool:
      return TypedSerialize<bool>(items_, builder);
      break;
    case reflection::Byte:
      return TypedSerialize<int8_t>(items_, builder);
      break;
    case reflection::UByte:
      return TypedSerialize<uint8_t>(items_, builder);
      break;
    case reflection::Int:
      return TypedSerialize<int>(items_, builder);
      break;
    case reflection::UInt:
      return TypedSerialize<uint>(items_, builder);
      break;
    case reflection::Long:
      return TypedSerialize<int64>(items_, builder);
      break;
    case reflection::ULong:
      return TypedSerialize<uint64>(items_, builder);
      break;
    case reflection::Float:
      return TypedSerialize<float>(items_, builder);
      break;
    case reflection::Double:
      return TypedSerialize<double>(items_, builder);
      break;
    default:
      TC3_LOG(FATAL) << "Unsupported type: " << field_->type()->element();
      break;
  }
  TC3_LOG(FATAL) << "Invalid state.";
  return 0;
}

flatbuffers::uoffset_t RepeatedField::SerializeString(
    flatbuffers::FlatBufferBuilder* builder) const {
  std::vector<flatbuffers::Offset<flatbuffers::String>> offsets(items_.size());
  for (int i = 0; i < items_.size(); i++) {
    offsets[i] = builder->CreateString(items_[i].ConstRefValue<std::string>());
  }
  return builder->CreateVector(offsets).o;
}

flatbuffers::uoffset_t RepeatedField::SerializeObject(
    flatbuffers::FlatBufferBuilder* builder) const {
  std::vector<flatbuffers::Offset<void>> offsets(object_items_.size());
  for (int i = 0; i < object_items_.size(); i++) {
    offsets[i] = object_items_[i]->Serialize(builder);
  }
  return builder->CreateVector(offsets).o;
}

}  // namespace libtextclassifier3
