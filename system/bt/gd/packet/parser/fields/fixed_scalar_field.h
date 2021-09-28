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

#include <variant>

#include "enum_def.h"
#include "fields/fixed_field.h"
#include "fields/packet_field.h"
#include "parse_location.h"

class FixedScalarField : public FixedField {
 public:
  FixedScalarField(int size, int64_t value, ParseLocation loc);

  static const std::string kFieldType;

  virtual const std::string& GetFieldType() const override;

  virtual std::string GetDataType() const override;

  static const std::string field_type;

 private:
  void GenValue(std::ostream& s) const;

  const int64_t value_;
};
