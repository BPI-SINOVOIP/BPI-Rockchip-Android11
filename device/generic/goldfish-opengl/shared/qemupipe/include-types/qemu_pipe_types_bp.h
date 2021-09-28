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

#pragma once
#include <stddef.h>
#include <stdbool.h>

#ifdef HOST_BUILD

typedef void* QEMU_PIPE_HANDLE;
#define QEMU_PIPE_INVALID_HANDLE NULL

inline bool qemu_pipe_valid(QEMU_PIPE_HANDLE h) {
    return h != QEMU_PIPE_INVALID_HANDLE;
}

#else  // ifdef HOST_BUILD

typedef int QEMU_PIPE_HANDLE;
#define QEMU_PIPE_INVALID_HANDLE (-1)

inline bool qemu_pipe_valid(QEMU_PIPE_HANDLE h) {
    return h > QEMU_PIPE_INVALID_HANDLE;
}

#endif // ifdef HOST_BUILD
