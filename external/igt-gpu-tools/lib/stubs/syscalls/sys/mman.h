/* SPDX-License-Identifier: MIT */

#pragma once

#include_next <sys/mman.h>

#if !defined(HAVE_MEMFD_CREATE) || !HAVE_MEMFD_CREATE
#include <errno.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __NR_memfd_create
#if defined __x86_64__
#define __NR_memfd_create 319
#elif defined __i386__
#define __NR_memfd_create 356
#elif defined __arm__
#define __NR_memfd_create 385
#else
#warning "__NR_memfd_create unknown for your architecture"
#endif
#endif

static inline int missing_memfd_create(const char *name, unsigned int flags)
{
#ifdef __NR_memfd_create
	return syscall(__NR_memfd_create, name, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}

#define memfd_create missing_memfd_create

#endif
