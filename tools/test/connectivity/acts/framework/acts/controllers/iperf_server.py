#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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

import json
import logging
import math
import os
import shlex
import subprocess
import threading
import time

from acts import context
from acts import utils
from acts.controllers.android_device import AndroidDevice
from acts.controllers.utils_lib.ssh import connection
from acts.controllers.utils_lib.ssh import settings
from acts.event import event_bus
from acts.event.decorators import subscribe_static
from acts.event.event import TestClassBeginEvent
from acts.event.event import TestClassEndEvent
from acts.libs.proc import job

ACTS_CONTROLLER_CONFIG_NAME = 'IPerfServer'
ACTS_CONTROLLER_REFERENCE_NAME = 'iperf_servers'
KILOBITS = 1024
MEGABITS = KILOBITS * 1024
GIGABITS = MEGABITS * 1024
BITS_IN_BYTE = 8


def create(configs):
    """ Factory method for iperf servers.

    The function creates iperf servers based on at least one config.
    If configs only specify a port number, a regular local IPerfServer object
    will be created. If configs contains ssh settings or and AndroidDevice,
    remote iperf servers will be started on those devices

    Args:
        configs: config parameters for the iperf server
    """
    results = []
    for c in configs:
        if type(c) in (str, int) and str(c).isdigit():
            results.append(IPerfServer(int(c)))
        elif type(c) is dict and 'AndroidDevice' in c and 'port' in c:
            results.append(IPerfServerOverAdb(c['AndroidDevice'], c['port']))
        elif type(c) is dict and 'ssh_config' in c and 'port' in c:
            results.append(
                IPerfServerOverSsh(c['ssh_config'],
                                   c['port'],
                                   test_interface=c.get('test_interface')))
        else:
            raise ValueError(
                'Config entry %s in %s is not a valid IPerfServer '
                'config.' % (repr(c), configs))
    return results


def get_info(iperf_servers):
    """Placeholder for info about iperf servers

    Returns:
        None
    """
    return None


def destroy(iperf_server_list):
    for iperf_server in iperf_server_list:
        try:
            iperf_server.stop()
        except Exception:
            logging.exception('Unable to properly clean up %s.' % iperf_server)


