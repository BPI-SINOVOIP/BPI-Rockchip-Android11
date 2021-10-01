//===-- Config.h -----------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_HOST_CONFIG_H
#define LLDB_HOST_CONFIG_H

#define LLDB_CONFIG_TERMIOS_SUPPORTED

#define LLDB_EDITLINE_USE_WCHAR 1

#define LLDB_HAVE_EL_RFUNC_T 0

/* #undef LLDB_DISABLE_POSIX */

#define LLDB_LIBDIR_SUFFIX "64"

#define HAVE_SYS_TYPES_H 1

#define HAVE_SYS_EVENT_H 1

#define HAVE_PPOLL 0

#define HAVE_SIGACTION 1

#define HAVE_PROCESS_VM_READV 0

#define HAVE_NR_PROCESS_VM_READV 0

#ifndef HAVE_LIBCOMPRESSION
#define HAVE_LIBCOMPRESSION
#endif

#endif // #ifndef LLDB_HOST_CONFIG_H
