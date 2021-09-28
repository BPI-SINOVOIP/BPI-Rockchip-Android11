# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os.path
import time
import uuid

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import path_utils
from autotest_lib.client.common_lib.cros.network import iw_runner


class PacketCapturesDisabledError(Exception):
    """Signifies that this remote host does not support packet captures."""
    pass


# local_pcap_path refers to the path of the result on the local host.
# local_log_path refers to the tcpdump log file path on the local host.
CaptureResult = collections.namedtuple('CaptureResult',
                                       ['local_pcap_path', 'local_log_path'])

# The number of bytes needed for a probe request is hard to define,
# because the frame contents are variable (e.g. radiotap header may
# contain different fields, maybe SSID isn't the first tagged
# parameter?). The value here is 2x the largest frame size observed in
# a quick sample.
SNAPLEN_WIFI_PROBE_REQUEST = 600

TCPDUMP_START_TIMEOUT_SECONDS = 5
TCPDUMP_START_POLL_SECONDS = 0.1

# These are WidthType objects from iw_runner
WIDTH_HT20 = iw_runner.WIDTH_HT20
WIDTH_HT40_PLUS = iw_runner.WIDTH_HT40_PLUS
WIDTH_HT40_MINUS = iw_runner.WIDTH_HT40_MINUS
WIDTH_VHT80 = iw_runner.WIDTH_VHT80
WIDTH_VHT160 = iw_runner.WIDTH_VHT160
WIDTH_VHT80_80 = iw_runner.WIDTH_VHT80_80

_WIDTH_STRINGS = {
    WIDTH_HT20: 'HT20',
    WIDTH_HT40_PLUS: 'HT40+',
    WIDTH_HT40_MINUS: 'HT40-',
    WIDTH_VHT80: '80',
    WIDTH_VHT160: '160',
    WIDTH_VHT80_80: '80+80',
}

def _get_width_string(width):
    """Returns a valid width parameter for "iw dev ${DEV} set freq".

    @param width object, one of WIDTH_*
    @return string iw readable width, or empty string

    """
    return _WIDTH_STRINGS.get(width, '')


def _get_center_freq_80(frequency):
    """Find the center frequency of a 80MHz channel.

    Raises an error upon an invalid frequency.

    @param frequency int Control frequency of the channel.
    @return center_freq int Center frequency of the channel.

    """
    vht80 = [ 5180, 5260, 5500, 5580, 5660, 5745 ]
    for f in vht80:
        if frequency >= f and frequency < f + 80:
            return f + 30
    raise error.TestError(
            'Frequency %s is not part of a 80MHz channel', frequency)


def _get_center_freq_160(frequency):
    """Find the center frequency of a 160MHz channel.

    Raises an error upon an invalid frequency.

    @param frequency int Control frequency of the channel.
    @return center_freq int Center frequency of the channel.

    """
    if (frequency >= 5180 and frequency <= 5320):
        return 5250
    if (frequency >= 5500 and frequency <= 5640):
        return 5570
    raise error.TestError(
            'Frequency %s is not part of a 160MHz channel', frequency)


def get_packet_capturer(host, host_description=None, cmd_ip=None, cmd_iw=None,
                        cmd_netdump=None, ignore_failures=False, logdir=None):
    cmd_iw = cmd_iw or path_utils.get_install_path('iw', host=host)
    cmd_ip = cmd_ip or path_utils.get_install_path('ip', host=host)
    cmd_netdump = (cmd_netdump or
                   path_utils.get_install_path('tcpdump', host=host))
    host_description = host_description or 'cap_%s' % uuid.uuid4().hex
    if None in [cmd_iw, cmd_ip, cmd_netdump, host_description, logdir]:
        if ignore_failures:
            logging.warning('Creating a disabled packet capturer for %s.',
                            host_description)
            return DisabledPacketCapturer()
        else:
            raise error.TestFail('Missing commands needed for '
                                 'capturing packets')

    return PacketCapturer(host, host_description, cmd_ip, cmd_iw, cmd_netdump,
                          logdir=logdir)


