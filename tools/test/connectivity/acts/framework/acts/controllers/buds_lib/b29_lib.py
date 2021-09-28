#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

"""
Class definition of B29 device for controlling the device.

B29 is an engineering device with serial capabilities. It is almost like
b20 except it has additional features that allow sending commands
to b10 via one-wire and to pull logs from b10 via one-wire.

Please see https://docs.google.com/document/d/17yJeJRNWxv5E9
fBvw0sXkgwCBkshU_l4SxWkKgAxVmk/edit for details about available operations.
"""

import os
import re
import time
from logging import Logger

from acts import utils
from acts.controllers.buds_lib import tako_trace_logger

logging = tako_trace_logger.TakoTraceLogger(Logger(__file__))
DEVICE_REGEX = (
    r'_(?P<device_serial>[A-Z0-9]+)-(?P<interface>\w+)\s->\s'
    r'(\.\./){2}(?P<port>\w+)'
)
# TODO: automate getting the latest version from x20
DEBUG_BRIDGE = ('/google/data/ro/teams/wearables/apollo/ota/jenkins-presubmit/'
                'ovyalov/master/apollo-sw/CL14060_v2-build13686/v13686/'
                'automation/apollo_debug_bridge/linux2/apollo_debug_bridge')
B29_CHIP = 'Cypress_Semiconductor_USBUART'


# TODO:
# as the need arises, additional functionalities of debug_bridge should be
# integrated
# TODO:
# https://docs.google.com/document/d/17yJeJRNWxv5E9fBvw0sXkgwCBkshU_
# l4SxWkKgAxVmk/edit

class B29Error(Exception):
    """Module Level Error."""


def get_b29_devices():
    """ Get all available B29 devices.

    Returns:
      (list) A list of available devices (ex: ['/dev/ttyACM4',...]) or empty
      list if none found
    """
    devices = []
    result = os.popen('ls -l /dev/serial/by-id/*%s*' % B29_CHIP).read()
    for line in result.splitlines():
        match = re.search(DEVICE_REGEX, line)
        device_serial = match.group('device_serial')
        log_port = None
        commander_port = '/dev/' + match.group('port')
        device = {
            'commander_port': commander_port,
            'log_port': log_port,
            'serial_number': device_serial
        }
        devices.append(device)
    return devices


class B29Device(object):
    """Class to control B29 device."""

    def __init__(self, b29_serial):
        """ Class to control B29 device
        Args: String type of serial number (ex: 'D96045152F121B00'
        """
        self.serial = b29_serial
        b29_port = [d['commander_port'] for d in get_b29_devices() if
                    d['serial_number'] == b29_serial]
        if not b29_port:
            logging.error("unable to find b29 with serial number %s" %
                          b29_serial)
            raise B29Error(
                "Recovery failed because b29_serial specified in device "
                "manifest file is not found or invalid")
        self.port = b29_port[0]
        self.ping_match = {'psoc': r'Pings: tx=[\d]* rx=[1-9][0-9]',
                           'csr': r'count=100, sent=[\d]*, received=[1-9][0-9]',
                           'charger': r'Pings: tx=[\d]* rx=[1-9][0-9]'}
        self.fw_version = self._get_version('fw')
        self.app_version = self._get_version('app')

    def _get_version(self, type='fw'):
        """ Method to get version of B29
        Returns:
            String version if found (ex: '0006'), None otherwise
        """
        command = '--serial={}'.format(self.port)
        debug_bridge_process = self._send_command(command=command)
        if type == 'fw':
            version_match = re.compile(r'CHARGER app version: version=([\d]*)')
        elif type == 'app':
            version_match = re.compile(r'APP VERSION: ([\d]*)')
        version_str = self._parse_output_of_running_process(
            debug_bridge_process, version_match)
        debug_bridge_process.kill()
        if version_str:
            match = version_match.search(version_str)
            version = match.groups()[0]
            return version
        return None

    def _parse_output_of_running_process(self, subprocess, match, timeout=30):
        """ Parses the logs from subprocess objects and checks to see if a
        match is found within the allotted time
        Args:
            subprocess: object returned by _send_command (which is the same as
            bject returned by subprocess.Popen()) match: regex match object
            (what is returned by re.compile(r'<regex>') timeout: int - time to
            keep retrying before bailing

        """
        start_time = time.time()
        success_match = re.compile(match)
        while start_time + timeout > time.time():
            out = subprocess.stderr.readline()
            if success_match.search(out):
                return out
            time.sleep(.5)
        return False

    def _send_command(self, command):
        """ Send command to b29 using apollo debug bridge
        Args:
          command: The command for apollo debug to execute
        Returns:
          subprocess object
        """
        return utils.start_standing_subprocess(
            '{} {} {}'.format(DEBUG_BRIDGE, '--rpc_port=-1', command),
            shell=True)

    def restore_golden_image(self):
        """ Start a subprocess that calls the debug-bridge executable with
        options that restores golden image of b10 attached to the b29. The
        recovery restores the 'golden image' which is available in b10 partition
         8. The process runs for 120 seconds which is adequate time for the
         recovery to have completed.
        """
        # TODO:
        # because we are accessing x20, we need to capture error resulting from
        #  expired prodaccess and report it explicitly
        # TODO:
        # possibly file not found error?

        # start the process, wait for two minutes and kill it
        logging.info('Restoring golden image...')
        command = '--serial=%s --debug_spi=dfu --sqif_partition=8' % self.port
        debug_bridge_process = self._send_command(command=command)
        success_match = re.compile('DFU on partition #8 successfully initiated')
        if self._parse_output_of_running_process(debug_bridge_process,
                                                 success_match):
            logging.info('Golden image restored successfully')
            debug_bridge_process.kill()
            return True
        logging.warning('Failed to restore golden image')
        debug_bridge_process.kill()
        return False

    def ping_component(self, component, timeout=30):
        """ Send ping to the specified component via B290
        Args:
            component = 'csr' or 'psoc' or 'charger'
        Returns:
            True if successful and False otherwise
        """
        if component not in ('csr', 'psoc', 'charger'):
            raise B29Error('specified parameter for component is not valid')
        logging.info('Pinging %s via B29...' % component)
        command = '--serial={} --ping={}'.format(self.port, component)
        debug_bridge_process = self._send_command(command=command)
        if self._parse_output_of_running_process(debug_bridge_process,
                                                 self.ping_match[component],
                                                 timeout):
            logging.info('Ping passes')
            debug_bridge_process.kill()
            return True
        else:
            logging.warning('Ping failed')
            debug_bridge_process.kill()
            return False

    def reset_charger(self):
        """ Send reset command to B29
        Raises: TimeoutError (lib.utils.TimeoutError) if the device does not
        come back within 120 seconds
        """
        # --charger_reset
        if int(self.fw_version) >= 6:
            logging.info('Resetting B29')
            command = '--serial={} --charger_reset'.format(self.port)
            reset_charger_process = self._send_command(command=command)
            time.sleep(2)
            reset_charger_process.kill()
            logging.info('Waiting for B29 to become available..')
            utils.wait_until(lambda: self.ping_component('charger'), 120)
        else:
            logging.warning('B20 firmware version %s does not support '
                            'charger_reset argument' % self.fw_version)
