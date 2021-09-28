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

#ifndef LIBTEXTCLASSIFIER_UTILS_VARIANT_H_
#define LIBTEXTCLASSIFIER_UTILS_VARIANT_H_

#include <map>
#include <string>
#include <vector>

#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Represents a type-tagged union of different basic types.
class Variant {
 public:
  enum Type {
    TYPE_EMPTY = 0,
    TYPE_INT8_VALUE = 1,
    TYPE_UINT8_VALUE = 2,
    TYPE_INT_VALUE = 3,
    TYPE_UINT_VALUE = 4,
    TYPE_INT64_VALUE = 5,
    TYPE_UINT64_VALUE = 6,
    TYPE_FLOAT_VALUE = 7,
    TYPE_DOUBLE_VALUE = 8,
    TYPE_BOOL_VALUE = 9,
    TYPE_STRING_VALUE = 10,
    TYPE_STRING_VECTOR_VALUE = 11,
    TYPE_FLOAT_VECTOR_VALUE = 12,
    TYPE_INT_VECTOR_VALUE = 13,
    TYPE_STRING_VARIANT_MAP_VALUE = 14,
  };

  Variant() : type_(TYPE_EMPTY) {}
  explicit Variant(const int8_t value)
      : type_(TYPE_INT8_VALUE), int8_value_(value) {}
  explicit Variant(const uint8_t value)
      : type_(TYPE_UINT8_VALUE), uint8_value_(value) {}
  explicit Variant(const int value)
      : type_(TYPE_INT_VALUE), int_value_(value) {}
  explicit Variant(const uint value)
      : type_(TYPE_UINT_VALUE), uint_value_(value) {}
  explicit Variant(const int64 value)
      : type_(TYPE_INT64_VALUE), long_value_(value) {}
  explicit Variant(const uint64 value)
      : type_(TYPE_UINT64_VALUE), ulong_value_(value) {}
  explicit Variant(const float value)
      : type_(TYPE_FLOAT_VALUE), float_value_(value) {}
  explicit Variant(const double value)
      : type_(TYPE_DOUBLE_VALUE), double_value_(value) {}
  explicit Variant(const StringPiece value)
      : type_(TYPE_STRING_VALUE), string_value_(value.ToString()) {}
  explicit Variant(const std::string value)
      : type_(TYPE_STRING_VALUE), string_value_(value) {}
  explicit Variant(const char* value)
      : type_(TYPE_STRING_VALUE), string_value_(value) {}
  explicit Variant(const bool value)
      : type_(TYPE_BOOL_VALUE), bool_value_(value) {}
  explicit Variant(const std::vector<std::string>& value)
      : type_(TYPE_STRING_VECTOR_VALUE), string_vector_value_(value) {}
  explicit Variant(const std::vector<float>& value)
      : type_(TYPE_FLOAT_VECTOR_VALUE), float_vector_value_(value) {}
  explicit Variant(const std::vector<int>& value)
      : type_(TYPE_INT_VECTOR_VALUE), int_vector_value_(value) {}
  explicit Variant(const std::map<std::string, Variant>& value)
      : type_(TYPE_STRING_VARIANT_MAP_VALUE),
        string_variant_map_value_(value) {}

  Variant& operator=(const Variant&) = default;

  template <class T>
  struct dependent_false : std::false_type {};

  template <typename T>
  T Value() const {
    static_assert(dependent_false<T>::value, "Not supported.");
  }

  template <>
  int8 Value() const {
    TC3_CHECK(Has<int8>());
    return int8_value_;
  }

  template <>
  uint8 Value() const {
    TC3_CHECK(Has<uint8>());
    return uint8_value_;
  }

  template <>
  int Value() const {
    TC3_CHECK(Has<int>());
    return int_value_;
  }

  template <>
  uint Value() const {
    TC3_CHECK(Has<uint>());
    return uint_value_;
  }

