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
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "memutils.h"

void exit_handler(void) {
    size_t page_size = getpagesize();
    for (int i = 0; i < s_mem_map_index; i++) {
        if (NULL != s_mem_map[i].start_ptr) {
            ENABLE_MEM_ACCESS(s_mem_map[i].start_ptr,
                              (s_mem_map[i].num_pages * page_size));
        }
    }
#ifdef CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (NULL != s_free_list[i].start_ptr) {
            ENABLE_MEM_ACCESS(s_free_list[i].start_ptr,
                    (s_free_list[i].num_pages * page_size));
            real_free(s_free_list[i].start_ptr);
            memset(&s_free_list[i], 0, sizeof(map_struct_t));
        }
    }
#endif /* CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE */
}

void sigsegv_handler(int signum, siginfo_t *info, void* context) {
    exit_handler();
    (*old_sa.sa_sigaction)(signum, info, context);
}

void sighandler_init(void) {
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = SA_SIGINFO;
    new_sa.sa_sigaction = sigsegv_handler;
    sigaction(SIGSEGV, &new_sa, &old_sa);
}

void memutils_init(void) {
    real_memalign = dlsym(RTLD_NEXT, "memalign");
    if (NULL == real_memalign) {
        return;
    }
#ifndef DISABLE_MALLOC_OVERLOADING
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    if (NULL == real_calloc) {
        return;
    }
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        return;
    }
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    if (NULL == real_realloc) {
        return;
    }
#endif /* DISABLE_MALLOC_OVERLOADING */
    real_free = dlsym(RTLD_NEXT, "free");
    if (NULL == real_free) {
        return;
    }
    memset(&s_mem_map, 0, MAX_ENTRIES * sizeof(map_struct_t));
    sighandler_init();
    atexit(exit_handler);
    s_memutils_initialized = 1;
}

void *memalign(size_t alignment, size_t size) {
    if (s_memutils_initialized == 0) {
        memutils_init();
    }
#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_MEMALIGN_CHECK) != ENABLE_MEMALIGN_CHECK) {
        return real_memalign(alignment, size);
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */
    char* start_ptr;
    char* mem_ptr;
    size_t total_size;
    size_t aligned_size = size;
    size_t num_pages;
    size_t page_size = getpagesize();

    if (s_mem_map_index == MAX_ENTRIES) {
        return real_memalign(alignment, size);
    }

    if (alignment > page_size) {
        return real_memalign(alignment, size);
    }

    if ((0 == page_size) || (0 == alignment) || (0 == size)) {
        return real_memalign(alignment, size);
    }
#ifdef CHECK_OVERFLOW
    /* User specified alignment is not respected and is overridden by
     * MINIMUM_ALIGNMENT. This is required to catch OOB read when read offset
     * is less than user specified alignment. "MINIMUM_ALIGNMENT" helps to
     * avoid bus errors due to non-aligned memory.                         */
    if (0 != (size % MINIMUM_ALIGNMENT)) {
        aligned_size = size + (MINIMUM_ALIGNMENT - (size % MINIMUM_ALIGNMENT));
    }
#endif

    if (0 != (aligned_size % page_size)) {
        num_pages = (aligned_size / page_size) + 2;
    } else {
        num_pages = (aligned_size / page_size) + 1;
    }

    total_size = (num_pages * page_size);
    start_ptr = (char *) real_memalign(page_size, total_size);
#ifdef CHECK_OVERFLOW
    mem_ptr = (char *) start_ptr + ((num_pages - 1) * page_size) - aligned_size;
    DISABLE_MEM_ACCESS((start_ptr + ((num_pages - 1) * page_size)), page_size);
#endif /* CHECK_OVERFLOW */
#ifdef CHECK_UNDERFLOW
    mem_ptr = (char *) start_ptr + page_size;
    DISABLE_MEM_ACCESS(start_ptr, page_size);
#endif /* CHECK_UNDERFLOW */
    s_mem_map[s_mem_map_index].start_ptr = start_ptr;
    s_mem_map[s_mem_map_index].mem_ptr = mem_ptr;
    s_mem_map[s_mem_map_index].num_pages = num_pages;
    s_mem_map[s_mem_map_index].mem_size = size;
    s_mem_map_index++;
    memset(mem_ptr, INITIAL_VAL, size);
    return mem_ptr;
}

