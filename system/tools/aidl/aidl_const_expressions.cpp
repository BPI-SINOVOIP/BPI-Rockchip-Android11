/*
 * Copyright (C) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aidl_language.h"
#include "aidl_typenames.h"
#include "logging.h"

#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include <memory>

#include <android-base/parsedouble.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

using android::base::ConsumeSuffix;
using android::base::Join;
using std::string;
using std::unique_ptr;
using std::vector;

#define SHOULD_NOT_REACH() CHECK(false) << LOG(FATAL) << ": should not reach here: "
#define OPEQ(__y__) (string(op_) == string(__y__))
#define COMPUTE_UNARY(__op__) \
  if (op == string(#__op__)) return __op__ val;
#define COMPUTE_BINARY(__op__) \
  if (op == string(#__op__)) return lval __op__ rval;
#define OP_IS_BIN_ARITHMETIC (OPEQ("+") || OPEQ("-") || OPEQ("*") || OPEQ("/") || OPEQ("%"))
#define OP_IS_BIN_BITFLIP (OPEQ("|") || OPEQ("^") || OPEQ("&"))
#define OP_IS_BIN_COMP \
  (OPEQ("<") || OPEQ(">") || OPEQ("<=") || OPEQ(">=") || OPEQ("==") || OPEQ("!="))
#define OP_IS_BIN_SHIFT (OPEQ(">>") || OPEQ("<<"))
#define OP_IS_BIN_LOGICAL (OPEQ("||") || OPEQ("&&"))

// NOLINT to suppress missing parentheses warnings about __def__.
#define SWITCH_KIND(__cond__, __action__, __def__) \
  switch (__cond__) {                              \
    case Type::BOOLEAN:                            \
      __action__(bool);                            \
    case Type::INT8:                               \
      __action__(int8_t);                          \
    case Type::INT32:                              \
      __action__(int32_t);                         \
    case Type::INT64:                              \
      __action__(int64_t);                         \
    default:                                       \
      __def__; /* NOLINT */                        \
  }

template <class T>
T handleUnary(const string& op, T val) {
  COMPUTE_UNARY(+)
  COMPUTE_UNARY(-)
  COMPUTE_UNARY(!)
  COMPUTE_UNARY(~)
  // Should not reach here.
  SHOULD_NOT_REACH() << "Could not handleUnary for " << op << " " << val;
  return static_cast<T>(0xdeadbeef);
}

template <class T>
T handleBinaryCommon(T lval, const string& op, T rval) {
  COMPUTE_BINARY(+)
  COMPUTE_BINARY(-)
  COMPUTE_BINARY(*)
  COMPUTE_BINARY(/)
  COMPUTE_BINARY(%)
  COMPUTE_BINARY(|)
  COMPUTE_BINARY(^)
  COMPUTE_BINARY(&)
  // comparison operators: return 0 or 1 by nature.
  COMPUTE_BINARY(==)
  COMPUTE_BINARY(!=)
  COMPUTE_BINARY(<)
  COMPUTE_BINARY(>)
  COMPUTE_BINARY(<=)
  COMPUTE_BINARY(>=)
  // Should not reach here.
  SHOULD_NOT_REACH() << "Could not handleBinaryCommon for " << lval << " " << op << " " << rval;
  return static_cast<T>(0xdeadbeef);
}

template <class T>
T handleShift(T lval, const string& op, int64_t rval) {
  // just cast rval to int64_t and it should fit.
  COMPUTE_BINARY(>>)
  COMPUTE_BINARY(<<)
  // Should not reach here.
  SHOULD_NOT_REACH() << "Could not handleShift for " << lval << " " << op << " " << rval;
  return static_cast<T>(0xdeadbeef);
}

bool handleLogical(bool lval, const string& op, bool rval) {
  COMPUTE_BINARY(||);
  COMPUTE_BINARY(&&);
  // Should not reach here.
  SHOULD_NOT_REACH() << "Could not handleLogical for " << lval << " " << op << " " << rval;
  return false;
}

