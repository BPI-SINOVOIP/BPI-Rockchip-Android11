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

#ifndef LIBTEXTCLASSIFIER_UTILS_LUA_UTILS_H_
#define LIBTEXTCLASSIFIER_UTILS_LUA_UTILS_H_

#include <vector>

#include "actions/types.h"
#include "annotator/types.h"
#include "utils/flatbuffers.h"
#include "utils/strings/stringpiece.h"
#include "utils/variant.h"
#include "flatbuffers/reflection_generated.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#ifdef __cplusplus
}
#endif

namespace libtextclassifier3 {

static constexpr const char kLengthKey[] = "__len";
static constexpr const char kPairsKey[] = "__pairs";
static constexpr const char kIndexKey[] = "__index";
static constexpr const char kGcKey[] = "__gc";
static constexpr const char kNextKey[] = "__next";

static constexpr const int kIndexStackTop = -1;

// Casts to the lua user data type.
template <typename T>
void* AsUserData(const T* value) {
  return static_cast<void*>(const_cast<T*>(value));
}
template <typename T>
void* AsUserData(const T value) {
  return reinterpret_cast<void*>(value);
}

// Retrieves up-values.
template <typename T>
T FromUpValue(const int index, lua_State* state) {
  return static_cast<T>(lua_touserdata(state, lua_upvalueindex(index)));
}

class LuaEnvironment {
 public:
  virtual ~LuaEnvironment();
  LuaEnvironment();

  // Compile a lua snippet into binary bytecode.
  // NOTE: The compiled bytecode might not be compatible across Lua versions
  // and platforms.
  bool Compile(StringPiece snippet, std::string* bytecode) const;

  // Loads default libraries.
  void LoadDefaultLibraries();

  // Provides a callback to Lua.
  template <typename T>
  void PushFunction(int (T::*handler)()) {
    PushFunction(std::bind(handler, static_cast<T*>(this)));
  }

  template <typename F>
  void PushFunction(const F& func) const {
    // Copy closure to the lua stack.
    new (lua_newuserdata(state_, sizeof(func))) F(func);

    // Register garbage collection callback.
    lua_newtable(state_);
    lua_pushcfunction(state_, &ReleaseFunction<F>);
    lua_setfield(state_, -2, kGcKey);
    lua_setmetatable(state_, -2);

    // Push dispatch.
    lua_pushcclosure(state_, &CallFunction<F>, 1);
  }

  // Sets up a named table that calls back whenever a member is accessed.
  // This allows to lazily provide required information to the script.
  template <typename T>
  void PushLazyObject(int (T::*handler)()) {
    PushLazyObject(std::bind(handler, static_cast<T*>(this)));
  }

  template <typename F>
  void PushLazyObject(const F& func) const {
    lua_newtable(state_);
    lua_newtable(state_);
    PushFunction(func);
    lua_setfield(state_, -2, kIndexKey);
    lua_setmetatable(state_, -2);
  }

  void Push(const int64 value) const { lua_pushinteger(state_, value); }
  void Push(const uint64 value) const { lua_pushinteger(state_, value); }
  void Push(const int32 value) const { lua_pushinteger(state_, value); }
  void Push(const uint32 value) const { lua_pushinteger(state_, value); }
  void Push(const int16 value) const { lua_pushinteger(state_, value); }
  void Push(const uint16 value) const { lua_pushinteger(state_, value); }
  void Push(const int8 value) const { lua_pushinteger(state_, value); }
  void Push(const uint8 value) const { lua_pushinteger(state_, value); }
  void Push(const float value) const { lua_pushnumber(state_, value); }
  void Push(const double value) const { lua_pushnumber(state_, value); }
  void Push(const bool value) const { lua_pushboolean(state_, value); }
  void Push(const StringPiece value) const { PushString(value); }
  void Push(const flatbuffers::String* value) const {
    if (value == nullptr) {
      PushString("");
    } else {
      PushString(StringPiece(value->c_str(), value->size()));
    }
  }

  template <typename T>
  T Read(const int index = -1) const;

