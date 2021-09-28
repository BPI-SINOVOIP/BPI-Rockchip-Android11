# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os
import time
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error, utils

SYSFS_CPUQUIET_ENABLE = '/sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable'
SYSFS_INTEL_PSTATE_PATH = '/sys/devices/system/cpu/intel_pstate'


class power_CPUFreq(test.test):
    version = 1

    def initialize(self):
        cpufreq_path = '/sys/devices/system/cpu/cpu*/cpufreq'

        dirs = glob.glob(cpufreq_path)
        if not dirs:
            raise error.TestFail('cpufreq not supported')

        self._cpufreq_dirs = dirs
        self._cpus = [cpufreq(dirname) for dirname in dirs]
        for cpu in self._cpus:
            cpu.save_state()

        # Store the setting if the system has CPUQuiet feature
        if os.path.exists(SYSFS_CPUQUIET_ENABLE):
            self.is_cpuquiet_enabled = utils.read_file(SYSFS_CPUQUIET_ENABLE)
            utils.write_one_line(SYSFS_CPUQUIET_ENABLE, '0')

    def run_once(self):
        # TODO(crbug.com/485276) Revisit this exception once we've refactored
        # test to account for intel_pstate cpufreq driver
        if os.path.exists(SYSFS_INTEL_PSTATE_PATH):
            raise error.TestNAError('Test does NOT support intel_pstate driver')

        keyvals = {}
        try:
            # First attempt to set all frequencies on each core before going
            # on to the next core.
            self.test_cores_in_series()
            # Record that it was the first test that passed.
            keyvals['test_cores_in_series'] = 1
        except error.TestFail as exception:
            if str(exception) == 'Unable to set frequency':
                # If test_cores_in_series fails, try to set each frequency for
                # all cores before moving on to the next frequency.
                logging.debug('trying to set freq in parallel')
                self.test_cores_in_parallel()
                # Record that it was the second test that passed.
                keyvals['test_cores_in_parallel'] = 1
            else:
                raise exception

        self.write_perf_keyval(keyvals)

    def test_cores_in_series(self):
        for cpu in self._cpus:
            if 'userspace' not in cpu.get_available_governors():
                raise error.TestError('userspace governor not supported')

            available_frequencies = cpu.get_available_frequencies()
            if len(available_frequencies) == 1:
                raise error.TestFail('Not enough frequencies supported!')

            # set cpufreq governor to userspace
            cpu.set_governor('userspace')

            # cycle through all available frequencies
            for freq in available_frequencies:
                cpu.set_frequency(freq)
                if not self.compare_freqs(freq, cpu):
                    raise error.TestFail('Unable to set frequency')

    def test_cores_in_parallel(self):
        cpus = self._cpus
        cpu0 = self._cpus[0]

        # Use the first CPU's frequencies for all CPUs.  Assume that they are
        # the same.
        available_frequencies = cpu0.get_available_frequencies()
        if len(available_frequencies) == 1:
            raise error.TestFail('Not enough frequencies supported!')

        for cpu in cpus:
            if 'userspace' not in cpu.get_available_governors():
                raise error.TestError('userspace governor not supported')

            # set cpufreq governor to userspace
            cpu.set_governor('userspace')

        # cycle through all available frequencies
        for freq in available_frequencies:
            for cpu in cpus:
                cpu.set_frequency(freq)
            for cpu in cpus:
                if not self.compare_freqs(freq, cpu):
                    raise error.TestFail('Unable to set frequency')

    def compare_freqs(self, set_freq, cpu):
        # crbug.com/848309 : older kernels have race between setting
        # governor and access to setting frequency so add a retry
        try:
            freq = cpu.get_current_frequency()
        except IOError:
            logging.warn('Frequency getting failed.  Retrying once.')
            time.sleep(.1)
            freq = cpu.get_current_frequency()

        logging.debug('frequency set:%d vs actual:%d',
                      set_freq, freq)

        # setting freq to a particular frequency isn't reliable so just test
        # that driver allows setting & getting.
        if cpu.get_driver() == 'acpi-cpufreq':
            return True

        return set_freq == freq

    def cleanup(self):
        if self._cpus:
            for cpu in self._cpus:
                # restore cpufreq state
                cpu.restore_state()

        # Restore the original setting if system has CPUQuiet feature
        if os.path.exists(SYSFS_CPUQUIET_ENABLE):
            utils.open_write_close(SYSFS_CPUQUIET_ENABLE,
                                   self.is_cpuquiet_enabled)