static bool isValidLiteralChar(char c) {
  return !(c <= 0x1f ||  // control characters are < 0x20
           c >= 0x7f ||  // DEL is 0x7f
           c == '\\');   // Disallow backslashes for future proofing.
}

bool AidlUnaryConstExpression::IsCompatibleType(Type type, const string& op) {
  // Verify the unary type here
  switch (type) {
    case Type::BOOLEAN:  // fall-through
    case Type::INT8:     // fall-through
    case Type::INT32:    // fall-through
    case Type::INT64:
      return true;
    case Type::FLOATING:
      return (op == "+" || op == "-");
    default:
      return false;
  }
}

bool AidlBinaryConstExpression::AreCompatibleTypes(Type t1, Type t2) {
  switch (t1) {
    case Type::STRING:
      if (t2 == Type::STRING) {
        return true;
      }
      break;
    case Type::BOOLEAN:  // fall-through
    case Type::INT8:     // fall-through
    case Type::INT32:    // fall-through
    case Type::INT64:
      switch (t2) {
        case Type::BOOLEAN:  // fall-through
        case Type::INT8:     // fall-through
        case Type::INT32:    // fall-through
        case Type::INT64:
          return true;
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  return false;
}

// Returns the promoted kind for both operands
AidlConstantValue::Type AidlBinaryConstExpression::UsualArithmeticConversion(Type left,
                                                                             Type right) {
  // These are handled as special cases
  CHECK(left != Type::STRING && right != Type::STRING);
  CHECK(left != Type::FLOATING && right != Type::FLOATING);

  // Kinds in concern: bool, (u)int[8|32|64]
  if (left == right) return left;  // easy case
  if (left == Type::BOOLEAN) return right;
  if (right == Type::BOOLEAN) return left;

  return left < right ? right : left;
}

// Returns the promoted integral type where INT32 is the smallest type
AidlConstantValue::Type AidlBinaryConstExpression::IntegralPromotion(Type in) {
  return (Type::INT32 < in) ? in : Type::INT32;
}

template <typename T>
T AidlConstantValue::cast() const {
  CHECK(is_evaluated_ == true);

#define CASE_CAST_T(__type__) return static_cast<T>(static_cast<__type__>(final_value_));

  SWITCH_KIND(final_type_, CASE_CAST_T, SHOULD_NOT_REACH(); return 0;);
}

AidlConstantValue* AidlConstantValue::Boolean(const AidlLocation& location, bool value) {
  return new AidlConstantValue(location, Type::BOOLEAN, value ? "true" : "false");
}

AidlConstantValue* AidlConstantValue::Character(const AidlLocation& location, char value) {
  const std::string explicit_value = string("'") + value + "'";
  if (!isValidLiteralChar(value)) {
    AIDL_ERROR(location) << "Invalid character literal " << value;
    return new AidlConstantValue(location, Type::ERROR, explicit_value);
  }
  return new AidlConstantValue(location, Type::CHARACTER, explicit_value);
}

AidlConstantValue* AidlConstantValue::Floating(const AidlLocation& location,
                                               const std::string& value) {
  return new AidlConstantValue(location, Type::FLOATING, value);
}

bool AidlConstantValue::IsHex(const string& value) {
  if (value.length() > (sizeof("0x") - 1)) {
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
      return true;
    }
  }
  return false;
}

bool AidlConstantValue::ParseIntegral(const string& value, int64_t* parsed_value,
                                      Type* parsed_type) {
  bool isLong = false;

  if (parsed_value == nullptr || parsed_type == nullptr) {
    return false;
  }

  if (IsHex(value)) {
    bool parseOK = false;
    uint32_t rawValue32;

    // AIDL considers 'const int foo = 0xffffffff' as -1, but if we want to
    // handle that when computing constant expressions, then we need to
    // represent 0xffffffff as a uint32_t. However, AIDL only has signed types;
    // so we parse as an unsigned int when possible and then cast to a signed
    // int. One example of this is in ICameraService.aidl where a constant int
    // is used for bit manipulations which ideally should be handled with an
    // unsigned int.
    parseOK = android::base::ParseUint<uint32_t>(value, &rawValue32);
    if (parseOK) {
      *parsed_value = static_cast<int32_t>(rawValue32);
      *parsed_type = Type::INT32;
    } else {
      parseOK = android::base::ParseInt<int64_t>(value, parsed_value);
      if (!parseOK) {
        *parsed_type = Type::ERROR;
        return false;
      }

      *parsed_type = Type::INT64;
    }
    return true;
  }

  if (value[value.size() - 1] == 'l' || value[value.size() - 1] == 'L') {
    isLong = true;
    *parsed_type = Type::INT64;
  }

  string value_substr = value.substr(0, isLong ? value.size() - 1 : value.size());
  bool parseOK = android::base::ParseInt<int64_t>(value_substr, parsed_value);
  if (!parseOK) {
    *parsed_type = Type::ERROR;
    return false;
  }

  if (!isLong) {
    // guess literal type.
    if (*parsed_value <= INT8_MAX && *parsed_value >= INT8_MIN) {
      *parsed_type = Type::INT8;
    } else if (*parsed_value <= INT32_MAX && *parsed_value >= INT32_MIN) {
      *parsed_type = Type::INT32;
    } else {
      *parsed_type = Type::INT64;
    }
  }
  return true;
}

AidlConstantValue* AidlConstantValue::Integral(const AidlLocation& location, const string& value) {
  CHECK(!value.empty());

  Type parsed_type;
  int64_t parsed_value = 0;
  bool success = ParseIntegral(value, &parsed_value, &parsed_type);
  if (!success) {
    return nullptr;
  }

  return new AidlConstantValue(location, parsed_type, parsed_value, value);
}

AidlConstantValue* AidlConstantValue::Array(
    const AidlLocation& location, std::unique_ptr<vector<unique_ptr<AidlConstantValue>>> values) {
  return new AidlConstantValue(location, Type::ARRAY, std::move(values));
}

AidlConstantValue* AidlConstantValue::String(const AidlLocation& location, const string& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isValidLiteralChar(value[i])) {
      AIDL_ERROR(location) << "Found invalid character at index " << i << " in string constant '"
                           << value << "'";
      return new AidlConstantValue(location, Type::ERROR, value);
    }
  }

  return new AidlConstantValue(location, Type::STRING, value);
}

