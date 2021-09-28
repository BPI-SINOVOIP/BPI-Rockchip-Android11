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

"""Apollo Commander through USB/UART interface.

It uses python serial lib to communicate to a Apollo device.
Some of the commander may not work yet, pending on the final version of the
commander implementation.

Typical usage examples:

    To get a list of all apollo devices:
    >>> devices = apollo_lib.get_devices()

    To work with a specific apollo device:
    >>> apollo = apollo_lib.Device(serial_number='ABCDEF0123456789',
    >>> commander_port='/dev/ttyACM0')

    To send a single command:
    >>> apollo.cmd('PowOff')

    To send a list of commands:
    >>> apollo.cmd(['PowOff', 'PowOn', 'VolUp', 'VolDown']
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import atexit
import os
import re
import subprocess
import time

import serial
from acts.controllers.buds_lib import tako_trace_logger
from acts.controllers.buds_lib import logserial
from acts.controllers.buds_lib.b29_lib import B29Device
from acts.controllers.buds_lib.dev_utils import apollo_log_decoder
from acts.controllers.buds_lib.dev_utils import apollo_log_regex
from acts.controllers.buds_lib.dev_utils import apollo_sink_events
from logging import Logger
from retry import retry

logging = tako_trace_logger.TakoTraceLogger(Logger('apollo'))

BAUD_RATE = 115200
BYTE_SIZE = 8
PARITY = 'N'
STOP_BITS = 1
DEFAULT_TIMEOUT = 3
WRITE_TO_FLASH_WAIT = 30  # wait 30 sec when writing to external flash.
LOG_REGEX = re.compile(r'(?P<time_stamp>\d+)\s(?P<msg>.*)')
STATUS_REGEX = r'(?P<time_stamp>\d+)\s(?P<key>.+?): (?P<value>.+)'
APOLLO_CHIP = '_Apollo_'
DEVICE_REGEX = (
    r'_(?P<device_serial>[A-Z0-9]+)-(?P<interface>\w+)'
    r'\s->\s(\.\./){2}(?P<port>\w+)'
)
OTA_VERIFICATION_FAILED = 'OTA verification failed. corrupt image?'
OTA_ERASING_PARTITION = 'INFO OTA eras ptns'
OTA_RECEIVE_CSR_REGEX = r'INFO OTA CSR rcv begin'
CODEC_REGEX = r'(?P<time_stamp>\d+)\s(?P<codec>\w+) codec is used.'
BUILD_REGEX = r'\d+\.\d+\.(?P<build>\d+)-?(?P<psoc_build>\d*)-?(?P<debug>\w*)'


class Error(Exception):
    """Module Level Error."""


class ResponseError(Error):
    """cmd Response Error."""


class DeviceError(Error):
    """Device Error."""


class ConnectError(Error):
    """Connection Error."""


def get_devices():
    """Get all available Apollo devices.

    Returns:
        (list) A list of available devices or empty list if none found

    Raises:
        Error: raises Error if no Apollo devices or wrong interfaces were found.
    """
    devices = []
    result = os.popen('ls -l /dev/serial/by-id/*%s*' % APOLLO_CHIP).read()
    if not result:
        raise Error('No Apollo Devices found.')
    for line in result.splitlines():
        match = re.search(DEVICE_REGEX, line)
        interface = match.group('interface')
        # TODO: The commander port will always be None.
        commander_port = None
        if interface == 'if00':
            commander_port = '/dev/' + match.group('port')
            continue
        elif interface == 'if02':
            log_port = '/dev/' + match.group('port')
        else:
            raise Error('Wrong interface found.')
        device_serial = match.group('device_serial')

        device = {
            'commander_port': commander_port,
            'log_port': log_port,
            'serial_number': device_serial
        }
        devices.append(device)
    return devices


class BudsDevice(object):
    """Provides a simple class to interact with Apollo."""

    def __init__(self, serial_number, commander_port=None, log_port=None,
                 serial_logger=None):
        """Establish a connection to a Apollo.

        Open a connection to a device with a specific serial number.

        Raises:
            ConnectError: raises ConnectError if cannot open the device.
        """
        self.set_log = False
        self.connection_handle = None
        self.device_closed = False
        if serial_logger:
            self.set_logger(serial_logger)
        self.pc = logserial.PortCheck()
        self.serial_number = serial_number
        # TODO (kselvakumaran): move this to an interface device class that
        # apollo_lib.BudsDevice should derive from
        if not commander_port and not log_port:
            self.get_device_ports(self.serial_number)
        if commander_port:
            self.commander_port = commander_port
        if log_port:
            self.log_port = log_port
        self.apollo_log = None
        self.cmd_log = None
        self.apollo_log_regex = apollo_log_regex
        self.dut_type = 'apollo'

        # TODO (kselvakumaran): move this to an interface device class that
        # apollo_lib.BudsDevice should derive from

        try:  # Try to open the device
            self.connection_handle = logserial.LogSerial(
                self.commander_port, BAUD_RATE, flush_output=False,
                serial_logger=logging)
            self.wait_for_commander()
        except (serial.SerialException, AssertionError, ConnectError) as e:
            logging.error(
                'error opening device {}: {}'.format(serial_number, e))
            raise ConnectError('Error open the device.')
        # disable sleep on idle
        self.stay_connected_state = 1
        atexit.register(self.close)

    def set_logger(self, serial_logger):
        global logging
        logging = serial_logger
        self.set_log = True
        if self.connection_handle:
            self.connection_handle.set_logger(serial_logger)

    def get_device_ports(self, serial_number):
        commander_query = {'ID_SERIAL_SHORT': serial_number,
                           'ID_USB_INTERFACE_NUM': '00'}
        log_query = {'ID_SERIAL_SHORT': serial_number,
                     'ID_USB_INTERFACE_NUM': '02'}
        self.commander_port = self.pc.search_port_by_property(commander_query)
        self.log_port = self.pc.search_port_by_property(log_query)
        if not self.commander_port and not self.log_port:
            raise ConnectError(
                'BudsDevice serial number %s not found' % serial_number)
        else:
            if not self.commander_port:
                raise ConnectError('No devices found')
            self.commander_port = self.commander_port[0]
            self.log_port = self.log_port[0]

    def get_all_log(self):
        return self.connection_handle.get_all_log()

    def query_log(self, from_timestamp, to_timestamp):
        return self.connection_handle.query_serial_log(
            from_timestamp=from_timestamp, to_timestamp=to_timestamp)

    def send(self, cmd):
        """Sends the command to serial port.

        It does not care about whether the cmd is successful or not.

        Args:
            cmd: The passed command

        Returns:
            The number of characters written
        """
        logging.debug(cmd)
        # with self._lock:
        self.connection_handle.write(cmd)
        result = self.connection_handle.read()
        return result

    def cmd(self, cmds, wait=None):
        """Sends the commands and check responses.

        Valid cmd will return something like '585857269 running cmd VolUp'.
        Invalid cmd will log an error and return something like '585826369 No
        command vol exists'.

        Args:
            cmds: The commands to the commander.
            wait: wait in seconds for the cmd response.

        Returns:
            (list) The second element of the array returned by _cmd.
        """
        if isinstance(cmds, str):
            cmds = [cmds]
        results = []
        for cmd in cmds:
            _, result = self._cmd(cmd, wait=wait)
            results.append(result)
        return results

    def _cmd(self, cmd, wait=None, throw_error=True):
        """Sends a single command and check responses.

        Valid cmd will return something like '585857269 running cmd VolUp'.
        Invalid cmd will log an error and return something like '585826369 No
        command vol exists'. Some cmd will return multiple lines of output.
        eg. 'menu'.

        Args:
            cmd: The command to the commander.
            wait: wait in seconds for the cmd response.
            throw_error: Throw exception on True

        Returns:
            (list) containing such as the following:
            [<return value>, [<protobuf dictionary>, str]]
            Hex strings (protobuf) are replaced by its decoded dictionaries
            and stored in an arry along with other string returned fom the
            device.

        Raises:
            DeviceError: On Error.(Optional)
        """
        self.connection_handle.write(cmd)

        while self.connection_handle.is_logging:
            time.sleep(.01)
        if wait:
            self.wait(wait)
        # Using read_serial_port as readlines is a blocking call until idle.
        res = self.read_serial_port()
        result = []
        self.cmd_log = res
        command_resv = False
        # TODO: Cleanup the usage of the two booleans below.
        command_finish = False
        command_rejected = False
        # for line in iter_res:
        for line in res:
            if isinstance(line, dict):
                if 'COMMANDER_RECV_COMMAND' in line.values():
                    command_resv = True
                elif 'COMMANDER_REJECT_COMMAND' in line.values():
                    logging.info('Command rejected')
                    command_rejected = True
                    break
                elif 'COMMANDER_FINISH_COMMAND' in line.values():
                    command_finish = True
                    break
                elif (command_resv and not command_finish and
                      not command_rejected):
                    result.append(line)
            # TODO(jesussalinas): Remove when only encoded lines are required
            elif command_resv and not command_finish and not command_rejected:
                if 'running cmd' not in line:
                    result.append(line)
        success = True
        if command_rejected or not command_resv:
            success = False
            if throw_error:
                logging.info(res)
                raise DeviceError('Unknown command %s' % cmd)
        return success, result

    def get_pdl(self):
        """Returns the PDL stack dictionary.

        The PDL stack stores paired devices of Apollo. Each PDL entry include
        mac_address, flags, link_key, priority fields.

        Returns:
            list of pdl dicts.
        """
        # Get the mask from CONNLIB41:
        # CONNLIB41 typically looks something like this: 2403 fff1
        # 2403 fff1 is actually two 16-bit words of a 32-bit integer
        # like 0xfff12403 . This tells the chronological order of the entries
        # in the paired device list one nibble each. LSB to MSB corresponds to
        # CONNLIB42 through CONNLIB49. So, the above tells us that the device at
        # 0x2638 is the 3rd most recent entry 0x2639 the latest entry etc. As
        # a device re-pairs the masks are updated.
        response = []
        mask = 'ffffffff'
        res = self.cmd('GetPSHex 0x2637')
        if len(res[0]) == 0:
            logging.warning('Error reading PDL mask @ 0x2637')
            return response
        else:
            regexp = r'\d+\s+(?P<m1>....)\s(?P<m2>....)'
            match = re.match(regexp, res[0][0])
            if match:
                connlib41 = match.group('m2') + match.group('m1')
                mask = connlib41[::-1]
                logging.debug('PDL mask: %s' % mask)

        # Now get the MAC/link key
        mask_idx = 0
        for i in range(9784, 9883):
            types = {}
            res = self.cmd('GetPSHex ' + '%0.2x' % i)
            if len(res[0]) == 0:
                break
            else:
                regexp = ('\d+\s+(?P<Mac>....\s....\s....)\s'
                          '(?P<Flags>....\s....)\s(?P<Linkkey>.*)')
                match = re.search(regexp, res[0][0])
                if match:
                    mac_address = match.group('Mac').replace(' ', '').upper()
                    formatted_mac = ''
                    for i in range(len(mac_address)):
                        formatted_mac += mac_address[i]
                        if i % 2 != 0 and i < (len(mac_address) - 1):
                            formatted_mac += ':'
                    types['mac_address'] = formatted_mac
                    types['flags'] = match.group('Flags').replace(' ', '')
                    types['link_key'] = match.group('Linkkey').replace(' ', '')
                    types['priority'] = int(mask[mask_idx], 16)
                    mask_idx += 1
                    response.append(types)

        return response

    def set_pairing_mode(self):
        """Enter Bluetooth Pairing mode."""
        logging.debug('Inside set_pairing_mode()...')
        try:
            return self.cmd('Pair')
        except DeviceError:
            logging.exception('Pair cmd failed')

    # TODO (kselvakumaran): move this to an interface BT class that
    # apollo_lib.BudsDevice should derive from
    def turn_on_bluetooth(self):
        return True

    # TODO (kselvakumaran): move this to an interface BT class that
    # apollo_lib.BudsDevice should derive from
    def is_bt_enabled(self):
        """Check if BT is enabled.

        (TODO:weisu)Currently it is always true since there is no way to disable
        BT in apollo

        Returns:
            True if BT is enabled.
        """
        logging.debug('Inside is_bt_enabled()...')
        return True

    def panic(self):
        """Hitting a panic, device will be automatically reset after 5s."""
        logging.debug('Inside panic()...')
        try:
            self.send('panic')
        except serial.SerialException:
            logging.exception('panic cmd failed')

    def power(self, cmd):
        """Controls the power state of the device.

        Args:
            cmd: If 'Off', powers the device off. Otherwise, powers the device
                 on.
        """
        logging.debug('Inside power({})...'.format(cmd))
        mode = '0' if cmd == 'Off' else '1'
        cmd = 'Pow ' + mode
        try:
            return self.cmd(cmd)
        except DeviceError:
            logging.exception('{} cmd failed'.format(cmd))

    def charge(self, state):
        """Charging Control of the device.

        Args:
          state: '1/0' to enable/disable charging.
        """
        logging.debug('Inside charge({})...'.format(state))
        cmd = 'chg ' + state
        try:
            self.cmd(cmd)
        except DeviceError:
            logging.exception('{} cmd failed'.format(cmd))

    def get_battery_level(self):
        """Get the battery charge level.

        Returns:
            charge percentage string.

        Raises:
            DeviceError: GetBatt response error.
        """
        response = self.cmd('GetBatt')
        for line in response[0]:
            if line.find('Batt:') > -1:
                # Response if in this format '<messageID> Batt: <percentage>'
                return line.split()[2]
        raise DeviceError('Battery Level not found in GetBatt response')

    def get_gas_gauge_current(self):
        """Get the Gauge current value.

        Returns:
            Float value with the info

        Raises:
            DeviceError: I2CRead response error.
        """
        response = self.cmd('I2CRead 2 0x29')
        for line in response[0]:
            if line.find('value') > -1:
                return float.fromhex(line.split()[6].replace(',', ''))
        raise DeviceError('Current Level not found in I2CRead response')

    def get_gas_gauge_voltage(self):
        """Get the Gauge voltage value.

        Returns:
            Float value with the info

        Raises:
            DeviceError: I2CRead response error.
        """
        response = self.cmd('I2CRead 2 0x2A')
        for line in response[0]:
            if line.find('value') > -1:
                return float.fromhex(line.split()[6].replace(',', ''))
        raise DeviceError('Voltage Level not found in I2CRead response')

    def reset(self, wait=5):
        """Resetting the device."""
        logging.debug('Inside reset()...')
        self.power('Off')
        self.wait(wait)
        self.power('On')

    def close(self):
        if not self.device_closed:
            self.connection_handle.close()
            self.device_closed = True
            if not self.set_log:
                logging.flush_log()

    def get_serial_log(self):
        """Retrieve the logs from connection handle."""
        return self.connection_handle.get_all_log()

    def factory_reset(self):
        """Erase paired device(s) (bond) data and reboot device."""
        cmd = 'FactoryReset 1'
        self.send(cmd)
        self.wait(5)
        self.reconnect()

    def reboot(self, reconnect=10, retry_timer=30):
        """Rebooting the device.

        Args:
            reconnect: reconnect attempts after reboot, None for no reconnect.
            retry_timer: wait time in seconds before next connect retry.

        Returns:
            True if successfully reboot or reconnect.
        """
        logging.debug('Inside reboot()...')
        self.panic()
        if not reconnect:
            return True
        ini_time = time.time()
        message = 'waiting for {} to shutdown'.format(self.serial_number)
        logging.info(message)
        while True:
            alive = self.connection_handle.is_port_alive()
            if not alive:
                logging.info('rebooted')
                break
            if time.time() - ini_time > 60:
                logging.info('Shutdown timeouted')
                break
            time.sleep(0.5)
        return self.reconnect(reconnect, retry_timer)

    def reconnect(self, iterations=30, retry_timer=20):
        """Reconnect to the device.

        Args:
            iterations: Number of retry iterations.
            retry_timer: wait time in seconds before next connect retry.

        Returns:
            True if reconnect to the device successfully.

        Raises:
            DeviceError: Failed to reconnect.
        """
        logging.debug('Inside reconnect()...')
        for i in range(iterations):
            try:
                # port might be changed, refresh the port list.
                self.get_device_ports(self.serial_number)
                message = 'commander_port: {}, log_port: {}'.format(
                    self.commander_port, self.log_port)
                logging.info(message)
                self.connection_handle.refresh_port_connection(
                    self.commander_port)
                # Sometimes there might be sfome delay when commander is
                # functioning.
                self.wait_for_commander()
                return True
            except Exception as e:  # pylint: disable=broad-except
                message = 'Fail to connect {} times due to {}'.format(
                    i + 1, e)
                logging.warning(message)
                # self.close()
                time.sleep(retry_timer)
        raise DeviceError('Cannot reconnect to %s with %d attempts.',
                          self.commander_port, iterations)

    @retry(Exception, tries=4, delay=1, backoff=2)
    def wait_for_commander(self):
        """Wait for commander to function.

        Returns:
            True if commander worked.

        Raises:
            DeviceError: Failed to bring up commander.
        """
        # self.Flush()
        result = self.cmd('menu')
        if result:
            return True
        else:
            raise DeviceError('Cannot start commander.')

    def wait(self, timeout=1):
        """Wait for the device."""
        logging.debug('Inside wait()...')
        time.sleep(timeout)

    def led(self, cmd):
        """LED control of the device."""
        message = 'Inside led({})...'.format(cmd)
        logging.debug(message)
        cmd = 'EventUsrLeds' + cmd
        try:
            return self.cmd(_evt_hex(cmd))
        except DeviceError:
            logging.exception('LED cmd failed')

    def volume(self, key, times=1):
        """Volume Control. (Down/Up).

        Args:
            key: Down --Decrease a volume.
                 Up --Increase a volume.
            times: Simulate number of swipes.

        Returns:
            (int) Volume level.

        Raises:
            DeviceError
        """
        message = 'Inside volume({}, {})...'.format(key, times)
        logging.debug(message)
        updown = {
            'Up': '1',
            'Down': '0',
        }
        cmds = ['ButtonSwipe ' + updown[key]] * times
        logging.info(cmds)
        try:
            self.cmd(cmds)
            for line in self.cmd_log:
                if isinstance(line, dict):
                    if 'id' in line and line['id'] == 'VOLUME_CHANGE':
                        if 'data' in line and line['data']:
                            return int(line['data'])
        except DeviceError:
            logging.exception('ButtonSwipe cmd failed')

    def menu(self):
        """Return a list of supported commands."""
        logging.debug('Inside menu()...')
        try:
            return self.cmd('menu')
        except DeviceError:
            logging.exception('menu cmd failed')

    def set_ohd(self, mode='AUTO'):
        """Manually set the OHD status and override auto-detection.

        Args:
            mode: ON --OHD manual mode with on-ear state.
                  OFF --OHD manual mode with off-ear state.
                  AUTO --OHD auto-detection mode.
        Raises:
            DeviceError: OHD Command failure.
        """
        logging.debug('Inside set_ohd()...')
        try:
            if mode != 'AUTO':
                # Set up OHD manual mode
                self.cmd('Test 14 0 2 1')
                if mode == 'ON':
                    # Detects on-ear
                    self.cmd('Test 14 0 2 1 0x3')
                else:
                    # Detects off-ear
                    self.cmd('Test 14 0 2 1 0x0')
            else:
                # Default mode (auto detect.)
                self.cmd('Test 14 0 2 0')
        except DeviceError:
            logging.exception('OHD cmd failed')

    def music_control_events(self, cmd, regexp=None, wait=.5):
        """Sends the EvtHex to control media player.

        Arguments:
            cmd: the command to perform.
            regexp: Optional pattern to validate the event logs.

        Returns:
            Boolean: True if the command triggers the correct events on the
                     device, False otherwise.

        # TODO(nviboonchan:) Add more supported commands.
        Supported commands:
            'PlayPause'
            'VolumeUp'
            'VolumeDown',
        """
        cmd_regexp = {
            # Play/ Pause would need to pass the regexp argument since it's
            # sending the same event but returns different responses depending
            # on the device state.
            'VolumeUp': apollo_log_regex.VOLUP_REGEX,
            'VolumeDown': apollo_log_regex.VOLDOWN_REGEX,
        }
        if not regexp:
            if cmd not in cmd_regexp:
                logmsg = 'Expected pattern is not defined for event %s' % cmd
                logging.exception(logmsg)
                return False
            regexp = cmd_regexp[cmd]
        self.cmd('EvtHex %s' % apollo_sink_events.SINK_EVENTS['EventUsr' + cmd],
                 wait=wait)
        for line in self.cmd_log:
            if isinstance(line, str):
                if re.search(regexp, line):
                    return True
            elif isinstance(line, dict):
                if line.get('id', None) == 'AVRCP_PLAY_STATUS_CHANGE':
                    return True
        return False

    def avrcp(self, cmd):
        """sends the Audio/Video Remote Control Profile (avrcp) control command.

        Supported commands:
            'PlayPause'
            'Stop'
            'SkipForward',
            'SkipBackward',
            'FastForwardPress',
            'FastForwardRelease',
            'RewindPress',
            'RewindRelease',
            'ShuffleOff',
            'ShuffleAllTrack',
            'ShuffleGroup',
            'RepeatOff':,
            'RepeatSingleTrack',
            'RepeatAllTrack',
            'RepeatGroup',
            'Play',
            'Pause',
            'ToggleActive',
            'NextGroupPress',
            'PreviousGroupPress',
            'NextGroupRelease',
            'PreviousGroupRelease',

        Args:
            cmd: The avrcp command.

        """
        cmd = 'EventUsrAvrcp' + cmd
        logging.debug(cmd)
        try:
            self.cmd(_evt_hex(cmd))
        except DeviceError:
            logging.exception('avrcp cmd failed')

    def enable_log(self, levels=None):
        """Enable specified logs."""
        logging.debug('Inside enable_log()...')
        if levels is None:
            levels = ['ALL']
        masks = hex(
            sum([int(apollo_sink_events.LOG_FEATURES[x], 16) for x in levels]))
        try:
            self.cmd('LogOff %s' % apollo_sink_events.LOG_FEATURES['ALL'])
            return self.cmd('LogOn %s' % masks)
        except DeviceError:
            logging.exception('Enable log failed')

    def disable_log(self, levels=None):
        """Disable specified logs."""
        logging.debug('Inside disable_log()...')
        if levels is None:
            levels = ['ALL']
        masks = hex(
            sum([int(apollo_sink_events.LOG_FEATURES[x], 16) for x in levels]))
        try:
            self.cmd('LogOn %s' % apollo_sink_events.LOG_FEATURES['ALL'])
            return self.cmd('LogOff %s' % masks)
        except DeviceError:
            logging.exception('Disable log failed')

    def write_to_flash(self, file_name=None):
        """Write file to external flash.

        Note: Assume pv is installed. If not, install it by
              'apt-get install pv'.

        Args:
            file_name: Full path file name.

        Returns:
            Boolean: True if write to partition is successful. False otherwise.
        """
        logging.debug('Inside write_to_flash()...')
        if not os.path.isfile(file_name):
            message = 'DFU file %s not found.'.format(file_name)
            logging.exception(message)
            return False
        logging.info(
            'Write file {} to external flash partition ...'.format(file_name))
        image_size = os.path.getsize(file_name)
        logging.info('image size is {}'.format(image_size))
        results = self.cmd('Ota {}'.format(image_size), wait=3)
        logging.debug('Result of Ota command' + str(results))
        if any(OTA_VERIFICATION_FAILED in result for result in results[0]):
            return False
        # finished cmd Ota
        if (any('OTA_ERASE_PARTITION' in result.values() for result in
                results[0] if
                isinstance(result, dict)) or
                any('OTA erasd ptns' in result for result in results[0])):
            try:
                # -B: buffer size in bytes, -L rate-limit in B/s.
                subcmd = ('pv --force -B 160 -L 10000 %s > %s' %
                          (file_name, self.commander_port))
                logging.info(subcmd)
                p = subprocess.Popen(subcmd, stdout=subprocess.PIPE, shell=True)
            except OSError:
                logging.exception(
                    'pv not installed, please install by: apt-get install pv')
                return False
            try:
                res = self.read_serial_port(read_until=6)
            except DeviceError:
                logging.exception('Unable to read the device port')
                return False
            for line in res:
                if isinstance(line, dict):
                    logging.info(line)
                else:
                    match = re.search(OTA_RECEIVE_CSR_REGEX, line)
                    if match:
                        logging.info(
                            'OTA Image received. Transfer is in progress...')
                        # Polling during a transfer could miss the final message
                        # when the device reboots, so we wait until the transfer
                        # completes.
                        p.wait()
                        return True
            # No image transfer in progress.
            return False
        else:
            return False

    def flash_from_file(self, file_name, reconnect=True):
        """Upgrade Apollo from an image file.

        Args:
            file_name: DFU file name. eg. /google/data/ro/teams/wearables/
                       apollo/ota/master/v76/apollo.dfu
            reconnect: True to reconnect the device after flashing
        Returns:
            Bool: True if the upgrade is successful. False otherwise.
        """
        logging.debug('Inside flash_from_file()...')
        if self.write_to_flash(file_name):
            logging.info('OTA image transfer is completed')
            if reconnect:
                # Transfer is completed; waiting for the device to reboot.
                logging.info('wait to make sure old connection disappears.')
                self.wait_for_reset(timeout=150)
                self.reconnect()
                logging.info('BudsDevice reboots successfully after OTA.')
            return True

    def open_mic(self, post_delay=5):
        """Open Microphone on the device using EvtHex command.

        Args:
            post_delay: time delay in seconds after the microphone is opened.

        Returns:
            Returns True or False based on whether the command was executed.
        """
        logging.debug('Inside open_mic()...')
        success, _ = self._cmd('Voicecmd 1', post_delay)
        return success

    def close_mic(self, post_delay=5):
        """Close Microphone on the device using EvtHex command.

        Args:
            post_delay: time delay in seconds after the microphone is closed.

        Returns:
            Returns true or false based on whether the command was executed.
        """
        logging.debug('Inside close_mic()...')
        success, _ = self._cmd('Voicecmd 0', post_delay)
        return success

    def touch_key_press_event(self, wait=1):
        """send key press event command.

        Args:
            wait: Inject delay after key press to simulate real touch event .
        """
        logging.debug('Inside KeyPress()...')
        self._cmd('Touch 6')
        self.wait(wait)

    def touch_tap_event(self, wait_if_pause=10):
        """send key release event after key press to simulate single tap.

        Args:
            wait_if_pause: Inject delay after avrcp pause was detected.

        Returns:
            Returns False if avrcp play orp ause not detected else True.
        """
        logging.debug('Inside Touch Tap event()...')
        self._cmd('Touch 4')
        for line in self.cmd_log:
            if 'avrcp play' in line:
                logging.info('avrcp play detected')
                return True
            if 'avrcp pause' in line:
                logging.info('avrcp pause detected')
                self.wait(wait_if_pause)
                return True
        return False

    def touch_hold_up_event(self):
        """Open Microphone on the device using touch hold up command.

        Returns:
            Returns True or False based on whether the command was executed.
        """
        logging.debug('Inside open_mic()...')
        self._cmd('Touch 3')
        for line in self.cmd_log:
            if 'Button 1 LONG_BEGIN' in line:
                logging.info('mic open success')
                return True
        return False

    def touch_hold_down_event(self):
        """Close Microphone on the device using touch hold down command.

        Returns:
            Returns true or false based on whether the command was executed.
        """
        logging.debug('Inside close_mic()...')
        self._cmd('Touch 8')
        for line in self.cmd_log:
            if 'Button 1 LONG_END' in line:
                logging.info('mic close success')
                return True
        return False

    def tap(self):
        """Performs a Tap gesture."""
        logging.debug('Inside tap()')
        self.cmd('ButtonTap 0')

    def hold(self, duration):
        """Tap and hold a button.

        Args:
            duration: (int) duration in milliseconds.
        """
        logging.debug('Inside hold()')
        self.cmd('ButtonHold ' + str(duration))

    def swipe(self, direction):
        """Perform a swipe gesture.

        Args:
            direction: (int) swipe direction 1 forward, 0 backward.
        """
        logging.debug('Inside swipe()')
        self.cmd('ButtonSwipe ' + direction)

    def get_pskey(self, key):
        """Fetch value from persistent store."""
        try:
            cmd = 'GetPSHex ' + apollo_sink_events.PSKEY[key]
        except KeyError:
            raise DeviceError('PS Key: %s not found' % key)
        pskey = ''
        try:
            ret = self.cmd(cmd)
            for result in ret[0]:
                if not re.search(r'pskey', result.lower()) and LOG_REGEX.match(
                        result):
                    # values are broken into words separated by spaces.
                    pskey += LOG_REGEX.match(result).group('msg').replace(' ',
                                                                          '')
                else:
                    continue
        except DeviceError:
            logging.exception('GetPSHex cmd failed')
        return pskey

    def get_version(self):
        """Return a device version information.

        Note: Version information is obtained from the firmware loader. Old
        information is lost when firmware is updated.
        Returns:
            A dictionary of device version info. eg.
            {
                'Fw Build': '73',
                'OTA Status': 'No OTA performed before this boot',
            }

        """
        logging.debug('Inside get_version()...')
        success, result = self._cmd('GetVer', throw_error=False)
        status = {}
        if result:
            for line in result:
                if isinstance(line, dict):
                    status['build'] = line['vm_build_number']
                    status['psoc_build'] = line['psoc_version']
                    status['debug'] = line['csr_fw_debug_build']
                    status['Fw Build Label'] = line['build_label']
                    if 'last_ota_status' in line.keys():
                        # Optional value in the proto response
                        status['OTA Status'] = line['last_ota_status']
                    else:
                        status['OTA Status'] = 'No info'
        return success, status

    def get_earcon_version(self):
        """Return a device Earson version information.

        Returns:
            Boolean:  True if success, False otherwise.
            String: Earon Version e.g. 7001 0201 6100 0000

        """
        # TODO(nviboonchan): Earcon version format would be changed in the
        # future.
        logging.debug('Inside get_earcon_version()...')
        result = self.get_pskey('PSKEY_EARCON_VERSION')
        if result:
            return True, result
        else:
            return False, None

    def get_bt_status(self):
        """Return a device bluetooth connection information.

        Returns:
            A dictionary of bluetooth status. eg.
            {
                'Comp. App': 'FALSE',
               'HFP (pri.)', 'FALSE',
               'HFP (sec.)': 'FALSE',
               'A2DP (pri.)': 'FALSE',
               'A2DP (sec.)': 'FALSE',
               'A2DP disconnects': '3',
               'A2DP Role (pri.)': 'slave',
               'A2DP RSSI (pri.)': '-Touch'
            }
        """
        logging.debug('Inside get_bt_status()...')
        return self._get_status('GetBTStatus')

    def get_conn_devices(self):
        """Gets the BT connected devices.

        Returns:
            A dictionary of BT connected devices. eg.
            {
                'HFP Pri': 'xxxx',
                'HFP Sec': 'xxxx',
                'A2DP Pri': 'xxxx',
                'A2DP Sec': 'xxxx',
                'RFCOMM devices': 'xxxx',
                'CTRL': 'xxxx',
                'AUDIO': 'None',
                'DEBUG': 'None',
                'TRANS': 'None'
             }

        Raises:
            ResponseError: If unexpected response occurs.
        """
        response_regex = re.compile('[0-9]+ .+: ')
        connected_status = {}
        response = self.cmd('GetConnDevices')
        if not response:
            raise ResponseError(
                'No response returned by GetConnDevices command')
        for line in response[0]:
            if response_regex.search(line):
                profile, value = line[line.find(' '):].split(':', 1)
                connected_status[profile] = value
        if not connected_status:
            raise ResponseError('No BT Profile Status in response.')
        return connected_status

    def _get_status(self, cmd):
        """Return a device status information."""
        status = {}
        try:
            results = self.cmd(cmd)
        except DeviceError as ex:
            # logging.exception('{} cmd failed'.format(cmd))
            logging.warning('Failed to get device status info.')
            raise ex
        results = results[0]
        for result in results:
            match = re.match(STATUS_REGEX, result)
            if match:
                key = match.group('key')
                value = match.group('value')
                status.update({key: value})
        return status

    def is_streaming(self):
        """Returns the music streaming status on Apollo.

        Returns:
            Boolean: True if device is streaming music. False otherwise.
        """

        status = self.cmd('GetDSPStatus')
        if any('active feature mask: 0' in log for log in
               status[0]):
            return False
        elif any('active feature mask: 2' in log for log in
                 status[0]):
            return True
        else:
            return False

    def is_in_call(self):
        """Returns the phone call status on Apollo.

        Returns:
            Boolean: True if device has incoming call. False otherwise.
        """

        status = self.cmd('GetDSPStatus')
        if not any('Inc' or 'out' in log for log in status[0]):
            return False
        return True

    def is_device_limbo(self):
        """Check if device is in Limbo state.

        Returns:
            Boolean: True if device is in limbo state, False otherwise.
        """
        device_state = self.get_device_state()
        logging.info('BudsDevice "{}" state {}'.format(self.serial_number,
                                                       device_state))
        return device_state == 'limbo'

    def get_device_state(self):
        """Get state of the device.

        Returns:
            String representing the device state.

        Raises:
            DeviceError: If command fails.
        """
        _, status = self._cmd('GetDSPStatus')
        for stat in status:
            if isinstance(stat, dict):
                logging.info(stat)
                return stat['sink_state'].lower()
        raise DeviceError('BudsDevice state not found in GetDSPStatus.')

    def set_stay_connected(self, value):
        """Run command to set the value for SetAlwaysConnected.

        Args:
            value: (int) 1 to keep connection engages at all time,
                         0 for restoring
        Returns:
            the set state of type int (0 or 1) or None if not applicable
        """

        if int(self.version) >= 1663:
            self._cmd('SetAlwaysConnected {}'.format(value))
            logging.info('Setting sleep on idle to {}'.format(value))
            return value

    def get_codec(self):
        """Get device's current audio codec.

        Returns:
            String representing the audio codec.

        Raises:
            DeviceError: If command fails.
        """
        success, status = self._cmd('get_codec')
        logging.info('---------------------------------------')
        logging.info(status)
        logging.info('---------------------------------------')
        if success:
            for line in status:
                if isinstance(line, dict):
                    logging.info('Codec found: %s'.format(line['codec']))
                    return line['codec']
        raise DeviceError('BudsDevice state not found in get_codec.')

    def crash_dump_detection(self):
        """Reads crash dump determines if a crash is detected.

        Returns:
            True if crash detection is supported and if a new crash is found.
            False otherwise.
        """
        # Detects if crashdump output is new
        new_crash_regex = r'new crash = ([01]+)'
        # filter crashdump for just the trace
        crash_stack_regex = r'BASIC(.*)\n[\d]+ APP_STACK(.*)\n'
        # remove time stamp commander output
        timestamp_remover_regex = '\n[\\d]+ '

        logging.debug('Inside IsCrashDumpDetection()...')
        cmd_return = self.cmd('CrashDump', wait=1)
        crash_dump_str = '\n'.join(cmd_return[0])
        logging.info(crash_dump_str)
        try:
            # check for crash
            match = re.search(new_crash_regex, crash_dump_str)
            if match is not None:
                if match.groups()[0] == '1':  # new crash found
                    logging.error('Crash detected!!')
                    basic, app_stack = re.search(crash_stack_regex,
                                                 crash_dump_str,
                                                 re.DOTALL).groups()
                    # remove time stamps from capture
                    basic = re.sub(timestamp_remover_regex, '', basic)
                    app_stack = re.sub(timestamp_remover_regex, '', app_stack)
                    # write to log
                    # pylint: disable=bad-whitespace
                    logging.info(
                        '\n&270d = %s\n&270e = %s\n' % (basic, app_stack))
                    # pylint: enable=bad-whitespace
                    return True
                else:  # no new crash
                    logging.info('No crash detected')
                    return False
        except AttributeError:
            logging.exception(
                'Apollo crash dump output is not in expected format')
            raise DeviceError('Apollo crash dump not in expected format')

    @property
    def version(self):
        """Application version.

        Returns:
            (String) Firmware version.
        """
        _, result = self.get_version()
        return result['build']

    @property
    def bluetooth_address(self):
        """Bluetooth MAC address.

        Returns:
            a string representing 48bit BT MAC address in Hex.

        Raises:
            DeviceError: Unable to find BT Address
        """
        results = self.get_pskey('PSKEY_BDADDR')
        if not results:
            raise DeviceError('Unable to find BT Address')
        logging.info(results)
        # Bluetooth lower address part, upper address part and non-significant
        # address part.
        bt_lap = results[2:8]
        bt_uap = results[10:12]
        bt_nap = results[12:16]
        results = bt_nap + bt_uap + bt_lap

        return ':'.join(map(''.join, zip(*[iter(results)] * 2))).upper()

    @property
    def device_name(self):
        """Device Friendly Name.

        Returns:
            a string representing device friendly name.

        Raises:
            DeviceError: Unable to find a wearable device name.
        """
        result = self.get_pskey('PSKEY_DEVICE_NAME')
        if not result:
            raise DeviceError('Unable to find BudsDevice Name')
        logging.info(_to_ascii(result))
        return _to_ascii(result)

    @property
    def stay_connected(self):
        return self.stay_connected_state

    @stay_connected.setter
    def stay_connected(self, value):
        self.stay_connected_state = self.set_stay_connected(value)

    def read_serial_port(self, read_until=None):
        """Read serial port until specified read_until value in seconds."""
        # use default read_until value if not specified
        if read_until:
            time.sleep(read_until)
        res = self.connection_handle.read()
        buf_read = []
        for line in res:
            if apollo_log_decoder.is_automation_protobuf(line):
                decoded = apollo_log_decoder.decode(line)
                buf_read.append(decoded)
            else:
                buf_read.append(line)
        return buf_read

    def wait_for_reset(self, timeout=30):
        """waits for the device to reset by check serial enumeration.

        Checks every .5 seconds for the port.

        Args:
            timeout: The max time to wait for the device to disappear.

        Returns:
            Bool: True if the device reset was detected. False if not.
        """
        start_time = time.time()
        while True:
            res = subprocess.Popen(['ls', self.commander_port],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
            res.communicate()
            if res.returncode != 0:
                logging.info('BudsDevice reset detected')
                return True
            elif (time.time() - start_time) > timeout:
                logging.info('Timeout waiting for device to reset.....')
                return False
            else:
                time.sleep(.5)

    def set_in_case(self, reconnect=True):
        """Simulates setting apollo in case and wait for device to come up.

        Args:
            reconnect: bool - if method should block until reconnect
        """
        logging.info('Setting device in case')
        out = self.send('Pow 2')
        for i in out:
            if 'No OTA wakeup condition' in i:
                logging.info('No wake up condition.')
            elif 'STM Wakeup 10s' in i:
                logging.info('Wake up condition detected.')
        if reconnect:
            self.wait_for_reset()
            self.reconnect()


class ParentDevice(BudsDevice):
    """Wrapper object for Device that addresses b10 recovery and build flashing.

    Recovery mechanism:
    In case a serial connection could not be established to b10, the recovery
    mechanism is activated  ONLY if'recover_device' is set to 'true' and
    b29_serial is defined in config file. This helps recover a device that has a
    bad build installed.
    """

    def __init__(self, serial_number, recover_device=False, b29_serial=None):
        # if recover device parameter is supplied and there is an error in
        # instantiating B10 try to recover device instantiating b10 has to fail
        # at most $tries_before_recovery time before initiating a recovery
        # try to run the recovery at most $recovery_times before raising Error
        # after the first recovery attempt failure try to reset b29 each
        # iteration
        self.b29_device = None
        if recover_device:
            if b29_serial is None:
                logging.error('B29 serial not defined')
                raise Error(
                    'Recovery failed because "b29_serial" definition not '
                    'present in device manifest file')
            else:
                self.b29_device = B29Device(b29_serial)
            tries_before_recovery = 5
            recovery_tries = 5
            for attempt in range(tries_before_recovery):
                try:
                    # build crash symptoms varies based on the nature of the
                    # crash connectError is thrown if the device never shows up
                    # in /dev/ sometimes device shows and can connect but
                    # sending commands fails or crashes apollo in that case,
                    # DeviceError is thrown
                    super().__init__(serial_number, commander_port=None,
                                     log_port=None, serial_logger=None)
                    break
                except (ConnectError, DeviceError) as ex:
                    logging.warning(
                        'Error initializing apollo object - # of attempt '
                        'left : %d' % (tries_before_recovery - attempt - 1))
                    if attempt + 1 >= tries_before_recovery:
                        logging.error(
                            'Retries exhausted - now attempting to restore '
                            'golden image')
                        for recovery_attempt in range(recovery_tries):
                            if not self.b29_device.restore_golden_image():
                                logging.error('Recovery failed - retrying...')
                                self.b29_device.reset_charger()
                                continue
                            # try to instantiate now
                            try:
                                super().__init__(serial_number,
                                                 commander_port=None,
                                                 log_port=None,
                                                 serial_logger=None)
                                break
                            except (ConnectError, DeviceError):
                                if recovery_attempt == recovery_tries - 1:
                                    raise Error(
                                        'Recovery failed - ensure that there '
                                        'is no mismatching serial numbers of '
                                        'b29 and b10 is specified in config')
                                else:
                                    logging.warning(
                                        'Recovery attempt failed - retrying...')
                    time.sleep(2)
        else:
            super().__init__(serial_number, commander_port=None, log_port=None,
                             serial_logger=None)
        # set this to prevent sleep
        self.set_stay_connected(1)

    def get_info(self):
        information_dictionary = {}
        information_dictionary['type'] = self.dut_type
        information_dictionary['serial'] = self.serial_number
        information_dictionary['log port'] = self.log_port
        information_dictionary['command port'] = self.commander_port
        information_dictionary['bluetooth address'] = self.bluetooth_address
        success, build_dict = self.get_version()
        information_dictionary['build'] = build_dict
        # Extract the build number as a separate key. Useful for BigQuery.
        information_dictionary['firmware build number'] = build_dict.get(
            'build', '9999')
        information_dictionary['name'] = self.device_name
        if self.b29_device:
            information_dictionary['b29 serial'] = self.b29_device.serial
            information_dictionary['b29 firmware'] = self.b29_device.fw_version
            information_dictionary['b29 commander port'] = self.b29_device.port
            information_dictionary[
                'b29 app version'] = self.b29_device.app_version
        return information_dictionary

    def setup(self, **kwargs):
        """

        Args:
            apollo_build: if specified, will be used in flashing the device to
                          that build prior to running any of the tests. If not
                          specified flashing is skipped.
        """
        if 'apollo_build' in kwargs and kwargs['apollo_build'] is not None:
            build = kwargs['apollo_build']
            X20_REGEX = re.compile(r'/google/data/')
            if not os.path.exists(build) or os.stat(build).st_size == 0:
                # if x20 path, retry on file-not-found error or if file size is
                # zero b/c X20 path does not update immediately
                if X20_REGEX.match(build):
                    for i in range(20):
                        # wait until file exists and size is > 0 w/ 6 second
                        # interval on retry
                        if os.path.exists(build) and os.stat(build).st_size > 0:
                            break

                        if i == 19:
                            logging.error('Build path (%s) does not exist or '
                                          'file size is 0 - aborted' % build)

                            raise Error('Specified build path (%s) does not '
                                        'exist or file size is 0' % build)
                        else:
                            logging.warning('Build path (%s) does not exist or '
                                            'file size is 0 - retrying...' %
                                            build)
                            time.sleep(6)
                else:
                    raise Error('Specified build path (%s) does not exist or '
                                'file size is 0' % build)
                self.flash_from_file(file_name=build, reconnect=True)
        else:
            logging.info('Not flashing apollo.')

    def teardown(self, **kwargs):
        self.close()


def _evt_hex(cmd):
    return 'EvtHex ' + apollo_sink_events.SINK_EVENTS[cmd]


def _to_ascii(orig):
    # Returned value need to be byte swapped. Remove last octet if it is 0.
    result = _byte_swap(orig)
    result = result[:-2] if result[-2:] == '00' else result
    return bytearray.fromhex(result).decode()


def _byte_swap(orig):
    """Simple function to swap bytes order.

    Args:
        orig: original string

    Returns:
        a string with bytes swapped.
        eg. orig = '6557276920736952006f'.
        After swap, return '57656927732052696f00'
    """
    return ''.join(
        sum([(c, d, a, b) for a, b, c, d in zip(*[iter(orig)] * 4)], ()))