class DisabledPacketCapturer(object):
    """Delegate meant to look like it could take packet captures."""

    @property
    def capture_running(self):
        """@return False"""
        return False


    def __init__(self):
        pass


    def  __enter__(self):
        return self


    def __exit__(self):
        pass


    def close(self):
        """No-op"""


    def create_raw_monitor(self, phy, frequency, width_type=None,
                           monitor_device=None):
        """Appears to fail while creating a raw monitor device.

        @param phy string ignored.
        @param frequency int ignored.
        @param width_type string ignored.
        @param monitor_device string ignored.
        @return None.

        """
        return None


    def configure_raw_monitor(self, monitor_device, frequency, width_type=None):
        """Fails to configure a raw monitor.

        @param monitor_device string ignored.
        @param frequency int ignored.
        @param width_type string ignored.

        """


    def create_managed_monitor(self, existing_dev, monitor_device=None):
        """Fails to create a managed monitor device.

        @param existing_device string ignored.
        @param monitor_device string ignored.
        @return None

        """
        return None


    def start_capture(self, interface, local_save_dir,
                      remote_file=None, snaplen=None):
        """Fails to start a packet capture.

        @param interface string ignored.
        @param local_save_dir string ignored.
        @param remote_file string ignored.
        @param snaplen int ignored.

        @raises PacketCapturesDisabledError.

        """
        raise PacketCapturesDisabledError()


    def stop_capture(self, capture_pid=None):
        """Stops all ongoing packet captures.

        @param capture_pid int ignored.

        """


