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

#include <string>

#include "utils/flatbuffers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::FloatEq;

std::string TestFlatbufferSchema() {
  // Creates a test schema for flatbuffer passing tests.
  // Cannot use the object oriented API here as that is not available for the
  // reflection schema.
  flatbuffers::FlatBufferBuilder schema_builder;
  std::vector<flatbuffers::Offset<reflection::Field>> fields = {
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("float_field"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::Float),
          /*id=*/0,
          /*offset=*/4),
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("nested_field"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::Obj,
                                 /*element=*/reflection::None,
                                 /*index=*/0 /* self */),
          /*id=*/1,
          /*offset=*/6),
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("repeated_nested_field"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::Vector,
                                 /*element=*/reflection::Obj,
                                 /*index=*/0 /* self */),
          /*id=*/2,
          /*offset=*/8),
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("repeated_string_field"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::Vector,
                                 /*element=*/reflection::String),
          /*id=*/3,
          /*offset=*/10),
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("string_field"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::String),
          /*id=*/4,
          /*offset=*/12)};

  std::vector<flatbuffers::Offset<reflection::Enum>> enums;
  std::vector<flatbuffers::Offset<reflection::Object>> objects = {
      reflection::CreateObject(
          schema_builder,
          /*name=*/schema_builder.CreateString("TestData"),
          /*fields=*/
          schema_builder.CreateVectorOfSortedTables(&fields))};
  schema_builder.Finish(reflection::CreateSchema(
      schema_builder, schema_builder.CreateVectorOfSortedTables(&objects),
      schema_builder.CreateVectorOfSortedTables(&enums),
      /*(unused) file_ident=*/0,
      /*(unused) file_ext=*/0,
      /*root_table*/ objects[0]));
  return std::string(
      reinterpret_cast<const char*>(schema_builder.GetBufferPointer()),
      schema_builder.GetSize());
}

class LuaUtilsTest : public testing::Test, protected LuaEnvironment {
 protected:
  LuaUtilsTest()
      : serialized_flatbuffer_schema_(TestFlatbufferSchema()),
        schema_(flatbuffers::GetRoot<reflection::Schema>(
            serialized_flatbuffer_schema_.data())),
        flatbuffer_builder_(schema_) {
    EXPECT_THAT(RunProtected([this] {
                  LoadDefaultLibraries();
                  return LUA_OK;
                }),
                Eq(LUA_OK));
  }

  void RunScript(StringPiece script) {
    EXPECT_THAT(luaL_loadbuffer(state_, script.data(), script.size(),
                                /*name=*/nullptr),
                Eq(LUA_OK));
    EXPECT_THAT(
        lua_pcall(state_, /*nargs=*/0, /*num_results=*/1, /*errfunc=*/0),
        Eq(LUA_OK));
  }

  const std::string serialized_flatbuffer_schema_;
  const reflection::Schema* schema_;
  ReflectiveFlatbufferBuilder flatbuffer_builder_;
};

TEST_F(LuaUtilsTest, HandlesVectors) {
  {
    PushVector(std::vector<int64>{1, 2, 3, 4, 5});
    EXPECT_THAT(ReadVector<int64>(), ElementsAre(1, 2, 3, 4, 5));
  }
  {
    PushVector(std::vector<std::string>{"hello", "there"});
    EXPECT_THAT(ReadVector<std::string>(), ElementsAre("hello", "there"));
  }
  {
    PushVector(std::vector<bool>{true, true, false});
    EXPECT_THAT(ReadVector<bool>(), ElementsAre(true, true, false));
  }
}

TEST_F(LuaUtilsTest, HandlesVectorIterators) {
  {
    const std::vector<int64> elements = {1, 2, 3, 4, 5};
    PushVectorIterator(&elements);
    EXPECT_THAT(ReadVector<int64>(), ElementsAre(1, 2, 3, 4, 5));
  }
  {
    const std::vector<std::string> elements = {"hello", "there"};
    PushVectorIterator(&elements);
    EXPECT_THAT(ReadVector<std::string>(), ElementsAre("hello", "there"));
  }
  {
    const std::vector<bool> elements = {true, true, false};
    PushVectorIterator(&elements);
    EXPECT_THAT(ReadVector<bool>(), ElementsAre(true, true, false));
  }
}

