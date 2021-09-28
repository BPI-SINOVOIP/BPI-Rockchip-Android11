#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import logging
import re
import threading

from autotest_lib.client.bin import utils

class SystemSampler():
    """A sampler class used to probe various system stat along with FPS.

    Sample usage:
        def get_consumer_pid():
            # Returns all memory consumers whose memory usage we care about.

        sampler = SystemSampler(get_consumer_pid)
        fps = ...
        sampler.sample(fps)
    """
    Sample = collections.namedtuple(
        'Sample', ['pswpin', 'pswpout', 'free_mem', 'buff_mem', 'cached_mem',
                   'anon_mem', 'file_mem', 'swap_free', 'swap_used',
                   'consumer_num', 'consumer_rss', 'consumer_swap',
                   'cpuload', 'fps'])

    VMStat = collections.namedtuple('VMStat', ['pswpin', 'pswpout'])

    def __init__(self, get_consumer_pid_func):
        self._get_consumer_pid_func = get_consumer_pid_func
        self._samples_lock = threading.Lock()
        self._samples = []
        self.reset()

    def reset(self):
        """Resets its internal stats."""
        self._prev_vmstat = self.get_vmstat()
        self._prev_cpustat = utils.get_cpu_usage()

    def get_samples(self):
        with self._samples_lock:
            return self._samples

    def get_last_avg_fps(self, num):
        """Gets average fps rate of last |num| samples.

        Returns None if there's not enough samples in hand.
        """
        if num <= 0:
            logging.warning('Num of samples must be > 0')
            return

        with self._samples_lock:
            if len(self._samples) >= num:
                return sum([s.fps for s in self._samples[-num:]])/float(num)

        logging.warning('Not enough samples')
        return None

    def sample(self, fps_info):
        """Gets a fps sample with system state."""
        vmstat = self.get_vmstat()
        vmstat_diff = self.VMStat(*[(end - start)
            for start, end in zip(self._prev_vmstat, vmstat)])
        self._prev_vmstat = vmstat

        cpustat = utils.get_cpu_usage()
        cpuload = utils.compute_active_cpu_time(
            self._prev_cpustat, cpustat)
        self._prev_cpustat = cpustat

        mem_info_in_kb = utils.get_meminfo()
        # Converts mem_info from KB to MB.
        mem_info = collections.namedtuple('MemInfo', mem_info_in_kb._fields)(
            *[v/float(1024) for v in mem_info_in_kb])

        consumer_pids = self._get_consumer_pid_func()
        logging.info('Consumers %s', consumer_pids)
        consumer_rss, consumer_swap = self.get_consumer_meminfo(consumer_pids)

        # fps_info = (frame_info, frame_times)
        fps_count = len([f for f in fps_info[0] if f != ' '])

        sample = self.Sample(
            pswpin=vmstat_diff.pswpin,
            pswpout=vmstat_diff.pswpout,
            free_mem=mem_info.MemFree,
            buff_mem=mem_info.Buffers,
            cached_mem=mem_info.Cached,
            anon_mem=mem_info.Active_anon + mem_info.Inactive_anon,
            file_mem=mem_info.Active_file + mem_info.Inactive_file,
            swap_free=mem_info.SwapFree,
            swap_used=mem_info.SwapTotal - mem_info.SwapFree,
            consumer_num=len(consumer_pids),
            consumer_rss=consumer_rss,
            consumer_swap=consumer_swap,
            cpuload=cpuload,
            fps=fps_count)

        logging.info(sample)

        with self._samples_lock:
            self._samples.append(sample)

    @staticmethod
    def parse_meminfo_from_proc_entry(pid):
        """Parses memory related info in /proc/<pid>/totmaps like:

        Rss:              144956 kB
        Pss:               74923 kB
        Shared_Clean:      50596 kB
        Shared_Dirty:      41660 kB
        Private_Clean:      1032 kB
        Private_Dirty:     51668 kB
        Referenced:       137424 kB
        Anonymous:         91772 kB
        AnonHugePages:     30720 kB
        Swap:                  0 kB
        """
        mem_info = {}
        line_pattern = re.compile(r'^(\w+):\s+(\d+)\s+kB')
        proc_entry = '/proc/%s/totmaps' % pid
        try:
            with open(proc_entry) as f:
                for line in f:
                    m = line_pattern.match(line)
                    if m:
                        key, value = m.groups()
                        mem_info[key] = float(value)/1024
        except IOError as e:
            logging.warning('Failed to open %s: %s', proc_entry, e)
        return mem_info

    @classmethod
    def get_consumer_meminfo(cls, pids):
        rss = 0.0
        swap = 0.0
        for pid in pids:
            mem_info = cls.parse_meminfo_from_proc_entry(pid)
            rss += mem_info.get('Rss', 0)
            swap += mem_info.get('Swap', 0)
        return rss, swap

    @classmethod
    def get_vmstat(cls):
        with open('/proc/vmstat') as f:
            lines = f.readlines()
        all_fields = dict([l.strip().split(' ') for l in lines])
        return cls.VMStat(
            *[int(all_fields.get(f, 0)) for f in cls.VMStat._fields])
