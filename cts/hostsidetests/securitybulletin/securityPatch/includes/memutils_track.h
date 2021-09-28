/**
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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#define MAX_ENTRIES               (32 * 1024)
#define INITIAL_VAL               0xBE

#define ENABLE_NONE               0x00
#define ENABLE_MEMALIGN_CHECK     0x01
#define ENABLE_MALLOC_CHECK       0x02
#define ENABLE_CALLOC_CHECK       0x04
#define ENABLE_ALL                ENABLE_MEMALIGN_CHECK | ENABLE_MALLOC_CHECK |\
    ENABLE_CALLOC_CHECK

typedef struct {
    void *mem_ptr;
    size_t mem_size;
} allocated_memory_struct;

static bool s_memutils_initialized = false;
static int s_allocation_index = 0;
static allocated_memory_struct s_allocation_list[MAX_ENTRIES] = { { 0, 0 } };

extern bool is_tracking_required(size_t size);
static void* (*real_memalign)(size_t, size_t) = NULL;
static void* (*real_malloc)(size_t) = NULL;
static void (*real_free)(void *) = NULL;

#ifdef ENABLE_SELECTIVE_OVERLOADING
extern char enable_selective_overload;
#endif /* ENABLE_SELECTIVE_OVERLOADING */

#ifdef CHECK_MEMORY_LEAK
static void* (*real_calloc)(size_t, size_t) = NULL;
void exit_vulnerable_if_memory_leak_detected(void);
#endif

#ifdef CHECK_UNINITIALIZED_MEMORY
extern bool is_memory_uninitialized();
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
