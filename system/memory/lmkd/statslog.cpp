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

#include <assert.h>
#include <errno.h>
#include <log/log.h>
#include <log/log_id.h>
#include <statslog.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>

#ifdef LMKD_LOG_STATS

#define LINE_MAX 128
#define STRINGIFY(x) STRINGIFY_INTERNAL(x)
#define STRINGIFY_INTERNAL(x) #x

static bool enable_stats_log = property_get_bool("ro.lmk.log_stats", false);

struct proc {
    int pid;
    char taskname[LINE_MAX];
    struct proc* pidhash_next;
};

#define PIDHASH_SZ 1024
static struct proc** pidhash = NULL;
#define pid_hashfn(x) ((((x) >> 8) ^ (x)) & (PIDHASH_SZ - 1))

/**
 * Logs the change in LMKD state which is used as start/stop boundaries for logging
 * LMK_KILL_OCCURRED event.
 * Code: LMK_STATE_CHANGED = 54
 */
int
stats_write_lmk_state_changed(int32_t state) {
    if (enable_stats_log) {
        return android::lmkd::stats::stats_write(android::lmkd::stats::LMK_STATE_CHANGED, state);
    } else {
        return -EINVAL;
    }
}

static struct proc* pid_lookup(int pid) {
    struct proc* procp;

    if (!pidhash) return NULL;

    for (procp = pidhash[pid_hashfn(pid)]; procp && procp->pid != pid; procp = procp->pidhash_next)
        ;

    return procp;
}

inline int32_t map_kill_reason(enum kill_reasons reason) {
    switch (reason) {
        case PRESSURE_AFTER_KILL:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__PRESSURE_AFTER_KILL;
        case NOT_RESPONDING:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__NOT_RESPONDING;
        case LOW_SWAP_AND_THRASHING:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__LOW_SWAP_AND_THRASHING;
        case LOW_MEM_AND_SWAP:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__LOW_MEM_AND_SWAP;
        case LOW_MEM_AND_THRASHING:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__LOW_MEM_AND_THRASHING;
        case DIRECT_RECL_AND_THRASHING:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__DIRECT_RECL_AND_THRASHING;
        case LOW_MEM_AND_SWAP_UTIL:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__LOW_MEM_AND_SWAP_UTIL;
        default:
            return android::lmkd::stats::LMK_KILL_OCCURRED__REASON__UNKNOWN;
    }
}

/**
 * Logs the event when LMKD kills a process to reduce memory pressure.
 * Code: LMK_KILL_OCCURRED = 51
 */
int stats_write_lmk_kill_occurred(struct kill_stat *kill_st, struct memory_stat *mem_st) {
    if (enable_stats_log) {
        return android::lmkd::stats::stats_write(
                android::lmkd::stats::LMK_KILL_OCCURRED,
                kill_st->uid,
                kill_st->taskname,
                kill_st->oom_score,
                mem_st ? mem_st->pgfault : -1,
                mem_st ? mem_st->pgmajfault : -1,
                mem_st ? mem_st->rss_in_bytes : kill_st->tasksize * BYTES_IN_KILOBYTE,
                mem_st ? mem_st->cache_in_bytes : -1,
                mem_st ? mem_st->swap_in_bytes : -1,
                mem_st ? mem_st->process_start_time_ns : -1,
                kill_st->min_oom_score,
                kill_st->free_mem_kb,
                kill_st->free_swap_kb,
                map_kill_reason(kill_st->kill_reason)
        );
    } else {
        return -EINVAL;
    }
}

int stats_write_lmk_kill_occurred_pid(int pid, struct kill_stat *kill_st,
                                      struct memory_stat* mem_st) {
    struct proc* proc = pid_lookup(pid);
    if (!proc) return -EINVAL;

    kill_st->taskname = proc->taskname;
    return stats_write_lmk_kill_occurred(kill_st, mem_st);
}

static void memory_stat_parse_line(char* line, struct memory_stat* mem_st) {
    char key[LINE_MAX + 1];
    int64_t value;

    sscanf(line, "%" STRINGIFY(LINE_MAX) "s  %" SCNd64 "", key, &value);

    if (strcmp(key, "total_") < 0) {
        return;
    }

    if (!strcmp(key, "total_pgfault"))
        mem_st->pgfault = value;
    else if (!strcmp(key, "total_pgmajfault"))
        mem_st->pgmajfault = value;
    else if (!strcmp(key, "total_rss"))
        mem_st->rss_in_bytes = value;
    else if (!strcmp(key, "total_cache"))
        mem_st->cache_in_bytes = value;
    else if (!strcmp(key, "total_swap"))
        mem_st->swap_in_bytes = value;
}

