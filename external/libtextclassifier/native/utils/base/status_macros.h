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

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_STATUS_MACROS_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_STATUS_MACROS_H_

#include <utility>

#include "utils/base/status.h"
#include "utils/base/statusor.h"

namespace libtextclassifier3 {

// An adapter to enable TC3_RETURN_IF_ERROR to be used with either Status or
// StatusOr.
class StatusAdapter {
 public:
  explicit StatusAdapter(const Status& s) : s_(s) {}
  explicit StatusAdapter(Status&& s) : s_(std::move(s)) {}
  template <typename T>
  explicit StatusAdapter(const StatusOr<T>& s) : s_(s.status()) {}
  template <typename T>
  explicit StatusAdapter(StatusOr<T>&& s) : s_(std::move(s).status()) {}

  bool ok() const { return s_.ok(); }
  explicit operator bool() const { return ok(); }

  const Status& status() const& { return s_; }
  Status status() && { return std::move(s_); }

 private:
  Status s_;
};

}  // namespace libtextclassifier3

// Evaluates an expression that produces a `libtextclassifier3::Status`. If the
// status is not ok, returns it from the current function.
//
// For example:
//   libtextclassifier3::Status MultiStepFunction() {
//     TC3_RETURN_IF_ERROR(Function(args...));
//     TC3_RETURN_IF_ERROR(foo.Method(args...));
//     return libtextclassifier3::Status();
//   }
#define TC3_RETURN_IF_ERROR(expr)                          \
  TC3_STATUS_MACROS_IMPL_ELSE_BLOCKER_                     \
  if (::libtextclassifier3::StatusAdapter adapter{expr}) { \
  } else /* NOLINT */                                      \
    return std::move(adapter).status()

// The GNU compiler emits a warning for code like:
//
//   if (foo)
//     if (bar) { } else baz;
//
// because it thinks you might want the else to bind to the first if.  This
// leads to problems with code like:
//
//   if (do_expr) TC3_RETURN_IF_ERROR(expr);
//
// The "switch (0) case 0:" idiom is used to suppress this.
#define TC3_STATUS_MACROS_IMPL_ELSE_BLOCKER_ \
  switch (0)                                 \
  case 0:                                    \
  default:  // NOLINT

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_STATUS_MACROS_H_
