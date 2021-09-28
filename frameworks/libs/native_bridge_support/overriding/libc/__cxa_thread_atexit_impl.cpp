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

#include <unistd.h>

extern "C" void __loader_add_thread_local_dtor(void* dso_handle) __attribute__((weak));
extern "C" void __loader_remove_thread_local_dtor(void* dso_handle) __attribute__((weak));

extern "C" int native_bridge___cxa_thread_atexit_impl(void (*func)(void*),
                                                      void* arg,
                                                      void* dso_handle);

struct WrappedArg {
  typedef void (*thread_atexit_fn_t)(void*);
  thread_atexit_fn_t fn;
  void* arg;
  void* dso_handle;
};

static void WrappedFn(void* arg) {
  WrappedArg* wrapped_arg = static_cast<WrappedArg*>(arg);
  WrappedArg::thread_atexit_fn_t origin_fn = wrapped_arg->fn;
  void* origin_arg = wrapped_arg->arg;
  void* dso_handle = wrapped_arg->dso_handle;

  delete wrapped_arg;

  origin_fn(origin_arg);

  if (__loader_remove_thread_local_dtor != nullptr) {
    __loader_remove_thread_local_dtor(dso_handle);
  }
}

extern "C" int __cxa_thread_atexit_impl(void (*func)(void*), void* arg, void* dso_handle) {
  WrappedArg* wrapped_arg = new WrappedArg();
  wrapped_arg->fn = func;
  wrapped_arg->arg = arg;
  wrapped_arg->dso_handle = dso_handle;

  if (__loader_add_thread_local_dtor != nullptr) {
    __loader_add_thread_local_dtor(dso_handle);
  }

  return native_bridge___cxa_thread_atexit_impl(WrappedFn, wrapped_arg, dso_handle);
}
