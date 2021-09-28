#   Copyright 2020 - The Android Open Source Project
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
import shlex
import tempfile
import time

from acts.controllers.utils_lib.commands import shell
from acts.libs.proc import job


class Error(Exception):
    """An error caused by radvd."""


class Radvd(object):
    """Manages the radvd program.

    https://en.wikipedia.org/wiki/Radvd
    This implements the Router Advertisement Daemon of IPv6 router addresses
    and IPv6 routing prefixes using the Neighbor Discovery Protocol.

    Attributes:
        config: The radvd configuration that is being used.
    """
    def __init__(self, runner, interface, working_dir=None, radvd_binary=None):
        """
        Args:
            runner: Object that has run_async and run methods for executing
                    shell commands (e.g. connection.SshConnection)
            interface: string, The name of the interface to use (eg. wlan0).
            working_dir: The directory to work out of.
            radvd_binary: Location of the radvd binary
        """
        if not radvd_binary:
            logging.debug('No radvd binary specified.  '
                          'Assuming radvd is in the path.')
            radvd_binary = 'radvd'
        else:
            logging.debug('Using radvd binary located at %s' % radvd_binary)
        if working_dir is None and runner == job.run:
            working_dir = tempfile.gettempdir()
        else:
            working_dir = '/tmp'
        self._radvd_binary = radvd_binary
        self._runner = runner
        self._interface = interface
        self._working_dir = working_dir
        self.config = None
        self._shell = shell.ShellCommand(runner, working_dir)
        self._log_file = '%s/radvd-%s.log' % (working_dir, self._interface)
        self._config_file = '%s/radvd-%s.conf' % (working_dir, self._interface)
        self._pid_file = '%s/radvd-%s.pid' % (working_dir, self._interface)
        self._ps_identifier = '%s.*%s' % (self._radvd_binary,
                                          self._config_file)

    def start(self, config, timeout=60):
        """Starts radvd

        Starts the radvd daemon and runs it in the background.

        Args:
            config: Configs to start the radvd with.
            timeout: Time to wait for radvd  to come up.

        Returns:
            True if the daemon could be started. Note that the daemon can still
            start and not work. Invalid configurations can take a long amount
            of time to be produced, and because the daemon runs indefinitely
            it's impossible to wait on. If you need to check if configs are ok
            then periodic checks to is_running and logs should be used.
        """
        if self.is_alive():
            self.stop()

        self.config = config

        self._shell.delete_file(self._log_file)
        self._shell.delete_file(self._config_file)
        self._write_configs(self.config)

        radvd_command = '%s -C %s -p %s -m logfile -d 5 -l %s' % (
            self._radvd_binary, shlex.quote(self._config_file),
            shlex.quote(self._pid_file), self._log_file)
        job_str = '%s > "%s" 2>&1' % (radvd_command, self._log_file)
        self._runner.run_async(job_str)

        try:
            self._wait_for_process(timeout=timeout)
        except Error:
            self.stop()
            raise

    def stop(self):
        """Kills the daemon if it is running."""
        self._shell.kill(self._ps_identifier)

    def is_alive(self):
        """
        Returns:
            True if the daemon is running.
        """
        return self._shell.is_alive(self._ps_identifier)

    def pull_logs(self):
        """Pulls the log files from where radvd is running.

        Returns:
            A string of the radvd logs.
        """
        # TODO: Auto pulling of logs when stop is called.
        return self._shell.read_file(self._log_file)

    def _wait_for_process(self, timeout=60):
        """Waits for the process to come up.

        Waits until the radvd process is found running, or there is
        a timeout. If the program never comes up then the log file
        will be scanned for errors.

        Raises: See _scan_for_errors
        """
        start_time = time.time()
        while time.time() - start_time < timeout and not self.is_alive():
            self._scan_for_errors(True)
            time.sleep(0.1)

    def _scan_for_errors(self, should_be_up):
        """Scans the radvd log for any errors.

        Args:
            should_be_up: If true then radvd program is expected to be alive.
                          If it is found not alive while this is true an error
                          is thrown.

        Raises:
            Error: Raised when a radvd error is found.
        """
        # Store this so that all other errors have priority.
        is_dead = not self.is_alive()

        exited_prematurely = self._shell.search_file('Exiting', self._log_file)
        if exited_prematurely:
            raise Error('Radvd exited prematurely.', self)
        if should_be_up and is_dead:
            raise Error('Radvd failed to start', self)

    def _write_configs(self, config):
        """Writes the configs to the radvd config file.

        Args:
            config: a RadvdConfig object.
        """
        self._shell.delete_file(self._config_file)
        conf = config.package_configs()
        lines = ['interface %s {' % self._interface]
        for (interface_option_key,
             interface_option) in conf['interface_options'].items():
            lines.append('\t%s %s;' %
                         (str(interface_option_key), str(interface_option)))
        lines.append('\tprefix %s' % conf['prefix'])
        lines.append('\t{')
        for prefix_option in conf['prefix_options'].items():
            lines.append('\t\t%s;' % ' '.join(map(str, prefix_option)))
        lines.append('\t};')
        if conf['clients']:
            lines.append('\tclients')
            lines.append('\t{')
            for client in conf['clients']:
                lines.append('\t\t%s;' % client)
            lines.append('\t};')
        if conf['route']:
            lines.append('\troute %s {' % conf['route'])
            for route_option in conf['route_options'].items():
                lines.append('\t\t%s;' % ' '.join(map(str, route_option)))
            lines.append('\t};')
        if conf['rdnss']:
            lines.append('\tRDNSS %s {' %
                         ' '.join([str(elem) for elem in conf['rdnss']]))
            for rdnss_option in conf['rdnss_options'].items():
                lines.append('\t\t%s;' % ' '.join(map(str, rdnss_option)))
            lines.append('\t};')
        lines.append('};')
        output_config = '\n'.join(lines)
        logging.info('Writing %s' % self._config_file)
        logging.debug('******************Start*******************')
        logging.debug('\n%s' % output_config)
        logging.debug('*******************End********************')

        self._shell.write_file(self._config_file, output_config)
