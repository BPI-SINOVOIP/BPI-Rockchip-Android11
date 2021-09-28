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

#include "utils/lua-utils.h"

// lua_dump takes an extra argument "strip" in 5.3, but not in 5.2.
#ifndef TC3_AOSP
#define lua_dump(L, w, d, s) lua_dump((L), (w), (d))
#endif

namespace libtextclassifier3 {
namespace {
static constexpr luaL_Reg defaultlibs[] = {{"_G", luaopen_base},
                                           {LUA_TABLIBNAME, luaopen_table},
                                           {LUA_STRLIBNAME, luaopen_string},
                                           {LUA_BITLIBNAME, luaopen_bit32},
                                           {LUA_MATHLIBNAME, luaopen_math},
                                           {nullptr, nullptr}};

static constexpr const char kTextKey[] = "text";
static constexpr const char kTimeUsecKey[] = "parsed_time_ms_utc";
static constexpr const char kGranularityKey[] = "granularity";
static constexpr const char kCollectionKey[] = "collection";
static constexpr const char kNameKey[] = "name";
static constexpr const char kScoreKey[] = "score";
static constexpr const char kPriorityScoreKey[] = "priority_score";
static constexpr const char kTypeKey[] = "type";
static constexpr const char kResponseTextKey[] = "response_text";
static constexpr const char kAnnotationKey[] = "annotation";
static constexpr const char kSpanKey[] = "span";
static constexpr const char kMessageKey[] = "message";
static constexpr const char kBeginKey[] = "begin";
static constexpr const char kEndKey[] = "end";
static constexpr const char kClassificationKey[] = "classification";
static constexpr const char kSerializedEntity[] = "serialized_entity";
static constexpr const char kEntityKey[] = "entity";

// Implementation of a lua_Writer that appends the data to a string.
int LuaStringWriter(lua_State* state, const void* data, size_t size,
                    void* result) {
  std::string* const result_string = static_cast<std::string*>(result);
  result_string->insert(result_string->size(), static_cast<const char*>(data),
                        size);
  return LUA_OK;
}

}  // namespace

LuaEnvironment::LuaEnvironment() { state_ = luaL_newstate(); }

LuaEnvironment::~LuaEnvironment() {
  if (state_ != nullptr) {
    lua_close(state_);
  }
}

void LuaEnvironment::PushFlatbuffer(const reflection::Schema* schema,
                                    const reflection::Object* type,
                                    const flatbuffers::Table* table) const {
  PushLazyObject(
      std::bind(&LuaEnvironment::GetField, this, schema, type, table));
}

int LuaEnvironment::GetField(const reflection::Schema* schema,
                             const reflection::Object* type,
                             const flatbuffers::Table* table) const {
  const char* field_name = lua_tostring(state_, /*idx=*/kIndexStackTop);
  const reflection::Field* field = type->fields()->LookupByKey(field_name);
  if (field == nullptr) {
    lua_error(state_);
    return 0;
  }
  // Provide primitive fields directly.
  const reflection::BaseType field_type = field->type()->base_type();
  switch (field_type) {
    case reflection::Bool:
    case reflection::UByte:
      Push(table->GetField<uint8>(field->offset(), field->default_integer()));
      break;
    case reflection::Byte:
      Push(table->GetField<int8>(field->offset(), field->default_integer()));
      break;
    case reflection::Int:
      Push(table->GetField<int32>(field->offset(), field->default_integer()));
      break;
    case reflection::UInt:
      Push(table->GetField<uint32>(field->offset(), field->default_integer()));
      break;
    case reflection::Short:
      Push(table->GetField<int16>(field->offset(), field->default_integer()));
      break;
    case reflection::UShort:
      Push(table->GetField<uint16>(field->offset(), field->default_integer()));
      break;
    case reflection::Long:
      Push(table->GetField<int64>(field->offset(), field->default_integer()));
      break;
    case reflection::ULong:
      Push(table->GetField<uint64>(field->offset(), field->default_integer()));
      break;
    case reflection::Float:
      Push(table->GetField<float>(field->offset(), field->default_real()));
      break;
    case reflection::Double:
      Push(table->GetField<double>(field->offset(), field->default_real()));
      break;
    case reflection::String: {
      Push(table->GetPointer<const flatbuffers::String*>(field->offset()));
      break;
    }
    case reflection::Obj: {
      const flatbuffers::Table* field_table =
          table->GetPointer<const flatbuffers::Table*>(field->offset());
      if (field_table == nullptr) {
        // Field was not set in entity data.
        return 0;
      }
      const reflection::Object* field_type =
          schema->objects()->Get(field->type()->index());
      PushFlatbuffer(schema, field_type, field_table);
      break;
    }
    case reflection::Vector: {
      const flatbuffers::Vector<flatbuffers::Offset<void>>* field_vector =
          table->GetPointer<
              const flatbuffers::Vector<flatbuffers::Offset<void>>*>(
              field->offset());
      if (field_vector == nullptr) {
        // Repeated field was not set in flatbuffer.
        PushEmptyVector();
        break;
      }
      switch (field->type()->element()) {
        case reflection::Bool:
        case reflection::UByte:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<uint8>*>(
                  field->offset()));
          break;
        case reflection::Byte:
          PushRepeatedField(table->GetPointer<const flatbuffers::Vector<int8>*>(
              field->offset()));
          break;
        case reflection::Int:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<int32>*>(
                  field->offset()));
          break;
        case reflection::UInt:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<uint32>*>(
                  field->offset()));
          break;
        case reflection::Short:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<int16>*>(
                  field->offset()));
          break;
        case reflection::UShort:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<uint16>*>(
                  field->offset()));
          break;
        case reflection::Long:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<int64>*>(
                  field->offset()));
          break;
        case reflection::ULong:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<uint64>*>(
                  field->offset()));
          break;
        case reflection::Float:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<float>*>(
                  field->offset()));
          break;
        case reflection::Double:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<double>*>(
                  field->offset()));
          break;
        case reflection::String:
          PushRepeatedField(
              table->GetPointer<const flatbuffers::Vector<
                  flatbuffers::Offset<flatbuffers::String>>*>(field->offset()));
          break;
        case reflection::Obj:
          PushRepeatedFlatbufferField(
              schema, schema->objects()->Get(field->type()->index()),
              table->GetPointer<const flatbuffers::Vector<
                  flatbuffers::Offset<flatbuffers::Table>>*>(field->offset()));
          break;
        default:
          TC3_LOG(ERROR) << "Unsupported repeated type: "
                         << field->type()->element();
          lua_error(state_);
          return 0;
      }
      break;
    }
    default:
      TC3_LOG(ERROR) << "Unsupported type: " << field_type;
      lua_error(state_);
      return 0;
  }
  return 1;
}

