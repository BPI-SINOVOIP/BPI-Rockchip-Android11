// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>
#include <zircon/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_CDECLS

// The |fdio_spawn| and |fdio_spawn_etc| functions allow some or all of the
// environment of the running process to be shared with the process being
// spawned.

// Provides the spawned process with the job in which the process was created.
//
// The spawned process receives the job using the |PA_JOB_DEFAULT| process
// argument.
#define FDIO_SPAWN_CLONE_JOB ((uint32_t)0x0001u)

// Provides the spawned process with the shared library loader resolved via
// fuchsia.process.Resolver (if resolved), or that which is used by this
// process.
//
// The shared library loader is passed as |PA_LDSVC_LOADER|.
#define FDIO_SPAWN_DEFAULT_LDSVC ((uint32_t)0x0002u)
// FDIO_SPAWN_CLONE_LDSVC is the same as FDIO_SPAWN_DEFAULT_LDSVC.
// TODO(ZX-3031): this name is deprecated.
#define FDIO_SPAWN_CLONE_LDSVC ((uint32_t)0x0002u)

// Clones the filesystem namespace into the spawned process.
#define FDIO_SPAWN_CLONE_NAMESPACE ((uint32_t)0x0004u)

// Clones file descriptors 0, 1, and 2 into the spawned process.
//
// Skips any of these file descriptors that are closed without generating an
// error.
#define FDIO_SPAWN_CLONE_STDIO ((uint32_t)0x0008u)

// Clones the environment into the spawned process.
#define FDIO_SPAWN_CLONE_ENVIRON ((uint32_t)0x0010u)

// Clones all of the above into the spawned process.
#define FDIO_SPAWN_CLONE_ALL ((uint32_t)0xFFFFu)

// Spawn a process in the given job.
//
// The program for the process is loaded from the given |path| and passed |argv|.
// The aspects of this process' environment indicated by |flags| are shared with
// the process. If the target program begins with |#!resolve | then the binary is
// resolved by url via |fuchsia.process.Resolver|.
//
// The |argv| array must be terminated with a null pointer.
//
// If |job| is |ZX_HANDLE_INVALID|, then the process is spawned in
// |zx_job_default()|. Does not take ownership of |job|.
//
// Upon success, |process_out| will be a handle to the process.
zx_status_t fdio_spawn(zx_handle_t job,
                       uint32_t flags,
                       const char* path,
                       const char* const* argv,
                       zx_handle_t* process_out);

// The |fdio_spawn_etc| function allows the running process to control the file
// descriptor table in the process being spawned.

// Duplicate a descriptor |local_fd| into |target_fd| in the spawned process.
//
// Uses the |fd| entry in the |fdio_spawn_action_t| union.
#define FDIO_SPAWN_ACTION_CLONE_FD ((uint32_t)0x0001u)

// Transfer local descriptor |local_fd| into |target_fd| in the spawned process.
//
// This action will fail if |local_fd| is not a valid |local_fd|, if |local_fd|
// has been duplicated, if |local_fd| is being used in an io operation, or if
// |local_fd| does not support this operation.
//
// From the point of view of the calling process, the |local_fd| will appear to
// have been closed, regardless of whether the |fdio_spawn_etc| call succeeds.
//
// Uses the |fd| entry in the |fdio_spawn_action_t| union.
#define FDIO_SPAWN_ACTION_TRANSFER_FD ((uint32_t)0x0002u)

// Add the given entry to the namespace of the spawned process.
//
// If |FDIO_SPAWN_CLONE_NAMESPACE| is specified via |flags|, the namespace entry
// is added to the cloned namespace from the calling process.
//
// The given handle will be closed regardless of whether the |fdio_spawn_etc|
// call succeeds.
//
// Uses the |ns| entry in the |fdio_spawn_action_t| union.
#define FDIO_SPAWN_ACTION_ADD_NS_ENTRY ((uint32_t)0x0003u)