AidlConstantValue* AidlConstantValue::ShallowIntegralCopy(const AidlConstantValue& other) {
  // TODO(b/141313220) Perform a full copy instead of parsing+unparsing
  AidlTypeSpecifier type = AidlTypeSpecifier(AIDL_LOCATION_HERE, "long", false, nullptr, "");
  // TODO(b/142722772) CheckValid() should be called before ValueString()
  if (!other.CheckValid() || !other.evaluate(type)) {
    AIDL_ERROR(other) << "Failed to parse expression as integer: " << other.value_;
    return nullptr;
  }
  const std::string& value = other.ValueString(type, AidlConstantValueDecorator);
  if (value.empty()) {
    return nullptr;  // error already logged
  }

  AidlConstantValue* result = Integral(AIDL_LOCATION_HERE, value);
  if (result == nullptr) {
    AIDL_FATAL(other) << "Unable to perform ShallowIntegralCopy.";
  }
  return result;
}

string AidlConstantValue::ValueString(const AidlTypeSpecifier& type,
                                      const ConstantValueDecorator& decorator) const {
  if (type.IsGeneric()) {
    AIDL_ERROR(type) << "Generic type cannot be specified with a constant literal.";
    return "";
  }
  if (!is_evaluated_) {
    // TODO(b/142722772) CheckValid() should be called before ValueString()
    bool success = CheckValid();
    success &= evaluate(type);
    if (!success) {
      // the detailed error message shall be printed in evaluate
      return "";
    }
  }
  if (!is_valid_) {
    AIDL_ERROR(this) << "Invalid constant value: " + value_;
    return "";
  }
  const string& type_string = type.GetName();
  int err = 0;

  switch (final_type_) {
    case Type::CHARACTER:
      if (type_string == "char") {
        return decorator(type, final_string_value_);
      }
      err = -1;
      break;
    case Type::STRING:
      if (type_string == "String") {
        return decorator(type, final_string_value_);
      }
      err = -1;
      break;
    case Type::BOOLEAN:  // fall-through
    case Type::INT8:     // fall-through
    case Type::INT32:    // fall-through
    case Type::INT64:
      if (type_string == "byte") {
        if (final_value_ > INT8_MAX || final_value_ < INT8_MIN) {
          err = -1;
          break;
        }
        return decorator(type, std::to_string(static_cast<int8_t>(final_value_)));
      } else if (type_string == "int") {
        if (final_value_ > INT32_MAX || final_value_ < INT32_MIN) {
          err = -1;
          break;
        }
        return decorator(type, std::to_string(static_cast<int32_t>(final_value_)));
      } else if (type_string == "long") {
        return decorator(type, std::to_string(final_value_));
      } else if (type_string == "boolean") {
        return decorator(type, final_value_ ? "true" : "false");
      }
      err = -1;
      break;
    case Type::ARRAY: {
      if (!type.IsArray()) {
        err = -1;
        break;
      }
      vector<string> value_strings;
      value_strings.reserve(values_.size());
      bool success = true;

      for (const auto& value : values_) {
        const AidlTypeSpecifier& array_base = type.ArrayBase();
        const string value_string = value->ValueString(array_base, decorator);
        if (value_string.empty()) {
          success = false;
          break;
        }
        value_strings.push_back(value_string);
      }
      if (!success) {
        err = -1;
        break;
      }

      return decorator(type, "{" + Join(value_strings, ", ") + "}");
    }
    case Type::FLOATING: {
      std::string_view raw_view(value_.c_str());
      bool is_float_literal = ConsumeSuffix(&raw_view, "f");
      std::string stripped_value = std::string(raw_view);

      if (type_string == "double") {
        double parsed_value;
        if (!android::base::ParseDouble(stripped_value, &parsed_value)) {
          AIDL_ERROR(this) << "Could not parse " << value_;
          err = -1;
          break;
        }
        return decorator(type, std::to_string(parsed_value));
      }
      if (is_float_literal && type_string == "float") {
        float parsed_value;
        if (!android::base::ParseFloat(stripped_value, &parsed_value)) {
          AIDL_ERROR(this) << "Could not parse " << value_;
          err = -1;
          break;
        }
        return decorator(type, std::to_string(parsed_value) + "f");
      }
      err = -1;
      break;
    }
    default:
      err = -1;
      break;
  }

  CHECK(err != 0);
  AIDL_ERROR(this) << "Invalid type specifier for " << ToString(final_type_) << ": " << type_string;
  return "";
}

