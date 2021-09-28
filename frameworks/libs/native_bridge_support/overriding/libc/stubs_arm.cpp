//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// clang-format off
#include "native_bridge_support/vdso/interceptable_functions.h"

DEFINE_INTERCEPTABLE_STUB_FUNCTION(__clone_for_fork);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__get_thread_stack_top);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__pthread_cleanup_pop);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__pthread_cleanup_push);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_properties_init);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_add);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_area_init);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_area_serial);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_find);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_find_nth);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_foreach);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_get);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_read);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_read_callback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_serial);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_set);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_set_filename);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_update);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_wait);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(__system_property_wait_any);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_longjmp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_setjmp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(android_getaddrinfofornet);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(android_getaddrinfofornetcontext);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(android_mallopt);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(android_set_abort_message);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(freeaddrinfo);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(gai_strerror);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(getaddrinfo);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(longjmp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge___cxa_thread_atexit_impl);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_aligned_alloc);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_calloc);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_exit);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_free);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_mallinfo);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_malloc);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_malloc_disable);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_malloc_enable);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_malloc_info_helper);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_malloc_iterate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_malloc_usable_size);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_mallopt);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_memalign);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_posix_memalign);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_pvalloc);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_realloc);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(native_bridge_valloc);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_destroy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getdetachstate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getguardsize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getinheritsched);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getschedparam);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getschedpolicy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getscope);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getstack);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_getstacksize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_init);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setdetachstate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setguardsize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setinheritsched);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setschedparam);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setschedpolicy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setscope);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setstack);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_attr_setstacksize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_detach);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_exit);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_getattr_np);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_getcpuclockid);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_getname_np);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_getschedparam);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_getspecific);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_gettid_np);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_join);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_key_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_key_delete);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_kill);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_setname_np);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_setschedparam);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_setschedprio);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_setspecific);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(pthread_sigqueue);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(setjmp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(siglongjmp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(sigsetjmp);
DEFINE_INTERCEPTABLE_STUB_VARIABLE(environ);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __clone_for_fork);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __get_thread_stack_top);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __pthread_cleanup_pop);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __pthread_cleanup_push);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_properties_init);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_add);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_area_init);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_area_serial);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_find);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_find_nth);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_foreach);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_get);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_read);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_read_callback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_serial);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_set);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_set_filename);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_update);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_wait);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", __system_property_wait_any);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", _longjmp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", _setjmp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", android_getaddrinfofornet);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", android_getaddrinfofornetcontext);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", android_mallopt);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", android_set_abort_message);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", freeaddrinfo);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", gai_strerror);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", getaddrinfo);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", longjmp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge___cxa_thread_atexit_impl);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_aligned_alloc);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_calloc);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_exit);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_free);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_mallinfo);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_malloc);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_malloc_disable);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_malloc_enable);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_malloc_info_helper);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_malloc_iterate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_malloc_usable_size);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_mallopt);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_memalign);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_posix_memalign);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_pvalloc);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_realloc);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", native_bridge_valloc);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_destroy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getdetachstate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getguardsize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getinheritsched);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getschedparam);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getschedpolicy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getscope);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getstack);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_getstacksize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_init);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setdetachstate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setguardsize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setinheritsched);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setschedparam);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setschedpolicy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setscope);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setstack);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_attr_setstacksize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_detach);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_exit);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_getattr_np);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_getcpuclockid);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_getname_np);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_getschedparam);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_getspecific);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_gettid_np);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_join);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_key_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_key_delete);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_kill);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_setname_np);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_setschedparam);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_setschedprio);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_setspecific);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", pthread_sigqueue);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", setjmp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", siglongjmp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libc.so", sigsetjmp);
  INIT_INTERCEPTABLE_STUB_VARIABLE("libc.so", environ);
}
// clang-format on