TEST_F(LuaUtilsTest, ReadsFlatbufferResults) {
  // Setup.
  RunScript(R"lua(
    return {
        float_field = 42.1,
        string_field = "hello there",

        -- Nested field.
        nested_field = {
          float_field = 64,
          string_field = "hello nested",
        },

        -- Repeated fields.
        repeated_string_field = { "a", "bold", "one" },
        repeated_nested_field = {
          { string_field = "a" },
          { string_field = "b" },
          { repeated_string_field = { "nested", "nested2" } },
        },
    }
  )lua");

  // Read the flatbuffer.
  std::unique_ptr<ReflectiveFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  ReadFlatbuffer(/*index=*/-1, buffer.get());
  const std::string serialized_buffer = buffer->Serialize();

  // Check fields. As we do not have flatbuffer compiled generated code for the
  // ad hoc generated test schema, we have to read by manually using field
  // offsets.
  const flatbuffers::Table* flatbuffer_data =
      flatbuffers::GetRoot<flatbuffers::Table>(serialized_buffer.data());
  EXPECT_THAT(flatbuffer_data->GetField<float>(/*field=*/4, /*defaultval=*/0),
              FloatEq(42.1));
  EXPECT_THAT(
      flatbuffer_data->GetPointer<const flatbuffers::String*>(/*field=*/12)
          ->str(),
      "hello there");

  // Read the nested field.
  const flatbuffers::Table* nested_field =
      flatbuffer_data->GetPointer<const flatbuffers::Table*>(/*field=*/6);
  EXPECT_THAT(nested_field->GetField<float>(/*field=*/4, /*defaultval=*/0),
              FloatEq(64));
  EXPECT_THAT(
      nested_field->GetPointer<const flatbuffers::String*>(/*field=*/12)->str(),
      "hello nested");

  // Read the repeated string field.
  auto repeated_strings = flatbuffer_data->GetPointer<
      flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(
      /*field=*/10);
  EXPECT_THAT(repeated_strings->size(), Eq(3));
  EXPECT_THAT(repeated_strings->GetAsString(0)->str(), Eq("a"));
  EXPECT_THAT(repeated_strings->GetAsString(1)->str(), Eq("bold"));
  EXPECT_THAT(repeated_strings->GetAsString(2)->str(), Eq("one"));

  // Read the repeated nested field.
  auto repeated_nested_fields = flatbuffer_data->GetPointer<
      flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>*>(
      /*field=*/8);
  EXPECT_THAT(repeated_nested_fields->size(), Eq(3));
  EXPECT_THAT(repeated_nested_fields->Get(0)
                  ->GetPointer<const flatbuffers::String*>(/*field=*/12)
                  ->str(),
              "a");
  EXPECT_THAT(repeated_nested_fields->Get(1)
                  ->GetPointer<const flatbuffers::String*>(/*field=*/12)
                  ->str(),
              "b");
}

TEST_F(LuaUtilsTest, HandlesSimpleFlatbufferFields) {
  // Create test flatbuffer.
  std::unique_ptr<ReflectiveFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  buffer->Set("float_field", 42.f);
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_, flatbuffers::GetRoot<flatbuffers::Table>(
                              serialized_buffer.data()));
  lua_setglobal(state_, "arg");

  // Setup.
  RunScript(R"lua(
    return arg.float_field
  )lua");

  EXPECT_THAT(Read<float>(), FloatEq(42));
}

TEST_F(LuaUtilsTest, HandlesRepeatedFlatbufferFields) {
  // Create test flatbuffer.
  std::unique_ptr<ReflectiveFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  RepeatedField* repeated_field = buffer->Repeated("repeated_string_field");
  repeated_field->Add("this");
  repeated_field->Add("is");
  repeated_field->Add("a");
  repeated_field->Add("test");
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_, flatbuffers::GetRoot<flatbuffers::Table>(
                              serialized_buffer.data()));
  lua_setglobal(state_, "arg");

  // Return flatbuffer repeated field as vector.
  RunScript(R"lua(
    return arg.repeated_string_field
  )lua");

  EXPECT_THAT(ReadVector<std::string>(),
              ElementsAre("this", "is", "a", "test"));
}

TEST_F(LuaUtilsTest, HandlesRepeatedNestedFlatbufferFields) {
  // Create test flatbuffer.
  std::unique_ptr<ReflectiveFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  RepeatedField* repeated_field = buffer->Repeated("repeated_nested_field");
  repeated_field->Add()->Set("string_field", "hello");
  repeated_field->Add()->Set("string_field", "my");
  ReflectiveFlatbuffer* nested = repeated_field->Add();
  nested->Set("string_field", "old");
  RepeatedField* nested_repeated = nested->Repeated("repeated_string_field");
  nested_repeated->Add("friend");
  nested_repeated->Add("how");
  nested_repeated->Add("are");
  repeated_field->Add()->Set("string_field", "you?");
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_, flatbuffers::GetRoot<flatbuffers::Table>(
                              serialized_buffer.data()));
  lua_setglobal(state_, "arg");

  RunScript(R"lua(
    result = {}
    for _, nested in pairs(arg.repeated_nested_field) do
      result[#result + 1] = nested.string_field
      for _, nested_string in pairs(nested.repeated_string_field) do
        result[#result + 1] = nested_string
      end
    end
    return result
  )lua");

  EXPECT_THAT(
      ReadVector<std::string>(),
      ElementsAre("hello", "my", "old", "friend", "how", "are", "you?"));
}

TEST_F(LuaUtilsTest, CorrectlyReadsTwoFlatbuffersSimultaneously) {
  // The first flatbuffer.
  std::unique_ptr<ReflectiveFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  buffer->Set("string_field", "first");
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_, flatbuffers::GetRoot<flatbuffers::Table>(
                              serialized_buffer.data()));
  lua_setglobal(state_, "arg");
  // The second flatbuffer.
  std::unique_ptr<ReflectiveFlatbuffer> buffer2 = flatbuffer_builder_.NewRoot();
  buffer2->Set("string_field", "second");
  const std::string serialized_buffer2 = buffer2->Serialize();
  PushFlatbuffer(schema_, flatbuffers::GetRoot<flatbuffers::Table>(
                              serialized_buffer2.data()));
  lua_setglobal(state_, "arg2");

  RunScript(R"lua(
    return {arg.string_field, arg2.string_field}
  )lua");

  EXPECT_THAT(ReadVector<std::string>(), ElementsAre("first", "second"));
}

}  // namespace
}  // namespace libtextclassifier3
