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

/* This file is separate because it's included both by eBPF programs (via include
 * in bpf_helpers.h) and directly by the boot time bpfloader (Loader.cpp).
 */

#include <linux/bpf.h>

// Pull in AID_* constants from //system/core/libcutils/include/private/android_filesystem_config.h
#define EXCLUDE_FS_CONFIG_STRUCTURES
#include <private/android_filesystem_config.h>

/*
 * Map structure to be used by Android eBPF C programs. The Android eBPF loader
 * uses this structure from eBPF object to create maps at boot time.
 *
 * The eBPF C program should define structure in the maps section using
 * SEC("maps") otherwise it will be ignored by the eBPF loader.
 *
 * For example:
 *   const struct bpf_map_def SEC("maps") mymap { .type=... , .key_size=... }
 *
 * See 'bpf_helpers.h' for helpful macros for eBPF program use.
 */
struct bpf_map_def {
    enum bpf_map_type type;
    unsigned int key_size;
    unsigned int value_size;
    unsigned int max_entries;
    unsigned int map_flags;

    // The following are not supported by the Android bpfloader:
    //   unsigned int inner_map_idx;
    //   unsigned int numa_node;

    unsigned int uid;   // uid_t
    unsigned int gid;   // gid_t
    unsigned int mode;  // mode_t
};

struct bpf_prog_def {
    unsigned int uid;
    unsigned int gid;

    unsigned int min_kver;  // KERNEL_MAJOR * 65536 + KERNEL_MINOR * 256 + KERNEL_SUB
    unsigned int max_kver;  // ie. 0x40900 for Linux 4.9 - but beware of hexadecimal for >= 10

    bool optional;  // program section (ie. function) may fail to load, continue onto next func. 
};