bool AidlConstantValue::CheckValid() const {
  // Nothing needs to be checked here. The constant value will be validated in
  // the constructor or in the evaluate() function.
  if (is_evaluated_) return is_valid_;

  switch (type_) {
    case Type::BOOLEAN:    // fall-through
    case Type::INT8:       // fall-through
    case Type::INT32:      // fall-through
    case Type::INT64:      // fall-through
    case Type::ARRAY:      // fall-through
    case Type::CHARACTER:  // fall-through
    case Type::STRING:     // fall-through
    case Type::FLOATING:   // fall-through
    case Type::UNARY:      // fall-through
    case Type::BINARY:
      is_valid_ = true;
      break;
    case Type::ERROR:
      return false;
    default:
      AIDL_FATAL(this) << "Unrecognized constant value type: " << ToString(type_);
      return false;
  }

  return true;
}

bool AidlConstantValue::evaluate(const AidlTypeSpecifier& type) const {
  if (is_evaluated_) {
    return is_valid_;
  }
  int err = 0;
  is_evaluated_ = true;

  switch (type_) {
    case Type::ARRAY: {
      if (!type.IsArray()) {
        AIDL_ERROR(this) << "Invalid constant array type: " << type.GetName();
        err = -1;
        break;
      }
      Type array_type = Type::ERROR;
      bool success = true;
      for (const auto& value : values_) {
        success = value->CheckValid();
        if (success) {
          success = value->evaluate(type.ArrayBase());
          if (!success) {
            AIDL_ERROR(this) << "Invalid array element: " << value->value_;
            break;
          }
          if (array_type == Type::ERROR) {
            array_type = value->final_type_;
          } else if (!AidlBinaryConstExpression::AreCompatibleTypes(array_type,
                                                                    value->final_type_)) {
            AIDL_ERROR(this) << "Incompatible array element type: " << ToString(value->final_type_)
                             << ". Expecting type compatible with " << ToString(array_type);
            success = false;
            break;
          }
        } else {
          break;
        }
      }
      if (!success) {
        err = -1;
        break;
      }
      final_type_ = type_;
      break;
    }
    case Type::BOOLEAN:
      if ((value_ != "true") && (value_ != "false")) {
        AIDL_ERROR(this) << "Invalid constant boolean value: " << value_;
        err = -1;
        break;
      }
      final_value_ = (value_ == "true") ? 1 : 0;
      final_type_ = type_;
      break;
    case Type::INT8:   // fall-through
    case Type::INT32:  // fall-through
    case Type::INT64:
      // Parsing happens in the constructor
      final_type_ = type_;
      break;
    case Type::CHARACTER:  // fall-through
    case Type::STRING:
      final_string_value_ = value_;
      final_type_ = type_;
      break;
    case Type::FLOATING:
      // Just parse on the fly in ValueString
      final_type_ = type_;
      break;
    default:
      AIDL_FATAL(this) << "Unrecognized constant value type: " << ToString(type_);
      err = -1;
  }

  return (err == 0) ? true : false;
}

