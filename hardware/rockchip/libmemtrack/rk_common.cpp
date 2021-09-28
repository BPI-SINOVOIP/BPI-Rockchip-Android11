/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2015 Andreas Schneider <asn@cryptomilk.org>
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

#define LOG_TAG "rkmemtrack"
#define LOG_NDEBUG 1

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mman.h>

#include <log/log.h>

#include <hardware/memtrack.h>

#include "memtrack.h"
#include <stdbool.h>
#include <stdlib.h>



#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

int gl_memtrack_get_memory(pid_t pid __unused,
                             int type  __unused,
                             struct memtrack_record *records __unused,
                             size_t *num_records __unused)
{
    return -EINVAL;
}

int egl_memtrack_get_memory(pid_t pid __unused,
                            int type __unused,
                            struct memtrack_record *records __unused,
                            size_t *num_records __unused)
{
    //TBD
    return  -EINVAL;
}


