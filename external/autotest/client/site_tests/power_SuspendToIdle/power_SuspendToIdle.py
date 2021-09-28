# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from contextlib import contextmanager

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.power import power_suspend
from autotest_lib.client.cros.power import power_status
from autotest_lib.client.cros.power import power_utils


class power_SuspendToIdle(test.test):
    """class for power_SuspendToIdle test."""
    version = 1

    @contextmanager
    def _log_error_message(self):
        """Suppress exception and log the message."""
        try:
            yield
        except Exception as e:
            self._error_count += 1
            self._error_message.append(str(e))

    def run_once(self, force_suspend_to_idle=False):
        """Main test method.
        """
        if utils.get_cpu_arch() != 'x86_64':
            raise error.TestNAError('This test only supports x86_64 CPU.')

        if power_utils.get_sleep_state() != 'freeze':
            if not force_suspend_to_idle:
                raise error.TestNAError(
                    'System default config is not suspend to idle.')
            else:
                logging.info('System default config is suspend to ram. '
                             'Force suspend to idle')

        self._error_count = 0
        self._error_message = []
        dmc_firmware_stats = None
        s0ix_residency_stats = None
        cpu_packages_stats = None
        rc6_residency_stats = None

        with self._log_error_message():
            dmc_firmware_stats = power_status.DMCFirmwareStats()
            if not dmc_firmware_stats.check_fw_loaded():
                raise error.TestFail('DMC firmware not loaded.')

        with self._log_error_message():
            pch_powergating_stats = power_status.PCHPowergatingStats()
            pch_powergating_stats.read_pch_powergating_info()
            if not pch_powergating_stats.check_s0ix_requirement():
                raise error.TestFail('PCH powergating check failed.')

        with self._log_error_message():
            s0ix_residency_stats = power_status.S0ixResidencyStats()

        with self._log_error_message():
            cpu_packages_stats = power_status.CPUPackageStats()

        with self._log_error_message():
            rc6_residency_stats = power_status.RC6ResidencyStats()

        with self._log_error_message():
            suspender = power_suspend.Suspender(self.resultsdir,
                                                suspend_state='freeze')
            suspender.suspend()

        with self._log_error_message():
            if (dmc_firmware_stats and
                dmc_firmware_stats.is_dc6_supported() and
                dmc_firmware_stats.get_accumulated_dc6_entry() <= 0):
                raise error.TestFail('DC6 entry check failed.')

        with self._log_error_message():
            if (s0ix_residency_stats and
                s0ix_residency_stats.get_accumulated_residency_secs() <= 0):
                raise error.TestFail('S0ix residency check failed.')

        with self._log_error_message():
            if (cpu_packages_stats and
                cpu_packages_stats.refresh().get('C10', 0) <= 0):
                raise error.TestFail('C10 state check failed.')

        with self._log_error_message():
            if (rc6_residency_stats and
                rc6_residency_stats.get_accumulated_residency_msecs() <= 0):
                raise error.TestFail('RC6 residency check failed.')

        if self._error_count > 0:
            raise error.TestFail('Found %d errors: ' % self._error_count,
                                 ', '.join(self._error_message))
