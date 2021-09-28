// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSCALLS_DEBUG_
#define ZIRCON_SYSCALLS_DEBUG_

#include <stdint.h>
#include <zircon/compiler.h>

__BEGIN_CDECLS

#if defined(__x86_64__)

// Value for ZX_THREAD_STATE_GENERAL_REGS on x86-64 platforms.
typedef struct zx_thread_state_general_regs {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t rflags;
} zx_thread_state_general_regs_t;

// Value for ZX_THREAD_STATE_FP_REGS on x64. Holds x87 and MMX state.
typedef struct zx_thread_state_fp_regs {
    uint16_t fcw; // Control word.
    uint16_t fsw; // Status word.
    uint8_t ftw;  // Tag word.
    uint8_t reserved;
    uint16_t fop; // Opcode.
    uint64_t fip; // Instruction pointer.
    uint64_t fdp; // Data pointer.

    // The x87/MMX state. For x87 the each "st" entry has the low 80 bits used for the register
    // contents. For MMX, the low 64 bits are used. The higher bits are unused.
    __ALIGNED(16)
    struct {
        uint64_t low;
        uint64_t high;
    } st[8];
} zx_thread_state_fp_regs_t;

// Value for ZX_THREAD_STATE_VECTOR_REGS on x64. Holds SSE and AVX registers.
//
// Setting vector registers will only work for threads that have previously executed an
// instruction using the corresponding register class.
typedef struct zx_thread_state_vector_regs {
    // When only 16 registers are supported (pre-AVX-512), zmm[16-31] will be 0.
    // YMM registers (256 bits) are v[0-4], XMM registers (128 bits) are v[0-2].
    struct {
        uint64_t v[8];
    } zmm[32];

    // AVX-512 opmask registers. Will be 0 unless AVX-512 is supported.
    uint64_t opmask[8];

    // SIMD control and status register.
    uint32_t mxcsr;
} zx_thread_state_vector_regs_t;

// Value for ZX_THREAD_STATE_DEBUG_REGS on x64 platforms.
typedef struct zx_thread_state_debug_regs {
  uint64_t dr[4];
  // DR4 and D5 are not used.
  uint64_t dr6;         // Status register.
  uint64_t dr7;         // Control register.
  // TODO(donosoc): These values are deprecated but are still used by zxdb. We debine both values
  //                in order to do a soft transition. Delete these values once zxdb has made the
  //                update.
  uint64_t dr6_status;  // Status register.
  uint64_t dr7_control; // Control register.
} zx_thread_state_debug_regs_t;

#elif defined(__aarch64__)

// Value for ZX_THREAD_STATE_GENERAL_REGS on ARM64 platforms.
typedef struct zx_thread_state_general_regs {
    uint64_t r[30];
    uint64_t lr;
    uint64_t sp;
    uint64_t pc;
    uint64_t cpsr;
} zx_thread_state_general_regs_t;

// Value for ZX_THREAD_STATE_FP_REGS on ARM64 platforms.
// This is unused because vector state is used for all floating point on ARM64.
typedef struct zx_thread_state_fp_regs {
    // Avoids sizing differences for empty structs between C and C++.
    uint32_t unused;
} zx_thread_state_fp_regs_t;

// Value for ZX_THREAD_STATE_VECTOR_REGS on ARM64 platforms.
typedef struct zx_thread_state_vector_regs {
    uint32_t fpcr;
    uint32_t fpsr;
    struct {
        uint64_t low;
        uint64_t high;
    } v[32];
} zx_thread_state_vector_regs_t;

// ARMv8-A provides 2 to 16 hardware breakpoint registers.
// The number is obtained by the BRPs field in the EDDFR register.
#define AARCH64_MAX_HW_BREAKPOINTS 16
// ARMv8-A provides 2 to 16 watchpoint breakpoint registers.
// The number is obtained by the WRPs field in the EDDFR register.
#define AARCH64_MAX_HW_WATCHPOINTS 16

// Value for XZ_THREAD_STATE_DEBUG_REGS for ARM64 platforms.
typedef struct zx_thread_state_debug_regs {
  struct {
    uint64_t dbgbvr;      //  HW Breakpoint Value register.
    uint32_t dbgbcr;      //  HW Breakpoint Control register.
  } hw_bps[AARCH64_MAX_HW_BREAKPOINTS];
  // Number of HW Breakpoints in the platform.
  // Will be set on read and ignored on write.
  uint8_t hw_bps_count;
  struct {
    uint64_t dbgwvr;      // HW Watchpoint Value register.
    uint32_t dbgwcr;      // HW Watchpoint Control register.
  } hw_wps[AARCH64_MAX_HW_WATCHPOINTS];
  // Number of HW Watchpoints in the platform.
  // Will be set on read and ignored on write.
  uint8_t hw_wps_count;
} zx_thread_state_debug_regs_t;

#endif

// Value for ZX_THREAD_STATE_SINGLE_STEP. The value can be 0 (not single-stepping), or 1
// (single-stepping). Other values will give ZX_ERR_INVALID_ARGS.
typedef uint32_t zx_thread_state_single_step_t;

// Values for ZX_THREAD_X86_REGISTER_FS and ZX_THREAD_X86_REGISTER_GS;
typedef uint64_t zx_thread_x86_register_fs_t;
typedef uint64_t zx_thread_x86_register_gs_t;

// Possible values for "kind" in zx_thread_read_state and zx_thread_write_state.
typedef uint32_t zx_thread_state_topic_t;
#define ZX_THREAD_STATE_GENERAL_REGS  ((uint32_t)0) // zx_thread_state_general_regs_t value.
#define ZX_THREAD_STATE_FP_REGS       ((uint32_t)1) // zx_thread_state_fp_regs_t value.
#define ZX_THREAD_STATE_VECTOR_REGS   ((uint32_t)2) // zx_thread_state_vector_regs_t value.
#define ZX_THREAD_STATE_DEBUG_REGS    ((uint32_t)4) // zx_thread_state_debug_regs_t value.
#define ZX_THREAD_STATE_SINGLE_STEP   ((uint32_t)5) // zx_thread_state_single_step_t value.
#define ZX_THREAD_X86_REGISTER_FS     ((uint32_t)6) // zx_thread_x86_register_fs_t value.
#define ZX_THREAD_X86_REGISTER_GS     ((uint32_t)7) // zx_thread_x86_register_gs_t value.

__END_CDECLS

#endif // ZIRCON_SYSCALLS_DEBUG_