string AidlConstantValue::ToString(Type type) {
  switch (type) {
    case Type::BOOLEAN:
      return "a literal boolean";
    case Type::INT8:
      return "an int8 literal";
    case Type::INT32:
      return "an int32 literal";
    case Type::INT64:
      return "an int64 literal";
    case Type::ARRAY:
      return "a literal array";
    case Type::CHARACTER:
      return "a literal char";
    case Type::STRING:
      return "a literal string";
    case Type::FLOATING:
      return "a literal float";
    case Type::UNARY:
      return "a unary expression";
    case Type::BINARY:
      return "a binary expression";
    case Type::ERROR:
      LOG(FATAL) << "aidl internal error: error type failed to halt program";
      return "";
    default:
      LOG(FATAL) << "aidl internal error: unknown constant type: " << static_cast<int>(type);
      return "";  // not reached
  }
}

bool AidlUnaryConstExpression::CheckValid() const {
  if (is_evaluated_) return is_valid_;
  CHECK(unary_ != nullptr);

  is_valid_ = unary_->CheckValid();
  if (!is_valid_) {
    final_type_ = Type::ERROR;
    return false;
  }

  return AidlConstantValue::CheckValid();
}

bool AidlUnaryConstExpression::evaluate(const AidlTypeSpecifier& type) const {
  if (is_evaluated_) {
    return is_valid_;
  }
  is_evaluated_ = true;

  // Recursively evaluate the expression tree
  if (!unary_->is_evaluated_) {
    // TODO(b/142722772) CheckValid() should be called before ValueString()
    bool success = CheckValid();
    success &= unary_->evaluate(type);
    if (!success) {
      is_valid_ = false;
      return false;
    }
  }
  if (!unary_->is_valid_ || !IsCompatibleType(unary_->final_type_, op_)) {
    AIDL_ERROR(type) << "Invalid constant unary expression: " + value_;
    is_valid_ = false;
    return false;
  }
  final_type_ = unary_->final_type_;

  if (final_type_ == Type::FLOATING) {
    // don't do anything here. ValueString() will handle everything.
    is_valid_ = true;
    return true;
  }

#define CASE_UNARY(__type__)                                                    \
  final_value_ = handleUnary(op_, static_cast<__type__>(unary_->final_value_)); \
  return true;

  SWITCH_KIND(final_type_, CASE_UNARY, SHOULD_NOT_REACH(); final_type_ = Type::ERROR;
              is_valid_ = false; return false;)
}

