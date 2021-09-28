#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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
"""The interface for a USB-connected Monsoon power meter.

Details on the protocol can be found at
(http://msoon.com/LabEquipment/PowerMonitor/)

Based on the original py2 script of kens@google.com.
"""
import collections
import fcntl
import logging
import os
import select
import struct
import sys
import time

import errno
import serial

from acts.controllers.monsoon_lib.api.common import MonsoonError


class LvpmStatusPacket(object):
    """The data received from asking an LVPM Monsoon for its status.

    Attributes names with the same values as HVPM match those defined in
    Monsoon.Operations.statusPacket.
    """

    def __init__(self, values):
        iter_value = iter(values)
        self.packetType = next(iter_value)
        self.firmwareVersion = next(iter_value)
        self.protocolVersion = next(iter_value)
        self.mainFineCurrent = next(iter_value)
        self.usbFineCurrent = next(iter_value)
        self.auxFineCurrent = next(iter_value)
        self.voltage1 = next(iter_value)
        self.mainCoarseCurrent = next(iter_value)
        self.usbCoarseCurrent = next(iter_value)
        self.auxCoarseCurrent = next(iter_value)
        self.voltage2 = next(iter_value)
        self.outputVoltageSetting = next(iter_value)
        self.temperature = next(iter_value)
        self.status = next(iter_value)
        self.leds = next(iter_value)
        self.mainFineResistor = next(iter_value)
        self.serialNumber = next(iter_value)
        self.sampleRate = next(iter_value)
        self.dacCalLow = next(iter_value)
        self.dacCalHigh = next(iter_value)
        self.powerupCurrentLimit = next(iter_value)
        self.runtimeCurrentLimit = next(iter_value)
        self.powerupTime = next(iter_value)
        self.usbFineResistor = next(iter_value)
        self.auxFineResistor = next(iter_value)
        self.initialUsbVoltage = next(iter_value)
        self.initialAuxVoltage = next(iter_value)
        self.hardwareRevision = next(iter_value)
        self.temperatureLimit = next(iter_value)
        self.usbPassthroughMode = next(iter_value)
        self.mainCoarseResistor = next(iter_value)
        self.usbCoarseResistor = next(iter_value)
        self.auxCoarseResistor = next(iter_value)
        self.defMainFineResistor = next(iter_value)
        self.defUsbFineResistor = next(iter_value)
        self.defAuxFineResistor = next(iter_value)
        self.defMainCoarseResistor = next(iter_value)
        self.defUsbCoarseResistor = next(iter_value)
        self.defAuxCoarseResistor = next(iter_value)
        self.eventCode = next(iter_value)
        self.eventData = next(iter_value)