  template <>
  int64 Read<int64>(const int index) const {
    return static_cast<int64>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  uint64 Read<uint64>(const int index) const {
    return static_cast<uint64>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  int32 Read<int32>(const int index) const {
    return static_cast<int32>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  uint32 Read<uint32>(const int index) const {
    return static_cast<uint32>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  int16 Read<int16>(const int index) const {
    return static_cast<int16>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  uint16 Read<uint16>(const int index) const {
    return static_cast<uint16>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  int8 Read<int8>(const int index) const {
    return static_cast<int8>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  uint8 Read<uint8>(const int index) const {
    return static_cast<uint8>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  float Read<float>(const int index) const {
    return static_cast<float>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  double Read<double>(const int index) const {
    return static_cast<double>(lua_tonumber(state_, /*idx=*/index));
  }

  template <>
  bool Read<bool>(const int index) const {
    return lua_toboolean(state_, /*idx=*/index);
  }

  template <>
  StringPiece Read<StringPiece>(const int index) const {
    return ReadString(index);
  }

  template <>
  std::string Read<std::string>(const int index) const {
    return ReadString(index).ToString();
  }

  // Reads a string from the stack.
  StringPiece ReadString(int index) const;

  // Pushes a string to the stack.
  void PushString(const StringPiece str) const;

  // Pushes a flatbuffer to the stack.
  void PushFlatbuffer(const reflection::Schema* schema,
                      const flatbuffers::Table* table) const {
    PushFlatbuffer(schema, schema->root_table(), table);
  }

  // Reads a flatbuffer from the stack.
  int ReadFlatbuffer(int index, ReflectiveFlatbuffer* buffer) const;

  // Pushes an iterator.
  template <typename ItemCallback, typename KeyCallback>
  void PushIterator(const int length, const ItemCallback& item_callback,
                    const KeyCallback& key_callback) const {
    lua_newtable(state_);
    CreateIteratorMetatable(length, item_callback);
    PushFunction([this, length, item_callback, key_callback]() {
      return Iterator::Dispatch(this, length, item_callback, key_callback);
    });
    lua_setfield(state_, -2, kIndexKey);
    lua_setmetatable(state_, -2);
  }

  template <typename ItemCallback>
  void PushIterator(const int length, const ItemCallback& item_callback) const {
    lua_newtable(state_);
    CreateIteratorMetatable(length, item_callback);
    PushFunction([this, length, item_callback]() {
      return Iterator::Dispatch(this, length, item_callback);
    });
    lua_setfield(state_, -2, kIndexKey);
    lua_setmetatable(state_, -2);
  }

  template <typename ItemCallback>
  void CreateIteratorMetatable(const int length,
                               const ItemCallback& item_callback) const {
    lua_newtable(state_);
    PushFunction([this, length]() { return Iterator::Length(this, length); });
    lua_setfield(state_, -2, kLengthKey);
    PushFunction([this, length, item_callback]() {
      return Iterator::IterItems(this, length, item_callback);
    });
    lua_setfield(state_, -2, kPairsKey);
    PushFunction([this, length, item_callback]() {
      return Iterator::Next(this, length, item_callback);
    });
    lua_setfield(state_, -2, kNextKey);
  }

  template <typename T>
  void PushVectorIterator(const std::vector<T>* items) const {
    PushIterator(items ? items->size() : 0, [this, items](const int64 pos) {
      this->Push(items->at(pos));
      return 1;
    });
  }

  template <typename T>
  void PushVector(const std::vector<T>& items) const {
    lua_newtable(state_);
    for (int i = 0; i < items.size(); i++) {
      // Key: index, 1-based.
      Push(i + 1);

      // Value.
      Push(items[i]);
      lua_settable(state_, /*idx=*/-3);
    }
  }

  void PushEmptyVector() const { lua_newtable(state_); }

  template <typename T>
  std::vector<T> ReadVector(const int index = -1) const {
    std::vector<T> result;
    if (lua_type(state_, /*idx=*/index) != LUA_TTABLE) {
      TC3_LOG(ERROR) << "Expected a table, got: "
                     << lua_type(state_, /*idx=*/kIndexStackTop);
      lua_pop(state_, 1);
      return {};
    }
    lua_pushnil(state_);
    while (Next(index - 1)) {
      result.push_back(Read<T>(/*index=*/kIndexStackTop));
      lua_pop(state_, 1);
    }
    return result;
  }

  // Runs a closure in protected mode.
  // `func`: closure to run in protected mode.
  // `num_lua_args`: number of arguments from the lua stack to process.
  // `num_results`: number of result values pushed on the stack.
  template <typename F>
  int RunProtected(const F& func, const int num_args = 0,
                   const int num_results = 0) const {
    PushFunction(func);
    // Put the closure before the arguments on the stack.
    if (num_args > 0) {
      lua_insert(state_, -(1 + num_args));
    }
    return lua_pcall(state_, num_args, num_results, /*errorfunc=*/0);
  }

  // Auxiliary methods to handle model results.
  // Provides an annotation to lua.
  void PushAnnotation(const ClassificationResult& classification,
                      const reflection::Schema* entity_data_schema) const;
  void PushAnnotation(const ClassificationResult& classification,
                      StringPiece text,
                      const reflection::Schema* entity_data_schema) const;
  void PushAnnotation(const ActionSuggestionAnnotation& annotation,
                      const reflection::Schema* entity_data_schema) const;

  template <typename Annotation>
  void PushAnnotations(const std::vector<Annotation>* annotations,
                       const reflection::Schema* entity_data_schema) const {
    PushIterator(
        annotations ? annotations->size() : 0,
        [this, annotations, entity_data_schema](const int64 index) {
          PushAnnotation(annotations->at(index), entity_data_schema);
          return 1;
        },
        [this, annotations, entity_data_schema](StringPiece name) {
          if (const Annotation* annotation =
                  GetAnnotationByName(*annotations, name)) {
            PushAnnotation(*annotation, entity_data_schema);
            return 1;
          } else {
            return 0;
          }
        });
  }

  // Pushes a span to the lua stack.
  void PushAnnotatedSpan(const AnnotatedSpan& annotated_span,
                         const reflection::Schema* entity_data_schema) const;
  void PushAnnotatedSpans(const std::vector<AnnotatedSpan>* annotated_spans,
                          const reflection::Schema* entity_data_schema) const;

  // Reads a message text span from lua.
  MessageTextSpan ReadSpan() const;

  ActionSuggestionAnnotation ReadAnnotation(
      const reflection::Schema* entity_data_schema) const;
  int ReadAnnotations(
      const reflection::Schema* entity_data_schema,
      std::vector<ActionSuggestionAnnotation>* annotations) const;
  ClassificationResult ReadClassificationResult(
      const reflection::Schema* entity_data_schema) const;

  // Provides an action to lua.
  void PushAction(
      const ActionSuggestion& action,
      const reflection::Schema* actions_entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema) const;

  void PushActions(
      const std::vector<ActionSuggestion>* actions,
      const reflection::Schema* actions_entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema) const;

  ActionSuggestion ReadAction(
      const reflection::Schema* actions_entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema) const;

  int ReadActions(const reflection::Schema* actions_entity_data_schema,
                  const reflection::Schema* annotations_entity_data_schema,
                  std::vector<ActionSuggestion>* actions) const;

  // Conversation message iterator.
  void PushConversation(
      const std::vector<ConversationMessage>* conversation,
      const reflection::Schema* annotations_entity_data_schema) const;

  lua_State* state() const { return state_; }

 protected:
  // Wrapper for handling iteration over containers.
  class Iterator {
   public:
    // Starts a new key-value pair iterator.
    template <typename ItemCallback>
    static int IterItems(const LuaEnvironment* env, const int length,
                         const ItemCallback& callback) {
      env->PushFunction([env, callback, length, pos = 0]() mutable {
        if (pos >= length) {
          lua_pushnil(env->state());
          return 1;
        }

        // Push key.
        lua_pushinteger(env->state(), pos + 1);

        // Push item.
        return 1 + callback(pos++);
      });
      return 1;  // Num. results.
    }

    // Gets the next element.
    template <typename ItemCallback>
    static int Next(const LuaEnvironment* env, const int length,
                    const ItemCallback& item_callback) {
      int64 pos = lua_isnil(env->state(), /*idx=*/kIndexStackTop)
                      ? 0
                      : env->Read<int64>(/*index=*/kIndexStackTop);
      if (pos < length) {
        // Push next key.
        lua_pushinteger(env->state(), pos + 1);

        // Push item.
        return 1 + item_callback(pos);
      } else {
        lua_pushnil(env->state());
        return 1;
      }
    }

    // Returns the length of the container the iterator processes.
    static int Length(const LuaEnvironment* env, const int length) {
      lua_pushinteger(env->state(), length);
      return 1;  // Num. results.
    }

    // Handles item queries to the iterator.
    // Elements of the container can either be queried by name or index.
    // Dispatch will check how an element is accessed and
    // calls `key_callback` for access by name and `item_callback` for access by
    // index.
    template <typename ItemCallback, typename KeyCallback>
    static int Dispatch(const LuaEnvironment* env, const int length,
                        const ItemCallback& item_callback,
                        const KeyCallback& key_callback) {
      switch (lua_type(env->state(), kIndexStackTop)) {
        case LUA_TNUMBER: {
          // Lua is one based, so adjust the index here.
          const int64 index = env->Read<int64>(/*index=*/kIndexStackTop) - 1;
          if (index < 0 || index >= length) {
            TC3_LOG(ERROR) << "Invalid index: " << index;
            lua_error(env->state());
            return 0;
          }
          return item_callback(index);
        }
        case LUA_TSTRING: {
          return key_callback(env->ReadString(kIndexStackTop));
        }
        default:
          TC3_LOG(ERROR) << "Unexpected access type: "
                         << lua_type(env->state(), kIndexStackTop);
          lua_error(env->state());
          return 0;
      }
    }

    template <typename ItemCallback>
    static int Dispatch(const LuaEnvironment* env, const int length,
                        const ItemCallback& item_callback) {
      switch (lua_type(env->state(), kIndexStackTop)) {
        case LUA_TNUMBER: {
          // Lua is one based, so adjust the index here.
          const int64 index = env->Read<int64>(/*index=*/kIndexStackTop) - 1;
          if (index < 0 || index >= length) {
            TC3_LOG(ERROR) << "Invalid index: " << index;
            lua_error(env->state());
            return 0;
          }
          return item_callback(index);
        }
        default:
          TC3_LOG(ERROR) << "Unexpected access type: "
                         << lua_type(env->state(), kIndexStackTop);
          lua_error(env->state());
          return 0;
      }
    }
  };

  // Calls the deconstructor from a previously pushed function.
  template <typename T>
  static int ReleaseFunction(lua_State* state) {
    static_cast<T*>(lua_touserdata(state, 1))->~T();
    return 0;
  }

  template <typename T>
  static int CallFunction(lua_State* state) {
    return (*static_cast<T*>(lua_touserdata(state, lua_upvalueindex(1))))();
  }

  // Auxiliary methods to expose (reflective) flatbuffer based data to Lua.
  void PushFlatbuffer(const reflection::Schema* schema,
                      const reflection::Object* type,
                      const flatbuffers::Table* table) const;
  int GetField(const reflection::Schema* schema, const reflection::Object* type,
               const flatbuffers::Table* table) const;

  // Reads a repeated field from lua.
  template <typename T>
  void ReadRepeatedField(const int index, RepeatedField* result) const {
    for (const auto& element : ReadVector<T>(index)) {
      result->Add(element);
    }
  }

  template <>
  void ReadRepeatedField<ReflectiveFlatbuffer>(const int index,
                                               RepeatedField* result) const {
    lua_pushnil(state_);
    while (Next(index - 1)) {
      ReadFlatbuffer(index, result->Add());
      lua_pop(state_, 1);
    }
  }

  // Pushes a repeated field to the lua stack.
  template <typename T>
  void PushRepeatedField(const flatbuffers::Vector<T>* items) const {
    PushIterator(items ? items->size() : 0, [this, items](const int64 pos) {
      Push(items->Get(pos));
      return 1;  // Num. results.
    });
  }

  void PushRepeatedFlatbufferField(
      const reflection::Schema* schema, const reflection::Object* type,
      const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>* items)
      const {
    PushIterator(items ? items->size() : 0,
                 [this, schema, type, items](const int64 pos) {
                   PushFlatbuffer(schema, type, items->Get(pos));
                   return 1;  // Num. results.
                 });
  }

  // Overloads Lua next function to use __next key on the metatable.
  // This allows us to treat lua objects and lazy objects provided by our
  // callbacks uniformly.
  int Next(int index) const {
    // Check whether the (meta)table of this object has an associated "__next"
    // entry. This means, we registered our own callback. So we explicitly call
    // that.
    if (luaL_getmetafield(state_, index, kNextKey)) {
      // Callback is now on top of the stack, so adjust relative indices by 1.
      if (index < 0) {
        index--;
      }

      // Copy the reference to the table.
      lua_pushvalue(state_, index);

      // Move the key to top to have it as second argument for the callback.
      // Copy the key to the top.
      lua_pushvalue(state_, -3);

      // Remove the copy of the key.
      lua_remove(state_, -4);

      // Call the callback with (key and table as arguments).
      lua_pcall(state_, /*nargs=*/2 /* table, key */,
                /*nresults=*/2 /* key, item */, 0);

      // Next returned nil, it's the end.
      if (lua_isnil(state_, kIndexStackTop)) {
        // Remove nil value.
        // Results will be padded to `nresults` specified above, so we need
        // to remove two elements here.
        lua_pop(state_, 2);
        return 0;
      }

      return 2;  // Num. results.
    } else if (lua_istable(state_, index)) {
      return lua_next(state_, index);
    }

    // Remove the key.
    lua_pop(state_, 1);
    return 0;
  }

  static const ClassificationResult* GetAnnotationByName(
      const std::vector<ClassificationResult>& annotations, StringPiece name) {
    // Lookup annotation by collection.
    for (const ClassificationResult& annotation : annotations) {
      if (name.Equals(annotation.collection)) {
        return &annotation;
      }
    }
    TC3_LOG(ERROR) << "No annotation with collection: " << name << " found.";
    return nullptr;
  }

  static const ActionSuggestionAnnotation* GetAnnotationByName(
      const std::vector<ActionSuggestionAnnotation>& annotations,
      StringPiece name) {
    // Lookup annotation by name.
    for (const ActionSuggestionAnnotation& annotation : annotations) {
      if (name.Equals(annotation.name)) {
        return &annotation;
      }
    }
    TC3_LOG(ERROR) << "No annotation with name: " << name << " found.";
    return nullptr;
  }

  lua_State* state_;
};  // namespace libtextclassifier3

bool Compile(StringPiece snippet, std::string* bytecode);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_LUA_UTILS_H_
