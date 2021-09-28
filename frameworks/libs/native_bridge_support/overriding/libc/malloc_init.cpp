/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <private/bionic_config.h>
#include <private/bionic_globals.h>

#include <stdio.h>

#include <async_safe/log.h>

#if !defined(LIBC_STATIC)
extern "C" void* native_bridge_calloc(size_t, size_t);
extern "C" void native_bridge_free(void*);
extern "C" struct mallinfo native_bridge_mallinfo();
extern "C" void* native_bridge_malloc(size_t);
extern "C" size_t native_bridge_malloc_usable_size(const void*);
extern "C" void* native_bridge_memalign(size_t, size_t);
extern "C" int native_bridge_posix_memalign(void**, size_t, size_t);
extern "C" void* native_bridge_realloc(void*, size_t);
extern "C" int native_bridge_malloc_iterate(uintptr_t, size_t, void (*)(uintptr_t, size_t, void*), void*);
extern "C" void native_bridge_malloc_disable();
extern "C" void native_bridge_malloc_enable();
extern "C" int native_bridge_mallopt(int, int);
extern "C" void* native_bridge_aligned_alloc(size_t, size_t);

#if defined(HAVE_DEPRECATED_MALLOC_FUNCS)
extern "C" void* native_bridge_pvalloc(size_t);
extern "C" void* native_bridge_valloc(size_t);
#endif

extern "C" int native_bridge_malloc_info_helper(int options, int fd);

static int native_bridge_malloc_info(int options, FILE* fp) {
  // FILE objects cannot cross architecture boundary!
  // HACK: extract underlying file descriptor and use it instead.
  // TODO(b/146494184): at the moment malloc_info succeeds but writes nothing to memory streams!
  fflush(fp);
  int fd = fileno(fp);
  if (fd == -1) {
    return 0;
  }

  return native_bridge_malloc_info_helper(options, fd);
}

static void malloc_init_impl(libc_globals* globals) {
  static const MallocDispatch malloc_default_dispatch __attribute__((unused)) = {
    native_bridge_calloc,
    native_bridge_free,
    native_bridge_mallinfo,
    native_bridge_malloc,
    native_bridge_malloc_usable_size,
    native_bridge_memalign,
    native_bridge_posix_memalign,
#if defined(HAVE_DEPRECATED_MALLOC_FUNCS)
    native_bridge_pvalloc,
#endif
    native_bridge_realloc,
#if defined(HAVE_DEPRECATED_MALLOC_FUNCS)
    native_bridge_valloc,
#endif
    native_bridge_malloc_iterate,
    native_bridge_malloc_disable,
    native_bridge_malloc_enable,
    native_bridge_mallopt,
    native_bridge_aligned_alloc,
    native_bridge_malloc_info,
  };
  globals->malloc_dispatch_table = malloc_default_dispatch;
  globals->current_dispatch_table = &globals->malloc_dispatch_table;
}

// Initializes memory allocation framework.
// This routine is called from __libc_init routines in libc_init_dynamic.cpp.
__LIBC_HIDDEN__ void __libc_init_malloc(libc_globals* globals) {
  malloc_init_impl(globals);
}
#endif  // !LIBC_STATIC