static int memory_stat_from_cgroup(struct memory_stat* mem_st, int pid, uid_t uid) {
    FILE *fp;
    char buf[PATH_MAX];

    snprintf(buf, sizeof(buf), MEMCG_PROCESS_MEMORY_STAT_PATH, uid, pid);

    fp = fopen(buf, "r");

    if (fp == NULL) {
        return -1;
    }

    while (fgets(buf, PAGE_SIZE, fp) != NULL) {
        memory_stat_parse_line(buf, mem_st);
    }
    fclose(fp);

    return 0;
}

static int memory_stat_from_procfs(struct memory_stat* mem_st, int pid) {
    char path[PATH_MAX];
    char buffer[PROC_STAT_BUFFER_SIZE];
    int fd, ret;

    snprintf(path, sizeof(path), PROC_STAT_FILE_PATH, pid);
    if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) {
        return -1;
    }

    ret = read(fd, buffer, sizeof(buffer));
    if (ret < 0) {
        close(fd);
        return -1;
    }
    close(fd);

    // field 10 is pgfault
    // field 12 is pgmajfault
    // field 22 is starttime
    // field 24 is rss_in_pages
    int64_t pgfault = 0, pgmajfault = 0, starttime = 0, rss_in_pages = 0;
    if (sscanf(buffer,
               "%*u %*s %*s %*d %*d %*d %*d %*d %*d %" SCNd64 " %*d "
               "%" SCNd64 " %*d %*u %*u %*d %*d %*d %*d %*d %*d "
               "%" SCNd64 " %*d %" SCNd64 "",
               &pgfault, &pgmajfault, &starttime, &rss_in_pages) != 4) {
        return -1;
    }
    mem_st->pgfault = pgfault;
    mem_st->pgmajfault = pgmajfault;
    mem_st->rss_in_bytes = (rss_in_pages * PAGE_SIZE);
    mem_st->process_start_time_ns = starttime * (NS_PER_SEC / sysconf(_SC_CLK_TCK));

    return 0;
}

struct memory_stat *stats_read_memory_stat(bool per_app_memcg, int pid, uid_t uid) {
    static struct memory_stat mem_st = {};

    if (!enable_stats_log) {
        return NULL;
    }

    if (per_app_memcg) {
        if (memory_stat_from_cgroup(&mem_st, pid, uid) == 0) {
            return &mem_st;
        }
    } else {
        if (memory_stat_from_procfs(&mem_st, pid) == 0) {
            return &mem_st;
        }
    }

    return NULL;
}

static void proc_insert(struct proc* procp) {
    if (!pidhash) {
        pidhash = static_cast<struct proc**>(calloc(PIDHASH_SZ, sizeof(*pidhash)));
    }

    int hval = pid_hashfn(procp->pid);
    procp->pidhash_next = pidhash[hval];
    pidhash[hval] = procp;
}

void stats_remove_taskname(int pid) {
    if (!enable_stats_log || !pidhash) {
        return;
    }

    int hval = pid_hashfn(pid);
    struct proc* procp;
    struct proc* prevp;

    for (procp = pidhash[hval], prevp = NULL; procp && procp->pid != pid;
         procp = procp->pidhash_next)
        prevp = procp;

    if (!procp)
        return;

    if (!prevp)
        pidhash[hval] = procp->pidhash_next;
    else
        prevp->pidhash_next = procp->pidhash_next;

    free(procp);
}

void stats_store_taskname(int pid, const char* taskname) {
    if (!enable_stats_log || !taskname) {
        return;
    }

    struct proc* procp = pid_lookup(pid);
    if (procp != NULL) {
        if (strcmp(procp->taskname, taskname) == 0) {
            return;
        }
        stats_remove_taskname(pid);
    }
    procp = static_cast<struct proc*>(malloc(sizeof(struct proc)));
    procp->pid = pid;
    strncpy(procp->taskname, taskname, LINE_MAX - 1);
    procp->taskname[LINE_MAX - 1] = '\0';
    proc_insert(procp);
}

void stats_purge_tasknames() {
    if (!enable_stats_log || !pidhash) {
        return;
    }

    struct proc* procp;
    struct proc* next;
    int i;
    for (i = 0; i < PIDHASH_SZ; i++) {
        procp = pidhash[i];
        while (procp) {
            next = procp->pidhash_next;
            free(procp);
            procp = next;
        }
    }
    memset(pidhash, 0, PIDHASH_SZ * sizeof(*pidhash));
}

#endif /* LMKD_LOG_STATS */
