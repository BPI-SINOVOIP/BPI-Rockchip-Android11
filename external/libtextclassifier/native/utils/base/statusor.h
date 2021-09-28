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

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_STATUSOR_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_STATUSOR_H_

#include <type_traits>
#include <utility>

#include "utils/base/logging.h"
#include "utils/base/macros.h"
#include "utils/base/status.h"

namespace libtextclassifier3 {

// A StatusOr holds a Status (in the case of an error), or a value T.
template <typename T>
class StatusOr {
 public:
  // Has status UNKNOWN.
  inline StatusOr();

  // Builds from a non-OK status. Crashes if an OK status is specified.
  inline StatusOr(const Status& status);             // NOLINT

  // Builds from the specified value.
  inline StatusOr(const T& value);  // NOLINT
  inline StatusOr(T&& value);       // NOLINT

  // Copy constructor.
  inline StatusOr(const StatusOr& other);
  // Move constructor.
  inline StatusOr(StatusOr&& other);

  // Conversion copy constructor, T must be copy constructible from U.
  template <typename U,
            std::enable_if_t<
                std::conjunction<std::negation<std::is_same<T, U>>,
                                 std::is_constructible<T, const U&>,
                                 std::is_convertible<const U&, T>>::value,
                int> = 0>
  inline StatusOr(const StatusOr<U>& other);  // NOLINT

  // Conversion move constructor, T must by move constructible from U.
  template <
      typename U,
      std::enable_if_t<std::conjunction<std::negation<std::is_same<T, U>>,
                                        std::is_constructible<T, U&&>,
                                        std::is_convertible<U&&, T>>::value,
                       int> = 0>
  inline StatusOr(StatusOr<U>&& other);  // NOLINT

  // Value conversion copy constructor, T must by copy constructible from U.
  template <typename U,
            std::enable_if_t<
                std::conjunction<std::negation<std::is_same<T, U>>,
                                 std::is_constructible<T, const U&>,
                                 std::is_convertible<const U&, T>>::value,
                int> = 0>
  inline StatusOr(const U& value);  // NOLINT

  // Value conversion move constructor, T must by move constructible from U.
  template <
      typename U,
      std::enable_if_t<std::conjunction<std::negation<std::is_same<T, U>>,
                                        std::is_constructible<T, U&&>,
                                        std::is_convertible<U&&, T>>::value,
                       int> = 0>
  inline StatusOr(U&& value);  // NOLINT

  // Assignment operator.
  inline StatusOr& operator=(const StatusOr& other);
  inline StatusOr& operator=(StatusOr&& other);

  // Conversion assignment operator, T must be assignable from U
  template <typename U>
  inline StatusOr& operator=(const StatusOr<U>& other);

  inline ~StatusOr();

  // Accessors.
  inline const Status& status() const& { return status_; }
  inline Status status() && { return std::move(status_); }

  // Shorthand for status().ok().
  inline bool ok() const { return status_.ok(); }

  // Returns value or crashes if ok() is false.
  inline const T& ValueOrDie() const& {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return value_;
  }
  inline T& ValueOrDie() & {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return value_;
  }
  inline const T&& ValueOrDie() const&& {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return std::move(value_);
  }
  inline T&& ValueOrDie() && {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return std::move(value_);
  }

  template <typename U>
  friend class StatusOr;

 private:
  Status status_;
  // The members of unions do not require initialization and are not destructed
  // unless specifically called. This allows us to construct instances of
  // StatusOr with only error statuses where T is not default constructible.
  union {
    // value_ is active iff status_.ok()==true
    // WARNING: The destructor of value_ is called ONLY if status_ is OK.
    T value_;
  };
};

// Implementation.

template <typename T>
inline StatusOr<T>::StatusOr() : status_(StatusCode::UNKNOWN, "") {}

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status) : status_(status) {
  if (status.ok()) {
    TC3_LOG(FATAL) << "OkStatus() is not a valid argument to StatusOr";
    exit(1);
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const T& value) : value_(value) {}

template <typename T>
inline StatusOr<T>::StatusOr(T&& value) : value_(std::move(value)) {}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
inline StatusOr<T>::StatusOr(StatusOr&& other)
    : status_(other.status_), value_(std::move(other.value_)) {}

template <typename T>
template <
    typename U,
    std::enable_if_t<std::conjunction<std::negation<std::is_same<T, U>>,
                                      std::is_constructible<T, const U&>,
                                      std::is_convertible<const U&, T>>::value,
                     int>>
inline StatusOr<T>::StatusOr(const StatusOr<U>& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
template <typename U,
          std::enable_if_t<std::conjunction<std::negation<std::is_same<T, U>>,
                                            std::is_constructible<T, U&&>,
                                            std::is_convertible<U&&, T>>::value,
                           int>>
inline StatusOr<T>::StatusOr(StatusOr<U>&& other)
    : status_(other.status_), value_(std::move(other.value_)) {}

template <typename T>
template <
    typename U,
    std::enable_if_t<std::conjunction<std::negation<std::is_same<T, U>>,
                                      std::is_constructible<T, const U&>,
                                      std::is_convertible<const U&, T>>::value,
                     int>>
inline StatusOr<T>::StatusOr(const U& value) : StatusOr(T(value)) {}

template <typename T>
template <typename U,
          std::enable_if_t<std::conjunction<std::negation<std::is_same<T, U>>,
                                            std::is_constructible<T, U&&>,
                                            std::is_convertible<U&&, T>>::value,
                           int>>
inline StatusOr<T>::StatusOr(U&& value) : StatusOr(T(std::forward<U>(value))) {}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr& other) {
  status_ = other.status_;
  if (status_.ok()) {
    value_ = other.value_;
  }
  return *this;
}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(StatusOr&& other) {
  status_ = other.status_;
  if (status_.ok()) {
    value_ = std::move(other.value_);
  }
  return *this;
}

template <typename T>
inline StatusOr<T>::~StatusOr() {
  if (ok()) {
    value_.~T();
  }
}

template <typename T>
template <typename U>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<U>& other) {
  status_ = other.status_;
  if (status_.ok()) {
    value_ = other.value_;
  }
  return *this;
}

}  // namespace libtextclassifier3