  template <>
  int64 Value() const {
    TC3_CHECK(Has<int64>());
    return long_value_;
  }

  template <>
  uint64 Value() const {
    TC3_CHECK(Has<uint64>());
    return ulong_value_;
  }

  template <>
  float Value() const {
    TC3_CHECK(Has<float>());
    return float_value_;
  }

  template <>
  double Value() const {
    TC3_CHECK(Has<double>());
    return double_value_;
  }

  template <>
  bool Value() const {
    TC3_CHECK(Has<bool>());
    return bool_value_;
  }

  template <typename T>
  const T& ConstRefValue() const;

  template <>
  const std::string& ConstRefValue() const {
    TC3_CHECK(Has<std::string>());
    return string_value_;
  }

  template <>
  const std::vector<std::string>& ConstRefValue() const {
    TC3_CHECK(Has<std::vector<std::string>>());
    return string_vector_value_;
  }

  template <>
  const std::vector<float>& ConstRefValue() const {
    TC3_CHECK(Has<std::vector<float>>());
    return float_vector_value_;
  }

  template <>
  const std::vector<int>& ConstRefValue() const {
    TC3_CHECK(Has<std::vector<int>>());
    return int_vector_value_;
  }

  template <>
  const std::map<std::string, Variant>& ConstRefValue() const {
    TC3_CHECK((Has<std::map<std::string, Variant>>()));
    return string_variant_map_value_;
  }

  template <typename T>
  bool Has() const;

  template <>
  bool Has<int8>() const {
    return type_ == TYPE_INT8_VALUE;
  }

  template <>
  bool Has<uint8>() const {
    return type_ == TYPE_UINT8_VALUE;
  }

  template <>
  bool Has<int>() const {
    return type_ == TYPE_INT_VALUE;
  }

  template <>
  bool Has<uint>() const {
    return type_ == TYPE_UINT_VALUE;
  }

  template <>
  bool Has<int64>() const {
    return type_ == TYPE_INT64_VALUE;
  }

  template <>
  bool Has<uint64>() const {
    return type_ == TYPE_UINT64_VALUE;
  }

  template <>
  bool Has<float>() const {
    return type_ == TYPE_FLOAT_VALUE;
  }

  template <>
  bool Has<double>() const {
    return type_ == TYPE_DOUBLE_VALUE;
  }

  template <>
  bool Has<bool>() const {
    return type_ == TYPE_BOOL_VALUE;
  }

  template <>
  bool Has<std::string>() const {
    return type_ == TYPE_STRING_VALUE;
  }

  template <>
  bool Has<std::vector<std::string>>() const {
    return type_ == TYPE_STRING_VECTOR_VALUE;
  }

  template <>
  bool Has<std::vector<float>>() const {
    return type_ == TYPE_FLOAT_VECTOR_VALUE;
  }

  template <>
  bool Has<std::vector<int>>() const {
    return type_ == TYPE_INT_VECTOR_VALUE;
  }

  template <>
  bool Has<std::map<std::string, Variant>>() const {
    return type_ == TYPE_STRING_VARIANT_MAP_VALUE;
  }

  // Converts the value of this variant to its string representation, regardless
  // of the type of the actual value.
  std::string ToString() const;

  Type GetType() const { return type_; }

  bool HasValue() const { return type_ != TYPE_EMPTY; }

 private:
  Type type_;
  union {
    int8_t int8_value_;
    uint8_t uint8_value_;
    int int_value_;
    uint uint_value_;
    int64 long_value_;
    uint64 ulong_value_;
    float float_value_;
    double double_value_;
    bool bool_value_;
  };
  std::string string_value_;
  std::vector<std::string> string_vector_value_;
  std::vector<float> float_vector_value_;
  std::vector<int> int_vector_value_;
  std::map<std::string, Variant> string_variant_map_value_;
};

// Pretty-printing function for Variant.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Variant& value);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_VARIANT_H_