class MonsoonProxy(object):
    """Class that directly talks to monsoon over serial.

    Provides a simple class to use the power meter.
    See http://wiki/Main/MonsoonProtocol for information on the protocol.
    """

    # The format of the status packet.
    STATUS_FORMAT = '>BBBhhhHhhhHBBBxBbHBHHHHBbbHHBBBbbbbbbbbbBH'

    # The list of fields that appear in the Monsoon status packet.
    STATUS_FIELDS = [
        'packetType',
        'firmwareVersion',
        'protocolVersion',
        'mainFineCurrent',
        'usbFineCurrent',
        'auxFineCurrent',
        'voltage1',
        'mainCoarseCurrent',
        'usbCoarseCurrent',
        'auxCoarseCurrent',
        'voltage2',
        'outputVoltageSetting',
        'temperature',
        'status',
        'leds',
        'mainFineResistorOffset',
        'serialNumber',
        'sampleRate',
        'dacCalLow',
        'dacCalHigh',
        'powerupCurrentLimit',
        'runtimeCurrentLimit',
        'powerupTime',
        'usbFineResistorOffset',
        'auxFineResistorOffset',
        'initialUsbVoltage',
        'initialAuxVoltage',
        'hardwareRevision',
        'temperatureLimit',
        'usbPassthroughMode',
        'mainCoarseResistorOffset',
        'usbCoarseResistorOffset',
        'auxCoarseResistorOffset',
        'defMainFineResistor',
        'defUsbFineResistor',
        'defAuxFineResistor',
        'defMainCoarseResistor',
        'defUsbCoarseResistor',
        'defAuxCoarseResistor',
        'eventCode',
        'eventData',
    ]

    def __init__(self, device=None, serialno=None, connection_timeout=600):
        """Establish a connection to a Monsoon.

        By default, opens the first available port, waiting if none are ready.

        Args:
            device: The particular device port to be used.
            serialno: The Monsoon's serial number.
            connection_timeout: The number of seconds to wait for the device to
                connect.

        Raises:
            TimeoutError if unable to connect to the device.
        """
        self.start_voltage = 0
        self.serial = serialno

        if device:
            self.ser = serial.Serial(device, timeout=1)
            return
        # Try all devices connected through USB virtual serial ports until we
        # find one we can use.
        self._tempfile = None
        self.obtain_dev_port(connection_timeout)
        self.log = logging.getLogger()

    def obtain_dev_port(self, timeout=600):
        """Obtains the device port for this Monsoon.

        Args:
            timeout: The time in seconds to wait for the device to connect.

        Raises:
            TimeoutError if the device was unable to be found, or was not
            available.
        """
        start_time = time.time()

        while start_time + timeout > time.time():
            for dev in os.listdir('/dev'):
                prefix = 'ttyACM'
                # Prefix is different on Mac OS X.
                if sys.platform == 'darwin':
                    prefix = 'tty.usbmodem'
                if not dev.startswith(prefix):
                    continue
                tmpname = '/tmp/monsoon.%s.%s' % (os.uname()[0], dev)
                self._tempfile = open(tmpname, 'w')
                if not os.access(tmpname, os.R_OK | os.W_OK):
                    try:
                        os.chmod(tmpname, 0o666)
                    except OSError as e:
                        if e.errno == errno.EACCES:
                            raise ValueError(
                                'Unable to set permissions to read/write to '
                                '%s. This file is owned by another user; '
                                'please grant o+wr access to this file, or '
                                'run as that user.')
                        raise

                try:  # Use a lock file to ensure exclusive access.
                    fcntl.flock(self._tempfile, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except IOError:
                    logging.error('Device %s is in use.', repr(dev))
                    continue

                try:  # try to open the device
                    self.ser = serial.Serial('/dev/%s' % dev, timeout=1)
                    self.stop_data_collection()  # just in case
                    self._flush_input()  # discard stale input
                    status = self.get_status()
                except Exception as e:
                    logging.exception('Error opening device %s: %s', dev, e)
                    continue

                if not status:
                    logging.error('No response from device %s.', dev)
                elif self.serial and status.serialNumber != self.serial:
                    logging.error('Another device serial #%d seen on %s',
                                  status.serialNumber, dev)
                else:
                    self.start_voltage = status.voltage1
                    return

            self._tempfile = None
            logging.info('Waiting for device...')
            time.sleep(1)
        raise TimeoutError(
            'Unable to connect to Monsoon device with '
            'serial "%s" within %s seconds.' % (self.serial, timeout))

    def release_dev_port(self):
        """Releases the dev port used to communicate with the Monsoon device."""
        fcntl.flock(self._tempfile, fcntl.LOCK_UN)
        self._tempfile.close()
        self.ser.close()

    def get_status(self):
        """Requests and waits for status.

        Returns:
            status dictionary.
        """
        self._send_struct('BBB', 0x01, 0x00, 0x00)
        read_bytes = self._read_packet()

        if not read_bytes:
            raise MonsoonError('Failed to read Monsoon status')
        expected_size = struct.calcsize(self.STATUS_FORMAT)
        if len(read_bytes) != expected_size or read_bytes[0] != 0x10:
            raise MonsoonError('Wanted status, dropped type=0x%02x, len=%d',
                               read_bytes[0], len(read_bytes))

        status = collections.OrderedDict(
            zip(self.STATUS_FIELDS,
                struct.unpack(self.STATUS_FORMAT, read_bytes)))
        p_type = status['packetType']
        if p_type != 0x10:
            raise MonsoonError('Packet type %s is not 0x10.' % p_type)

        for k in status.keys():
            if k.endswith('VoltageSetting'):
                status[k] = 2.0 + status[k] * 0.01
            elif k.endswith('FineCurrent'):
                pass  # needs calibration data
            elif k.endswith('CoarseCurrent'):
                pass  # needs calibration data
            elif k.startswith('voltage') or k.endswith('Voltage'):
                status[k] = status[k] * 0.000125
            elif k.endswith('Resistor'):
                status[k] = 0.05 + status[k] * 0.0001
                if k.startswith('aux') or k.startswith('defAux'):
                    status[k] += 0.05
            elif k.endswith('CurrentLimit'):
                status[k] = 8 * (1023 - status[k]) / 1023.0
        return LvpmStatusPacket(status.values())

    def set_voltage(self, voltage):
        """Sets the voltage on the device to the specified value.

        Args:
            voltage: Either 0 or a value between 2.01 and 4.55 inclusive.

        Raises:
            struct.error if voltage is an invalid value.
        """
        # The device has a range of 255 voltage values:
        #
        #     0   is "off". Note this value not set outputVoltageSetting to
        #             zero. The previous outputVoltageSetting value is
        #             maintained.
        #     1   is 2.01V.
        #     255 is 4.55V.
        voltage_byte = max(0, round((voltage - 2.0) * 100))
        self._send_struct('BBB', 0x01, 0x01, voltage_byte)

    def get_voltage(self):
        """Get the output voltage.

        Returns:
            Current Output Voltage (in unit of V).
        """
        return self.get_status().outputVoltageSetting

    def set_max_current(self, i):
        """Set the max output current."""
        if i < 0 or i > 8:
            raise MonsoonError(('Target max current %sA, is out of acceptable '
                                'range [0, 8].') % i)
        val = 1023 - int((i / 8) * 1023)
        self._send_struct('BBB', 0x01, 0x0a, val & 0xff)
        self._send_struct('BBB', 0x01, 0x0b, val >> 8)

    def set_max_initial_current(self, current):
        """Sets the maximum initial current, in mA."""
        if current < 0 or current > 8:
            raise MonsoonError(('Target max current %sA, is out of acceptable '
                                'range [0, 8].') % current)
        val = 1023 - int((current / 8) * 1023)
        self._send_struct('BBB', 0x01, 0x08, val & 0xff)
        self._send_struct('BBB', 0x01, 0x09, val >> 8)

    def set_usb_passthrough(self, passthrough_mode):
        """Set the USB passthrough mode.

        Args:
            passthrough_mode: The mode used for passthrough. Must be the integer
                value. See common.PassthroughModes for a list of values and
                their meanings.
        """
        self._send_struct('BBB', 0x01, 0x10, passthrough_mode)

    def get_usb_passthrough(self):
        """Get the USB passthrough mode: 0 = off, 1 = on,  2 = auto.

        Returns:
            The mode used for passthrough, as an integer. See
                common.PassthroughModes for a list of values and their meanings.
        """
        return self.get_status().usbPassthroughMode

    def start_data_collection(self):
        """Tell the device to start collecting and sending measurement data."""
        self._send_struct('BBB', 0x01, 0x1b, 0x01)  # Mystery command
        self._send_struct('BBBBBBB', 0x02, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe8)

    def stop_data_collection(self):
        """Tell the device to stop collecting measurement data."""
        self._send_struct('BB', 0x03, 0x00)  # stop

    def _send_struct(self, fmt, *args):
        """Pack a struct (without length or checksum) and send it."""
        # Flush out the input buffer before sending data
        self._flush_input()
        data = struct.pack(fmt, *args)
        data_len = len(data) + 1
        checksum = (data_len + sum(bytearray(data))) % 256
        out = struct.pack('B', data_len) + data + struct.pack('B', checksum)
        self.ser.write(out)

    def _read_packet(self):
        """Returns a single packet as a string (without length or checksum)."""
        len_char = self.ser.read(1)
        if not len_char:
            raise MonsoonError('Reading from serial port timed out')

        data_len = ord(len_char)
        if not data_len:
            return ''
        result = self.ser.read(int(data_len))
        result = bytearray(result)
        if len(result) != data_len:
            raise MonsoonError(
                'Length mismatch, expected %d bytes, got %d bytes.', data_len,
                len(result))
        body = result[:-1]
        checksum = (sum(struct.unpack('B' * len(body), body)) + data_len) % 256
        if result[-1] != checksum:
            raise MonsoonError(
                'Invalid checksum from serial port! Expected %s, got %s',
                hex(checksum), hex(result[-1]))
        return result[:-1]

    def _flush_input(self):
        """Flushes all read data until the input is empty."""
        self.ser.reset_input_buffer()
        while True:
            ready_r, ready_w, ready_x = select.select([self.ser], [],
                                                      [self.ser], 0)
            if len(ready_x) > 0:
                raise MonsoonError('Exception from serial port.')
            elif len(ready_r) > 0:
                self.ser.read(1)  # This may cause underlying buffering.
                # Flush the underlying buffer too.
                self.ser.reset_input_buffer()
            else:
                break