#define TC3_ASSIGN_OR_RETURN(...)                              \
  TC_STATUS_MACROS_IMPL_GET_VARIADIC_(                         \
      (__VA_ARGS__, TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_, \
       TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_))             \
  (__VA_ARGS__)

#define TC3_ASSIGN_OR_RETURN_NULL(lhs, rexpr) \
  TC3_ASSIGN_OR_RETURN(lhs, rexpr, nullptr)

#define TC3_ASSIGN_OR_RETURN_FALSE(lhs, rexpr) \
  TC3_ASSIGN_OR_RETURN(lhs, rexpr, false)

#define TC3_ASSIGN_OR_RETURN_0(lhs, rexpr) TC3_ASSIGN_OR_RETURN(lhs, rexpr, 0)

// =================================================================
// == Implementation details, do not rely on anything below here. ==
// =================================================================

// Some builds do not support C++14 fully yet, using C++11 constexpr technique.
constexpr bool HasPossiblyConditionalOperator(const char* lhs, int index) {
  return (index == -1 ? false
                      : (lhs[index] == '?'
                             ? true
                             : HasPossiblyConditionalOperator(lhs, index - 1)));
}

// MSVC incorrectly expands variadic macros, splice together a macro call to
// work around the bug.
#define TC_STATUS_MACROS_IMPL_GET_VARIADIC_HELPER_(_1, _2, _3, NAME, ...) NAME
#define TC_STATUS_MACROS_IMPL_GET_VARIADIC_(args) \
  TC_STATUS_MACROS_IMPL_GET_VARIADIC_HELPER_ args

#define TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_(lhs, rexpr) \
  TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_(lhs, rexpr, _)
#define TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_(lhs, rexpr,                \
                                                  error_expression)          \
  TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(                                   \
      TC_STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__), lhs, rexpr, \
      error_expression)
#define TC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(statusor, lhs, rexpr,          \
                                                error_expression)              \
  auto statusor = (rexpr);                                                     \
  if (!statusor.ok()) {                                                        \
    ::libtextclassifier3::Status _(std::move(statusor).status());              \
    (void)_; /* error_expression is allowed to not use this variable */        \
    return (error_expression);                                                 \
  }                                                                            \
  {                                                                            \
    static_assert(#lhs[0] != '(' || #lhs[sizeof(#lhs) - 2] != ')' ||           \
                      !HasPossiblyConditionalOperator(#lhs, sizeof(#lhs) - 2), \
                  "Identified potential conditional operator, consider not "   \
                  "using ASSIGN_OR_RETURN");                                   \
  }                                                                            \
  TC_STATUS_MACROS_IMPL_UNPARENTHESIZE_IF_PARENTHESIZED(lhs) =                 \
      std::move(statusor).ValueOrDie()

// Internal helpers for macro expansion.
#define TC_STATUS_MACROS_IMPL_EAT(...)
#define TC_STATUS_MACROS_IMPL_REM(...) __VA_ARGS__
#define TC_STATUS_MACROS_IMPL_EMPTY()

// Internal helpers for emptyness arguments check.
#define TC_STATUS_MACROS_IMPL_IS_EMPTY_INNER(...) \
  TC_STATUS_MACROS_IMPL_IS_EMPTY_INNER_I(__VA_ARGS__, 0, 1)
#define TC_STATUS_MACROS_IMPL_IS_EMPTY_INNER_I(e0, e1, is_empty, ...) is_empty

#define TC_STATUS_MACROS_IMPL_IS_EMPTY(...) \
  TC_STATUS_MACROS_IMPL_IS_EMPTY_I(__VA_ARGS__)
#define TC_STATUS_MACROS_IMPL_IS_EMPTY_I(...) \
  TC_STATUS_MACROS_IMPL_IS_EMPTY_INNER(_, ##__VA_ARGS__)

// Internal helpers for if statement.
#define TC_STATUS_MACROS_IMPL_IF_1(_Then, _Else) _Then
#define TC_STATUS_MACROS_IMPL_IF_0(_Then, _Else) _Else
#define TC_STATUS_MACROS_IMPL_IF(_Cond, _Then, _Else) \
  TC_STATUS_MACROS_IMPL_CONCAT_(TC_STATUS_MACROS_IMPL_IF_, _Cond)(_Then, _Else)

// Expands to 1 if the input is parenthesized. Otherwise expands to 0.
#define TC_STATUS_MACROS_IMPL_IS_PARENTHESIZED(...) \
  TC_STATUS_MACROS_IMPL_IS_EMPTY(TC_STATUS_MACROS_IMPL_EAT __VA_ARGS__)

// If the input is parenthesized, removes the parentheses. Otherwise expands to
// the input unchanged.
#define TC_STATUS_MACROS_IMPL_UNPARENTHESIZE_IF_PARENTHESIZED(...) \
  TC_STATUS_MACROS_IMPL_IF(                                        \
      TC_STATUS_MACROS_IMPL_IS_PARENTHESIZED(__VA_ARGS__),         \
      TC_STATUS_MACROS_IMPL_REM, TC_STATUS_MACROS_IMPL_EMPTY())    \
  __VA_ARGS__

// Internal helper for concatenating macro values.
#define TC_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define TC_STATUS_MACROS_IMPL_CONCAT_(x, y) \
  TC_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_STATUSOR_H_