bool AidlBinaryConstExpression::CheckValid() const {
  bool success = false;
  if (is_evaluated_) return is_valid_;
  CHECK(left_val_ != nullptr);
  CHECK(right_val_ != nullptr);

  success = left_val_->CheckValid();
  if (!success) {
    final_type_ = Type::ERROR;
    AIDL_ERROR(this) << "Invalid left operand in binary expression: " + value_;
  }

  success = right_val_->CheckValid();
  if (!success) {
    AIDL_ERROR(this) << "Invalid right operand in binary expression: " + value_;
    final_type_ = Type::ERROR;
  }

  if (final_type_ == Type::ERROR) {
    is_valid_ = false;
    return false;
  }

  is_valid_ = true;
  return AidlConstantValue::CheckValid();
}

bool AidlBinaryConstExpression::evaluate(const AidlTypeSpecifier& type) const {
  if (is_evaluated_) {
    return is_valid_;
  }
  is_evaluated_ = true;
  CHECK(left_val_ != nullptr);
  CHECK(right_val_ != nullptr);

  // Recursively evaluate the binary expression tree
  if (!left_val_->is_evaluated_ || !right_val_->is_evaluated_) {
    // TODO(b/142722772) CheckValid() should be called before ValueString()
    bool success = CheckValid();
    success &= left_val_->evaluate(type);
    success &= right_val_->evaluate(type);
    if (!success) {
      is_valid_ = false;
      return false;
    }
  }
  if (!left_val_->is_valid_ || !right_val_->is_valid_) {
    is_valid_ = false;
    return false;
  }
  is_valid_ = AreCompatibleTypes(left_val_->final_type_, right_val_->final_type_);
  if (!is_valid_) {
    return false;
  }

  bool isArithmeticOrBitflip = OP_IS_BIN_ARITHMETIC || OP_IS_BIN_BITFLIP;

  // Handle String case first
  if (left_val_->final_type_ == Type::STRING) {
    if (!OPEQ("+")) {
      // invalid operation on strings
      final_type_ = Type::ERROR;
      is_valid_ = false;
      return false;
    }

    // Remove trailing " from lhs
    const string& lhs = left_val_->final_string_value_;
    if (lhs.back() != '"') {
      AIDL_ERROR(this) << "'" << lhs << "' is missing a trailing quote.";
      final_type_ = Type::ERROR;
      is_valid_ = false;
      return false;
    }
    const string& rhs = right_val_->final_string_value_;
    // Remove starting " from rhs
    if (rhs.front() != '"') {
      AIDL_ERROR(this) << "'" << rhs << "' is missing a leading quote.";
      final_type_ = Type::ERROR;
      is_valid_ = false;
      return false;
    }

    final_string_value_ = string(lhs.begin(), lhs.end() - 1).append(rhs.begin() + 1, rhs.end());
    final_type_ = Type::STRING;
    return true;
  }

  // TODO(b/139877950) Add support for handling overflows

  // CASE: + - *  / % | ^ & < > <= >= == !=
  if (isArithmeticOrBitflip || OP_IS_BIN_COMP) {
    if ((op_ == "/" || op_ == "%") && right_val_->final_value_ == 0) {
      final_type_ = Type::ERROR;
      is_valid_ = false;
      AIDL_ERROR(this) << "Cannot do division operation with zero for expression: " + value_;
      return false;
    }

    // promoted kind for both operands.
    Type promoted = UsualArithmeticConversion(IntegralPromotion(left_val_->final_type_),
                                              IntegralPromotion(right_val_->final_type_));
    // result kind.
    final_type_ = isArithmeticOrBitflip
                      ? promoted        // arithmetic or bitflip operators generates promoted type
                      : Type::BOOLEAN;  // comparison operators generates bool

#define CASE_BINARY_COMMON(__type__)                                                     \
  final_value_ = handleBinaryCommon(static_cast<__type__>(left_val_->final_value_), op_, \
                                    static_cast<__type__>(right_val_->final_value_));    \
  return true;

    SWITCH_KIND(promoted, CASE_BINARY_COMMON, SHOULD_NOT_REACH(); final_type_ = Type::ERROR;
                is_valid_ = false; return false;)
  }

  // CASE: << >>
  string newOp = op_;
  if (OP_IS_BIN_SHIFT) {
    final_type_ = IntegralPromotion(left_val_->final_type_);
    // instead of promoting rval, simply casting it to int64 should also be good.
    int64_t numBits = right_val_->cast<int64_t>();
    if (numBits < 0) {
      // shifting with negative number of bits is undefined in C. In AIDL it
      // is defined as shifting into the other direction.
      newOp = OPEQ("<<") ? ">>" : "<<";
      numBits = -numBits;
    }

#define CASE_SHIFT(__type__)                                                                  \
  final_value_ = handleShift(static_cast<__type__>(left_val_->final_value_), newOp, numBits); \
  return true;

    SWITCH_KIND(final_type_, CASE_SHIFT, SHOULD_NOT_REACH(); final_type_ = Type::ERROR;
                is_valid_ = false; return false;)
  }

  // CASE: && ||
  if (OP_IS_BIN_LOGICAL) {
    final_type_ = Type::BOOLEAN;
    // easy; everything is bool.
    final_value_ = handleLogical(left_val_->final_value_, op_, right_val_->final_value_);
    return true;
  }

  SHOULD_NOT_REACH();
  is_valid_ = false;
  return false;
}

