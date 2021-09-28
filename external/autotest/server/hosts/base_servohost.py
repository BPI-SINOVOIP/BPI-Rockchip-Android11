# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Expects to be run in an environment with sudo and no interactive password
# prompt, such as within the Chromium OS development chroot.


"""This is a base host class for servohost and labstation."""


import httplib
import logging
import socket
import traceback
import xmlrpclib

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import hosts
from autotest_lib.client.common_lib import lsbrelease_utils
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.client.cros import constants as client_constants
from autotest_lib.server import afe_utils
from autotest_lib.server import site_utils as server_utils
from autotest_lib.server.cros import autoupdater
from autotest_lib.server.hosts import ssh_host
from autotest_lib.site_utils.rpm_control_system import rpm_client

try:
    from chromite.lib import metrics
except ImportError:
    metrics = utils.metrics_mock


class BaseServoHost(ssh_host.SSHHost):
    """Base host class for a host that manage servo(s).
     E.g. beaglebone, labstation.
    """
    REBOOT_CMD = 'sleep 5; reboot & sleep 10; reboot -f'

    TEMP_FILE_DIR = '/var/lib/servod/'

    LOCK_FILE_POSTFIX = '_in_use'
    REBOOT_FILE_POSTFIX = '_reboot'

    # Time to wait a rebooting servohost, in seconds.
    REBOOT_TIMEOUT = 240

    # Timeout value to power cycle a servohost, in seconds.
    BOOT_TIMEOUT = 240


    def _initialize(self, hostname, is_in_lab=None, *args, **dargs):
        """Construct a BaseServoHost object.

        @param is_in_lab: True if the servo host is in Cros Lab. Default is set
                          to None, for which utils.host_is_in_lab_zone will be
                          called to check if the servo host is in Cros lab.

        """
        super(BaseServoHost, self)._initialize(hostname=hostname,
                                               *args, **dargs)
        self._is_localhost = (self.hostname == 'localhost')
        if self._is_localhost:
            self._is_in_lab = False
        elif is_in_lab is None:
            self._is_in_lab = utils.host_is_in_lab_zone(self.hostname)
        else:
            self._is_in_lab = is_in_lab

        # Commands on the servo host must be run by the superuser.
        # Our account on a remote host is root, but if our target is
        # localhost then we might be running unprivileged.  If so,
        # `sudo` will have to be added to the commands.
        if self._is_localhost:
            self._sudo_required = utils.system_output('id -u') != '0'
        else:
            self._sudo_required = False

        self._is_labstation = None
        self._dut_host_info = None


    def get_board(self):
        """Determine the board for this servo host. E.g. fizz-labstation

        @returns a string representing this labstation's board or None if
         target host is not using a ChromeOS image(e.g. test in chroot).
        """
        output = self.run('cat /etc/lsb-release', ignore_status=True).stdout
        return lsbrelease_utils.get_current_board(lsb_release_content=output)


    def set_dut_host_info(self, hi):
        logging.info('setting dut_host_info field to (%s)', hi)
        self._dut_host_info = hi


    def is_labstation(self):
        """Determine if the host is a labstation

        @returns True if ths host is a labstation otherwise False.
        """
        if self._is_labstation is None:
            board = self.get_board()
            self._is_labstation = board is not None and 'labstation' in board

        return self._is_labstation


    def _get_release_version(self):
        """Get the value of attribute CHROMEOS_RELEASE_VERSION from lsb-release.

        @returns The version string in lsb-release, under attribute
                 CHROMEOS_RELEASE_VERSION.
        """
        lsb_release_content = self.run(
            'cat "%s"' % client_constants.LSB_RELEASE).stdout.strip()
        return lsbrelease_utils.get_chromeos_release_version(
            lsb_release_content=lsb_release_content)


    def _check_update_status(self):
        dummy_updater = autoupdater.ChromiumOSUpdater(update_url="", host=self)
        return dummy_updater.check_update_status()


    def is_in_lab(self):
        """Check whether the servo host is a lab device.

        @returns: True if the servo host is in Cros Lab, otherwise False.

        """
        return self._is_in_lab


    def is_localhost(self):
        """Checks whether the servo host points to localhost.

        @returns: True if it points to localhost, otherwise False.

        """
        return self._is_localhost


    def is_cros_host(self):
        """Check if a servo host is running chromeos.

        @return: True if the servo host is running chromeos.
            False if it isn't, or we don't have enough information.
        """
        try:
            result = self.run('grep -q CHROMEOS /etc/lsb-release',
                              ignore_status=True, timeout=10)
        except (error.AutoservRunError, error.AutoservSSHTimeout):
            return False
        return result.exit_status == 0


    def reboot(self, *args, **dargs):
        """Reboot using special servo host reboot command."""
        super(BaseServoHost, self).reboot(reboot_cmd=self.REBOOT_CMD,
                                          *args, **dargs)


    def update_image(self, wait_for_update=False):
        """Update the image on the servo host, if needed.

        This method recognizes the following cases:
          * If the Host is not running Chrome OS, do nothing.
          * If a previously triggered update is now complete, reboot
            to the new version.
          * If the host is processing a previously triggered update,
            do nothing.
          * If the host is running a version of Chrome OS different
            from the default for servo Hosts, trigger an update, but
            don't wait for it to complete.

        @param wait_for_update If an update needs to be applied and
            this is true, then don't return until the update is
            downloaded and finalized, and the host rebooted.
        @raises dev_server.DevServerException: If all the devservers are down.
        @raises site_utils.ParseBuildNameException: If the devserver returns
            an invalid build name.
        @raises AutoservRunError: If the update_engine_client isn't present on
            the host, and the host is a cros_host.

        """
        # servod could be running in a Ubuntu workstation.
        if not self.is_cros_host():
            logging.info('Not attempting an update, either %s is not running '
                         'chromeos or we cannot find enough information about '
                         'the host.', self.hostname)
            return

        if lsbrelease_utils.is_moblab():
            logging.info('Not attempting an update, %s is running moblab.',
                         self.hostname)
            return


        # NOTE: we can't just use getattr because servo_cros_stable_version is a property
        servo_version_from_hi = None
        logging.debug("BaseServoHost::update_image attempted to get servo cros stable version")
        try:
            servo_version_from_hi = self._dut_host_info.servo_cros_stable_version
        except Exception:
            logging.error("BaseServoHost::update_image failed to get servo cros stable version (%s)", traceback.format_exc())

        target_build = afe_utils.get_stable_servo_cros_image_name_v2(
            servo_version_from_hi=servo_version_from_hi,
            board=self.get_board(),
        )
        target_build_number = server_utils.ParseBuildName(
            target_build)[3]
        current_build_number = self._get_release_version()

        if current_build_number == target_build_number:
            logging.info('servo host %s does not require an update.',
                         self.hostname)
            return

        status = self._check_update_status()
        if status in autoupdater.UPDATER_PROCESSING_UPDATE:
            logging.info('servo host %s already processing an update, update '
                         'engine client status=%s', self.hostname, status)
        elif status == autoupdater.UPDATER_NEED_REBOOT:
            logging.info('An update has been completed and pending reboot now.')
            # Labstation reboot is handled separately here as it require
            # synchronized reboot among all managed DUTs.
            if not self.is_labstation():
                self._servo_host_reboot()
        else:
            # For servo image staging, we want it as more widely distributed as
            # possible, so that devservers' load can be evenly distributed.
            # So use hostname instead of target_build as hash.
            ds = dev_server.ImageServer.resolve(self.hostname,
                                                hostname=self.hostname)
            url = ds.get_update_url(target_build)

            updater = autoupdater.ChromiumOSUpdater(update_url=url, host=self)

            logging.info('Using devserver url: %s to trigger update on '
                         'servo host %s, from %s to %s', url, self.hostname,
                         current_build_number, target_build_number)
            try:
                ds.stage_artifacts(target_build,
                                   artifacts=['full_payload'])
            except Exception as e:
                logging.error('Staging artifacts failed: %s', str(e))
                logging.error('Abandoning update for this cycle.')
            else:
                try:
                    updater.trigger_update()
                except autoupdater.RootFSUpdateError as e:
                    trigger_download_status = 'failed with %s' % str(e)
                    metrics.Counter('chromeos/autotest/servo/'
                                    'rootfs_update_failed').increment()
                else:
                    trigger_download_status = 'passed'
                logging.info('Triggered download and update %s for %s, '
                             'update engine currently in status %s',
                             trigger_download_status, self.hostname,
                             updater.check_update_status())

        if wait_for_update:
            logging.info('Waiting for servo update to complete.')
            self.run('update_engine_client --follow', ignore_status=True)


    def has_power(self):
        """Return whether or not the servo host is powered by PoE or RPM."""
        # TODO(fdeng): See crbug.com/302791
        # For now, assume all servo hosts in the lab have power.
        return self.is_in_lab()


    def power_cycle(self):
        """Cycle power to this host via PoE(servo v3) or RPM(labstation)
        if it is a lab device.

        @raises AutoservRepairError if it fails to power cycle the
                servo host.

        """
        if self.has_power():
            try:
                rpm_client.set_power(self, 'CYCLE')
            except (socket.error, xmlrpclib.Error,
                    httplib.BadStatusLine,
                    rpm_client.RemotePowerException) as e:
                raise hosts.AutoservRepairError(
                    'Power cycling %s failed: %s' % (self.hostname, e),
                    'power_cycle_via_rpm_failed'
                )
        else:
            logging.info('Skipping power cycling, not a lab device.')


    def _servo_host_reboot(self):
        """Reboot this servo host because a reboot is requested."""
        logging.info('Rebooting servo host %s from build %s', self.hostname,
                     self._get_release_version())
        # Tell the reboot() call not to wait for completion.
        # Otherwise, the call will log reboot failure if servo does
        # not come back.  The logged reboot failure will lead to
        # test job failure.  If the test does not require servo, we
        # don't want servo failure to fail the test with error:
        # `Host did not return from reboot` in status.log.
        self.reboot(fastsync=True, wait=False)

        # We told the reboot() call not to wait, but we need to wait
        # for the reboot before we continue.  Alas.  The code from
        # here below is basically a copy of Host.wait_for_restart(),
        # with the logging bits ripped out, so that they can't cause
        # the failure logging problem described above.
        #
        # The black stain that this has left on my soul can never be
        # erased.
        old_boot_id = self.get_boot_id()
        if not self.wait_down(timeout=self.WAIT_DOWN_REBOOT_TIMEOUT,
                              warning_timer=self.WAIT_DOWN_REBOOT_WARNING,
                              old_boot_id=old_boot_id):
            raise error.AutoservHostError(
                'servo host %s failed to shut down.' %
                self.hostname)
        if self.wait_up(timeout=self.REBOOT_TIMEOUT):
            logging.info('servo host %s back from reboot, with build %s',
                         self.hostname, self._get_release_version())
        else:
            raise error.AutoservHostError(
                'servo host %s failed to come back from reboot.' %
                self.hostname)


    def make_ssh_command(self, user='root', port=22, opts='', hosts_file=None,
        connect_timeout=None, alive_interval=None, alive_count_max=None,
        connection_attempts=None):
        """Override default make_ssh_command to use tuned options.

        Tuning changes:
          - ConnectTimeout=30; maximum of 30 seconds allowed for an SSH
          connection failure. Consistency with remote_access.py.

          - ServerAliveInterval=180; which causes SSH to ping connection every
          180 seconds. In conjunction with ServerAliveCountMax ensures
          that if the connection dies, Autotest will bail out quickly.

          - ServerAliveCountMax=3; consistency with remote_access.py.

          - ConnectAttempts=4; reduce flakiness in connection errors;
          consistency with remote_access.py.

          - UserKnownHostsFile=/dev/null; we don't care about the keys.

          - SSH protocol forced to 2; needed for ServerAliveInterval.

        @param user User name to use for the ssh connection.
        @param port Port on the target host to use for ssh connection.
        @param opts Additional options to the ssh command.
        @param hosts_file Ignored.
        @param connect_timeout Ignored.
        @param alive_interval Ignored.
        @param alive_count_max Ignored.
        @param connection_attempts Ignored.

        @returns: An ssh command with the requested settings.

        """
        options = ' '.join([opts, '-o Protocol=2'])
        return super(BaseServoHost, self).make_ssh_command(
            user=user, port=port, opts=options, hosts_file='/dev/null',
            connect_timeout=30, alive_interval=180, alive_count_max=3,
            connection_attempts=4)


    def _make_scp_cmd(self, sources, dest):
        """Format scp command.

        Given a list of source paths and a destination path, produces the
        appropriate scp command for encoding it. Remote paths must be
        pre-encoded. Overrides _make_scp_cmd in AbstractSSHHost
        to allow additional ssh options.

        @param sources: A list of source paths to copy from.
        @param dest: Destination path to copy to.

        @returns: An scp command that copies |sources| on local machine to
                  |dest| on the remote servo host.

        """
        command = ('scp -rq %s -o BatchMode=yes -o StrictHostKeyChecking=no '
                   '-o UserKnownHostsFile=/dev/null -P %d %s "%s"')
        return command % (self._master_ssh.ssh_option,
                          self.port, sources, dest)


    def run(self, command, timeout=3600, ignore_status=False,
        stdout_tee=utils.TEE_TO_LOGS, stderr_tee=utils.TEE_TO_LOGS,
        connect_timeout=30, ssh_failure_retry_ok=False,
        options='', stdin=None, verbose=True, args=()):
        """Run a command on the servo host.

        Extends method `run` in SSHHost. If the servo host is a remote device,
        it will call `run` in SSHost without changing anything.
        If the servo host is 'localhost', it will call utils.system_output.

        @param command: The command line string.
        @param timeout: Time limit in seconds before attempting to
                        kill the running process. The run() function
                        will take a few seconds longer than 'timeout'
                        to complete if it has to kill the process.
        @param ignore_status: Do not raise an exception, no matter
                              what the exit code of the command is.
        @param stdout_tee/stderr_tee: Where to tee the stdout/stderr.
        @param connect_timeout: SSH connection timeout (in seconds)
                                Ignored if host is 'localhost'.
        @param options: String with additional ssh command options
                        Ignored if host is 'localhost'.
        @param ssh_failure_retry_ok: when True and ssh connection failure is
                                     suspected, OK to retry command (but not
                                     compulsory, and likely not needed here)
        @param stdin: Stdin to pass (a string) to the executed command.
        @param verbose: Log the commands.
        @param args: Sequence of strings to pass as arguments to command by
                     quoting them in " and escaping their contents if necessary.

        @returns: A utils.CmdResult object.

        @raises AutoservRunError if the command failed.
        @raises AutoservSSHTimeout SSH connection has timed out. Only applies
                when servo host is not 'localhost'.

        """
        run_args = {'command': command, 'timeout': timeout,
                    'ignore_status': ignore_status, 'stdout_tee': stdout_tee,
                    'stderr_tee': stderr_tee, 'stdin': stdin,
                    'verbose': verbose, 'args': args}
        if self.is_localhost():
            if self._sudo_required:
                run_args['command'] = 'sudo -n sh -c "%s"' % utils.sh_escape(
                    command)
            try:
                return utils.run(**run_args)
            except error.CmdError as e:
                logging.error(e)
                raise error.AutoservRunError('command execution error',
                                             e.result_obj)
        else:
            run_args['connect_timeout'] = connect_timeout
            run_args['options'] = options
            return super(BaseServoHost, self).run(**run_args)