class IPerfResult(object):
    def __init__(self, result_path, reporting_speed_units='Mbytes'):
        """Loads iperf result from file.

        Loads iperf result from JSON formatted server log. File can be accessed
        before or after server is stopped. Note that only the first JSON object
        will be loaded and this funtion is not intended to be used with files
        containing multiple iperf client runs.
        """
        # if result_path isn't a path, treat it as JSON
        self.reporting_speed_units = reporting_speed_units
        if not os.path.exists(result_path):
            self.result = json.loads(result_path)
        else:
            try:
                with open(result_path, 'r') as f:
                    iperf_output = f.readlines()
                    if '}\n' in iperf_output:
                        iperf_output = iperf_output[:iperf_output.index('}\n'
                                                                        ) + 1]
                    iperf_string = ''.join(iperf_output)
                    iperf_string = iperf_string.replace('nan', '0')
                    self.result = json.loads(iperf_string)
            except ValueError:
                with open(result_path, 'r') as f:
                    # Possibly a result from interrupted iperf run,
                    # skip first line and try again.
                    lines = f.readlines()[1:]
                    self.result = json.loads(''.join(lines))

    def _has_data(self):
        """Checks if the iperf result has valid throughput data.

        Returns:
            True if the result contains throughput data. False otherwise.
        """
        return ('end' in self.result) and ('sum_received' in self.result['end']
                                           or 'sum' in self.result['end'])

    def _get_reporting_speed(self, network_speed_in_bits_per_second):
        """Sets the units for the network speed reporting based on how the
        object was initiated.  Defaults to Megabytes per second.  Currently
        supported, bits per second (bits), kilobits per second (kbits), megabits
        per second (mbits), gigabits per second (gbits), bytes per second
        (bytes), kilobits per second (kbytes), megabits per second (mbytes),
        gigabytes per second (gbytes).

        Args:
            network_speed_in_bits_per_second: The network speed from iperf in
                bits per second.

        Returns:
            The value of the throughput in the appropriate units.
        """
        speed_divisor = 1
        if self.reporting_speed_units[1:].lower() == 'bytes':
            speed_divisor = speed_divisor * BITS_IN_BYTE
        if self.reporting_speed_units[0:1].lower() == 'k':
            speed_divisor = speed_divisor * KILOBITS
        if self.reporting_speed_units[0:1].lower() == 'm':
            speed_divisor = speed_divisor * MEGABITS
        if self.reporting_speed_units[0:1].lower() == 'g':
            speed_divisor = speed_divisor * GIGABITS
        return network_speed_in_bits_per_second / speed_divisor

    def get_json(self):
        """Returns the raw json output from iPerf."""
        return self.result

    @property
    def error(self):
        return self.result.get('error', None)

    @property
    def avg_rate(self):
        """Average UDP rate in MB/s over the entire run.

        This is the average UDP rate observed at the terminal the iperf result
        is pulled from. According to iperf3 documentation this is calculated
        based on bytes sent and thus is not a good representation of the
        quality of the link. If the result is not from a success run, this
        property is None.
        """
        if not self._has_data() or 'sum' not in self.result['end']:
            return None
        bps = self.result['end']['sum']['bits_per_second']
        return self._get_reporting_speed(bps)

    @property
    def avg_receive_rate(self):
        """Average receiving rate in MB/s over the entire run.

        This data may not exist if iperf was interrupted. If the result is not
        from a success run, this property is None.
        """
        if not self._has_data() or 'sum_received' not in self.result['end']:
            return None
        bps = self.result['end']['sum_received']['bits_per_second']
        return self._get_reporting_speed(bps)

    @property
    def avg_send_rate(self):
        """Average sending rate in MB/s over the entire run.

        This data may not exist if iperf was interrupted. If the result is not
        from a success run, this property is None.
        """
        if not self._has_data() or 'sum_sent' not in self.result['end']:
            return None
        bps = self.result['end']['sum_sent']['bits_per_second']
        return self._get_reporting_speed(bps)

    @property
    def instantaneous_rates(self):
        """Instantaneous received rate in MB/s over entire run.

        This data may not exist if iperf was interrupted. If the result is not
        from a success run, this property is None.
        """
        if not self._has_data():
            return None
        intervals = [
            self._get_reporting_speed(interval['sum']['bits_per_second'])
            for interval in self.result['intervals']
        ]
        return intervals

    @property
    def std_deviation(self):
        """Standard deviation of rates in MB/s over entire run.

        This data may not exist if iperf was interrupted. If the result is not
        from a success run, this property is None.
        """
        return self.get_std_deviation(0)

    def get_std_deviation(self, iperf_ignored_interval):
        """Standard deviation of rates in MB/s over entire run.

        This data may not exist if iperf was interrupted. If the result is not
        from a success run, this property is None. A configurable number of
        beginning (and the single last) intervals are ignored in the
        calculation as they are inaccurate (e.g. the last is from a very small
        interval)

        Args:
            iperf_ignored_interval: number of iperf interval to ignored in
            calculating standard deviation

        Returns:
            The standard deviation.
        """
        if not self._has_data():
            return None
        instantaneous_rates = self.instantaneous_rates[iperf_ignored_interval:
                                                       -1]
        avg_rate = math.fsum(instantaneous_rates) / len(instantaneous_rates)
        sqd_deviations = ([(rate - avg_rate)**2
                           for rate in instantaneous_rates])
        std_dev = math.sqrt(
            math.fsum(sqd_deviations) / (len(sqd_deviations) - 1))
        return std_dev


