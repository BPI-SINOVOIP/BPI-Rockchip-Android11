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
#include <variant>

#include "enum_def.h"
#include "field_list.h"
#include "fields/packet_field.h"
#include "parent_def.h"

class PacketDef : public ParentDef {
 public:
  PacketDef(std::string name, FieldList fields);
  PacketDef(std::string name, FieldList fields, PacketDef* parent);

  PacketField* GetNewField(const std::string& name, ParseLocation loc) const;

  void GenParserDefinition(std::ostream& s) const;

  void GenParserDefinitionPybind11(std::ostream& s) const;

  void GenParserFieldGetter(std::ostream& s, const PacketField* field) const;

  void GenValidator(std::ostream& s) const;

  TypeDef::Type GetDefinitionType() const;

  void GenBuilderDefinition(std::ostream& s) const;

  void GenBuilderDefinitionPybind11(std::ostream& s) const;

  void GenTestDefine(std::ostream& s) const;

  void GenFuzzTestDefine(std::ostream& s) const;

  FieldList GetParametersToValidate() const;

  void GenBuilderCreate(std::ostream& s) const;

  void GenBuilderCreatePybind11(std::ostream& s) const;

  void GenBuilderParameterChecker(std::ostream& s) const;

  void GenBuilderConstructor(std::ostream& s) const;
};
