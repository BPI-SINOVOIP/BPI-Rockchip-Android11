# Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros.power import power_dashboard

class ServerTestDashboard(power_dashboard.BaseDashboard):
    """Dashboard class for autotests that run on server side.
    """

    def __init__(self, logger, testname, host, start_ts=None, resultsdir=None,
                 uploadurl=None, note=''):
        """Create ServerTestDashboard objects.

        Args:
            logger: object that store the log. This will get convert to
                    dictionary by self._convert()
            testname: name of current test
            resultsdir: directory to save the power json
            uploadurl: url to upload power data
            host: autotest_lib.server.hosts.cros_host.CrosHost object of DUT
            note: note for current test run
        """

        self._host = host
        self._note = note
        super(ServerTestDashboard, self).__init__(logger, testname, start_ts,
                                                  resultsdir, uploadurl)

    def _create_dut_info_dict(self, power_rails):
        """Create a dictionary that contain information of the DUT.

        Args:
            power_rails: list of measured power rails

        Returns:
            DUT info dictionary
        """

        board = self._host.get_board().replace('board:', '')
        platform = self._host.get_platform_from_mosys()

        if platform and not platform.startswith(board):
            board += '_' + platform

        if self._host.has_hammer():
            board += '_hammer'

        dut_info_dict = {
            'board': board,
            'version': {
                'hw': self._host.get_hardware_revision(),
                'milestone': self._host.get_chromeos_release_milestone(),
                'os': self._host.get_release_version(),
                'channel': self._host.get_channel(),
                'firmware': self._host.get_firmware_version(),
                'ec': self._host.get_ec_version(),
                'kernel': self._host.get_kernel_version(),
            },
            'sku' : {
                'cpu': self._host.get_cpu_name(),
                'memory_size': self._host.get_mem_total_gb(),
                'storage_size': self._host.get_disk_size_gb(),
                'display_resolution': self._host.get_screen_resolution(),
            },
            'ina': {
                'version': 0,
                'ina': power_rails,
            },
            'note': self._note,
        }

        if self._host.has_battery():
            # Round the battery size to nearest tenth because it is fluctuated
            # for platform without battery nominal voltage data.
            dut_info_dict['sku']['battery_size'] = round(
                    self._host.get_battery_size(), 1)
            dut_info_dict['sku']['battery_shutdown_percent'] = \
                    self._host.get_low_battery_shutdown_percent()
        return dut_info_dict

class PowerTelemetryLoggerDashboard(ServerTestDashboard):
    """Dashboard class for power_telemetry_logger.PowerTelemetryLogger class.
    """

    def __init__(self, logger, testname, host, start_ts, checkpoint_logger,
                 resultsdir=None, uploadurl=None, note=''):
        if uploadurl is None:
            uploadurl = 'http://chrome-power.appspot.com'
        self._checkpoint_logger = checkpoint_logger
        super(PowerTelemetryLoggerDashboard, self).__init__(
                logger, testname, host, start_ts, resultsdir, uploadurl, note)

    def _create_checkpoint_dict(self):
        """Create dictionary for checkpoint.
        """
        return self._checkpoint_logger.convert_relative(self._start_ts)

    def _convert(self):
        """
        self._logger is already in correct format, so just return it.

        Returns:
            raw measurement dictionary
        """
        self._tag_with_checkpoint(self._logger)
        return self._logger