class cpufreq(object):

    def __init__(self, path):
        self.__base_path = path
        self.__save_files_list = [
            'scaling_max_freq', 'scaling_min_freq', 'scaling_governor'
        ]
        self._freqs = None
        # disable boost to limit how much freq can vary in userspace mode
        if self.get_driver() == 'acpi-cpufreq':
            self.disable_boost()

    def __del__(self):
        if self.get_driver() == 'acpi-cpufreq':
            self.enable_boost()

    def __write_file(self, file_name, data):
        path = os.path.join(self.__base_path, file_name)
        try:
            utils.open_write_close(path, data)
        except IOError as e:
            logging.warn('write of %s failed: %s', path, str(e))

    def __read_file(self, file_name):
        path = os.path.join(self.__base_path, file_name)
        f = open(path, 'r')
        data = f.read()
        f.close()
        return data

    def save_state(self):
        logging.info('saving state:')
        for fname in self.__save_files_list:
            data = self.__read_file(fname)
            setattr(self, fname, data)
            logging.info(fname + ': ' + data)

    def restore_state(self):
        logging.info('restoring state:')
        for fname in self.__save_files_list:
            # Sometimes a newline gets appended to a data string and it throws
            # an error when being written to a sysfs file.  Call strip() to
            # eliminateextra whitespace characters so it can be written cleanly
            # to the file.
            data = getattr(self, fname).strip()
            logging.info(fname + ': ' + data)
            self.__write_file(fname, data)

    def get_available_governors(self):
        governors = self.__read_file('scaling_available_governors')
        logging.info('available governors: %s', governors)
        return governors.split()

    def get_current_governor(self):
        governor = self.__read_file('scaling_governor')
        logging.info('current governor: %s', governor)
        return governor.split()[0]

    def get_driver(self):
        driver = self.__read_file('scaling_driver')
        logging.info('current driver: %s', driver)
        return driver.split()[0]

    def set_governor(self, governor):
        logging.info('setting governor to %s', governor)
        self.__write_file('scaling_governor', governor)

    def get_available_frequencies(self):
        if self._freqs:
            return self._freqs
        frequencies = self.__read_file('scaling_available_frequencies')
        logging.info('available frequencies: %s', frequencies)
        self._freqs = [int(i) for i in frequencies.split()]
        return self._freqs

    def get_current_frequency(self):
        freq = int(self.__read_file('scaling_cur_freq'))
        logging.info('current frequency: %s', freq)
        return freq

    def set_frequency(self, frequency):
        logging.info('setting frequency to %d', frequency)
        if frequency >= self.get_current_frequency():
            file_list = [
                'scaling_max_freq', 'scaling_min_freq', 'scaling_setspeed'
            ]
        else:
            file_list = [
                'scaling_min_freq', 'scaling_max_freq', 'scaling_setspeed'
            ]

        for fname in file_list:
            self.__write_file(fname, str(frequency))

    def disable_boost(self):
        """Disable boost.

        Note, boost is NOT a per-cpu parameter,
          /sys/device/system/cpu/cpufreq/boost

        So code below would unnecessarily disable it per-cpu but that should not
        cause any issues.
        """
        logging.debug('Disable boost')
        self.__write_file('../../cpufreq/boost', '0')

    def enable_boost(self):
        """Enable boost.

        Note, boost is NOT a per-cpu parameter,
          /sys/device/system/cpu/cpufreq/boost

        So code below would unnecessarily enable it per-cpu but that should not
        cause any issues.
        """
        logging.debug('Enable boost')
        self.__write_file('../../cpufreq/boost', '1')
