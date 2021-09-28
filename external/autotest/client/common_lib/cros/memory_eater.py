#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import subprocess
import time
import threading

from autotest_lib.client.bin import utils

class MemoryEater(object):
    """A util class which run programs to consume memory in the background.

    Sample usage:
    with MemoryEator() as memory_eater:
      # Allocate mlocked memory.
      memory_eater.consume_locked_memory(123)

      # Allocate memory and sequentially traverse them over and over.
      memory_eater.consume_active_memory(500)

    When it goes out of the "with" context or the object is destructed, all
    allocated memory are released.
    """

    memory_eater_locked = 'memory-eater-locked'
    memory_eater = 'memory-eater'

    _all_instances = []

    def __init__(self):
        self._locked_consumers = []
        self._active_consumers_lock = threading.Lock()
        self._active_consumers = []
        self._all_instances.append(self)

    def __enter__(self):
        return self

    @staticmethod
    def cleanup_consumers(consumers):
        """Kill all processes in |consumers|

        @param consumers: The list of consumers to clean.
        """
        while len(consumers):
            job = consumers.pop()
            logging.info('Killing %d', job.pid)
            job.kill()

    def cleanup(self):
        """Releases all allocated memory."""
        # Kill all hanging jobs.
        logging.info('Cleaning hanging memory consuming processes...')
        self.cleanup_consumers(self._locked_consumers)
        with self._active_consumers_lock:
            self.cleanup_consumers(self._active_consumers)

    def __exit__(self, type, value, traceback):
        self.cleanup()

    def __del__(self):
        self.cleanup()
        if self in self._all_instances:
            self._all_instances.remove(self)

    def consume_locked_memory(self, mb):
        """Consume non-swappable memory."""
        logging.info('Consuming locked memory %d MB', mb)
        cmd = [self.memory_eater_locked, str(mb)]
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        self._locked_consumers.append(p)
        # Wait until memory allocation is done.
        while True:
            line = p.stdout.readline()
            if line.find('Done') != -1:
                break

    def consume_active_memory(self, mb):
        """Consume active memory."""
        logging.info('Consuming active memory %d MB', mb)
        cmd = [self.memory_eater, '--size', str(mb), '--chunk', '128']
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        with self._active_consumers_lock:
            self._active_consumers.append(p)

    @classmethod
    def get_active_consumer_pids(cls):
        """Gets pid of active consumers by all instances of the class."""
        all_pids = []
        for instance in cls._all_instances:
            with instance._active_consumers_lock:
                all_pids.extend([p.pid for p in instance._active_consumers])
        return all_pids


def consume_free_memory(memory_to_reserve_mb):
    """Consumes free memory until |memory_to_reserve_mb| is remained.

    Non-swappable memory is allocated to consume memory.
    memory_to_reserve_mb: Consume memory until this amount of free memory
        is remained.
    @return The MemoryEater() object on which memory is allocated. One can
        catch it in a context manager.
    """
    consumer = MemoryEater()
    while True:
        mem_free_mb = utils.read_from_meminfo('MemFree') / 1024
        logging.info('Current Free Memory %d', mem_free_mb)
        if mem_free_mb <= memory_to_reserve_mb:
            break
        memory_to_consume = min(
            2047, mem_free_mb - memory_to_reserve_mb + 1)
        logging.info('Consuming %d MB locked memory', memory_to_consume)
        consumer.consume_locked_memory(memory_to_consume)
    return consumer


class TimeoutException(Exception):
    """Exception to return if timeout happens."""
    def __init__(self, message):
        super(TimeoutException, self).__init__(message)


class _Timer(object):
    """A simple timer class to check timeout."""
    def __init__(self, timeout, des):
        """Initializer.

        @param timeout: Timeout in seconds.
        @param des: A short description for this timer.
        """
        self.timeout = timeout
        self.des = des
        if self.timeout:
            self.start_time = time.time()

    def check_timeout(self):
        """Raise TimeoutException if timeout happens."""
        if not self.timeout:
          return
        time_delta = time.time() - self.start_time
        if time_delta > self.timeout:
            err_message = '%s timeout after %s seconds' % (self.des, time_delta)
            logging.warning(err_message)
            raise TimeoutException(err_message)


def run_single_memory_pressure(
    starting_mb, step_mb, end_condition, duration, cool_down, timeout=None):
    """Runs a single memory consumer to produce memory pressure.

    Keep adding memory pressure. In each round, it runs a memory consumer
    and waits for a while before checking whether to end the process. If not,
    kill current memory consumer and allocate more memory pressure in the next
    round.
    @param starting_mb: The amount of memory to start with.
    @param step_mb: If |end_condition| is not met, allocate |step_mb| more
        memory in the next round.
    @param end_condition: A boolean function returns whether to end the process.
    @param duration: Time (in seconds) to wait between running a memory
        consumer and checking |end_condition|.
    @param cool_down:  Time (in seconds) to wait between each round.
    @param timeout: Seconds to stop the function is |end_condition| is not met.
    @return The size of memory allocated in the last round.
    @raise TimeoutException if timeout.
    """
    current_mb = starting_mb
    timer = _Timer(timeout, 'run_single_memory_pressure')
    while True:
        timer.check_timeout()
        with MemoryEater() as consumer:
            consumer.consume_active_memory(current_mb)
            time.sleep(duration)
            if end_condition():
                return current_mb
        current_mb += step_mb
        time.sleep(cool_down)


def run_multi_memory_pressure(size_mb, end_condition, duration, timeout=None):
    """Runs concurrent memory consumers to produce memory pressure.

    In each round, it runs a new memory consumer until a certain condition is
    met.
    @param size_mb: The amount of memory each memory consumer allocates.
    @param end_condition: A boolean function returns whether to end the process.
    @param duration: Time (in seconds) to wait between running a memory
        consumer and checking |end_condition|.
    @param timeout: Seconds to stop the function is |end_condition| is not met.
    @return Total allocated memory.
    @raise TimeoutException if timeout.
    """
    total_mb = 0
    timer = _Timer(timeout, 'run_multi_memory_pressure')
    with MemoryEater() as consumer:
        while True:
            timer.check_timeout()
            consumer.consume_active_memory(size_mb)
            time.sleep(duration)
            if end_condition():
                return total_mb
            total_mb += size_mb
