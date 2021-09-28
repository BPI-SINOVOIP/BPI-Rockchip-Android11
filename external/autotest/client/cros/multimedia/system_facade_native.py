# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Facade to access the system-related functionality."""

import StringIO
import os
import threading
import time

from autotest_lib.client.bin import utils


class SystemFacadeNativeError(Exception):
    """Error in SystemFacadeNative."""
    pass


class SystemFacadeNative(object):
    """Facede to access the system-related functionality.

    The methods inside this class only accept Python native types.

    """
    SCALING_GOVERNOR_MODES = [
            'interactive',
            'performance',
            'ondemand',
            'powersave',
            'sched'
            ]

    def __init__(self):
        self._bg_worker = None

    def set_scaling_governor_mode(self, index, mode):
        """Set mode of CPU scaling governor on one CPU.

        @param index: CPU index starting from 0.

        @param mode: Mode of scaling governor, accept 'interactive' or
                     'performance'.

        @returns: The original mode.

        """
        if mode not in self.SCALING_GOVERNOR_MODES:
            raise SystemFacadeNativeError('mode %s is invalid' % mode)

        governor_path = os.path.join(
                '/sys/devices/system/cpu/cpu%d' % index,
                'cpufreq/scaling_governor')
        if not os.path.exists(governor_path):
            raise SystemFacadeNativeError(
                    'scaling governor of CPU %d is not available' % index)

        original_mode = utils.read_one_line(governor_path)
        utils.open_write_close(governor_path, mode)

        return original_mode


    def get_cpu_usage(self):
        """Returns machine's CPU usage.

        Returns:
            A dictionary with 'user', 'nice', 'system' and 'idle' values.
            Sample dictionary:
            {
                'user': 254544,
                'nice': 9,
                'system': 254768,
                'idle': 2859878,
            }
        """
        return utils.get_cpu_usage()


    def compute_active_cpu_time(self, cpu_usage_start, cpu_usage_end):
        """Computes the fraction of CPU time spent non-idling.

        This function should be invoked using before/after values from calls to
        get_cpu_usage().
        """
        return utils.compute_active_cpu_time(cpu_usage_start,
                                                  cpu_usage_end)


    def get_mem_total(self):
        """Returns the total memory available in the system in MBytes."""
        return utils.get_mem_total()


    def get_mem_free(self):
        """Returns the currently free memory in the system in MBytes."""
        return utils.get_mem_free()

    def get_mem_free_plus_buffers_and_cached(self):
        """
        Returns the free memory in MBytes, counting buffers and cached as free.

        This is most often the most interesting number since buffers and cached
        memory can be reclaimed on demand. Note however, that there are cases
        where this as misleading as well, for example used tmpfs space
        count as Cached but can not be reclaimed on demand.
        See https://www.kernel.org/doc/Documentation/filesystems/tmpfs.txt.
        """
        return utils.get_mem_free_plus_buffers_and_cached()

    def get_ec_temperatures(self):
        """Uses ectool to return a list of all sensor temperatures in Celsius.
        """
        return utils.get_ec_temperatures()

    def get_current_temperature_max(self):
        """
        Returns the highest reported board temperature (all sensors) in Celsius.
        """
        return utils.get_current_temperature_max()

    def get_current_board(self):
        """Returns the current device board name."""
        return utils.get_current_board()


    def get_chromeos_release_version(self):
        """Returns chromeos version in device under test as string. None on
        fail.
        """
        return utils.get_chromeos_release_version()

    def get_num_allocated_file_handles(self):
        """
        Returns the number of currently allocated file handles.
        """
        return utils.get_num_allocated_file_handles()

    def get_storage_statistics(self, device=None):
        """
        Fetches statistics for a storage device.
        """
        return utils.get_storage_statistics(device)

    def start_bg_worker(self, command):
        """
        Start executing the command in a background worker.
        """
        self._bg_worker = BackgroundWorker(command, do_process_output=True)
        self._bg_worker.start()

    def get_and_discard_bg_worker_output(self):
        """
        Returns the output collected so far since the last call to this method.
        """
        if self._bg_worker is None:
            SystemFacadeNativeError('Background worker has not been started.')

        return self._bg_worker.get_and_discard_output()

    def stop_bg_worker(self):
        """
        Stop the worker.
        """
        if self._bg_worker is None:
            SystemFacadeNativeError('Background worker has not been started.')

        self._bg_worker.stop()
        self._bg_worker = None


class BackgroundWorker(object):
    """
    Worker intended for executing a command in the background and collecting its
    output.
    """

    def __init__(self, command, do_process_output=False):
        self._bg_job = None
        self._command = command
        self._do_process_output = do_process_output
        self._output_lock = threading.Lock()
        self._process_output_thread = None
        self._stdout = StringIO.StringIO()

    def start(self):
        """
        Start executing the command.
        """
        self._bg_job = utils.BgJob(self._command, stdout_tee=self._stdout)
        self._bg_job.sp.poll()
        if self._bg_job.sp.returncode is not None:
            self._exit_bg_job()

        if self._do_process_output:
            self._process_output_thread = threading.Thread(
                    target=self._process_output)
            self._process_output_thread.start()

    def _process_output(self, sleep_interval=0.01):
        while self._do_process_output:
            with self._output_lock:
                self._bg_job.process_output()
            time.sleep(sleep_interval)

    def get_and_discard_output(self):
        """
        Returns the output collected so far and then clears the output buffer.
        In other words, subsequent calls to this method will not include output
        that has already been returned before.
        """
        output = ""
        with self._output_lock:
            self._stdout.flush()
            output = self._stdout.getvalue()
            self._stdout.truncate(0)
            self._stdout.seek(0)
        return output

    def stop(self):
        """
        Stop executing the command.
        """
        if self._do_process_output:
            self._do_process_output = False
            self._process_output_thread.join(1)
        self._exit_bg_job()

    def _exit_bg_job(self):
        utils.nuke_subprocess(self._bg_job.sp)
        utils.join_bg_jobs([self._bg_job])
        if self._bg_job.result.exit_status > 0:
            raise SystemFacadeNativeError('Background job failed: %s' %
                                          self._bg_job.result.command)
