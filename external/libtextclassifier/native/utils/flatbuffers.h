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

// Utility functions for working with FlatBuffers.

#ifndef LIBTEXTCLASSIFIER_UTILS_FLATBUFFERS_H_
#define LIBTEXTCLASSIFIER_UTILS_FLATBUFFERS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "annotator/model_generated.h"
#include "utils/base/logging.h"
#include "utils/flatbuffers_generated.h"
#include "utils/strings/stringpiece.h"
#include "utils/variant.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/reflection.h"
#include "flatbuffers/reflection_generated.h"

namespace libtextclassifier3 {

class ReflectiveFlatBuffer;
class RepeatedField;

// Loads and interprets the buffer as 'FlatbufferMessage' and verifies its
// integrity.
template <typename FlatbufferMessage>
const FlatbufferMessage* LoadAndVerifyFlatbuffer(const void* buffer, int size) {
  const FlatbufferMessage* message =
      flatbuffers::GetRoot<FlatbufferMessage>(buffer);
  if (message == nullptr) {
    return nullptr;
  }
  flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(buffer),
                                 size);
  if (message->Verify(verifier)) {
    return message;
  } else {
    return nullptr;
  }
}

// Same as above but takes string.
template <typename FlatbufferMessage>
const FlatbufferMessage* LoadAndVerifyFlatbuffer(const std::string& buffer) {
  return LoadAndVerifyFlatbuffer<FlatbufferMessage>(buffer.c_str(),
                                                    buffer.size());
}

