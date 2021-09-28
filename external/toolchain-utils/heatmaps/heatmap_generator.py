# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Python module to draw heat map for Chrome

heat map is a histogram used to analyze the locality of function layout.

This module is used by heat_map.py. HeatmapGenerator is a class to
generate data for drawing heat maps (the actual drawing of heat maps is
performed by another script perf-to-inst-page.sh). It can also analyze
the symbol names in hot pages.
"""

from __future__ import division, print_function

import bisect
import collections
import os
import pipes
import subprocess

from cros_utils import command_executer

HugepageRange = collections.namedtuple('HugepageRange', ['start', 'end'])


class MMap(object):
  """Class to store mmap information in perf report.

  We assume ASLR is disabled, so MMap for all Chrome is assumed to be
  the same. This class deals with the case hugepage creates several
  mmaps for Chrome but should be merged together. In these case, we
  assume the first MMAP is not affected by the bug and use the MMAP.
  """

  def __init__(self, addr, size, offset):
    self.start_address = addr
    self.size = size
    self.offset = offset

  def __str__(self):
    return '(%x, %x, %x)' % (self.start_address, self.size, self.offset)

  def merge(self, mmap):
    # This function should not be needed, since we should only have
    # one MMAP on Chrome of each process. This function only deals with
    # images that is affected by http://crbug.com/931465.

    # This function is only checking a few conditions to make sure
    # the bug is within our expectation.

    if self.start_address == mmap.start_address:
      assert self.size >= mmap.size, \
        'Original MMAP size(%x) is smaller than the forked process(%x).' % (
            self.size, mmap.size)
      # The case that the MMAP is forked from the previous process
      # No need to do anything, OR
      # The case where hugepage causes a small Chrome mmap.
      # In this case, we use the prior MMAP for the whole Chrome
      return

    assert self.start_address < mmap.start_address, \
      'Original MMAP starting address(%x) is larger than the forked' \
      'process(%x).' % (self.start_address, mmap.start_address)

    assert self.start_address + self.size >= mmap.start_address + mmap.size, \
      'MMAP of the forked process exceeds the end of original MMAP.'


class HeatmapGenerator(object):
  """Class to generate heat map with a perf report, containing mmaps and

  samples. This class contains two interfaces with other modules:
  draw() and analyze().

  draw() draws a heatmap with the sample information given in the perf report
  analyze() prints out the symbol names in hottest pages with the given
  chrome binary
  """

  def __init__(self,
               perf_report,
               page_size,
               hugepage,
               title,
               log_level='verbose'):
    self.perf_report = perf_report
    # Pick 1G as a relatively large number. All addresses less than it will
    # be recorded. The actual heatmap will show up to a boundary of the
    # largest address in text segment.
    self.max_addr = 1024 * 1024 * 1024
    self.ce = command_executer.GetCommandExecuter(log_level=log_level)
    self.dir = os.path.dirname(os.path.realpath(__file__))
    with open(perf_report, 'r', encoding='utf-8') as f:
      self.perf_report_contents = f.readlines()
    # Write histogram results to a text file, in order to use gnu plot to draw
    self.hist_temp_output = open('out.txt', 'w', encoding='utf-8')
    self.processes = {}
    self.deleted_processes = {}
    self.count = 0
    if hugepage:
      self.hugepage = HugepageRange(start=hugepage[0], end=hugepage[1])
    else:
      self.hugepage = None
    self.title = title
    self.symbol_addresses = []
    self.symbol_names = []
    self.page_size = page_size

  def _parse_perf_sample(self, line):
    # In a perf report, generated with -D, a PERF_RECORD_SAMPLE command should
    # look like this: TODO: some arguments are unknown
    #
    # cpuid cycle unknown [unknown]: PERF_RECORD_SAMPLE(IP, 0x2): pid/tid:
    # 0xaddr period: period addr: addr
    # ... thread: threadname:tid
    # ...... dso: process
    #
    # This is an example:
    # 1 136712833349 0x6a558 [0x30]: PERF_RECORD_SAMPLE(IP, 0x2): 5227/5227:
    # 0x55555683b810 period: 372151 addr: 0
    # ... thread: chrome:5227
    # ...... dso: /opt/google/chrome/chrome
    #
    # For this function, the 7th argument (args[6]) after spltting with spaces
    # is pid/tid. We use the combination of the two as the pid.
    # Also, we add an assertion here to check the tid in the 7th argument(
    # args[6]) and the 15th argument(arg[14]) are the same
    #
    # The function returns the ((pid,tid), address) pair if the sampling
    # is on Chrome. Otherwise, return (None, None) pair.

    if 'thread: chrome' not in line or \
    'dso: /opt/google/chrome/chrome' not in line:
      return None, None
    args = line.split(' ')
    pid_raw = args[6].split('/')
    assert pid_raw[1][:-1] == args[14].split(':')[1][:-1], \
    'TID in %s of sample is not the same: %s/%s' % (
        line[:-1], pid_raw[1][:-1], args[14].split(':')[1][:-1])
    key = (int(pid_raw[0]), int(pid_raw[1][:-1]))
    address = int(args[7], base=16)
    return key, address

  def _parse_perf_record(self, line):
    # In a perf report, generated with -D, a PERF_RECORD_MMAP2 command should
    # look like this: TODO: some arguments are unknown
    #
    # cpuid cycle unknown [unknown]: PERF_RECORD_MMAP2 pid/tid:
    # [0xaddr(0xlength) @ pageoffset maj:min ino ino_generation]:
    # permission process
    #
    # This is an example.
    # 2 136690556823 0xa6898 [0x80]: PERF_RECORD_MMAP2 5227/5227:
    # [0x555556496000(0x8d1b000) @ 0xf42000 b3:03 92844 1892514370]:
    # r-xp /opt/google/chrome/chrome
    #
    # For this function, the 6th argument (args[5]) after spltting with spaces
    # is pid/tid. We use the combination of the two as the pid.
    # The 7th argument (args[6]) is the [0xaddr(0xlength). We can peel the
    # string to get the address and size of the mmap.
    # The 9th argument (args[8]) is the page offset.
    # The function returns the ((pid,tid), mmap) pair if the mmap is for Chrome
    # is on Chrome. Otherwise, return (None, None) pair.

    if 'chrome/chrome' not in line:
      return None, None
    args = line.split(' ')
    pid_raw = args[5].split('/')
    assert pid_raw[0] == pid_raw[1][:-1], \
    'PID in %s of mmap is not the same: %s/%s' % (
        line[:-1], pid_raw[0], pid_raw[1])
    pid = (int(pid_raw[0]), int(pid_raw[1][:-1]))
    address_raw = args[6].split('(')
    start_address = int(address_raw[0][1:], base=16)
    size = int(address_raw[1][:-1], base=16)
    offset = int(args[8], base=16)
    # Return an mmap object instead of only starting address,
    # in case there are many mmaps for the sample PID
    return pid, MMap(start_address, size, offset)

  def _parse_pair_event(self, arg):
    # This function is called by the _parse_* functions that has a pattern of
    # pids like: (pid:tid):(pid:tid), i.e.
    # PERF_RECORD_FORK and PERF_RECORD_COMM
    _, remain = arg.split('(', 1)
    pid1, remain = remain.split(':', 1)
    pid2, remain = remain.split(')', 1)
    _, remain = remain.split('(', 1)
    pid3, remain = remain.split(':', 1)
    pid4, remain = remain.split(')', 1)
    return (int(pid1), int(pid2)), (int(pid3), int(pid4))

  def _process_perf_record(self, line):
    # This function calls _parse_perf_record() to get information from
    # PERF_RECORD_MMAP2. It records the mmap object for each pid (a pair of
    # pid,tid), into a dictionary.
    pid, mmap = self._parse_perf_record(line)
    if pid is None:
      # PID = None meaning the mmap is not for chrome
      return
    if pid in self.processes:
      # This should never happen for a correct profiling result, as we
      # should only have one MMAP for Chrome for each process.
      # If it happens, see http://crbug.com/931465
      self.processes[pid].merge(mmap)
    else:
      self.processes[pid] = mmap

  def _process_perf_fork(self, line):
    # In a perf report, generated with -D, a PERF_RECORD_FORK command should
    # look like this:
    #
    # cpuid cycle unknown [unknown]:
    # PERF_RECORD_FORK(pid_to:tid_to):(pid_from:tid_from)
    #
    # This is an example.
    # 0 0 0x22a8 [0x38]: PERF_RECORD_FORK(1:1):(0:0)
    #
    # In this function, we need to peel the information of pid:tid pairs
    # So we get the last argument and send it to function _parse_pair_event()
    # for analysis.
    # We use (pid, tid) as the pid.
    args = line.split(' ')
    pid_to, pid_from = self._parse_pair_event(args[-1])
    if pid_from in self.processes:
      assert pid_to not in self.processes
      self.processes[pid_to] = MMap(self.processes[pid_from].start_address,
                                    self.processes[pid_from].size,
                                    self.processes[pid_from].offset)

  def _process_perf_exit(self, line):
    # In a perf report, generated with -D, a PERF_RECORD_EXIT command should
    # look like this:
    #
    # cpuid cycle unknown [unknown]:
    # PERF_RECORD_EXIT(pid1:tid1):(pid2:tid2)
    #
    # This is an example.
    # 1 136082505621 0x30810 [0x38]: PERF_RECORD_EXIT(3851:3851):(3851:3851)
    #
    # In this function, we need to peel the information of pid:tid pairs
    # So we get the last argument and send it to function _parse_pair_event()
    # for analysis.
    # We use (pid, tid) as the pid.
    args = line.split(' ')
    pid_to, pid_from = self._parse_pair_event(args[-1])
    assert pid_to == pid_from, '(%d, %d) (%d, %d)' % (pid_to[0], pid_to[1],
                                                      pid_from[0], pid_from[1])
    if pid_to in self.processes:
      # Don't delete the process yet
      self.deleted_processes[pid_from] = self.processes[pid_from]

  def _process_perf_sample(self, line):
    # This function calls _parse_perf_sample() to get information from
    # the perf report.
    # It needs to check the starting address of allocated mmap from
    # the dictionary (self.processes) to calculate the offset within
    # the text section of the sampling.
    # The offset is calculated into pages (4KB or 2MB) and writes into
    # out.txt together with the total counts, which will be used to
    # calculate histogram.
    pid, addr = self._parse_perf_sample(line)
    if pid is None:
      return

    assert pid in self.processes and pid not in self.deleted_processes, \
    'PID %d not found mmap and not forked from another process'

    start_address = self.processes[pid].start_address
    address = addr - start_address
    assert address >= 0 and \
    'addresses accessed in PERF_RECORD_SAMPLE should be larger than' \
    ' the starting address of Chrome'
    if address < self.max_addr:
      self.count += 1
      line = '%d/%d: %d %d' % (pid[0], pid[1], self.count,
                               address // self.page_size * self.page_size)
      if self.hugepage:
        if self.hugepage.start <= address < self.hugepage.end:
          line += ' hugepage'
        else:
          line += ' smallpage'
      print(line, file=self.hist_temp_output)

  def _read_perf_report(self):
    # Serve as main function to read perf report, generated by -D
    lines = iter(self.perf_report_contents)
    for line in lines:
      if 'PERF_RECORD_MMAP' in line:
        self._process_perf_record(line)
      elif 'PERF_RECORD_FORK' in line:
        self._process_perf_fork(line)
      elif 'PERF_RECORD_EXIT' in line:
        self._process_perf_exit(line)
      elif 'PERF_RECORD_SAMPLE' in line:
        # Perf sample is multi-line
        self._process_perf_sample(line + next(lines) + next(lines))
    self.hist_temp_output.close()

  def _draw_heat_map(self):
    # Calls a script (perf-to-inst-page.sh) to calculate histogram
    # of results written in out.txt and also generate pngs for
    # heat maps.
    heatmap_script = os.path.join(self.dir, 'perf-to-inst-page.sh')
    if self.hugepage:
      hp_arg = 'hugepage'
    else:
      hp_arg = 'none'

    cmd = '{0} {1} {2}'.format(heatmap_script, pipes.quote(self.title), hp_arg)
    retval = self.ce.RunCommand(cmd)
    if retval:
      raise RuntimeError('Failed to run script to generate heatmap')

  def _restore_histogram(self):
    # When hugepage is used, there are two files inst-histo-{hp,sp}.txt
    # So we need to read in all the files.
    names = [x for x in os.listdir('.') if 'inst-histo' in x and '.txt' in x]
    hist = {}
    for n in names:
      with open(n, encoding='utf-8') as f:
        for l in f.readlines():
          num, addr = l.strip().split(' ')
          assert int(addr) not in hist
          hist[int(addr)] = int(num)
    return hist

  def _read_symbols_from_binary(self, binary):
    # FIXME: We are using nm to read symbol names from Chrome binary
    # for now. Can we get perf to hand us symbol names, instead of
    # using nm in the future?
    #
    # Get all the symbols (and their starting addresses) that fall into
    # the page. Will be used to print out information of hot pages
    # Each line shows the information of a symbol:
    # [symbol value (0xaddr)] [symbol type] [symbol name]
    # For some symbols, the [symbol name] field might be missing.
    # e.g.
    # 0000000001129da0 t Builtins_LdaNamedPropertyHandler

    # Generate a list of symbols from nm tool and check each line
    # to extract symbols names
    text_section_start = 0
    for l in subprocess.check_output(['nm', '-n', binary]).split('\n'):
      args = l.strip().split(' ')
      if len(args) < 3:
        # No name field
        continue
      addr_raw, symbol_type, name = args
      addr = int(addr_raw, base=16)
      if 't' not in symbol_type and 'T' not in symbol_type:
        # Filter out symbols not in text sections
        continue
      if not self.symbol_addresses:
        # The first symbol in text sections
        text_section_start = addr
        self.symbol_addresses.append(0)
        self.symbol_names.append(name)
      else:
        assert text_section_start != 0, \
        'The starting address of text section has not been found'
        if addr == self.symbol_addresses[-1]:
          # if the same address has multiple symbols, put them together
          # and separate symbol names with '/'
          self.symbol_names[-1] += '/' + name
        else:
          # The output of nm -n command is already sorted by address
          # Insert to the end will result in a sorted array for bisect
          self.symbol_addresses.append(addr - text_section_start)
          self.symbol_names.append(name)

  def _map_addr_to_symbol(self, addr):
    # Find out the symbol name
    assert self.symbol_addresses
    index = bisect.bisect(self.symbol_addresses, addr)
    assert 0 < index <= len(self.symbol_names), \
    'Failed to find an index (%d) in the list (len=%d)' % (
        index, len(self.symbol_names))
    return self.symbol_names[index - 1]

  def _print_symbols_in_hot_pages(self, fp, pages_to_show):
    # Print symbols in all the pages of interest
    for page_num, sample_num in pages_to_show:
      print(
          '----------------------------------------------------------', file=fp)
      print(
          'Page Offset: %d MB, Count: %d' % (page_num // 1024 // 1024,
                                             sample_num),
          file=fp)

      symbol_counts = collections.Counter()
      # Read Sample File and find out the occurance of symbols in the page
      lines = iter(self.perf_report_contents)
      for line in lines:
        if 'PERF_RECORD_SAMPLE' in line:
          pid, addr = self._parse_perf_sample(line + next(lines) + next(lines))
          if pid is None:
            # The sampling is not on Chrome
            continue
          if addr // self.page_size != (
              self.processes[pid].start_address + page_num) // self.page_size:
            # Sampling not in the current page
            continue

          name = self._map_addr_to_symbol(addr -
                                          self.processes[pid].start_address)
          assert name, 'Failed to find symbol name of addr %x' % addr
          symbol_counts[name] += 1

      assert sum(symbol_counts.values()) == sample_num, \
      'Symbol name matching missing for some addresses: %d vs %d' % (
          sum(symbol_counts.values()), sample_num)

      # Print out the symbol names sorted by the number of samples in
      # the page
      for name, count in sorted(
          symbol_counts.items(), key=lambda kv: kv[1], reverse=True):
        if count == 0:
          break
        print('> %s : %d' % (name, count), file=fp)
      print('\n\n', file=fp)

  def draw(self):
    # First read perf report to process information and save histogram
    # into a text file
    self._read_perf_report()
    # Then use gnu plot to draw heat map
    self._draw_heat_map()

  def analyze(self, binary, top_n):
    # Read histogram from histo.txt
    hist = self._restore_histogram()
    # Sort the pages in histogram
    sorted_hist = sorted(hist.items(), key=lambda value: value[1], reverse=True)

    # Generate symbolizations
    self._read_symbols_from_binary(binary)

    # Write hottest pages
    with open('addr2symbol.txt', 'w', encoding='utf-8') as fp:
      if self.hugepage:
        # Print hugepage region first
        print(
            'Hugepage top %d hot pages (%d MB - %d MB):' %
            (top_n, self.hugepage.start // 1024 // 1024,
             self.hugepage.end // 1024 // 1024),
            file=fp)
        pages_to_print = [(k, v)
                          for k, v in sorted_hist
                          if self.hugepage.start <= k < self.hugepage.end
                         ][:top_n]
        self._print_symbols_in_hot_pages(fp, pages_to_print)
        print('==========================================', file=fp)
        print('Top %d hot pages landed outside of hugepage:' % top_n, file=fp)
        # Then print outside pages
        pages_to_print = [(k, v)
                          for k, v in sorted_hist
                          if k < self.hugepage.start or k >= self.hugepage.end
                         ][:top_n]
        self._print_symbols_in_hot_pages(fp, pages_to_print)
      else:
        # Print top_n hottest pages.
        pages_to_print = sorted_hist[:top_n]
        self._print_symbols_in_hot_pages(fp, pages_to_print)