#ifndef DISABLE_MALLOC_OVERLOADING
void *malloc(size_t size) {
    if (s_memutils_initialized == 0) {
        memutils_init();
    }
#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_MALLOC_CHECK) != ENABLE_MALLOC_CHECK) {
        return real_malloc(size);
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */
    return memalign(MINIMUM_ALIGNMENT, size);
}

void *calloc(size_t nitems, size_t size) {
    if (s_memutils_initialized == 0) {
        memutils_init();
    }
#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_CALLOC_CHECK) != ENABLE_CALLOC_CHECK) {
        return real_calloc(nitems, size);
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */
    void *ptr = memalign(sizeof(size_t), (nitems * size));
    if (ptr)
        memset(ptr, 0, (nitems * size));
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (s_memutils_initialized == 0) {
        memutils_init();
    }
#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_REALLOC_CHECK) != ENABLE_REALLOC_CHECK) {
        return real_realloc(ptr, size);
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */
    if (ptr != NULL) {
        int i = 0;
        for (i = 0; i < s_mem_map_index; i++) {
            if (ptr == s_mem_map[i].mem_ptr) {
                void* temp = malloc(size);
                if (temp == NULL) {
                    return NULL;
                }
                if (s_mem_map[i].mem_size > size) {
                    memcpy(temp, ptr, size);
                } else {
                    memcpy(temp, ptr, s_mem_map[i].mem_size);
                }
                free(s_mem_map[i].mem_ptr);
                return temp;
            }
        }
    }
    return real_realloc(ptr, size);
}
#endif /* DISABLE_MALLOC_OVERLOADING */

void free(void *ptr) {
    if (s_memutils_initialized == 0) {
        memutils_init();
    }
#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_FREE_CHECK) != ENABLE_FREE_CHECK) {
        return real_free(ptr);
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */
    if (ptr != NULL) {
        int i = 0;
        size_t page_size = getpagesize();
        for (i = 0; i < s_mem_map_index; i++) {
            if (ptr == s_mem_map[i].mem_ptr) {
#ifdef CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE
                s_free_list[s_free_write_index].start_ptr =
                s_mem_map[i].start_ptr;
                s_free_list[s_free_write_index].mem_ptr = s_mem_map[i].mem_ptr;
                s_free_list[s_free_write_index].num_pages =
                s_mem_map[i].num_pages;
                s_free_list[s_free_write_index].mem_size = s_mem_map[i].mem_size;
                s_free_write_index++;
                s_free_list_size += s_mem_map[i].mem_size;
                DISABLE_MEM_ACCESS(s_mem_map[i].start_ptr,
                        (s_mem_map[i].num_pages * page_size));
                memset(&s_mem_map[i], 0, sizeof(map_struct_t));
                while (s_free_list_size > CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE) {
                    ENABLE_MEM_ACCESS(
                            s_free_list[s_free_read_index].start_ptr,
                            (s_free_list[s_free_read_index].num_pages * page_size));
                    real_free(s_free_list[s_free_read_index].start_ptr);
                    s_free_list_size -= s_free_list[s_free_read_index].mem_size;
                    memset(&s_free_list[s_free_read_index], 0,
                            sizeof(map_struct_t));
                    s_free_read_index++;
                    if ((s_free_read_index == MAX_ENTRIES)
                            || (s_free_read_index >= s_free_write_index)) {
                        break;
                    }
                }
                return;
#else
                ENABLE_MEM_ACCESS(s_mem_map[i].start_ptr,
                                  (s_mem_map[i].num_pages * page_size));
                real_free(s_mem_map[i].start_ptr);
                memset(&s_mem_map[i], 0, sizeof(map_struct_t));
                return;
#endif /* CHECK_USE_AFTER_FREE_WITH_WINDOW_SIZE */
            }
        }
    }
    real_free(ptr);
    return;
}