// Add the given handle to the process arguments of the spawned process.
//
// The given handle will be closed regardless of whether the |fdio_spawn_etc|
// call succeeds.
//
// Uses the |h| entry in the |fdio_spawn_action_t| union.
#define FDIO_SPAWN_ACTION_ADD_HANDLE ((uint32_t)0x0004u)

// Sets the name of the spawned process to the given name.
//
// Overrides the default of use the first argument to name the process.
//
// Uses the |name| entry in the |fdio_spawn_action_t| union.
#define FDIO_SPAWN_ACTION_SET_NAME ((uint32_t)0x0005u)

// Instructs |fdio_spawn_etc| which actions to take.
typedef struct fdio_spawn_action fdio_spawn_action_t;
struct fdio_spawn_action {
    // The action to take.
    //
    // See |FDIO_SPAWN_ACTION_*| above. If |action| is invalid, the action will
    // be ignored (rather than generate an error).
    uint32_t action;
    union {
        struct {
            // The file descriptor in this process to clone or transfer.
            int local_fd;

            // The file descriptor in the spawned process that will receive the
            // clone or transfer.
            int target_fd;
        } fd;
        struct {
            // The prefix in which to install the given handle in the namespace
            // of the spawned process.
            const char* prefix;

            // The handle to install with the given prefix in the namespace of
            // the spawned process.
            zx_handle_t handle;
        } ns;
        struct {
            // The process argument identifier of the handle to pass to the
            // spawned process.
            uint32_t id;

            // The handle to pass to the process on startup.
            zx_handle_t handle;
        } h;
        struct {
            // The name to assign to the spawned process.
            const char* data;
        } name;
    };
};

// The maximum size for error messages from |fdio_spawn_etc|.
//
// Including the null terminator.
#define FDIO_SPAWN_ERR_MSG_MAX_LENGTH ((size_t)1024u)

// Spawn a process in the given job.
//
// The binary for the process is loaded from the given |path| and passed |argv|.
// The aspects of this process' environment indicated by |clone| are shared with
// the process.
//
// The spawned process is also given |environ| as its environment and the given
// |actions| are applied when creating the process.
//
// The |argv| array must be terminated with a null pointer.
//
// If non-null, the |environ| array must be terminated with a null pointer.
//
// If non-null, the |err_msg_out| buffer must have space for
// |FDIO_SPAWN_ERR_MSG_MAX_LENGTH| bytes.
//
// If both |FDIO_SPAWN_CLONE_ENVIRON| and |environ| are specified, then the
// spawned process is given |environ| as its environment. If both
// |FDIO_SPAWN_CLONE_STDIO| and |actions| that target any of fds 0, 1, and 2 are
// specified, then the actions that target those fds will control their
// semantics in the spawned process.
//
// If |job| is |ZX_HANDLE_INVALID|, then the process is spawned in
// |zx_job_default()|. Does not take ownership of |job|.
//
// Upon success, |process_out| will be a handle to the process. Upon failure, if
// |err_msg_out| is not null, an error message will be we written to
// |err_msg_out|, including a null terminator.
zx_status_t fdio_spawn_etc(zx_handle_t job,
                           uint32_t flags,
                           const char* path,
                           const char* const* argv,
                           const char* const* environ,
                           size_t action_count,
                           const fdio_spawn_action_t* actions,
                           zx_handle_t* process_out,
                           char err_msg_out[FDIO_SPAWN_ERR_MSG_MAX_LENGTH]);

// Spawn a process using the given executable in the given job.
//
// See |fdio_spawn_etc| for details. Rather than loading the binary for the
// process from a path, this function receives the binary as the contents of a
// vmo.
//
// Always consumes |executable_vmo|.
zx_status_t fdio_spawn_vmo(zx_handle_t job,
                           uint32_t flags,
                           zx_handle_t executable_vmo,
                           const char* const* argv,
                           const char* const* environ,
                           size_t action_count,
                           const fdio_spawn_action_t* actions,
                           zx_handle_t* process_out,
                           char err_msg_out[FDIO_SPAWN_ERR_MSG_MAX_LENGTH]);

__END_CDECLS
