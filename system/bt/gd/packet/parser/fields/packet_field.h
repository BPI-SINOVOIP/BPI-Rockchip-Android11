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

#include <iostream>
#include <string>

#include "logging.h"
#include "parse_location.h"
#include "size.h"

// The base field that every packet needs to inherit from.
class PacketField : public Loggable {
 public:
  virtual ~PacketField() = default;

  PacketField(std::string name, ParseLocation loc);

  // Get the type for this field.
  virtual const std::string& GetFieldType() const = 0;

  // Returns the size of the field in bits.
  virtual Size GetSize() const = 0;

  // Returns the size of the field in bits given the information in the builder.
  // For most field types, this will be the same as GetSize();
  virtual Size GetBuilderSize() const;

  // Returns the size of the field in bits given the information in the parsed struct.
  // For most field types, this will be the same as GetSize();
  virtual Size GetStructSize() const;

  // Get the type of the field to be used in the member variables.
  virtual std::string GetDataType() const = 0;

  // Given an iterator {name}_it, extract the type.
  virtual void GenExtractor(std::ostream& s, int num_leading_bits, bool for_struct) const = 0;

  // Calculate field_begin and field_end using the given offsets and size, return the number of leading bits
  virtual int GenBounds(std::ostream& s, Size start_offset, Size end_offset, Size size) const;

  // Get the name of the getter function, return empty string if there is a getter function
  virtual std::string GetGetterFunctionName() const = 0;

  // Get parser getter definition. Start_offset points to the first bit of the
  // field. end_offset is the first bit after the field. If an offset is empty
  // that means that there was a field with an unknown size when trying to
  // calculate the offset.
  virtual void GenGetter(std::ostream& s, Size start_offset, Size end_offset) const = 0;

  // Get the type of parameter used in Create(), return empty string if a parameter type was NOT generated
  virtual std::string GetBuilderParameterType() const = 0;

  // Generate the parameter for Create(), return true if a parameter was added.
  virtual bool GenBuilderParameter(std::ostream& s) const;

  // Return true if the Builder parameter has to be moved.
  virtual bool BuilderParameterMustBeMoved() const;

  // Generate the actual storage for the parameter, return true if it was added.
  virtual bool GenBuilderMember(std::ostream& s) const;

  // Helper for reflection tests.
  virtual void GenBuilderParameterFromView(std::ostream& s) const;

  // Returns whether or not the field must be validated.
  virtual bool HasParameterValidator() const = 0;

  // Fail if the value doesn't fit in the field.
  virtual void GenParameterValidator(std::ostream& s) const = 0;

  // Generate the inserter for pushing the data in the builder.
  virtual void GenInserter(std::ostream& s) const = 0;

  // Generate the validator for a field for the IsValid() function.
  //
  // The way this function works is by assuming that there is an iterator |it|
  // that was defined earlier. The implementer of the function will then move
  // it forward based on the dynamic size of the field and then check to see if
  // its past the end of the packet.
  // It should be unused for fixed size fields unless special consideration is
  // needed. This is because all fixed size fields are tallied together with
  // GetSize() and used as an initial offset. One special consideration is for
  // enums where instead of checking if they can be read, they are checked to
  // see if they contain the correct value.
  virtual void GenValidator(std::ostream& s) const = 0;

  // Some fields are containers of other fields, e.g. array, vector, etc.
  // Assume STL containers that support swap()
  virtual bool IsContainerField() const;

  // Get field of nested elements if this is a container field, nullptr if none
  virtual const PacketField* GetElementField() const;

  std::string GetDebugName() const override;

  ParseLocation GetLocation() const override;

  virtual std::string GetName() const;

 private:
  ParseLocation loc_;
  std::string name_;
};
