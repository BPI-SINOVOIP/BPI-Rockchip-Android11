// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#ifndef __ASSEMBLER__

#if !defined(__GNUC__) && !defined(__clang__)
#error "Unrecognized compiler!"
#endif

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#define __UNUSED __attribute__((__unused__))
#define __USED __attribute__((__used__))
#define __PACKED __attribute__((packed))
#define __ALIGNED(x) __attribute__((aligned(x)))
#define __PRINTFLIKE(__fmt,__varargs) __attribute__((__format__ (__printf__, __fmt, __varargs)))
#define __SCANFLIKE(__fmt,__varargs) __attribute__((__format__ (__scanf__, __fmt, __varargs)))
#define __SECTION(x) __attribute__((__section__(x)))
#define __PURE __attribute__((__pure__))
#define __CONST __attribute__((__const__))
#define __NO_RETURN __attribute__((__noreturn__))
#define __MALLOC __attribute__((__malloc__))
#define __WEAK __attribute__((__weak__))
#define __GNU_INLINE __attribute__((__gnu_inline__))
#define __GET_CALLER(x) __builtin_return_address(0)
#define __GET_FRAME(x) __builtin_frame_address(0)
#define __NAKED __attribute__((__naked__))
#define __ISCONSTANT(x) __builtin_constant_p(x)
#define __NO_INLINE __attribute__((__noinline__))
#define __SRAM __NO_INLINE __SECTION(".sram.text")
#define __CONSTRUCTOR __attribute__((__constructor__))
#define __DESTRUCTOR __attribute__((__destructor__))
#define __RESTRICT __restrict

#ifndef __clang__
#define __LEAF_FN __attribute__((__leaf__))
#define __OPTIMIZE(x) __attribute__((__optimize__(x)))
#define __EXTERNALLY_VISIBLE __attribute__((__externally_visible__))
#define __THREAD_ANNOTATION(x)
#define __NO_SAFESTACK
#else
#define __LEAF_FN
#define __OPTIMIZE(x)
#define __EXTERNALLY_VISIBLE
#define __THREAD_ANNOTATION(x) __attribute__((x))
#define __NO_SAFESTACK __attribute__((__no_sanitize__("safe-stack")))
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#define __ALWAYS_INLINE __attribute__((__always_inline__))
#define __MAY_ALIAS __attribute__((__may_alias__))
#define __NONNULL(x) __attribute__((__nonnull__ x))
#define __WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#define __UNREACHABLE __builtin_unreachable()
#define __WEAK_ALIAS(x) __attribute__((__weak__, __alias__(x)))
#define __ALIAS(x) __attribute__((__alias__(x)))
#define __EXPORT __attribute__ ((__visibility__("default")))
#define __LOCAL  __attribute__ ((__visibility__("hidden")))
#define __THREAD __thread
#define __offsetof(type, field) __builtin_offsetof(type, field)

#if defined(__cplusplus) && __cplusplus >= 201703L
#define __FALLTHROUGH [[fallthrough]]
#elif defined(__cplusplus) && defined(__clang__)
#define __FALLTHROUGH [[clang::fallthrough]]
#elif __GNUC__ >= 7
#define __FALLTHROUGH __attribute__((__fallthrough__))
#else
#define __FALLTHROUGH do {} while (0)
#endif

// Publicly exposed thread annotation macros. These have a long and ugly name to
// minimize the chance of collision with consumers of Zircon's public headers.
#define __TA_CAPABILITY(x) __THREAD_ANNOTATION(__capability__(x))
#define __TA_GUARDED(x) __THREAD_ANNOTATION(__guarded_by__(x))
#define __TA_ACQUIRE(...) __THREAD_ANNOTATION(__acquire_capability__(__VA_ARGS__))
#define __TA_TRY_ACQUIRE(...) __THREAD_ANNOTATION(__try_acquire_capability__(__VA_ARGS__))
#define __TA_ACQUIRED_BEFORE(...) __THREAD_ANNOTATION(__acquired_before__(__VA_ARGS__))
#define __TA_ACQUIRED_AFTER(...) __THREAD_ANNOTATION(__acquired_after__(__VA_ARGS__))
#define __TA_RELEASE(...) __THREAD_ANNOTATION(__release_capability__(__VA_ARGS__))
#define __TA_REQUIRES(...) __THREAD_ANNOTATION(__requires_capability__(__VA_ARGS__))
#define __TA_EXCLUDES(...) __THREAD_ANNOTATION(__locks_excluded__(__VA_ARGS__))
#define __TA_RETURN_CAPABILITY(x) __THREAD_ANNOTATION(__lock_returned__(x))
#define __TA_SCOPED_CAPABILITY __THREAD_ANNOTATION(__scoped_lockable__)
#define __TA_NO_THREAD_SAFETY_ANALYSIS __THREAD_ANNOTATION(__no_thread_safety_analysis__)

#endif  // ifndef __ASSEMBLER__

#if !defined(__DEPRECATE)
#define __DEPRECATE __attribute__((__deprecated__))
#endif

/* TODO: add type check */
#if !defined(countof)
#define countof(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* CPP header guards */
#ifdef __cplusplus
#define __BEGIN_CDECLS  extern "C" {
#define __END_CDECLS    }
#else
#define __BEGIN_CDECLS
#define __END_CDECLS
#endif

// constexpr annotation for use in static inlines usable in both C and C++
#ifdef __cplusplus
#define __CONSTEXPR constexpr
#else
#define __CONSTEXPR
#endif

#define add_overflow(a, b, c) __builtin_add_overflow(a, b, c)
#define sub_overflow(a, b, c) __builtin_sub_overflow(a, b, c)
#define mul_overflow(a, b, c) __builtin_mul_overflow(a, b, c)