int LuaEnvironment::ReadFlatbuffer(const int index,
                                   ReflectiveFlatbuffer* buffer) const {
  if (buffer == nullptr) {
    TC3_LOG(ERROR) << "Called ReadFlatbuffer with null buffer: " << index;
    lua_error(state_);
    return LUA_ERRRUN;
  }
  if (lua_type(state_, /*idx=*/index) != LUA_TTABLE) {
    TC3_LOG(ERROR) << "Expected table, got: "
                   << lua_type(state_, /*idx=*/kIndexStackTop);
    lua_error(state_);
    return LUA_ERRRUN;
  }

  lua_pushnil(state_);
  while (Next(index - 1)) {
    const StringPiece key = ReadString(/*index=*/index - 1);
    const reflection::Field* field = buffer->GetFieldOrNull(key);
    if (field == nullptr) {
      TC3_LOG(ERROR) << "Unknown field: " << key;
      lua_error(state_);
      return LUA_ERRRUN;
    }
    switch (field->type()->base_type()) {
      case reflection::Obj:
        ReadFlatbuffer(/*index=*/kIndexStackTop, buffer->Mutable(field));
        break;
      case reflection::Bool:
        buffer->Set(field, Read<bool>(/*index=*/kIndexStackTop));
        break;
      case reflection::Byte:
        buffer->Set(field, Read<int8>(/*index=*/kIndexStackTop));
        break;
      case reflection::UByte:
        buffer->Set(field, Read<uint8>(/*index=*/kIndexStackTop));
        break;
      case reflection::Int:
        buffer->Set(field, Read<int32>(/*index=*/kIndexStackTop));
        break;
      case reflection::UInt:
        buffer->Set(field, Read<uint32>(/*index=*/kIndexStackTop));
        break;
      case reflection::Long:
        buffer->Set(field, Read<int64>(/*index=*/kIndexStackTop));
        break;
      case reflection::ULong:
        buffer->Set(field, Read<uint64>(/*index=*/kIndexStackTop));
        break;
      case reflection::Float:
        buffer->Set(field, Read<float>(/*index=*/kIndexStackTop));
        break;
      case reflection::Double:
        buffer->Set(field, Read<double>(/*index=*/kIndexStackTop));
        break;
      case reflection::String: {
        buffer->Set(field, ReadString(/*index=*/kIndexStackTop));
        break;
      }
      case reflection::Vector: {
        // Read repeated field.
        switch (field->type()->element()) {
          case reflection::Bool:
            ReadRepeatedField<bool>(/*index=*/kIndexStackTop,
                                    buffer->Repeated(field));
            break;
          case reflection::Byte:
            ReadRepeatedField<int8>(/*index=*/kIndexStackTop,
                                    buffer->Repeated(field));
            break;
          case reflection::UByte:
            ReadRepeatedField<uint8>(/*index=*/kIndexStackTop,
                                     buffer->Repeated(field));
            break;
          case reflection::Int:
            ReadRepeatedField<int32>(/*index=*/kIndexStackTop,
                                     buffer->Repeated(field));
            break;
          case reflection::UInt:
            ReadRepeatedField<uint32>(/*index=*/kIndexStackTop,
                                      buffer->Repeated(field));
            break;
          case reflection::Long:
            ReadRepeatedField<int64>(/*index=*/kIndexStackTop,
                                     buffer->Repeated(field));
            break;
          case reflection::ULong:
            ReadRepeatedField<uint64>(/*index=*/kIndexStackTop,
                                      buffer->Repeated(field));
            break;
          case reflection::Float:
            ReadRepeatedField<float>(/*index=*/kIndexStackTop,
                                     buffer->Repeated(field));
            break;
          case reflection::Double:
            ReadRepeatedField<double>(/*index=*/kIndexStackTop,
                                      buffer->Repeated(field));
            break;
          case reflection::String:
            ReadRepeatedField<std::string>(/*index=*/kIndexStackTop,
                                           buffer->Repeated(field));
            break;
          case reflection::Obj:
            ReadRepeatedField<ReflectiveFlatbuffer>(/*index=*/kIndexStackTop,
                                                    buffer->Repeated(field));
            break;
          default:
            TC3_LOG(ERROR) << "Unsupported repeated field type: "
                           << field->type()->element();
            lua_error(state_);
            return LUA_ERRRUN;
        }
        break;
      }
      default:
        TC3_LOG(ERROR) << "Unsupported type: " << field->type()->base_type();
        lua_error(state_);
        return LUA_ERRRUN;
    }
    lua_pop(state_, 1);
  }
  return LUA_OK;
}