// Loads and interprets the buffer as 'FlatbufferMessage', verifies its
// integrity and returns its mutable version.
template <typename FlatbufferMessage>
std::unique_ptr<typename FlatbufferMessage::NativeTableType>
LoadAndVerifyMutableFlatbuffer(const void* buffer, int size) {
  const FlatbufferMessage* message =
      LoadAndVerifyFlatbuffer<FlatbufferMessage>(buffer, size);
  if (message == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<typename FlatbufferMessage::NativeTableType>(
      message->UnPack());
}

// Same as above but takes string.
template <typename FlatbufferMessage>
std::unique_ptr<typename FlatbufferMessage::NativeTableType>
LoadAndVerifyMutableFlatbuffer(const std::string& buffer) {
  return LoadAndVerifyMutableFlatbuffer<FlatbufferMessage>(buffer.c_str(),
                                                           buffer.size());
}

template <typename FlatbufferMessage>
const char* FlatbufferFileIdentifier() {
  return nullptr;
}

template <>
const char* FlatbufferFileIdentifier<Model>();

// Packs the mutable flatbuffer message to string.
template <typename FlatbufferMessage>
std::string PackFlatbuffer(
    const typename FlatbufferMessage::NativeTableType* mutable_message) {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(FlatbufferMessage::Pack(builder, mutable_message),
                 FlatbufferFileIdentifier<FlatbufferMessage>());
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

class ReflectiveFlatbuffer;

// Checks whether a variant value type agrees with a field type.
template <typename T>
bool IsMatchingType(const reflection::BaseType type) {
  switch (type) {
    case reflection::Bool:
      return std::is_same<T, bool>::value;
    case reflection::Byte:
      return std::is_same<T, int8>::value;
    case reflection::UByte:
      return std::is_same<T, uint8>::value;
    case reflection::Int:
      return std::is_same<T, int32>::value;
    case reflection::UInt:
      return std::is_same<T, uint32>::value;
    case reflection::Long:
      return std::is_same<T, int64>::value;
    case reflection::ULong:
      return std::is_same<T, uint64>::value;
    case reflection::Float:
      return std::is_same<T, float>::value;
    case reflection::Double:
      return std::is_same<T, double>::value;
    case reflection::String:
      return std::is_same<T, std::string>::value ||
             std::is_same<T, StringPiece>::value ||
             std::is_same<T, const char*>::value;
    case reflection::Obj:
      return std::is_same<T, ReflectiveFlatbuffer>::value;
    default:
      return false;
  }
}

// A flatbuffer that can be built using flatbuffer reflection data of the
// schema.
// Normally, field information is hard-coded in code generated from a flatbuffer
// schema. Here we lookup the necessary information for building a flatbuffer
// from the provided reflection meta data.
// When serializing a flatbuffer, the library requires that the sub messages
// are already serialized, therefore we explicitly keep the field values and
// serialize the message in (reverse) topological dependency order.
class ReflectiveFlatbuffer {
 public:
  ReflectiveFlatbuffer(const reflection::Schema* schema,
                       const reflection::Object* type)
      : schema_(schema), type_(type) {}

  // Gets the field information for a field name, returns nullptr if the
  // field was not defined.
  const reflection::Field* GetFieldOrNull(const StringPiece field_name) const;
  const reflection::Field* GetFieldOrNull(const FlatbufferField* field) const;
  const reflection::Field* GetFieldOrNull(const int field_offset) const;

  // Gets a nested field and the message it is defined on.
  bool GetFieldWithParent(const FlatbufferFieldPath* field_path,
                          ReflectiveFlatbuffer** parent,
                          reflection::Field const** field);

  // Sets a field to a specific value.
  // Returns true if successful, and false if the field was not found or the
  // expected type doesn't match.
  template <typename T>
  bool Set(StringPiece field_name, T value);

  // Sets a field to a specific value.
  // Returns true if successful, and false if the expected type doesn't match.
  // Expects `field` to be non-null.
  template <typename T>
  bool Set(const reflection::Field* field, T value);

  // Sets a field to a specific value. Field is specified by path.
  template <typename T>
  bool Set(const FlatbufferFieldPath* path, T value);

  // Sets sub-message field (if not set yet), and returns a pointer to it.
  // Returns nullptr if the field was not found, or the field type was not a
  // table.
  ReflectiveFlatbuffer* Mutable(StringPiece field_name);
  ReflectiveFlatbuffer* Mutable(const reflection::Field* field);

  // Parses the value (according to the type) and sets a primitive field to the
  // parsed value.
  bool ParseAndSet(const reflection::Field* field, const std::string& value);
  bool ParseAndSet(const FlatbufferFieldPath* path, const std::string& value);

  // Adds a primitive value to the repeated field.
  template <typename T>
  bool Add(StringPiece field_name, T value);

  // Add a sub-message to the repeated field.
  ReflectiveFlatbuffer* Add(StringPiece field_name);

  template <typename T>
  bool Add(const reflection::Field* field, T value);

  ReflectiveFlatbuffer* Add(const reflection::Field* field);

  // Gets the reflective flatbuffer for a repeated field.
  // Returns nullptr if the field was not found, or the field type was not a
  // vector.
  RepeatedField* Repeated(StringPiece field_name);
  RepeatedField* Repeated(const reflection::Field* field);

  // Serializes the flatbuffer.
  flatbuffers::uoffset_t Serialize(
      flatbuffers::FlatBufferBuilder* builder) const;
  std::string Serialize() const;

  // Merges the fields from the given flatbuffer table into this flatbuffer.
  // Scalar fields will be overwritten, if present in `from`.
  // Embedded messages will be merged.
  bool MergeFrom(const flatbuffers::Table* from);
  bool MergeFromSerializedFlatbuffer(StringPiece from);

  // Flattens the flatbuffer as a flat map.
  // (Nested) fields names are joined by `key_separator`.
  std::map<std::string, Variant> AsFlatMap(
      const std::string& key_separator = ".") const {
    std::map<std::string, Variant> result;
    AsFlatMap(key_separator, /*key_prefix=*/"", &result);
    return result;
  }

  // Converts the flatbuffer's content to a human-readable textproto
  // representation.
  std::string ToTextProto() const;

  bool HasExplicitlySetFields() const {
    return !fields_.empty() || !children_.empty() || !repeated_fields_.empty();
  }

 private:
  // Helper function for merging given repeated field from given flatbuffer
  // table. Appends the elements.
  template <typename T>
  bool AppendFromVector(const flatbuffers::Table* from,
                        const reflection::Field* field);

  const reflection::Schema* const schema_;
  const reflection::Object* const type_;

  // Cached primitive fields (scalars and strings).
  std::unordered_map<const reflection::Field*, Variant> fields_;

  // Cached sub-messages.
  std::unordered_map<const reflection::Field*,
                     std::unique_ptr<ReflectiveFlatbuffer>>
      children_;

  // Cached repeated fields.
  std::unordered_map<const reflection::Field*, std::unique_ptr<RepeatedField>>
      repeated_fields_;

  // Flattens the flatbuffer as a flat map.
  // (Nested) fields names are joined by `key_separator` and prefixed by
  // `key_prefix`.
  void AsFlatMap(const std::string& key_separator,
                 const std::string& key_prefix,
                 std::map<std::string, Variant>* result) const;
};

// A helper class to build flatbuffers based on schema reflection data.
// Can be used to a `ReflectiveFlatbuffer` for the root message of the
// schema, or any defined table via name.
class ReflectiveFlatbufferBuilder {
 public:
  explicit ReflectiveFlatbufferBuilder(const reflection::Schema* schema)
      : schema_(schema) {}

  // Starts a new root table message.
  std::unique_ptr<ReflectiveFlatbuffer> NewRoot() const;

  // Starts a new table message. Returns nullptr if no table with given name is
  // found in the schema.
  std::unique_ptr<ReflectiveFlatbuffer> NewTable(
      const StringPiece table_name) const;

 private:
  const reflection::Schema* const schema_;
};

// Encapsulates a repeated field.
// Serves as a common base class for repeated fields.
class RepeatedField {
 public:
  RepeatedField(const reflection::Schema* const schema,
                const reflection::Field* field)
      : schema_(schema),
        field_(field),
        is_primitive_(field->type()->element() != reflection::BaseType::Obj) {}

  template <typename T>
  bool Add(const T value);

  ReflectiveFlatbuffer* Add();

  template <typename T>
  T Get(int index) const {
    return items_.at(index).Value<T>();
  }

  template <>
  ReflectiveFlatbuffer* Get(int index) const {
    if (is_primitive_) {
      TC3_LOG(ERROR) << "Trying to get primitive value out of non-primitive "
                        "repeated field.";
      return nullptr;
    }
    return object_items_.at(index).get();
  }

  int Size() const {
    if (is_primitive_) {
      return items_.size();
    } else {
      return object_items_.size();
    }
  }

  flatbuffers::uoffset_t Serialize(
      flatbuffers::FlatBufferBuilder* builder) const;

 private:
  flatbuffers::uoffset_t SerializeString(
      flatbuffers::FlatBufferBuilder* builder) const;
  flatbuffers::uoffset_t SerializeObject(
      flatbuffers::FlatBufferBuilder* builder) const;

  const reflection::Schema* const schema_;
  const reflection::Field* field_;
  bool is_primitive_;

  std::vector<Variant> items_;
  std::vector<std::unique_ptr<ReflectiveFlatbuffer>> object_items_;
};

template <typename T>
bool ReflectiveFlatbuffer::Set(StringPiece field_name, T value) {
  if (const reflection::Field* field = GetFieldOrNull(field_name)) {
    if (field->type()->base_type() == reflection::BaseType::Vector ||
        field->type()->base_type() == reflection::BaseType::Obj) {
      TC3_LOG(ERROR)
          << "Trying to set a primitive value on a non-scalar field.";
      return false;
    }
    return Set<T>(field, value);
  }
  TC3_LOG(ERROR) << "Couldn't find a field: " << field_name;
  return false;
}

template <typename T>
bool ReflectiveFlatbuffer::Set(const reflection::Field* field, T value) {
  if (field == nullptr) {
    TC3_LOG(ERROR) << "Expected non-null field.";
    return false;
  }
  Variant variant_value(value);
  if (!IsMatchingType<T>(field->type()->base_type())) {
    TC3_LOG(ERROR) << "Type mismatch for field `" << field->name()->str()
                   << "`, expected: " << field->type()->base_type()
                   << ", got: " << variant_value.GetType();
    return false;
  }
  fields_[field] = variant_value;
  return true;
}

template <typename T>
bool ReflectiveFlatbuffer::Set(const FlatbufferFieldPath* path, T value) {
  ReflectiveFlatbuffer* parent;
  const reflection::Field* field;
  if (!GetFieldWithParent(path, &parent, &field)) {
    return false;
  }
  return parent->Set<T>(field, value);
}

template <typename T>
bool ReflectiveFlatbuffer::Add(StringPiece field_name, T value) {
  const reflection::Field* field = GetFieldOrNull(field_name);
  if (field == nullptr) {
    return false;
  }

  if (field->type()->base_type() != reflection::BaseType::Vector) {
    return false;
  }

  return Add<T>(field, value);
}

template <typename T>
bool ReflectiveFlatbuffer::Add(const reflection::Field* field, T value) {
  if (field == nullptr) {
    return false;
  }
  Repeated(field)->Add(value);
  return true;
}

template <typename T>
bool RepeatedField::Add(const T value) {
  if (!is_primitive_ || !IsMatchingType<T>(field_->type()->element())) {
    TC3_LOG(ERROR) << "Trying to add value of unmatching type.";
    return false;
  }
  items_.push_back(Variant{value});
  return true;
}

// Resolves field lookups by name to the concrete field offsets.
bool SwapFieldNamesForOffsetsInPath(const reflection::Schema* schema,
                                    FlatbufferFieldPathT* path);

template <typename T>
bool ReflectiveFlatbuffer::AppendFromVector(const flatbuffers::Table* from,
                                            const reflection::Field* field) {
  const flatbuffers::Vector<T>* from_vector =
      from->GetPointer<const flatbuffers::Vector<T>*>(field->offset());
  if (from_vector == nullptr) {
    return false;
  }

  RepeatedField* to_repeated = Repeated(field);
  for (const T element : *from_vector) {
    to_repeated->Add(element);
  }
  return true;
}

inline logging::LoggingStringStream& operator<<(
    logging::LoggingStringStream& stream, flatbuffers::String* message) {
  if (message != nullptr) {
    stream.message.append(message->c_str(), message->size());
  }
  return stream;
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_FLATBUFFERS_H_
