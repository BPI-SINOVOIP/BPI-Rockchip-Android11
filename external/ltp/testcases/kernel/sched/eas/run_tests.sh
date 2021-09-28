#!/bin/sh

cd /data/nativetest64/ltp/testcases/bin

./eas_one_small_task
./eas_one_big_task
./eas_small_to_big
./eas_big_to_small
./eas_small_big_toggle
./eas_two_big_three_small

./sched_cfs_prio
./sched_dl_runtime
./sched_latency_dl
./sched_latency_rt
./sched_prio_3_fifo
./sched_prio_3_rr

./sugov_latency
./sugov_wakeups
./sched_boost