void LuaEnvironment::LoadDefaultLibraries() {
  for (const luaL_Reg* lib = defaultlibs; lib->func; lib++) {
    luaL_requiref(state_, lib->name, lib->func, 1);
    lua_pop(state_, 1);  // Remove lib.
  }
}

StringPiece LuaEnvironment::ReadString(const int index) const {
  size_t length = 0;
  const char* data = lua_tolstring(state_, index, &length);
  return StringPiece(data, length);
}

void LuaEnvironment::PushString(const StringPiece str) const {
  lua_pushlstring(state_, str.data(), str.size());
}

bool LuaEnvironment::Compile(StringPiece snippet, std::string* bytecode) const {
  if (luaL_loadbuffer(state_, snippet.data(), snippet.size(),
                      /*name=*/nullptr) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not compile lua snippet: "
                   << ReadString(/*index=*/kIndexStackTop);
    lua_pop(state_, 1);
    return false;
  }
  if (lua_dump(state_, LuaStringWriter, bytecode, /*strip*/ 1) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not dump compiled lua snippet.";
    lua_pop(state_, 1);
    return false;
  }
  lua_pop(state_, 1);
  return true;
}

void LuaEnvironment::PushAnnotation(
    const ClassificationResult& classification,
    const reflection::Schema* entity_data_schema) const {
  if (entity_data_schema == nullptr ||
      classification.serialized_entity_data.empty()) {
    // Empty table.
    lua_newtable(state_);
  } else {
    PushFlatbuffer(entity_data_schema,
                   flatbuffers::GetRoot<flatbuffers::Table>(
                       classification.serialized_entity_data.data()));
  }
  Push(classification.datetime_parse_result.time_ms_utc);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kTimeUsecKey);
  Push(classification.datetime_parse_result.granularity);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kGranularityKey);
  Push(classification.collection);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kCollectionKey);
  Push(classification.score);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kScoreKey);
  Push(classification.serialized_entity_data);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kSerializedEntity);
}