class IPerfServerBase(object):
    # Keeps track of the number of IPerfServer logs to prevent file name
    # collisions.
    __log_file_counter = 0

    __log_file_lock = threading.Lock()

    def __init__(self, port):
        self._port = port
        # TODO(markdr): We shouldn't be storing the log files in an array like
        # this. Nobody should be reading this property either. Instead, the
        # IPerfResult should be returned in stop() with all the necessary info.
        # See aosp/1012824 for a WIP implementation.
        self.log_files = []

    @property
    def port(self):
        raise NotImplementedError('port must be specified.')

    @property
    def started(self):
        raise NotImplementedError('started must be specified.')

    def start(self, extra_args='', tag=''):
        """Starts an iperf3 server.

        Args:
            extra_args: A string representing extra arguments to start iperf
                server with.
            tag: Appended to log file name to identify logs from different
                iperf runs.
        """
        raise NotImplementedError('start() must be specified.')

    def stop(self):
        """Stops the iperf server.

        Returns:
            The name of the log file generated from the terminated session.
        """
        raise NotImplementedError('stop() must be specified.')

    def _get_full_file_path(self, tag=None):
        """Returns the full file path for the IPerfServer log file.

        Note: If the directory for the file path does not exist, it will be
        created.

        Args:
            tag: The tag passed in to the server run.
        """
        out_dir = self.log_path

        with IPerfServerBase.__log_file_lock:
            tags = [tag, IPerfServerBase.__log_file_counter]
            out_file_name = 'IPerfServer,%s.log' % (','.join(
                [str(x) for x in tags if x != '' and x is not None]))
            IPerfServerBase.__log_file_counter += 1

        file_path = os.path.join(out_dir, out_file_name)
        self.log_files.append(file_path)
        return file_path

    @property
    def log_path(self):
        current_context = context.get_current_context()
        full_out_dir = os.path.join(current_context.get_full_output_path(),
                                    'IPerfServer%s' % self.port)

        # Ensure the directory exists.
        os.makedirs(full_out_dir, exist_ok=True)

        return full_out_dir


def _get_port_from_ss_output(ss_output, pid):
    pid = str(pid)
    lines = ss_output.split('\n')
    for line in lines:
        if pid in line:
            # Expected format:
            # tcp LISTEN  0 5 *:<PORT>  *:* users:(("cmd",pid=<PID>,fd=3))
            return line.split()[4].split(':')[-1]
    else:
        raise ProcessLookupError('Could not find started iperf3 process.')


class IPerfServer(IPerfServerBase):
    """Class that handles iperf server commands on localhost."""
    def __init__(self, port=5201):
        super().__init__(port)
        self._hinted_port = port
        self._current_log_file = None
        self._iperf_process = None
        self._last_opened_file = None

    @property
    def port(self):
        return self._port

    @property
    def started(self):
        return self._iperf_process is not None

    def start(self, extra_args='', tag=''):
        """Starts iperf server on local machine.

        Args:
            extra_args: A string representing extra arguments to start iperf
                server with.
            tag: Appended to log file name to identify logs from different
                iperf runs.
        """
        if self._iperf_process is not None:
            return

        self._current_log_file = self._get_full_file_path(tag)

        # Run an iperf3 server on the hinted port with JSON output.
        command = ['iperf3', '-s', '-p', str(self._hinted_port), '-J']

        command.extend(shlex.split(extra_args))

        if self._last_opened_file:
            self._last_opened_file.close()
        self._last_opened_file = open(self._current_log_file, 'w')
        self._iperf_process = subprocess.Popen(command,
                                               stdout=self._last_opened_file,
                                               stderr=subprocess.DEVNULL)
        for attempts_left in reversed(range(3)):
            try:
                self._port = int(
                    _get_port_from_ss_output(
                        job.run('ss -l -p -n | grep iperf').stdout,
                        self._iperf_process.pid))
                break
            except ProcessLookupError:
                if attempts_left == 0:
                    raise
                logging.debug('iperf3 process not started yet.')
                time.sleep(.01)

    def stop(self):
        """Stops the iperf server.

        Returns:
            The name of the log file generated from the terminated session.
        """
        if self._iperf_process is None:
            return

        if self._last_opened_file:
            self._last_opened_file.close()
            self._last_opened_file = None

        self._iperf_process.terminate()
        self._iperf_process = None

        return self._current_log_file

    def __del__(self):
        self.stop()


