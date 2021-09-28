// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header provides Address Sanitizer specific macros.
//
#ifndef LIBBRILLO_BRILLO_ASAN_H_
#define LIBBRILLO_BRILLO_ASAN_H_

#if defined(__has_feature) && __has_feature(address_sanitizer)
// ASan is enabled.
#define BRILLO_ASAN_BUILD 1
// Provide BRILLO_DISABLE_ASAN hook to disable ASan.
// Put this in front on functions or global variables where required.
#define BRILLO_DISABLE_ASAN __attribute__((no_sanitize("address")))
#else
#define BRILLO_DISABLE_ASAN
#endif

#endif