void LuaEnvironment::PushAnnotation(
    const ClassificationResult& classification, StringPiece text,
    const reflection::Schema* entity_data_schema) const {
  PushAnnotation(classification, entity_data_schema);
  Push(text);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kTextKey);
}

void LuaEnvironment::PushAnnotation(
    const ActionSuggestionAnnotation& annotation,
    const reflection::Schema* entity_data_schema) const {
  PushAnnotation(annotation.entity, annotation.span.text, entity_data_schema);
  PushString(annotation.name);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kNameKey);
  {
    lua_newtable(state_);
    Push(annotation.span.message_index);
    lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kMessageKey);
    Push(annotation.span.span.first);
    lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kBeginKey);
    Push(annotation.span.span.second);
    lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kEndKey);
  }
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kSpanKey);
}

void LuaEnvironment::PushAnnotatedSpan(
    const AnnotatedSpan& annotated_span,
    const reflection::Schema* entity_data_schema) const {
  lua_newtable(state_);
  {
    lua_newtable(state_);
    Push(annotated_span.span.first);
    lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kBeginKey);
    Push(annotated_span.span.second);
    lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kEndKey);
  }
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kSpanKey);
  PushAnnotations(&annotated_span.classification, entity_data_schema);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kClassificationKey);
}

void LuaEnvironment::PushAnnotatedSpans(
    const std::vector<AnnotatedSpan>* annotated_spans,
    const reflection::Schema* entity_data_schema) const {
  PushIterator(annotated_spans ? annotated_spans->size() : 0,
               [this, annotated_spans, entity_data_schema](const int64 index) {
                 PushAnnotatedSpan(annotated_spans->at(index),
                                   entity_data_schema);
                 return 1;
               });
}

MessageTextSpan LuaEnvironment::ReadSpan() const {
  MessageTextSpan span;
  lua_pushnil(state_);
  while (Next(/*index=*/kIndexStackTop - 1)) {
    const StringPiece key = ReadString(/*index=*/kIndexStackTop - 1);
    if (key.Equals(kMessageKey)) {
      span.message_index = Read<int>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kBeginKey)) {
      span.span.first = Read<int>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kEndKey)) {
      span.span.second = Read<int>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kTextKey)) {
      span.text = Read<std::string>(/*index=*/kIndexStackTop);
    } else {
      TC3_LOG(INFO) << "Unknown span field: " << key;
    }
    lua_pop(state_, 1);
  }
  return span;
}

