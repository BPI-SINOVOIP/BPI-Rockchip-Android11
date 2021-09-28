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

import os
import re
import select
import subprocess
import sys
import time
import uuid
from logging import Logger
from threading import Thread

import serial
from serial.tools import list_ports

from acts.controllers.buds_lib import tako_trace_logger

logging = tako_trace_logger.TakoTraceLogger(Logger(__file__))

RETRIES = 0


class LogSerialException(Exception):
    """LogSerial Exception."""


class PortCheck(object):
    def get_serial_ports(self):
        """Gets the computer available serial ports.

        Returns:
            Dictionary object with all the serial port names.
        """
        result = {}
        ports = list_ports.comports()
        for port_name, description, address in ports:
            result[port_name] = (description, address)
        return result

    # TODO: Clean up this function. The boolean logic can be simplified.
    def search_port_by_property(self, search_params):
        """Search ports by a dictionary of the search parameters.

        Args:
            search_params: Dictionary object with the parameters
                           to search. i.e:
                           {'ID_SERIAL_SHORT':'213213',
                           'ID_USB_INTERFACE_NUM': '01'}
        Returns:
            Array with the ports found
        """
        ports_result = []
        for port in self.get_serial_ports():
            properties = self.get_port_properties(port=port)
            if properties:
                properties_exists = True
                for port_property in search_params:
                    properties_exists *= (port_property in properties)
                properties_exists = True if properties_exists == 1 else False
                if properties_exists:
                    found = True
                    for port_property in search_params.keys():
                        search_value = search_params[port_property]
                        if properties[port_property] == search_value:
                            found *= True
                        else:
                            found = False
                            break
                    found = True if found == 1 else False
                    if found:
                        ports_result.append(port)
        return ports_result

    def get_port_properties(self, port):
        """Get all the properties from a given port.

        Args:
            port: String object with the port name. i.e. '/dev/ttyACM1'

        Returns:
            dictionary object with all the properties.
        """
        ports = self.get_serial_ports()
        if port in ports:
            result = {}
            port_address = ports[port][1]
            property_list = None
            if sys.platform.startswith('linux') or sys.platform.startswith(
                    'cygwin'):
                try:
                    command = 'udevadm info -q property -n {}'.format(port)
                    property_list = subprocess.check_output(command, shell=True)
                    property_list = property_list.decode(errors='replace')
                except subprocess.CalledProcessError as error:
                    logging.error(error)
                if property_list:
                    properties = filter(None, property_list.split('\n'))
                    for prop in properties:
                        p = prop.split('=')
                        result[p[0]] = p[1]
            elif sys.platform.startswith('win'):
                regex = ('(?P<type>[A-Z]*)\sVID\:PID\=(?P<vid>\w*)'
                         '\:(?P<pid>\w*)\s+(?P<adprop>.*$)')
                m = re.search(regex, port_address)
                if m:
                    result['type'] = m.group('type')
                    result['vid'] = m.group('vid')
                    result['pid'] = m.group('pid')
                    adprop = m.group('adprop').strip()
                    if adprop:
                        prop_array = adprop.split(' ')
                        for prop in prop_array:
                            p = prop.split('=')
                            result[p[0]] = p[1]
                    if 'LOCATION' in result:
                        interface = int(result['LOCATION'].split('.')[1])
                        if interface < 10:
                            result['ID_USB_INTERFACE_NUM'] = '0{}'.format(
                                interface)
                        else:
                            result['ID_USB_INTERFACE_NUM'] = '{}'.format(
                                interface)
                    win_vid_pid = '*VID_{}*PID_{}*'.format(result['vid'],
                                                           result['pid'])
                    command = (
                            'powershell gwmi "Win32_USBControllerDevice |' +
                            ' %{[wmi]($_.Dependent)} |' +
                            ' Where-Object -Property PNPDeviceID -Like "' +
                            win_vid_pid + '" |' +
                            ' Where-Object -Property Service -Eq "usbccgp" |' +
                            ' Select-Object -Property PNPDeviceID"')
                    res = subprocess.check_output(command, shell=True)
                    r = res.decode('ascii')
                    m = re.search('USB\\\\.*', r)
                    if m:
                        result['ID_SERIAL_SHORT'] = (
                            m.group().strip().split('\\')[2])
            return result

    def port_exists(self, port):
        """Check if a serial port exists in the computer by the port name.

        Args:
            port: String object with the port name. i.e. '/dev/ttyACM1'

        Returns:
            True if it was found, False if not.
        """
        exists = port in self.get_serial_ports()
        return exists


