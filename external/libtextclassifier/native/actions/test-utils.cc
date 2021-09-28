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

#include "actions/test-utils.h"

namespace libtextclassifier3 {

std::string TestEntityDataSchema() {
  // Create fake entity data schema meta data.
  // Cannot use object oriented API here as that is not available for the
  // reflection schema.
  flatbuffers::FlatBufferBuilder schema_builder;
  std::vector<flatbuffers::Offset<reflection::Field>> fields = {
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("greeting"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::String),
          /*id=*/0,
          /*offset=*/4),
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("location"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::String),
          /*id=*/1,
          /*offset=*/6),
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("person"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::String),
          /*id=*/2,
          /*offset=*/8)};
  std::vector<flatbuffers::Offset<reflection::Enum>> enums;
  std::vector<flatbuffers::Offset<reflection::Object>> objects = {
      reflection::CreateObject(
          schema_builder,
          /*name=*/schema_builder.CreateString("EntityData"),
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

void SetTestEntityDataSchema(ActionsModelT* test_model) {
  const std::string serialized_schema = TestEntityDataSchema();

  test_model->actions_entity_data_schema.assign(
      serialized_schema.data(),
      serialized_schema.data() + serialized_schema.size());
}

}  // namespace libtextclassifier3
