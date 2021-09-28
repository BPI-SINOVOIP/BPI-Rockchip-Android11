/*
 * time_in_state eBPF program
 *
 * Copyright (C) 2018 Google
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <bpf_helpers.h>
#include <bpf_timeinstate.h>

DEFINE_BPF_MAP_GRW(uid_time_in_state_map, PERCPU_HASH, time_key_t, tis_val_t, 1024, AID_SYSTEM)

DEFINE_BPF_MAP_GRW(uid_concurrent_times_map, PERCPU_HASH, time_key_t, concurrent_val_t, 1024, AID_SYSTEM)
DEFINE_BPF_MAP_GRW(uid_last_update_map, HASH, uint32_t, uint64_t, 1024, AID_SYSTEM)

DEFINE_BPF_MAP_GWO(cpu_last_update_map, PERCPU_ARRAY, uint32_t, uint64_t, 1, AID_SYSTEM)

DEFINE_BPF_MAP_GWO(cpu_policy_map, ARRAY, uint32_t, uint32_t, 1024, AID_SYSTEM)
DEFINE_BPF_MAP_GWO(policy_freq_idx_map, ARRAY, uint32_t, uint8_t, 1024, AID_SYSTEM)

DEFINE_BPF_MAP_GWO(freq_to_idx_map, HASH, freq_idx_key_t, uint8_t, 2048, AID_SYSTEM)

DEFINE_BPF_MAP_GWO(nr_active_map, ARRAY, uint32_t, uint32_t, 1, AID_SYSTEM)
DEFINE_BPF_MAP_GWO(policy_nr_active_map, ARRAY, uint32_t, uint32_t, 1024, AID_SYSTEM)

struct switch_args {
    unsigned long long ignore;
    char prev_comm[16];
    int prev_pid;
    int prev_prio;
    long long prev_state;
    char next_comm[16];
    int next_pid;
    int next_prio;
};

DEFINE_BPF_PROG("tracepoint/sched/sched_switch", AID_ROOT, AID_SYSTEM, tp_sched_switch)
(struct switch_args* args) {
    const int ALLOW = 1;  // return 1 to avoid blocking simpleperf from receiving events.
    uint32_t zero = 0;
    uint64_t* last = bpf_cpu_last_update_map_lookup_elem(&zero);
    if (!last) return ALLOW;
    uint64_t old_last = *last;
    uint64_t time = bpf_ktime_get_ns();
    *last = time;

    uint32_t* active = bpf_nr_active_map_lookup_elem(&zero);
    if (!active) return ALLOW;

    uint32_t cpu = bpf_get_smp_processor_id();
    uint32_t* policyp = bpf_cpu_policy_map_lookup_elem(&cpu);
    if (!policyp) return ALLOW;
    uint32_t policy = *policyp;

    uint32_t* policy_active = bpf_policy_nr_active_map_lookup_elem(&policy);
    if (!policy_active) return ALLOW;

    uint32_t nactive = *active - 1;
    uint32_t policy_nactive = *policy_active - 1;

    if (!args->prev_pid || (!old_last && args->next_pid)) {
        __sync_fetch_and_add(active, 1);
        __sync_fetch_and_add(policy_active, 1);
    }

    // Return here in 2 scenarios:
    // 1) prev_pid == 0, so we're exiting idle. No UID stats need updating, and active CPUs can't be
    //    decreasing.
    // 2) old_last == 0, so this is the first time we've seen this CPU. Any delta will be invalid,
    //    and our active CPU counts don't include this CPU yet so we shouldn't decrement them even
    //    if we're going idle.
    if (!args->prev_pid || !old_last) return ALLOW;

    if (!args->next_pid) {
        __sync_fetch_and_add(active, -1);
        __sync_fetch_and_add(policy_active, -1);
    }

    uint8_t* freq_idxp = bpf_policy_freq_idx_map_lookup_elem(&policy);
    if (!freq_idxp || !*freq_idxp) return ALLOW;
    // freq_to_idx_map uses 1 as its minimum index so that *freq_idxp == 0 only when uninitialized
    uint8_t freq_idx = *freq_idxp - 1;

    uint32_t uid = bpf_get_current_uid_gid();
    time_key_t key = {.uid = uid, .bucket = freq_idx / FREQS_PER_ENTRY};
    tis_val_t* val = bpf_uid_time_in_state_map_lookup_elem(&key);
    if (!val) {
        tis_val_t zero_val = {.ar = {0}};
        bpf_uid_time_in_state_map_update_elem(&key, &zero_val, BPF_NOEXIST);
        val = bpf_uid_time_in_state_map_lookup_elem(&key);
    }
    uint64_t delta = time - old_last;
    if (val) val->ar[freq_idx % FREQS_PER_ENTRY] += delta;

    key.bucket = nactive / CPUS_PER_ENTRY;
    concurrent_val_t* ct = bpf_uid_concurrent_times_map_lookup_elem(&key);
    if (!ct) {
        concurrent_val_t zero_val = {.active = {0}, .policy = {0}};
        bpf_uid_concurrent_times_map_update_elem(&key, &zero_val, BPF_NOEXIST);
        ct = bpf_uid_concurrent_times_map_lookup_elem(&key);
    }
    if (ct) ct->active[nactive % CPUS_PER_ENTRY] += delta;

    if (policy_nactive / CPUS_PER_ENTRY != key.bucket) {
        key.bucket = policy_nactive / CPUS_PER_ENTRY;
        ct = bpf_uid_concurrent_times_map_lookup_elem(&key);
        if (!ct) {
            concurrent_val_t zero_val = {.active = {0}, .policy = {0}};
            bpf_uid_concurrent_times_map_update_elem(&key, &zero_val, BPF_NOEXIST);
            ct = bpf_uid_concurrent_times_map_lookup_elem(&key);
        }
    }
    if (ct) ct->policy[policy_nactive % CPUS_PER_ENTRY] += delta;
    uint64_t* uid_last_update = bpf_uid_last_update_map_lookup_elem(&uid);
    if (uid_last_update) {
        *uid_last_update = time;
    } else {
        bpf_uid_last_update_map_update_elem(&uid, &time, BPF_NOEXIST);
    }
    return ALLOW;
}

struct cpufreq_args {
    unsigned long long ignore;
    unsigned int state;
    unsigned int cpu_id;
};

DEFINE_BPF_PROG("tracepoint/power/cpu_frequency", AID_ROOT, AID_SYSTEM, tp_cpufreq)
(struct cpufreq_args* args) {
    uint32_t cpu = args->cpu_id;
    unsigned int new = args->state;
    uint32_t* policyp = bpf_cpu_policy_map_lookup_elem(&cpu);
    if (!policyp) return 0;
    uint32_t policy = *policyp;
    freq_idx_key_t key = {.policy = policy, .freq = new};
    uint8_t* idxp = bpf_freq_to_idx_map_lookup_elem(&key);
    if (!idxp) return 0;
    uint8_t idx = *idxp;
    bpf_policy_freq_idx_map_update_elem(&policy, &idx, BPF_ANY);
    return 0;
}

LICENSE("GPL");
