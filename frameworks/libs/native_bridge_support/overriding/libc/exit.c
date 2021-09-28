/*
 * Copyright (C) 2020 The Android Open Source Project
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

extern void __cxa_finalize(void* dso_handle);
extern void native_bridge_exit(int status);

void exit(int status) {
  // We don't need to call __cxa_thread_finalize because host "exit" would do that for us.
  // __cxa_thread_finalize();
  // That's slight deviation from standard behavior: there we first call __cxa_thread_finalize()
  // for all objects and __cxa_finalize() for all objects and here we would first do
  // __cxa_finalize() for guest objects, then __cxa_thread_finalize() (and after that we would do
  // __cxa_finalize() for host objects, of course).
  // TODO(b/65052237): Fix that with bionic refactoring?
  __cxa_finalize(NULL);
  native_bridge_exit(status);
  __builtin_unreachable();
}