int LuaEnvironment::ReadAnnotations(
    const reflection::Schema* entity_data_schema,
    std::vector<ActionSuggestionAnnotation>* annotations) const {
  if (lua_type(state_, /*idx=*/kIndexStackTop) != LUA_TTABLE) {
    TC3_LOG(ERROR) << "Expected annotations table, got: "
                   << lua_type(state_, /*idx=*/kIndexStackTop);
    lua_pop(state_, 1);
    lua_error(state_);
    return LUA_ERRRUN;
  }

  // Read actions.
  lua_pushnil(state_);
  while (Next(/*index=*/kIndexStackTop - 1)) {
    if (lua_type(state_, /*idx=*/kIndexStackTop) != LUA_TTABLE) {
      TC3_LOG(ERROR) << "Expected annotation table, got: "
                     << lua_type(state_, /*idx=*/kIndexStackTop);
      lua_pop(state_, 1);
      continue;
    }
    annotations->push_back(ReadAnnotation(entity_data_schema));
    lua_pop(state_, 1);
  }
  return LUA_OK;
}

ActionSuggestionAnnotation LuaEnvironment::ReadAnnotation(
    const reflection::Schema* entity_data_schema) const {
  ActionSuggestionAnnotation annotation;
  lua_pushnil(state_);
  while (Next(/*index=*/kIndexStackTop - 1)) {
    const StringPiece key = ReadString(/*index=*/kIndexStackTop - 1);
    if (key.Equals(kNameKey)) {
      annotation.name = Read<std::string>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kSpanKey)) {
      annotation.span = ReadSpan();
    } else if (key.Equals(kEntityKey)) {
      annotation.entity = ReadClassificationResult(entity_data_schema);
    } else {
      TC3_LOG(ERROR) << "Unknown annotation field: " << key;
    }
    lua_pop(state_, 1);
  }
  return annotation;
}

ClassificationResult LuaEnvironment::ReadClassificationResult(
    const reflection::Schema* entity_data_schema) const {
  ClassificationResult classification;
  lua_pushnil(state_);
  while (Next(/*index=*/kIndexStackTop - 1)) {
    const StringPiece key = ReadString(/*index=*/kIndexStackTop - 1);
    if (key.Equals(kCollectionKey)) {
      classification.collection = Read<std::string>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kScoreKey)) {
      classification.score = Read<float>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kTimeUsecKey)) {
      classification.datetime_parse_result.time_ms_utc =
          Read<int64>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kGranularityKey)) {
      classification.datetime_parse_result.granularity =
          static_cast<DatetimeGranularity>(
              lua_tonumber(state_, /*idx=*/kIndexStackTop));
    } else if (key.Equals(kSerializedEntity)) {
      classification.serialized_entity_data =
          Read<std::string>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kEntityKey)) {
      auto buffer = ReflectiveFlatbufferBuilder(entity_data_schema).NewRoot();
      ReadFlatbuffer(/*index=*/kIndexStackTop, buffer.get());
      classification.serialized_entity_data = buffer->Serialize();
    } else {
      TC3_LOG(INFO) << "Unknown classification result field: " << key;
    }
    lua_pop(state_, 1);
  }
  return classification;
}

void LuaEnvironment::PushAction(
    const ActionSuggestion& action,
    const reflection::Schema* actions_entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema) const {
  if (actions_entity_data_schema == nullptr ||
      action.serialized_entity_data.empty()) {
    // Empty table.
    lua_newtable(state_);
  } else {
    PushFlatbuffer(actions_entity_data_schema,
                   flatbuffers::GetRoot<flatbuffers::Table>(
                       action.serialized_entity_data.data()));
  }
  PushString(action.type);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kTypeKey);
  PushString(action.response_text);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kResponseTextKey);
  Push(action.score);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kScoreKey);
  Push(action.priority_score);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kPriorityScoreKey);
  PushAnnotations(&action.annotations, annotations_entity_data_schema);
  lua_setfield(state_, /*idx=*/kIndexStackTop - 1, kAnnotationKey);
}

