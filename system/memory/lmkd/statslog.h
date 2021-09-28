/*
 *  Copyright 2018 Google, Inc
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _STATSLOG_H_
#define _STATSLOG_H_

#include <assert.h>
#include <inttypes.h>
#include <statslog_lmkd.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <cutils/properties.h>

__BEGIN_DECLS

struct memory_stat {
    int64_t pgfault;
    int64_t pgmajfault;
    int64_t rss_in_bytes;
    int64_t cache_in_bytes;
    int64_t swap_in_bytes;
    int64_t process_start_time_ns;
};

// If you update this, also update the corresponding stats enum mapping.
enum kill_reasons {
    NONE = -1, /* To denote no kill condition */
    PRESSURE_AFTER_KILL = 0,
    NOT_RESPONDING,
    LOW_SWAP_AND_THRASHING,
    LOW_MEM_AND_SWAP,
    LOW_MEM_AND_THRASHING,
    DIRECT_RECL_AND_THRASHING,
    LOW_MEM_AND_SWAP_UTIL,
    KILL_REASON_COUNT
};

struct kill_stat {
    int32_t uid;
    char *taskname;
    enum kill_reasons kill_reason;
    int32_t oom_score;
    int32_t min_oom_score;
    int64_t free_mem_kb;
    int64_t free_swap_kb;
    int tasksize;
};

#ifdef LMKD_LOG_STATS

#define MEMCG_PROCESS_MEMORY_STAT_PATH "/dev/memcg/apps/uid_%u/pid_%u/memory.stat"
#define PROC_STAT_FILE_PATH "/proc/%d/stat"
#define PROC_STAT_BUFFER_SIZE 1024
#define BYTES_IN_KILOBYTE 1024

/**
 * Logs the change in LMKD state which is used as start/stop boundaries for logging
 * LMK_KILL_OCCURRED event.
 * Code: LMK_STATE_CHANGED = 54
 */
int
stats_write_lmk_state_changed(int32_t state);

/**
 * Logs the event when LMKD kills a process to reduce memory pressure.
 * Code: LMK_KILL_OCCURRED = 51
 */
int stats_write_lmk_kill_occurred(struct kill_stat *kill_st, struct memory_stat *mem_st);

/**
 * Logs the event when LMKD kills a process to reduce memory pressure.
 * Code: LMK_KILL_OCCURRED = 51
 */
int stats_write_lmk_kill_occurred_pid(int pid, struct kill_stat *kill_st,
                                      struct memory_stat* mem_st);

struct memory_stat *stats_read_memory_stat(bool per_app_memcg, int pid, uid_t uid);

/**
 * Registers a process taskname by pid, while it is still alive.
 */
void stats_store_taskname(int pid, const char* taskname);

/**
 * Unregister all process tasknames.
 */
void stats_purge_tasknames();

/**
 * Unregister a process taskname, e.g. after it has been killed.
 */
void stats_remove_taskname(int pid);

#else /* LMKD_LOG_STATS */

static inline int
stats_write_lmk_state_changed(int32_t state __unused) { return -EINVAL; }

static inline int
stats_write_lmk_kill_occurred(struct kill_stat *kill_st __unused,
                              struct memory_stat *mem_st __unused) {
    return -EINVAL;
}

int stats_write_lmk_kill_occurred_pid(int pid __unused, struct kill_stat *kill_st __unused,
                                      struct memory_stat* mem_st __unused) {
    return -EINVAL;
}

static inline struct memory_stat *stats_read_memory_stat(bool per_app_memcg __unused,
                                    int pid __unused, uid_t uid __unused) { return NULL; }

static inline void stats_store_taskname(int pid __unused, const char* taskname __unused) {}

static inline void stats_purge_tasknames() {}

static inline void stats_remove_taskname(int pid __unused) {}

#endif /* LMKD_LOG_STATS */

__END_DECLS

#endif /* _STATSLOG_H_ */
