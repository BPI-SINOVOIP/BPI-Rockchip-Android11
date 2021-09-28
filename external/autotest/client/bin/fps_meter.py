#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks kernel tracing events to get the FPS of a CrOS device.

This script requires root privilege to work properly. It may interfere
Chrome tracing because both use ftrace.

Limitation:
It doesn't support multi-screen.
It assumes 60 HZ screen refresh rate.

This script can be used both as a stand alone script or a library.

Sample output (when used as a stand alone script):
    # ./fps_meter.py
    trace method: workq
    [111111111111111111111111111111111111111111111111111111111111] FPS: 60
    [111111111111111111111111111111111111111111111111111111111111] FPS: 60
    [11111111111111111111111111111111111111111111111 111111111111] FPS: 59
    [111111111111111111111111111111111111111111111111111111111111] FPS: 60
    [11111111111111 11111111111111111111 11111111 111111111111111] FPS: 57
    [111111111111111111111111111111111    11111111111111111111111] FPS: 56
    [111   111111111111111111111111111111111111111111111111111111] FPS: 57
     ^
         1 : Frame update count detected in this 1/60 sec interval.

Sample Usage (when used as a library).
    def callback(fps):
        ...

    with FPSMeter(callback) as fps_meter:
        ...

When used as a library, it launches two threads to monitor system FPS rate
periodically. Each time when a FPS rate is sampled, callback() is called with
the FPS number as its parameter.
"""

from __future__ import print_function

import argparse
import atexit
import collections
import common
import functools
import logging
import os
import re
import sys
import threading
import time

from autotest_lib.client.bin import utils as bin_utils
from autotest_lib.client.common_lib import utils as common_lib_utils

TRACE_ROOT = '/sys/kernel/debug/tracing/'
VBLANK_SWITCH = os.path.join(TRACE_ROOT, 'events/drm/drm_vblank_event/enable')
FLIP_SWITCH = os.path.join(TRACE_ROOT, 'events/i915/i915_flip_complete/enable')
WORKQ_SWITCH = os.path.join(
        TRACE_ROOT, 'events/workqueue/workqueue_execute_start/enable')
WORKQ_FILTER = os.path.join(
        TRACE_ROOT, 'events/workqueue/workqueue_execute_start/filter')
TRACE_SWITCH = os.path.join(TRACE_ROOT, 'tracing_on')
TRACE_CLOCK = os.path.join(TRACE_ROOT, 'trace_clock')
TRACE_LOG = os.path.join(TRACE_ROOT, 'trace')
TRACE_PIPE = os.path.join(TRACE_ROOT, 'trace_pipe')
TRACE_MARKER = os.path.join(TRACE_ROOT, 'trace_marker')
REFRESH_RATE = 60
NOTIFY_STRING = 'notify_collection'
STOP_STRING = 'stop_tracing'


def is_intel_gpu():
    """Determines whether it is intel GPU."""
    return os.path.isdir('/sys/bus/pci/drivers/i915')


def get_kernel_version():
    """ Retruns the kernel version in form of xx.xx. """
    m = re.match(r'(\d+\.\d+)\.\d+', bin_utils.get_kernel_version())
    if m:
        return m.group(1)
    return 'unknown'


def get_trace_method():
    """Gets the FPS checking method.

    Checks i915_flip_complete for Intel GPU on Kernel 3.18.
    Checks intel_atomic_commit_work for Intel GPU on Kernel 4.4.
    Checks drm_vblank_event otherwise.
    """
    if is_intel_gpu():
        kernel_version = get_kernel_version()
        if kernel_version == '4.4':
            return 'workq'
        elif kernel_version == '3.18':
            return 'flip'
    # Fallback.
    return 'vblank'


def set_simple_switch(value, filename):
    """ Sets simple switch '1' to the file. """
    orig = common_lib_utils.read_file(filename).strip()
    atexit.register(common_lib_utils.open_write_close, filename, orig)
    common_lib_utils.open_write_close(filename, value)


def set_trace_clock():
    """ Sets trace clock to mono time as chrome tracing in CrOS. """
    PREFERRED_TRACE_CLOCK = 'mono'
    clock = common_lib_utils.read_file(TRACE_CLOCK)
    m = re.match(r'.*\[(\w+)\]', clock)
    if m:
        orig_clock = m.group(1)
        atexit.register(common_lib_utils.open_write_close,
                        TRACE_CLOCK, orig_clock)
    common_lib_utils.open_write_close(TRACE_CLOCK, PREFERRED_TRACE_CLOCK)


def get_kernel_symbol_addr(symbol):
    """ Gets the kernel symple address. Example line in kallsyms:
     ffffffffbc46cb03 T sys_exit
    """
    with open('/proc/kallsyms') as kallsyms:
        for line in kallsyms:
            items = line.split()
            if items[2] == symbol:
                addr = items[0]
                return addr
    return None


def set_workq_filter(function_name):
    """ Sets the workq filter. """
    addr = get_kernel_symbol_addr(function_name)
    if addr:
        filter = 'function == 0x%s' % addr
        common_lib_utils.open_write_close(WORKQ_FILTER, filter)
        # Sets to 0 to remove the filter.
        atexit.register(common_lib_utils.open_write_close, WORKQ_FILTER, '0')


def enable_tracing(trace_method):
    """Enables tracing."""
    if trace_method == 'workq':
        set_simple_switch('1', WORKQ_SWITCH)
        # There are many workqueue_execute_start events,
        # filter to reduce CPU usage.
        set_workq_filter('intel_atomic_commit_work')
    elif trace_method == 'flip':
        set_simple_switch('1', FLIP_SWITCH)
    else:
        set_simple_switch('1', VBLANK_SWITCH)

    set_simple_switch('1', TRACE_SWITCH)
    set_trace_clock()


def get_fps_info(trace_buffer, end_time):
    """ Checks all vblanks in the range [end_time - 1.0, end_time]. """
    frame_info = []
    step = 1.0 / REFRESH_RATE
    step_time = end_time - 1.0 + step
    frame_times = []
    for _ in range(REFRESH_RATE):
        # Checks if there are vblanks in a refresh interval.
        step_count = 0
        while trace_buffer and trace_buffer[0] < step_time:
            frame_times.append(trace_buffer.popleft())
            step_count += 1

        # Each character represent an 1 / REFRESH_RATE interval.
        if step_count:
            if step_count >= 10:
                frame_info.append('*')
            else:
                frame_info.append(str(step_count))
        else:
            frame_info.append(' ')
        step_time += step

    return frame_info, frame_times


def start_thread(function, args=()):
    """ Starts a thread with given argument. """
    new_thread = threading.Thread(target=function, args=args)
    new_thread.start()


class FPSMeter():
    """ Initializes a FPSMeter to measure system FPS periodically. """
    def __init__(self, callback):
        self.is_stopping = threading.Event()
        self.callback = callback


    def __enter__(self):
        self.start()
        return self


    def __exit__(self, type, value, traceback):
        self.stop()


    def notify_collection(self, period_sec=1.0):
        """ Writes a notification mark periodically. """
        logging.info('Notification thread is started')
        next_notify_time = time.time() + period_sec
        while True:
            # Calling time.sleep(1) may suspend for more than 1 second.
            # Sleeps until a specific time to avoid error accumulation.
            sleep_time = next_notify_time - time.time()
            next_notify_time += period_sec
            # Skips if current time is larger than next_notify_time.
            if sleep_time > 0:
                if self.is_stopping.wait(sleep_time):
                    logging.info('Leaving notification thread')
                    # So the main loop doesn't stuck in the readline().
                    common_lib_utils.open_write_close(TRACE_MARKER, STOP_STRING)
                    break
                common_lib_utils.open_write_close(TRACE_MARKER, NOTIFY_STRING)


    def main_loop(self, trace_method):
        """ Main loop to parse the trace.

        There are 2 threads involved:
        Main thread:
            Using blocking read to get data from trace_pipe.
        Notify thread: The main thread may wait indifinitely if there
            is no new trace. Writes to the pipe once per second to avoid
            the indefinite waiting.
        """
        logging.info('Fps meter main thread is started.')

        # Sample trace:
        # <idle>-0  [000] dNh3 631.905961: drm_vblank_event: crtc=0, seq=65496
        # <idle>-0  [000] d.h3 631.922681: drm_vblank_event: crtc=0, seq=65497
        # fps_meter [003] ..1 632.393953: tracing_mark_write: notify_collection
        # ..
        re_notify = re.compile(
                r'.* (\d+\.\d+): tracing_mark_write: ' + NOTIFY_STRING)
        if trace_method == 'workq':
            re_trace = re.compile(
                    r'.* (\d+\.\d+): workqueue_execute_start: '
                    r'work struct ([\da-f]+): '
                    r'function intel_atomic_commit_work')
        elif trace_method == 'flip':
            re_trace = re.compile(
                    r'.* (\d+\.\d+): i915_flip_complete: '
                    r'plane=(\d+), obj=([\da-f]+)')
        else:
            re_trace = re.compile(
                    r'.* (\d+\.\d+): drm_vblank_event: crtc=(\d+), seq=(\d+)')

        trace_buffer = collections.deque()
        with open(TRACE_PIPE) as trace_pipe:
            # The pipe may block a few seconds if using:
            # for line in trace_pipe
            while not self.is_stopping.is_set():
                line = trace_pipe.readline()
                m_trace = re_trace.match(line)
                if m_trace:
                    timestamp = float(m_trace.group(1))
                    trace_buffer.append(timestamp)
                else:
                    m_notify = re_notify.match(line)
                    if m_notify:
                        timestamp = float(m_notify.group(1))
                        self.callback(get_fps_info(trace_buffer, timestamp))
            logging.info('Leaving fps meter main thread')


    def start(self):
        """ Starts the FPS meter by launching needed threads. """
        # The notificaiton thread.
        start_thread(self.notify_collection)

        # The main thread.
        trace_method = get_trace_method()
        enable_tracing(trace_method)
        start_thread(self.main_loop, [trace_method])


    def stop(self):
        """ Stops the FPS meter. Shut down threads. """
        logging.info('Shutting down FPS meter')
        self.is_stopping.set()


def output_fps_info(verbose, fps_info):
    """ Print the fps info to the screen. """
    frame_info, frame_times = fps_info
    fps_count = len([f for f in frame_info if f != ' '])
    frame_info_str = ''.join(frame_info)
    print('[%s] FPS: %2d' % (frame_info_str, fps_count))
    if frame_times and verbose:
        print(', '.join(['%.3f' % t for t in frame_times]))


def main(argv):
    """ Print fps information on the screen. """
    parser = argparse.ArgumentParser(description='Print fps infomation.')
    parser.add_argument('--verbose', action='store_true',
                                        help='print verbose frame time info')
    parser.add_argument('--debug', action='store_true',
                                        help='print debug message')
    options = parser.parse_args()

    if options.debug:
        rootLogger = logging.getLogger()
        rootLogger.setLevel(logging.DEBUG)
        # StreamHandler() defaults to stderr.
        rootLogger.addHandler(logging.StreamHandler())

    print('FPS meter trace method %s' % get_trace_method())
    with FPSMeter(functools.partial(output_fps_info, options.verbose)):
        while True:
            try:
                time.sleep(86400)
            except KeyboardInterrupt:
                print('Exiting...')
                break


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