void LuaEnvironment::PushActions(
    const std::vector<ActionSuggestion>* actions,
    const reflection::Schema* actions_entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema) const {
  PushIterator(actions ? actions->size() : 0,
               [this, actions, actions_entity_data_schema,
                annotations_entity_data_schema](const int64 index) {
                 PushAction(actions->at(index), actions_entity_data_schema,
                            annotations_entity_data_schema);
                 return 1;
               });
}

ActionSuggestion LuaEnvironment::ReadAction(
    const reflection::Schema* actions_entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema) const {
  ActionSuggestion action;
  lua_pushnil(state_);
  while (Next(/*index=*/kIndexStackTop - 1)) {
    const StringPiece key = ReadString(/*index=*/kIndexStackTop - 1);
    if (key.Equals(kResponseTextKey)) {
      action.response_text = Read<std::string>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kTypeKey)) {
      action.type = Read<std::string>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kScoreKey)) {
      action.score = Read<float>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kPriorityScoreKey)) {
      action.priority_score = Read<float>(/*index=*/kIndexStackTop);
    } else if (key.Equals(kAnnotationKey)) {
      ReadAnnotations(actions_entity_data_schema, &action.annotations);
    } else if (key.Equals(kEntityKey)) {
      auto buffer =
          ReflectiveFlatbufferBuilder(actions_entity_data_schema).NewRoot();
      ReadFlatbuffer(/*index=*/kIndexStackTop, buffer.get());
      action.serialized_entity_data = buffer->Serialize();
    } else {
      TC3_LOG(INFO) << "Unknown action field: " << key;
    }
    lua_pop(state_, 1);
  }
  return action;
}

int LuaEnvironment::ReadActions(
    const reflection::Schema* actions_entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema,
    std::vector<ActionSuggestion>* actions) const {
  // Read actions.
  lua_pushnil(state_);
  while (Next(/*index=*/kIndexStackTop - 1)) {
    if (lua_type(state_, /*idx=*/kIndexStackTop) != LUA_TTABLE) {
      TC3_LOG(ERROR) << "Expected action table, got: "
                     << lua_type(state_, /*idx=*/kIndexStackTop);
      lua_pop(state_, 1);
      continue;
    }
    actions->push_back(
        ReadAction(actions_entity_data_schema, annotations_entity_data_schema));
    lua_pop(state_, /*n=*/1);
  }
  lua_pop(state_, /*n=*/1);

  return LUA_OK;
}

void LuaEnvironment::PushConversation(
    const std::vector<ConversationMessage>* conversation,
    const reflection::Schema* annotations_entity_data_schema) const {
  PushIterator(
      conversation ? conversation->size() : 0,
      [this, conversation, annotations_entity_data_schema](const int64 index) {
        const ConversationMessage& message = conversation->at(index);
        lua_newtable(state_);
        Push(message.user_id);
        lua_setfield(state_, /*idx=*/kIndexStackTop - 1, "user_id");
        Push(message.text);
        lua_setfield(state_, /*idx=*/kIndexStackTop - 1, "text");
        Push(message.reference_time_ms_utc);
        lua_setfield(state_, /*idx=*/kIndexStackTop - 1, "time_ms_utc");
        Push(message.reference_timezone);
        lua_setfield(state_, /*idx=*/kIndexStackTop - 1, "timezone");
        PushAnnotatedSpans(&message.annotations,
                           annotations_entity_data_schema);
        lua_setfield(state_, /*idx=*/kIndexStackTop - 1, "annotation");
        return 1;
      });
}

bool Compile(StringPiece snippet, std::string* bytecode) {
  return LuaEnvironment().Compile(snippet, bytecode);
}

}  // namespace libtextclassifier3
