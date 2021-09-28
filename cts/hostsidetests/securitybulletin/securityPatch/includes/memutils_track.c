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
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "memutils_track.h"

void memutils_init(void) {
    real_memalign = dlsym(RTLD_NEXT, "memalign");
    if (!real_memalign) {
        return;
    }
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (!real_malloc) {
        return;
    }
    real_free = dlsym(RTLD_NEXT, "free");
    if (!real_free) {
        return;
    }

#ifdef CHECK_MEMORY_LEAK
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    if (!real_calloc) {
        return;
    }
    atexit(exit_vulnerable_if_memory_leak_detected);
#endif /* CHECK_MEMORY_LEAK */

    s_memutils_initialized = true;
}

void *memalign(size_t alignment, size_t size) {
    if (!s_memutils_initialized) {
        memutils_init();
    }
    void* mem_ptr = real_memalign(alignment, size);

#ifdef CHECK_UNINITIALIZED_MEMORY
    if(mem_ptr) {
        memset(mem_ptr, INITIAL_VAL, size);
    }
#endif /* CHECK_UNINITIALIZED_MEMORY */

#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_MEMALIGN_CHECK) != ENABLE_MEMALIGN_CHECK) {
        return mem_ptr;
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */

    if (!is_tracking_required(size)) {
        return mem_ptr;
    }
    if (s_allocation_index >= MAX_ENTRIES) {
        return mem_ptr;
    }
    s_allocation_list[s_allocation_index].mem_ptr = mem_ptr;
    s_allocation_list[s_allocation_index].mem_size = size;
    ++s_allocation_index;
    return mem_ptr;
}

void *malloc(size_t size) {
    if (!s_memutils_initialized) {
        memutils_init();
    }
    void* mem_ptr = real_malloc(size);

#ifdef CHECK_UNINITIALIZED_MEMORY
    if(mem_ptr) {
        memset(mem_ptr, INITIAL_VAL, size);
    }
#endif /* CHECK_UNINITIALIZED_MEMORY */

#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_MALLOC_CHECK) != ENABLE_MALLOC_CHECK) {
        return mem_ptr;
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */

    if (!is_tracking_required(size)) {
        return mem_ptr;
    }
    if (s_allocation_index >= MAX_ENTRIES) {
        return mem_ptr;
    }
    s_allocation_list[s_allocation_index].mem_ptr = mem_ptr;
    s_allocation_list[s_allocation_index].mem_size = size;
    ++s_allocation_index;
    return mem_ptr;
}

void free(void *ptr) {
    if (!s_memutils_initialized) {
        memutils_init();
    }
    if (ptr) {
        for (int i = 0; i < s_allocation_index; ++i) {
            if (ptr == s_allocation_list[i].mem_ptr) {
                real_free(ptr);
                memset(&s_allocation_list[i], 0,
                       sizeof(allocated_memory_struct));
                return;
            }
        }
    }
    return real_free(ptr);
}

#ifdef CHECK_MEMORY_LEAK
void *calloc(size_t nitems, size_t size) {
    if (!s_memutils_initialized) {
        memutils_init();
    }
    void* mem_ptr = real_calloc(nitems, size);

#ifdef ENABLE_SELECTIVE_OVERLOADING
    if ((enable_selective_overload & ENABLE_CALLOC_CHECK) != ENABLE_CALLOC_CHECK) {
        return mem_ptr;
    }
#endif /* ENABLE_SELECTIVE_OVERLOADING */

    if (!is_tracking_required((nitems *size))) {
        return mem_ptr;
    }
    if (s_allocation_index >= MAX_ENTRIES) {
        return mem_ptr;
    }
    s_allocation_list[s_allocation_index].mem_ptr = mem_ptr;
    s_allocation_list[s_allocation_index].mem_size = nitems * size;
    ++s_allocation_index;
    return mem_ptr;
}

void exit_vulnerable_if_memory_leak_detected(void) {
    bool memory_leak_detected = false;
    for (int i = 0; i < s_allocation_index; ++i) {
        if (s_allocation_list[i].mem_ptr) {
            real_free(s_allocation_list[i].mem_ptr);
            memset(&s_allocation_list[i], 0,
                    sizeof(allocated_memory_struct));
            memory_leak_detected = true;
        }
    }
    if(memory_leak_detected) {
        exit(EXIT_VULNERABLE);
    }
    return;
}
#endif /* CHECK_MEMORY_LEAK */

#ifdef CHECK_UNINITIALIZED_MEMORY
bool is_memory_uninitialized() {
    for (int i = 0; i < s_allocation_index; ++i) {
        uint8_t *mem_ptr = s_allocation_list[i].mem_ptr;
        size_t mem_size = s_allocation_list[i].mem_size;
        if (mem_ptr) {

#ifdef CHECK_FOUR_BYTES
            if(mem_size > (2 * sizeof(uint32_t))) {
                uint8_t *mem_ptr_start = (uint8_t *) s_allocation_list[i].mem_ptr;
                uint8_t *mem_ptr_end = (uint8_t *) s_allocation_list[i].mem_ptr + mem_size - 1;
                for (size_t j = 0; j < sizeof(uint32_t); ++j) {
                    if (*mem_ptr_start++ == INITIAL_VAL) {
                        return true;
                    }
                    if (*mem_ptr_end-- == INITIAL_VAL) {
                        return true;
                    }
                }
                continue;
            }
#endif /* CHECK_FOUR_BYTES */

            for (size_t j = 0; j < mem_size; ++j) {
                if (*mem_ptr++ == INITIAL_VAL) {
                    return true;
                }
            }
        }
    }
    return false;
}

#endif /* CHECK_UNINITIALIZED_MEMORY */