class LogSerial(object):
    def __init__(self,
                 port,
                 baudrate,
                 bytesize=8,
                 parity='N',
                 stopbits=1,
                 timeout=0.15,
                 retries=0,
                 flush_output=True,
                 terminator='\n',
                 output_path=None,
                 serial_logger=None):
        global RETRIES
        self.set_log = False
        self.output_path = None
        self.set_output_path(output_path)
        if serial_logger:
            self.set_logger(serial_logger)
        self.monitor_port = PortCheck()
        if self.monitor_port.port_exists(port=port):
            self.connection_handle = serial.Serial()
            RETRIES = retries
            self.reading = True
            self.log = []
            self.log_thread = Thread()
            self.command_ini_index = None
            self.is_logging = False
            self.flush_output = flush_output
            self.terminator = terminator
            if port:
                self.connection_handle.port = port
            if baudrate:
                self.connection_handle.baudrate = baudrate
            if bytesize:
                self.connection_handle.bytesize = bytesize
            if parity:
                self.connection_handle.parity = parity
            if stopbits:
                self.connection_handle.stopbits = stopbits
            if timeout:
                self.connection_handle.timeout = timeout
            try:
                self.open()
            except Exception as e:
                self.close()
                logging.error(e)
        else:
            raise LogSerialException(
                'The port {} does not exist'.format(port))

    def set_logger(self, serial_logger):
        global logging
        logging = serial_logger
        self.set_output_path(getattr(logging, 'output_path', '/tmp'))
        self.set_log = True

    def set_output_path(self, output_path):
        """Set the output path for the flushed log.

        Args:
            output_path: String object with the path
        """
        if output_path:
            if os.path.exists(output_path):
                self.output_path = output_path
            else:
                raise LogSerialException('The output path does not exist.')

    def refresh_port_connection(self, port):
        """Will update the port connection without closing the read thread.

        Args:
            port: String object with the new port name. i.e. '/dev/ttyACM1'

        Raises:
            LogSerialException if the port is not alive.
        """
        if self.monitor_port.port_exists(port=port):
            self.connection_handle.port = port
            self.open()
        else:
            raise LogSerialException(
                'The port {} does not exist'.format(port))

    def is_port_alive(self):
        """Verify if the current port is alive in the computer.

        Returns:
            True if its alive, False if its missing.
        """
        alive = self.monitor_port.port_exists(port=self.connection_handle.port)
        return alive

    # @retry(Exception, tries=RETRIES, delay=1, backoff=2)
    def open(self):
        """Will open the connection with the current port settings."""
        while self.connection_handle.isOpen():
            self.connection_handle.close()
            time.sleep(0.5)
        self.connection_handle.open()
        if self.flush_output:
            self.flush()
        self.start_reading()
        logging.info('Connection Open')

    def close(self):
        """Will close the connection and the read thread."""
        self.stop_reading()
        if self.connection_handle:
            self.connection_handle.close()
        if not self.set_log:
            logging.flush_log()
        self.flush_log()
        logging.info('Connection Closed')

    def flush(self):
        """Will flush any input from the serial connection."""
        self.write('\n')
        self.connection_handle.flushInput()
        self.connection_handle.flush()
        flushed = 0
        while True:
            ready_r, _, ready_x = (select.select([self.connection_handle], [],
                                                 [self.connection_handle], 0))
            if ready_x:
                logging.exception('exception from serial port')
                return
            elif ready_r:
                flushed += 1
                # This may cause underlying buffering.
                self.connection_handle.read(1)
                # Flush the underlying buffer too.
                self.connection_handle.flush()
            else:
                break
            if flushed > 0:
                logging.debug('dropped >{} bytes'.format(flushed))

    def write(self, command, wait_time=0.2):
        """Will write into the serial connection.

        Args:
            command: String object with the text to write.
            wait_time: Float object with the seconds to wait after the
                       command was issued.
        """
        if command:
            if self.terminator:
                command += self.terminator
            self.command_ini_index = len(self.log)
            self.connection_handle.write(command.encode())
            if wait_time:
                time.sleep(wait_time)
            logging.info('cmd [{}] sent.'.format(command.strip()))

    def flush_log(self):
        """Will output the log into a CSV file."""
        if len(self.log) > 0:
            path = ''
            if not self.output_path:
                self.output_path = os.getcwd()
            elif not os.path.exists(self.output_path):
                self.output_path = os.getcwd()
            path = os.path.join(self.output_path,
                                str(uuid.uuid4()) + '_serial.log')
            with open(path, 'a') as log_file:
                for info in self.log:
                    log_file.write('{}, {}\n'.format(info[0], info[1]))

    def read(self):
        """Will read from the log the output from the serial connection
        after a write command was issued. It will take the initial time
        of the command as a reference.

        Returns:
            Array object with the log lines.
        """
        buf_read = []
        command_end_index = len(self.log)
        info = self.query_serial_log(self.command_ini_index, command_end_index)
        for line in info:
            buf_read.append(line[1])
        self.command_ini_index = command_end_index
        return buf_read

    def get_all_log(self):
        """Gets the log object that collects the logs.

        Returns:
            DataFrame object with all the logs.
        """
        return self.log

    def query_serial_log(self, from_index, to_index):
        """Will query the session log from a given time in EPOC format.

        Args:
            from_timestamp: Double value with the EPOC timestamp to start
                            the search.
            to_timestamp: Double value with the EPOC timestamp to finish the
                          rearch.

        Returns:
            DataFrame with the result query.
        """
        if from_index < to_index:
            info = self.log[from_index:to_index]
            return info

    def _start_reading_thread(self):
        if self.connection_handle.isOpen():
            self.reading = True
            while self.reading:
                try:
                    data = self.connection_handle.readline().decode('utf-8')
                    if data:
                        self.is_logging = True
                        data.replace('/n', '')
                        data.replace('/r', '')
                        data = data.strip()
                        self.log.append([time.time(), data])
                    else:
                        self.is_logging = False
                except Exception:
                    time.sleep(1)
            logging.info('Read thread closed')

    def start_reading(self):
        """Method to start the log collection."""
        if not self.log_thread.isAlive():
            self.log_thread = Thread(target=self._start_reading_thread, args=())
            self.log_thread.daemon = True
            try:
                self.log_thread.start()
            except(KeyboardInterrupt, SystemExit):
                self.close()
        else:
            logging.warning('Not running log thread, is already alive')

    def stop_reading(self):
        """Method to stop the log collection."""
        self.reading = False
        self.log_thread.join(timeout=600)
