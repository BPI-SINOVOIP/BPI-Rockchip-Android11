/**
 * Copyright (C) 2019 The Android Open Source Project
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
#define MAX_ENTRIES        (1024 * 1024)
#define INITIAL_VAL        (0xBE)
#define MINIMUM_ALIGNMENT  (16)

#define DISABLE_MEM_ACCESS(mem, size)\
    mprotect((char *) mem, size, PROT_NONE);

#define ENABLE_MEM_ACCESS(mem, size)\
    mprotect((char *) mem, size, PROT_READ | PROT_WRITE);

#define ENABLE_NONE               0x00
#define ENABLE_MEMALIGN_CHECK     0x01
#define ENABLE_MALLOC_CHECK       0x02
#define ENABLE_CALLOC_CHECK       0x04
#define ENABLE_REALLOC_CHECK      0x08
#define ENABLE_FREE_CHECK         0x10
#define ENABLE_ALL                ENABLE_MEMALIGN_CHECK | ENABLE_MALLOC_CHECK |\
    ENABLE_CALLOC_CHECK | ENABLE_REALLOC_CHECK | ENABLE_FREE_CHECK

typedef struct _map_struct_t {
    void *start_ptr;
    void *mem_ptr;
    int num_pages;
    size_t mem_size;
} map_struct_t;

static void* (*real_memalign)(size_t, size_t) = NULL;
#ifndef DISABLE_MALLOC_OVERLOADING
static void* (*real_calloc)(size_t, size_t) = NULL;
static void* (*real_malloc)(size_t) = NULL;
static void* (*real_realloc)(void *ptr, size_t size) = NULL;
#endif /* DISABLE_MALLOC_OVERLOADING */
static void (*real_free)(void *) = NULL;
static int s_memutils_initialized = 0;
static int s_mem_map_index = 0;
static struct sigaction new_sa, old_sa;
#ifdef ENABLE_SELECTIVE_OVERLOADING
extern char enable_selective_overload;
#endif /* ENABLE_SELECTIVE_OVERLOADING */
#ifdef CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE
static int s_free_write_index = 0;
static int s_free_read_index = 0;
static int s_free_list_size = 0;
map_struct_t s_free_list[MAX_ENTRIES];
#endif /* CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE */
map_struct_t s_mem_map[MAX_ENTRIES];
#if (!(defined CHECK_OVERFLOW) && !(defined CHECK_UNDERFLOW))
    #error "CHECK MACROS NOT DEFINED"
#endif
#ifdef __cplusplus
}
#endif /* __cplusplus */
