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

import logging
import time

from threading import Thread

from acts.libs.logging import log_stream
from acts.libs.logging.log_stream import LogStyles
from acts.controllers.android_lib.logcat import TimestampTracker
from acts.controllers.fuchsia_lib.utils_lib import create_ssh_connection


def _log_line_func(log, timestamp_tracker):
    """Returns a lambda that logs a message to the given logger."""

    def log_line(message):
        timestamp_tracker.read_output(message)
        log.info(message)

    return log_line


def start_syslog(serial,
                 base_path,
                 ip_address,
                 ssh_username,
                 ssh_config,
                 extra_params=''):
    """Creates a FuchsiaSyslogProcess that automatically attempts to reconnect.

    Args:
        serial: The unique identifier for the device.
        base_path: The base directory used for syslog file output.
        ip_address: The ip address of the device to get the syslog.
        ssh_username: Username for the device for the Fuchsia Device.
        ssh_config: Location of the ssh_config for connecting to the remote
            device
        extra_params: Any additional params to be added to the syslog cmdline.

    Returns:
        A FuchsiaSyslogProcess object.
    """
    logger = log_stream.create_logger(
        'fuchsia_log_%s' % serial, base_path=base_path,
        log_styles=(LogStyles.LOG_DEBUG | LogStyles.MONOLITH_LOG))
    syslog = FuchsiaSyslogProcess(ssh_username,
                                  ssh_config,
                                  ip_address,
                                  extra_params)
    timestamp_tracker = TimestampTracker()
    syslog.set_on_output_callback(_log_line_func(logger, timestamp_tracker))
    return syslog


class FuchsiaSyslogError(Exception):
    """Raised when invalid operations are run on a Fuchsia Syslog."""


class FuchsiaSyslogProcess(object):
    """A class representing a Fuchsia Syslog object that communicates over ssh.
    """

    def __init__(self, ssh_username, ssh_config, ip_address, extra_params):
        """
        Args:
            ssh_username: The username to connect to Fuchsia over ssh.
            ssh_config: The ssh config that holds the information to connect to
            a Fuchsia device over ssh.
            ip_address: The ip address of the Fuchsia device.
        """
        self.ssh_config = ssh_config
        self.ip_address = ip_address
        self.extra_params = extra_params
        self.ssh_username = ssh_username
        self._output_file = None
        self._ssh_client = None
        self._listening_thread = None
        self._redirection_thread = None
        self._on_output_callback = lambda *args, **kw: None

        self._started = False
        self._stopped = False

    def start(self):
        """Starts reading the data from the syslog ssh connection."""
        if self._started:
            raise FuchsiaSyslogError('Syslog has already started for '
                                     'FuchsiaDevice (%s).' % self.ip_address)
        self._started = True

        self._listening_thread = Thread(target=self._exec_loop)
        self._listening_thread.start()

        time_up_at = time.time() + 10

        while self._ssh_client is None:
            if time.time() > time_up_at:
                raise FuchsiaSyslogError('Unable to connect to syslog!')

        self._stopped = False

    def stop(self):
        """Stops listening to the syslog ssh connection and coalesces the
        threads.
        """
        if self._stopped:
            raise FuchsiaSyslogError('Syslog is already being stopped for '
                                     'FuchsiaDevice (%s).' % self.ip_address)
        self._stopped = True

        try:
            self._ssh_client.close()
        except Exception as e:
            raise e
        finally:
            self._join_threads()
            self._started = False
            return None

    def _join_threads(self):
        """Waits for the threads associated with the process to terminate."""
        if self._listening_thread is not None:
            self._listening_thread.join()
            self._listening_thread = None

        if self._redirection_thread is not None:
            self._redirection_thread.join()
            self._redirection_thread = None

    def _redirect_output(self):
        """Redirects the output from the ssh connection into the
        on_output_callback.
        """
        while True:
            line = self._output_file.readline()

            if not line:
                return
            else:
                # Output the line without trailing \n and whitespace.
                self._on_output_callback(line.rstrip())

    def set_on_output_callback(self, on_output_callback, binary=False):
        """Sets the on_output_callback function.

        Args:
            on_output_callback: The function to be called when output is sent to
                the output. The output callback has the following signature:

                >>> def on_output_callback(output_line):
                >>>     return None

            binary: If True, read the process output as raw binary.
        Returns:
            self
        """
        self._on_output_callback = on_output_callback
        self._binary_output = binary
        return self

    def __start_process(self):
        """A convenient wrapper function for starting the ssh connection and
        starting the syslog."""

        self._ssh_client = create_ssh_connection(self.ip_address,
                                                 self.ssh_username,
                                                 self.ssh_config)
        transport = self._ssh_client.get_transport()
        channel = transport.open_session()
        channel.get_pty()
        self._output_file = channel.makefile()
        logging.debug('Starting FuchsiaDevice (%s) syslog over ssh.'
                      % self.ssh_username)
        channel.exec_command('log_listener %s' % self.extra_params)
        return transport

    def _exec_loop(self):
        """Executes a ssh connection to the Fuchsia Device syslog in a loop.

        When the ssh connection terminates without stop() being called,
        the threads are coalesced and the syslog is restarted.
        """
        start_up = True
        while True:
            if self._stopped:
                break
            else:
                if start_up or not ssh_transport.is_alive():
                    if start_up:
                        logging.debug('Starting SSH connection for '
                                      'FuchsiaDevice (%s) syslog.'
                                      % self.ip_address)
                        start_up = False
                    else:
                        logging.debug('SSH connection for FuchsiaDevice (%s) is'
                                      ' down.  Restarting.' % self.ip_address)
                    ssh_transport = self.__start_process()
                    self._redirection_thread = Thread(
                        target=self._redirect_output)
                    self._redirection_thread.start()
                    self._redirection_thread.join()
