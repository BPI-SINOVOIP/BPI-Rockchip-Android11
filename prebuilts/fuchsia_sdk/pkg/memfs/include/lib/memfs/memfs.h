// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_MEMFS_INCLUDE_LIB_MEMFS_MEMFS_H_
#define LIB_MEMFS_INCLUDE_LIB_MEMFS_MEMFS_H_

#include <lib/async/dispatcher.h>
#include <lib/sync/completion.h>
#include <zircon/compiler.h>
#include <zircon/types.h>

__BEGIN_CDECLS

typedef struct memfs_filesystem memfs_filesystem_t;

// Given an async dispatcher, create an in-memory filesystem.
//
// Returns the MemFS filesystem object in |out_fs|. This object
// must be freed by memfs_free_filesystem.
//
// Returns a handle to the root directory in |out_root|.
__EXPORT zx_status_t memfs_create_filesystem(async_dispatcher_t* dispatcher,
                                             memfs_filesystem_t** out_fs,
                                             zx_handle_t* out_root);

// Same as memfs_create_filesystem, but with an extra |max_num_pages| option.
//
// Specify the maximum number of pages available to the fs via |max_num_pages|.
// This puts a bound on memory consumption.
__EXPORT zx_status_t memfs_create_filesystem_with_page_limit(async_dispatcher_t* dispatcher,
                                                             size_t max_num_pages,
                                                             memfs_filesystem_t** out_fs,
                                                             zx_handle_t* out_root);

// Frees a MemFS filesystem, unmounting any sub-filesystems that
// may exist.
//
// Requires that the async handler dispatcher provided to
// |memfs_create_filesystem| still be running.
//
// Signals the optional argument |unmounted| when memfs has torn down.
__EXPORT void memfs_free_filesystem(memfs_filesystem_t* fs, sync_completion_t* unmounted);

// Creates an in-memory filesystem and installs it into the local namespace at
// the given path.
//
// Operations on the filesystem are serviced by the given async dispatcher.
//
// Returns |ZX_ERR_ALREADY_EXISTS| if |path| already exists in the namespace for
// this process.
__EXPORT zx_status_t memfs_install_at(async_dispatcher_t* dispatcher, const char* path);

// Same as memfs_install_at, but with an extra |max_num_pages| option.
//
// Specify the maximum number of pages available to the fs via |max_num_pages|.
// This puts a bound on memory consumption.
__EXPORT zx_status_t memfs_install_at_with_page_limit(async_dispatcher_t* dispatcher,
                                                      size_t max_num_pages,
                                                      const char* path);

__END_CDECLS

#endif // LIB_MEMFS_INCLUDE_LIB_MEMFS_MEMFS_H_
