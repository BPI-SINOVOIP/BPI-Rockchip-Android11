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

#include "fields/packet_field.h"
#include "fields/size_field.h"
#include "parse_location.h"
#include "type_def.h"

class VectorField : public PacketField {
 public:
  VectorField(std::string name, int element_size, std::string size_modifier, ParseLocation loc);

  VectorField(std::string name, TypeDef* type_def, std::string size_modifier, ParseLocation loc);

  static const std::string kFieldType;

  virtual const std::string& GetFieldType() const override;

  virtual Size GetSize() const override;

  virtual Size GetBuilderSize() const override;

  virtual Size GetStructSize() const override;

  virtual std::string GetDataType() const override;

  virtual void GenExtractor(std::ostream& s, int num_leading_bits, bool for_struct) const override;

  virtual std::string GetGetterFunctionName() const override;

  virtual void GenGetter(std::ostream& s, Size start_offset, Size end_offset) const override;

  virtual std::string GetBuilderParameterType() const override;

  virtual bool BuilderParameterMustBeMoved() const override;

  virtual bool GenBuilderMember(std::ostream& s) const override;

  virtual bool HasParameterValidator() const override;

  virtual void GenParameterValidator(std::ostream& s) const override;

  virtual void GenInserter(std::ostream& s) const override;

  virtual void GenValidator(std::ostream&) const override;

  void SetSizeField(const SizeField* size_field);

  const std::string& GetSizeModifier() const;

  virtual bool IsContainerField() const override;

  virtual const PacketField* GetElementField() const override;

  const std::string name_;

  const PacketField* element_field_{nullptr};

  const Size element_size_{};

  // Size is always in bytes, unless it is a count.
  const SizeField* size_field_{nullptr};

  // Size modifier is only used when size_field_ is of type SIZE and is not used with COUNT.
  std::string size_modifier_{""};
};