class IPerfServerOverSsh(IPerfServerBase):
    """Class that handles iperf3 operations on remote machines."""
    def __init__(self, ssh_config, port, test_interface=None):
        super().__init__(port)
        ssh_settings = settings.from_config(ssh_config)
        self._ssh_session = connection.SshConnection(ssh_settings)

        self._iperf_pid = None
        self._current_tag = None
        self.hostname = ssh_settings.hostname
        try:
            # A test interface can only be found if an ip address is specified.
            # A fully qualified hostname will return None for the
            # test_interface.
            self.test_interface = self._get_test_interface_based_on_ip(
                test_interface)
        except Exception:
            self.test_interface = None

    @property
    def port(self):
        return self._port

    @property
    def started(self):
        return self._iperf_pid is not None

    def _get_remote_log_path(self):
        return 'iperf_server_port%s.log' % self.port

    def _get_test_interface_based_on_ip(self, test_interface):
        """Gets the test interface for a particular IP if the test interface
            passed in test_interface is None

        Args:
            test_interface: Either a interface name, ie eth0, or None

        Returns:
            The name of the test interface.
        """
        if test_interface:
            return test_interface
        return utils.get_interface_based_on_ip(self._ssh_session,
                                               self.hostname)

    def get_interface_ip_addresses(self, interface):
        """Gets all of the ip addresses, ipv4 and ipv6, associated with a
           particular interface name.

        Args:
            interface: The interface name on the device, ie eth0

    Returns:
        A list of dictionaries of the the various IP addresses:
            ipv4_private_local_addresses: Any 192.168, 172.16, or 10
                addresses
            ipv4_public_addresses: Any IPv4 public addresses
            ipv6_link_local_addresses: Any fe80:: addresses
            ipv6_private_local_addresses: Any fd00:: addresses
            ipv6_public_addresses: Any publicly routable addresses
    """
        return utils.get_interface_ip_addresses(self._ssh_session, interface)

    def renew_test_interface_ip_address(self):
        """Renews the test interface's IP address.  Necessary for changing
           DHCP scopes during a test.
        """
        utils.renew_linux_ip_address(self._ssh_session, self.test_interface)

    def start(self, extra_args='', tag='', iperf_binary=None):
        """Starts iperf server on specified machine and port.

        Args:
            extra_args: A string representing extra arguments to start iperf
                server with.
            tag: Appended to log file name to identify logs from different
                iperf runs.
            iperf_binary: Location of iperf3 binary. If none, it is assumed the
                the binary is in the path.
        """
        if self.started:
            return

        if not iperf_binary:
            logging.debug('No iperf3 binary specified.  '
                          'Assuming iperf3 is in the path.')
            iperf_binary = 'iperf3'
        else:
            logging.debug('Using iperf3 binary located at %s' % iperf_binary)
        iperf_command = '{} -s -J -p {}'.format(iperf_binary, self.port)

        cmd = '{cmd} {extra_flags} > {log_file}'.format(
            cmd=iperf_command,
            extra_flags=extra_args,
            log_file=self._get_remote_log_path())

        job_result = self._ssh_session.run_async(cmd)
        self._iperf_pid = job_result.stdout
        self._current_tag = tag

    def stop(self):
        """Stops the iperf server.

        Returns:
            The name of the log file generated from the terminated session.
        """
        if not self.started:
            return

        self._ssh_session.run_async('kill -9 {}'.format(str(self._iperf_pid)))
        iperf_result = self._ssh_session.run('cat {}'.format(
            self._get_remote_log_path()))

        log_file = self._get_full_file_path(self._current_tag)
        with open(log_file, 'w') as f:
            f.write(iperf_result.stdout)

        self._ssh_session.run_async('rm {}'.format(
            self._get_remote_log_path()))
        self._iperf_pid = None
        return log_file


