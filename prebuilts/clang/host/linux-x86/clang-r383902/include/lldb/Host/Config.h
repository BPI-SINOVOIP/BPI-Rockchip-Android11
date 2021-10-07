//===-- Config.h -----------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_HOST_CONFIG_H
#define LLDB_HOST_CONFIG_H

#define LLDB_EDITLINE_USE_WCHAR 1

#define LLDB_HAVE_EL_RFUNC_T 1


#define HAVE_SYS_TYPES_H 1

#define HAVE_SYS_EVENT_H 0

#define HAVE_PPOLL 1

#define HAVE_SIGACTION 1

#define HAVE_PROCESS_VM_READV 1

#define HAVE_NR_PROCESS_VM_READV 1

#ifndef HAVE_LIBCOMPRESSION
/* #undef HAVE_LIBCOMPRESSION */
#endif

#define LLDB_ENABLE_POSIX 1

#define LLDB_ENABLE_TERMIOS 1

#define LLDB_ENABLE_LZMA 0

#define LLDB_ENABLE_CURSES 1

#define LLDB_ENABLE_LIBEDIT 1

#define LLDB_ENABLE_LIBXML2 0

#define LLDB_ENABLE_LUA 0

#define LLDB_ENABLE_PYTHON 1

#define LLDB_EMBED_PYTHON_HOME 1

#define LLDB_PYTHON_HOME "../python3"

#define LLDB_LIBDIR_SUFFIX "64"

#endif // #ifndef LLDB_HOST_CONFIG_H
