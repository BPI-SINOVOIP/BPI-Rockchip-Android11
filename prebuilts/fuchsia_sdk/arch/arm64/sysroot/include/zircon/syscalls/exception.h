// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSCALLS_EXCEPTION_H_
#define ZIRCON_SYSCALLS_EXCEPTION_H_

#include <zircon/compiler.h>
#include <zircon/syscalls/port.h>
#include <zircon/types.h>

__BEGIN_CDECLS

// ask clang format not to mess up the indentation:
// clang-format off

// This bit is set for synthetic exceptions to distinguish them from
// architectural exceptions.
// Note: Port packet types provide 8 bits to distinguish the exception type.
// See zircon/port.h.
#define ZX_EXCP_SYNTH ((uint8_t)0x80)

// The kind of an exception.
// Exception types are a subset of port packet types. See zircon/port.h.

// These are architectural exceptions.
// Depending on the exception, further information can be found in
// |report.context.arch|.

// General exception not covered by another value.
#define ZX_EXCP_GENERAL ZX_PKT_TYPE_EXCEPTION(0)
#define ZX_EXCP_FATAL_PAGE_FAULT ZX_PKT_TYPE_EXCEPTION(1)
#define ZX_EXCP_UNDEFINED_INSTRUCTION ZX_PKT_TYPE_EXCEPTION(2)
#define ZX_EXCP_SW_BREAKPOINT ZX_PKT_TYPE_EXCEPTION(3)
#define ZX_EXCP_HW_BREAKPOINT ZX_PKT_TYPE_EXCEPTION(4)
#define ZX_EXCP_UNALIGNED_ACCESS ZX_PKT_TYPE_EXCEPTION(5)

// Synthetic exceptions.

// A thread is starting.
// This exception is sent to debuggers only (ZX_EXCEPTION_PORT_TYPE_DEBUGGER).
// The thread is paused until it is resumed by the debugger
// with zx_task_resume_from_exception.
#define ZX_EXCP_THREAD_STARTING ZX_PKT_TYPE_EXCEPTION(ZX_EXCP_SYNTH | 0)

// A thread is exiting.
// This exception is sent to debuggers only (ZX_EXCEPTION_PORT_TYPE_DEBUGGER).
// This exception is different from ZX_EXCP_GONE in that a debugger can
// still examine thread state.
// The thread is paused until it is resumed by the debugger
// with zx_task_resume_from_exception.
#define ZX_EXCP_THREAD_EXITING ZX_PKT_TYPE_EXCEPTION(ZX_EXCP_SYNTH | 1)

// This exception is generated when a syscall fails with a job policy
// error (for example, an invalid handle argument is passed to the
// syscall when the ZX_POL_BAD_HANDLE policy is enabled) and
// ZX_POL_ACTION_EXCEPTION is set for the policy.  The thread that
// invoked the syscall may be resumed with zx_task_resume_from_exception.
#define ZX_EXCP_POLICY_ERROR ZX_PKT_TYPE_EXCEPTION(ZX_EXCP_SYNTH | 2)

// A process is starting.
// This exception is sent to job debuggers only
// (ZX_EXCEPTION_PORT_TYPE_JOB_DEBUGGER).
// The initial thread is paused until it is resumed by the debugger with
// zx_task_resume_from_exception.
#define ZX_EXCP_PROCESS_STARTING ZX_PKT_TYPE_EXCEPTION(ZX_EXCP_SYNTH | 3)

typedef uint32_t zx_excp_type_t;

// Assuming |excp| is an exception type, return non-zero if it is an
// architectural exception.
#define ZX_EXCP_IS_ARCH(excp) \
  (((excp) & (ZX_PKT_TYPE_EXCEPTION(ZX_EXCP_SYNTH) & ~ZX_PKT_TYPE_MASK)) == 0)

typedef struct zx_x86_64_exc_data {
    uint64_t vector;
    uint64_t err_code;
    uint64_t cr2;
} zx_x86_64_exc_data_t;

typedef struct zx_arm64_exc_data {
    uint32_t esr;
    uint64_t far;
} zx_arm64_exc_data_t;

// data associated with an exception (siginfo in linux parlance)
// Things available from regsets (e.g., pc) are not included here.
// For an example list of things one might add, see linux siginfo.
typedef struct zx_exception_context {
    struct {
        union {
            zx_x86_64_exc_data_t x86_64;
            zx_arm64_exc_data_t  arm_64;
        } u;
    } arch;
} zx_exception_context_t;

// The common header of all exception reports.
typedef struct zx_exception_header {
    // The actual size, in bytes, of the report (including this field).
    uint32_t size;

    zx_excp_type_t type;
} zx_exception_header_t;

// Data reported to an exception handler for most exceptions.
typedef struct zx_exception_report {
    zx_exception_header_t header;
    // The remainder of the report is exception-specific.
    zx_exception_context_t context;
} zx_exception_report_t;

// Options for zx_task_resume_from_exception()
#define ZX_RESUME_TRY_NEXT ((uint32_t)2)
// Indicates that instead of resuming from the faulting instruction we instead
// let the next exception handler in the search order, if any, process the
// exception. If there are no more then the entire process is killed.

// Options for zx_task_bind_exception_port.
#define ZX_EXCEPTION_PORT_DEBUGGER ((uint32_t)1)
// When binding an exception port to a process, set the process's debugger
// exception port.

// The type of exception port a thread may be waiting for a response from.
// These values are reported in zx_info_thread_t.wait_exception_port_type.
#define ZX_EXCEPTION_PORT_TYPE_NONE         ((uint32_t)0u)
#define ZX_EXCEPTION_PORT_TYPE_DEBUGGER     ((uint32_t)1u)
#define ZX_EXCEPTION_PORT_TYPE_THREAD       ((uint32_t)2u)
#define ZX_EXCEPTION_PORT_TYPE_PROCESS      ((uint32_t)3u)
#define ZX_EXCEPTION_PORT_TYPE_JOB          ((uint32_t)4u)
#define ZX_EXCEPTION_PORT_TYPE_JOB_DEBUGGER ((uint32_t)5u)

__END_CDECLS

#endif // ZIRCON_SYSCALLS_EXCEPTION_H_
