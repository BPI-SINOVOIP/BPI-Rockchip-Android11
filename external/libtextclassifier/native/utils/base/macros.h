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

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_MACROS_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_MACROS_H_

#include "utils/base/config.h"

namespace libtextclassifier3 {

#define TC3_ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / (size_t)(!(sizeof(a) % sizeof(*(a)))))

#if LANG_CXX11
#define TC3_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName &) = delete;         \
  TypeName &operator=(const TypeName &) = delete
#else  // C++98 case follows

// Note that these C++98 implementations cannot completely disallow copying,
// as members and friends can still accidentally make elided copies without
// triggering a linker error.
#define TC3_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName &);                  \
  TypeName &operator=(const TypeName &)
#endif  // LANG_CXX11

// The TC3_FALLTHROUGH_INTENDED macro can be used to annotate implicit
// fall-through between switch labels:
//
//  switch (x) {
//    case 40:
//    case 41:
//      if (truth_is_out_there) {
//        ++x;
//        TC3_FALLTHROUGH_INTENDED;  // Use instead of/along with annotations in
//                                  // comments.
//      } else {
//        return x;
//      }
//    case 42:
//      ...
//
//  As shown in the example above, the TC3_FALLTHROUGH_INTENDED macro should be
//  followed by a semicolon. It is designed to mimic control-flow statements
//  like 'break;', so it can be placed in most places where 'break;' can, but
//  only if there are no statements on the execution path between it and the
//  next switch label.
//
//  When compiled with clang in C++11 mode, the TC3_FALLTHROUGH_INTENDED macro
//  is expanded to [[clang::fallthrough]] attribute, which is analysed when
//  performing switch labels fall-through diagnostic ('-Wimplicit-fallthrough').
//  See clang documentation on language extensions for details:
//  http://clang.llvm.org/docs/AttributeReference.html#fallthrough-clang-fallthrough
//
//  When used with unsupported compilers, the TC3_FALLTHROUGH_INTENDED macro has
//  no effect on diagnostics.
//
//  In either case this macro has no effect on runtime behavior and performance
//  of code.
#if defined(__clang__) && defined(__has_warning)
#if __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
#define TC3_FALLTHROUGH_INTENDED [[clang::fallthrough]]
#endif
#elif defined(__GNUC__) && __GNUC__ >= 7
#define TC3_FALLTHROUGH_INTENDED [[gnu::fallthrough]]
#endif

#ifndef TC3_FALLTHROUGH_INTENDED
#define TC3_FALLTHROUGH_INTENDED \
  do {                           \
  } while (0)
#endif

#ifdef __has_builtin
#define TC3_HAS_BUILTIN(x) __has_builtin(x)
#else
#define TC3_HAS_BUILTIN(x) 0
#endif

// Compilers can be told that a certain branch is not likely to be taken
// (for instance, a CHECK failure), and use that information in static
// analysis. Giving it this information can help it optimize for the
// common case in the absence of better information (ie.
// -fprofile-arcs).
//
// We need to disable this for GPU builds, though, since nvcc8 and older
// don't recognize `__builtin_expect` as a builtin, and fail compilation.
#if (!defined(__NVCC__)) && (TC3_HAS_BUILTIN(__builtin_expect) || \
                             (defined(__GNUC__) && __GNUC__ >= 3))
#define TC3_PREDICT_FALSE(x) (__builtin_expect(x, 0))
#define TC3_PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))
#else
#define TC3_PREDICT_FALSE(x) (x)
#define TC3_PREDICT_TRUE(x) (x)
#endif

// TC3_HAVE_ATTRIBUTE
//
// A function-like feature checking macro that is a wrapper around
// `__has_attribute`, which is defined by GCC 5+ and Clang and evaluates to a
// nonzero constant integer if the attribute is supported or 0 if not.
//
// It evaluates to zero if `__has_attribute` is not defined by the compiler.
//
// GCC: https://gcc.gnu.org/gcc-5/changes.html
// Clang: https://clang.llvm.org/docs/LanguageExtensions.html
#ifdef __has_attribute
#define TC3_HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define TC3_HAVE_ATTRIBUTE(x) 0
#endif

// TC3_ATTRIBUTE_PACKED
//
// Prevents the compiler from padding a structure to natural alignment
#if TC3_HAVE_ATTRIBUTE(packed) || (defined(__GNUC__) && !defined(__clang__))
#define TC3_ATTRIBUTE_PACKED __attribute__((__packed__))
#else
#define TC3_ATTRIBUTE_PACKED
#endif

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_MACROS_H_