AidlConstantValue::AidlConstantValue(const AidlLocation& location, Type parsed_type,
                                     int64_t parsed_value, const string& checked_value)
    : AidlNode(location),
      type_(parsed_type),
      value_(checked_value),
      final_type_(parsed_type),
      final_value_(parsed_value) {
  CHECK(!value_.empty() || type_ == Type::ERROR);
  CHECK(type_ == Type::INT8 || type_ == Type::INT32 || type_ == Type::INT64);
}

AidlConstantValue::AidlConstantValue(const AidlLocation& location, Type type,
                                     const string& checked_value)
    : AidlNode(location),
      type_(type),
      value_(checked_value),
      final_type_(type) {
  CHECK(!value_.empty() || type_ == Type::ERROR);
  switch (type_) {
    case Type::INT8:
    case Type::INT32:
    case Type::INT64:
    case Type::ARRAY:
      AIDL_FATAL(this) << "Invalid type: " << ToString(type_);
      break;
    default:
      break;
  }
}

AidlConstantValue::AidlConstantValue(const AidlLocation& location, Type type,
                                     std::unique_ptr<vector<unique_ptr<AidlConstantValue>>> values)
    : AidlNode(location),
      type_(type),
      values_(std::move(*values)),
      is_valid_(false),
      is_evaluated_(false),
      final_type_(type) {
  CHECK(type_ == Type::ARRAY);
}

AidlUnaryConstExpression::AidlUnaryConstExpression(const AidlLocation& location, const string& op,
                                                   std::unique_ptr<AidlConstantValue> rval)
    : AidlConstantValue(location, Type::UNARY, op + rval->value_),
      unary_(std::move(rval)),
      op_(op) {
  final_type_ = Type::UNARY;
}

AidlBinaryConstExpression::AidlBinaryConstExpression(const AidlLocation& location,
                                                     std::unique_ptr<AidlConstantValue> lval,
                                                     const string& op,
                                                     std::unique_ptr<AidlConstantValue> rval)
    : AidlConstantValue(location, Type::BINARY, lval->value_ + op + rval->value_),
      left_val_(std::move(lval)),
      right_val_(std::move(rval)),
      op_(op) {
  final_type_ = Type::BINARY;
}