class PacketCapturer(object):
    """Delegate with capability to initiate packet captures on a remote host."""

    LIBPCAP_POLL_FREQ_SECS = 1

    @property
    def capture_running(self):
        """@return True iff we have at least one ongoing packet capture."""
        if self._ongoing_captures:
            return True

        return False


    def __init__(self, host, host_description, cmd_ip, cmd_iw, cmd_netdump,
                 logdir, disable_captures=False):
        self._cmd_netdump = cmd_netdump
        self._cmd_iw = cmd_iw
        self._cmd_ip = cmd_ip
        self._host = host
        self._ongoing_captures = {}
        self._cap_num = 0
        self._if_num = 0
        self._created_managed_devices = []
        self._created_raw_devices = []
        self._host_description = host_description
        self._logdir = logdir


    def __enter__(self):
        return self


    def __exit__(self):
        self.close()


    def close(self):
        """Stop ongoing captures and destroy all created devices."""
        self.stop_capture()
        for device in self._created_managed_devices:
            self._host.run("%s dev %s del" % (self._cmd_iw, device))
        self._created_managed_devices = []
        for device in self._created_raw_devices:
            self._host.run("%s link set %s down" % (self._cmd_ip, device))
            self._host.run("%s dev %s del" % (self._cmd_iw, device))
        self._created_raw_devices = []


    def create_raw_monitor(self, phy, frequency, width_type=None,
                           monitor_device=None):
        """Create and configure a monitor type WiFi interface on a phy.

        If a device called |monitor_device| already exists, it is first removed.

        @param phy string phy name for created monitor (e.g. phy0).
        @param frequency int frequency for created monitor to watch.
        @param width_type object optional HT or VHT type, one of the keys in
                self.WIDTH_STRINGS.
        @param monitor_device string name of monitor interface to create.
        @return string monitor device name created or None on failure.

        """
        if not monitor_device:
            monitor_device = 'mon%d' % self._if_num
            self._if_num += 1

        self._host.run('%s dev %s del' % (self._cmd_iw, monitor_device),
                       ignore_status=True)
        result = self._host.run('%s phy %s interface add %s type monitor' %
                                (self._cmd_iw,
                                 phy,
                                 monitor_device),
                                ignore_status=True)
        if result.exit_status:
            logging.error('Failed creating raw monitor.')
            return None

        self.configure_raw_monitor(monitor_device, frequency, width_type)
        self._created_raw_devices.append(monitor_device)
        return monitor_device


    def configure_raw_monitor(self, monitor_device, frequency, width_type=None):
        """Configure a raw monitor with frequency and HT params.

        Note that this will stomp on earlier device settings.

        @param monitor_device string name of device to configure.
        @param frequency int WiFi frequency to dwell on.
        @param width_type object width_type, one of the WIDTH_* objects.

        """
        channel_args = str(frequency)

        if width_type:
            width_string = _get_width_string(width_type)
            if not width_string:
                raise error.TestError('Invalid width type: %r' % width_type)
            if width_type == WIDTH_VHT80_80:
                raise error.TestError('VHT80+80 packet capture not supported')
            if width_type == WIDTH_VHT80:
                width_string = '%s %d' % (width_string,
                                          _get_center_freq_80(frequency))
            elif width_type == WIDTH_VHT160:
                width_string = '%s %d' % (width_string,
                                          _get_center_freq_160(frequency))
            channel_args = '%s %s' % (channel_args, width_string)

        self._host.run("%s link set %s up" % (self._cmd_ip, monitor_device))
        self._host.run("%s dev %s set freq %s" % (self._cmd_iw,
                                                  monitor_device,
                                                  channel_args))


    def create_managed_monitor(self, existing_dev, monitor_device=None):
        """Create a monitor type WiFi interface next to a managed interface.

        If a device called |monitor_device| already exists, it is first removed.

        @param existing_device string existing interface (e.g. mlan0).
        @param monitor_device string name of monitor interface to create.
        @return string monitor device name created or None on failure.

        """
        if not monitor_device:
            monitor_device = 'mon%d' % self._if_num
            self._if_num += 1
        self._host.run('%s dev %s del' % (self._cmd_iw, monitor_device),
                       ignore_status=True)
        result = self._host.run('%s dev %s interface add %s type monitor' %
                                (self._cmd_iw,
                                 existing_dev,
                                 monitor_device),
                                ignore_status=True)
        if result.exit_status:
            logging.warning('Failed creating monitor.')
            return None

        self._host.run('%s link set %s up' % (self._cmd_ip, monitor_device))
        self._created_managed_devices.append(monitor_device)
        return monitor_device


    def _is_capture_active(self, remote_log_file):
        """Check if a packet capture has completed initialization.

        @param remote_log_file string path to the capture's log file
        @return True iff log file indicates that tcpdump is listening.
        """
        return self._host.run(
            'grep "listening on" "%s"' % remote_log_file, ignore_status=True
            ).exit_status == 0


    def start_capture(self, interface, local_save_dir,
                      remote_file=None, snaplen=None):
        """Start a packet capture on an existing interface.

        @param interface string existing interface to capture on.
        @param local_save_dir string directory on local machine to hold results.
        @param remote_file string full path on remote host to hold the capture.
        @param snaplen int maximum captured frame length.
        @return int pid of started packet capture.

        """
        remote_file = (remote_file or
                       '%s/%s.%d.pcap' % (self._logdir, self._host_description,
                                            self._cap_num))
        self._cap_num += 1
        remote_log_file = '%s.log' % remote_file
        # Redirect output because SSH refuses to return until the child file
        # descriptors are closed.
        cmd = '%s -U -i %s -w %s -s %d >%s 2>&1 & echo $!' % (
            self._cmd_netdump,
            interface,
            remote_file,
            snaplen or 0,
            remote_log_file)
        logging.debug('Starting managed packet capture')
        pid = int(self._host.run(cmd).stdout)
        self._ongoing_captures[pid] = (remote_file,
                                       remote_log_file,
                                       local_save_dir)
        is_capture_active = lambda: self._is_capture_active(remote_log_file)
        utils.poll_for_condition(
            is_capture_active,
            timeout=TCPDUMP_START_TIMEOUT_SECONDS,
            sleep_interval=TCPDUMP_START_POLL_SECONDS,
            desc='Timeout waiting for tcpdump to start.')
        return pid


    def stop_capture(self, capture_pid=None, local_save_dir=None,
                     local_pcap_filename=None):
        """Stop an ongoing packet capture, or all ongoing packet captures.

        If |capture_pid| is given, stops that capture, otherwise stops all
        ongoing captures.

        This method may sleep for a small amount of time, to ensure that
        libpcap has completed its last poll(). The caller must ensure that
        no unwanted traffic is received during this time.

        @param capture_pid int pid of ongoing packet capture or None.
        @param local_save_dir path to directory to save pcap file in locally.
        @param local_pcap_filename name of file to store pcap in
                (basename only).
        @return list of RemoteCaptureResult tuples

        """
        if capture_pid:
            pids_to_kill = [capture_pid]
        else:
            pids_to_kill = list(self._ongoing_captures.keys())

        if pids_to_kill:
            time.sleep(self.LIBPCAP_POLL_FREQ_SECS * 2)

        results = []
        for pid in pids_to_kill:
            self._host.run('kill -INT %d' % pid, ignore_status=True)
            remote_pcap, remote_pcap_log, save_dir = self._ongoing_captures[pid]
            pcap_filename = os.path.basename(remote_pcap)
            pcap_log_filename = os.path.basename(remote_pcap_log)
            if local_pcap_filename:
                pcap_filename = os.path.join(local_save_dir or save_dir,
                                             local_pcap_filename)
                pcap_log_filename = os.path.join(local_save_dir or save_dir,
                                                 '%s.log' % local_pcap_filename)
            pairs = [(remote_pcap, pcap_filename),
                     (remote_pcap_log, pcap_log_filename)]

            for remote_file, local_file in pairs:
                self._host.get_file(remote_file, local_file)
                self._host.run('rm -f %s' % remote_file)

            self._ongoing_captures.pop(pid)
            results.append(CaptureResult(pcap_filename,
                                         pcap_log_filename))
        return results