# TODO(markdr): Remove this after automagic controller creation has been
# removed.
class _AndroidDeviceBridge(object):
    """A helper class for connecting serial numbers to AndroidDevices."""

    _test_class = None

    @staticmethod
    @subscribe_static(TestClassBeginEvent)
    def on_test_begin(event):
        _AndroidDeviceBridge._test_class = event.test_class

    @staticmethod
    @subscribe_static(TestClassEndEvent)
    def on_test_end(_):
        _AndroidDeviceBridge._test_class = None

    @staticmethod
    def android_devices():
        """A dict of serial -> AndroidDevice, where AndroidDevice is a device
        found in the current TestClass's controllers.
        """
        if not _AndroidDeviceBridge._test_class:
            return {}
        return {
            device.serial: device
            for device in _AndroidDeviceBridge._test_class.android_devices
        }


event_bus.register_subscription(
    _AndroidDeviceBridge.on_test_begin.subscription)
event_bus.register_subscription(_AndroidDeviceBridge.on_test_end.subscription)


class IPerfServerOverAdb(IPerfServerBase):
    """Class that handles iperf3 operations over ADB devices."""
    def __init__(self, android_device_or_serial, port):
        """Creates a new IPerfServerOverAdb object.

        Args:
            android_device_or_serial: Either an AndroidDevice object, or the
                serial that corresponds to the AndroidDevice. Note that the
                serial must be present in an AndroidDevice entry in the ACTS
                config.
            port: The port number to open the iperf server on.
        """
        super().__init__(port)
        self._android_device_or_serial = android_device_or_serial

        self._iperf_process = None
        self._current_tag = ''

    @property
    def port(self):
        return self._port

    @property
    def started(self):
        return self._iperf_process is not None

    @property
    def _android_device(self):
        if isinstance(self._android_device_or_serial, AndroidDevice):
            return self._android_device_or_serial
        else:
            return _AndroidDeviceBridge.android_devices()[
                self._android_device_or_serial]

    def _get_device_log_path(self):
        return '~/data/iperf_server_port%s.log' % self.port

    def start(self, extra_args='', tag='', iperf_binary=None):
        """Starts iperf server on an ADB device.

        Args:
            extra_args: A string representing extra arguments to start iperf
                server with.
            tag: Appended to log file name to identify logs from different
                iperf runs.
            iperf_binary: Location of iperf3 binary. If none, it is assumed the
                the binary is in the path.
        """
        if self._iperf_process is not None:
            return

        if not iperf_binary:
            logging.debug('No iperf3 binary specified.  '
                          'Assuming iperf3 is in the path.')
            iperf_binary = 'iperf3'
        else:
            logging.debug('Using iperf3 binary located at %s' % iperf_binary)
        iperf_command = '{} -s -J -p {}'.format(iperf_binary, self.port)

        self._iperf_process = self._android_device.adb.shell_nb(
            '{cmd} {extra_flags} > {log_file}'.format(
                cmd=iperf_command,
                extra_flags=extra_args,
                log_file=self._get_device_log_path()))

        self._iperf_process_adb_pid = ''
        while len(self._iperf_process_adb_pid) == 0:
            self._iperf_process_adb_pid = self._android_device.adb.shell(
                'pgrep iperf3 -n')

        self._current_tag = tag

    def stop(self):
        """Stops the iperf server.

        Returns:
            The name of the log file generated from the terminated session.
        """
        if self._iperf_process is None:
            return

        job.run('kill -9 {}'.format(self._iperf_process.pid))

        # TODO(markdr): update with definitive kill method
        while True:
            iperf_process_list = self._android_device.adb.shell('pgrep iperf3')
            if iperf_process_list.find(self._iperf_process_adb_pid) == -1:
                break
            else:
                self._android_device.adb.shell("kill -9 {}".format(
                    self._iperf_process_adb_pid))

        iperf_result = self._android_device.adb.shell('cat {}'.format(
            self._get_device_log_path()))

        log_file = self._get_full_file_path(self._current_tag)
        with open(log_file, 'w') as f:
            f.write(iperf_result)

        self._android_device.adb.shell('rm {}'.format(
            self._get_device_log_path()))

        self._iperf_process = None
        return log_file
