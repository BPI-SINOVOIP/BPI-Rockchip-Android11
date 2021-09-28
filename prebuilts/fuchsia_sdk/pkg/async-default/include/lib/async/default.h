// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_ASYNC_DEFAULT_H_
#define LIB_ASYNC_DEFAULT_H_

#include <lib/async/dispatcher.h>
#include <zircon/compiler.h>

__BEGIN_CDECLS

// Gets the current thread's default asynchronous dispatcher interface.
// Returns |NULL| if none.
__EXPORT async_dispatcher_t* async_get_default_dispatcher(void);

// Sets the current thread's default asynchronous dispatcher interface.
// May be set to |NULL| if this thread doesn't have a default dispatcher.
__EXPORT void async_set_default_dispatcher(async_dispatcher_t* dispatcher);

__END_CDECLS

#endif  // LIB_ASYNC_DEFAULT_H_
