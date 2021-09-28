# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os
import re
import sys
import urllib2
import urlparse

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error, global_config
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.server import autotest
from autotest_lib.server import utils as server_utils
from autotest_lib.server.cros.dynamic_suite import constants as ds_constants
from autotest_lib.server.cros.dynamic_suite import tools
from chromite.lib import retry_util

try:
    from chromite.lib import metrics
except ImportError:
    metrics = utils.metrics_mock


def _metric_name(base_name):
    return 'chromeos/autotest/provision/' + base_name


# Local stateful update path is relative to the CrOS source directory.
UPDATER_IDLE = 'UPDATE_STATUS_IDLE'
UPDATER_NEED_REBOOT = 'UPDATE_STATUS_UPDATED_NEED_REBOOT'
# A list of update engine client states that occur after an update is triggered.
UPDATER_PROCESSING_UPDATE = ['UPDATE_STATUS_CHECKING_FOR_UPDATE',
                             'UPDATE_STATUS_UPDATE_AVAILABLE',
                             'UPDATE_STATUS_DOWNLOADING',
                             'UPDATE_STATUS_FINALIZING',
                             'UPDATE_STATUS_VERIFYING',
                             'UPDATE_STATUS_REPORTING_ERROR_EVENT',
                             'UPDATE_STATUS_ATTEMPTING_ROLLBACK']


_STATEFUL_UPDATE_SCRIPT = 'stateful_update'
_QUICK_PROVISION_SCRIPT = 'quick-provision'

_UPDATER_BIN = '/usr/bin/update_engine_client'
_UPDATER_LOGS = ['/var/log/messages', '/var/log/update_engine']

_KERNEL_A = {'name': 'KERN-A', 'kernel': 2, 'root': 3}
_KERNEL_B = {'name': 'KERN-B', 'kernel': 4, 'root': 5}

# Time to wait for new kernel to be marked successful after
# auto update.
_KERNEL_UPDATE_TIMEOUT = 120


# PROVISION_FAILED - A flag file to indicate provision failures.  The
# file is created at the start of any AU procedure (see
# `ChromiumOSUpdater._prepare_host()`).  The file's location in
# stateful means that on successul update it will be removed.  Thus, if
# this file exists, it indicates that we've tried and failed in a
# previous attempt to update.
PROVISION_FAILED = '/var/tmp/provision_failed'


# A flag file used to enable special handling in lab DUTs.  Some
# parts of the system in Chromium OS test images will behave in ways
# convenient to the test lab when this file is present.  Generally,
# we create this immediately after any update completes.
_LAB_MACHINE_FILE = '/mnt/stateful_partition/.labmachine'


# _TARGET_VERSION - A file containing the new version to which we plan
# to update.  This file is used by the CrOS shutdown code to detect and
# handle certain version downgrade cases.  Specifically:  Downgrading
# may trigger an unwanted powerwash in the target build when the
# following conditions are met:
#  * Source build is a v4.4 kernel with R69-10756.0.0 or later.
#  * Target build predates the R69-10756.0.0 cutoff.
# When this file is present and indicates a downgrade, the OS shutdown
# code on the DUT knows how to prevent the powerwash.
_TARGET_VERSION = '/run/update_target_version'


# _REBOOT_FAILURE_MESSAGE - This is the standard message text returned
# when the Host.reboot() method fails.  The source of this text comes
# from `wait_for_restart()` in client/common_lib/hosts/base_classes.py.

_REBOOT_FAILURE_MESSAGE = 'Host did not return from reboot'


DEVSERVER_PORT = '8082'
GS_CACHE_PORT = '8888'


class RootFSUpdateError(error.TestFail):
    """Raised when the RootFS fails to update."""


class StatefulUpdateError(error.TestFail):
    """Raised when the stateful partition fails to update."""


class _AttributedUpdateError(error.TestFail):
    """Update failure with an attributed cause."""

    def __init__(self, attribution, msg):
        super(_AttributedUpdateError, self).__init__(
            '%s: %s' % (attribution, msg))
        self._message = msg

    def _classify(self):
        for err_pattern, classification in self._CLASSIFIERS:
            if re.match(err_pattern, self._message):
                return classification
        return None

    @property
    def failure_summary(self):
        """Summarize this error for metrics reporting."""
        classification = self._classify()
        if classification:
            return '%s: %s' % (self._SUMMARY, classification)
        else:
            return self._SUMMARY


class HostUpdateError(_AttributedUpdateError):
    """Failure updating a DUT attributable to the DUT.

    This class of exception should be raised when the most likely cause
    of failure was a condition existing on the DUT prior to the update,
    such as a hardware problem, or a bug in the software on the DUT.
    """

    DUT_DOWN = 'No answer to ssh'

    _SUMMARY = 'DUT failed prior to update'
    _CLASSIFIERS = [
        (DUT_DOWN, DUT_DOWN),
        (_REBOOT_FAILURE_MESSAGE, 'Reboot failed'),
    ]

    def __init__(self, hostname, msg):
        super(HostUpdateError, self).__init__(
            'Error on %s prior to update' % hostname, msg)


class DevServerError(_AttributedUpdateError):
    """Failure updating a DUT attributable to the devserver.

    This class of exception should be raised when the most likely cause
    of failure was the devserver serving the target image for update.
    """

    _SUMMARY = 'Devserver failed prior to update'
    _CLASSIFIERS = []

    def __init__(self, devserver, msg):
        super(DevServerError, self).__init__(
            'Devserver error on %s' % devserver, msg)


class ImageInstallError(_AttributedUpdateError):
    """Failure updating a DUT when installing from the devserver.

    This class of exception should be raised when the target DUT fails
    to download and install the target image from the devserver, and
    either the devserver or the DUT might be at fault.
    """

    _SUMMARY = 'Image failed to download and install'
    _CLASSIFIERS = []

    def __init__(self, hostname, devserver, msg):
        super(ImageInstallError, self).__init__(
            'Download and install failed from %s onto %s'
            % (devserver, hostname), msg)


class NewBuildUpdateError(_AttributedUpdateError):
    """Failure updating a DUT attributable to the target build.

    This class of exception should be raised when updating to a new
    build fails, and the most likely cause of the failure is a bug in
    the newly installed target build.
    """

    CHROME_FAILURE = 'Chrome failed to reach login screen'
    UPDATE_ENGINE_FAILURE = ('update-engine failed to call '
                             'chromeos-setgoodkernel')
    ROLLBACK_FAILURE = 'System rolled back to previous build'

    _SUMMARY = 'New build failed'
    _CLASSIFIERS = [
        (CHROME_FAILURE, 'Chrome did not start'),
        (UPDATE_ENGINE_FAILURE, 'update-engine did not start'),
        (ROLLBACK_FAILURE, ROLLBACK_FAILURE),
    ]

    def __init__(self, update_version, msg):
        super(NewBuildUpdateError, self).__init__(
            'Failure in build %s' % update_version, msg)

    @property
    def failure_summary(self):
        #pylint: disable=missing-docstring
        return 'Build failed to work after installing'


def _url_to_version(update_url):
    """Return the version based on update_url.

    @param update_url: url to the image to update to.

    """
    # The Chrome OS version is generally the last element in the URL. The only
    # exception is delta update URLs, which are rooted under the version; e.g.,
    # http://.../update/.../0.14.755.0/au/0.14.754.0. In this case we want to
    # strip off the au section of the path before reading the version.
    return re.sub('/au/.*', '',
                  urlparse.urlparse(update_url).path).split('/')[-1].strip()


def url_to_image_name(update_url):
    """Return the image name based on update_url.

    From a URL like:
        http://172.22.50.205:8082/update/lumpy-release/R27-3837.0.0
    return lumpy-release/R27-3837.0.0

    @param update_url: url to the image to update to.
    @returns a string representing the image name in the update_url.

    """
    return '/'.join(urlparse.urlparse(update_url).path.split('/')[-2:])


def get_update_failure_reason(exception):
    """Convert an exception into a failure reason for metrics.

    The passed in `exception` should be one raised by failure of
    `ChromiumOSUpdater.run_update`.  The returned string will describe
    the failure.  If the input exception value is not a truish value
    the return value will be `None`.

    The number of possible return strings is restricted to a limited
    enumeration of values so that the string may be safely used in
    Monarch metrics without worrying about cardinality of the range of
    string values.

    @param exception  Exception to be converted to a failure reason.

    @return A string suitable for use in Monarch metrics, or `None`.
    """
    if exception:
        if isinstance(exception, _AttributedUpdateError):
            return exception.failure_summary
        else:
            return 'Unknown Error: %s' % type(exception).__name__
    return None


def _get_devserver_build_from_update_url(update_url):
    """Get the devserver and build from the update url.

    @param update_url: The url for update.
        Eg: http://devserver:port/update/build.

    @return: A tuple of (devserver url, build) or None if the update_url
        doesn't match the expected pattern.

    @raises ValueError: If the update_url doesn't match the expected pattern.
    @raises ValueError: If no global_config was found, or it doesn't contain an
        image_url_pattern.
    """
    pattern = global_config.global_config.get_config_value(
            'CROS', 'image_url_pattern', type=str, default='')
    if not pattern:
        raise ValueError('Cannot parse update_url, the global config needs '
                'an image_url_pattern.')
    re_pattern = pattern.replace('%s', '(\S+)')
    parts = re.search(re_pattern, update_url)
    if not parts or len(parts.groups()) < 2:
        raise ValueError('%s is not an update url' % update_url)
    return parts.groups()


def _list_image_dir_contents(update_url):
    """Lists the contents of the devserver for a given build/update_url.

    @param update_url: An update url. Eg: http://devserver:port/update/build.
    """
    if not update_url:
        logging.warning('Need update_url to list contents of the devserver.')
        return
    error_msg = 'Cannot check contents of devserver, update url %s' % update_url
    try:
        devserver_url, build = _get_devserver_build_from_update_url(update_url)
    except ValueError as e:
        logging.warning('%s: %s', error_msg, e)
        return
    devserver = dev_server.ImageServer(devserver_url)
    try:
        devserver.list_image_dir(build)
    # The devserver will retry on URLError to avoid flaky connections, but will
    # eventually raise the URLError if it persists. All HTTPErrors get
    # converted to DevServerExceptions.
    except (dev_server.DevServerException, urllib2.URLError) as e:
        logging.warning('%s: %s', error_msg, e)


def _get_metric_fields(update_url):
    """Return a dict of metric fields.

    This is used for sending autoupdate metrics for the given update URL.

    @param update_url  Metrics fields will be calculated from this URL.
    """
    build_name = url_to_image_name(update_url)
    try:
        board, build_type, milestone, _ = server_utils.ParseBuildName(
            build_name)
    except server_utils.ParseBuildNameException:
        logging.warning('Unable to parse build name %s for metrics. '
                        'Continuing anyway.', build_name)
        board, build_type, milestone = ('', '', '')
    return {
        'dev_server': dev_server.get_resolved_hostname(update_url),
        'board': board,
        'build_type': build_type,
        'milestone': milestone,
    }


# TODO(garnold) This implements shared updater functionality needed for
# supporting the autoupdate_EndToEnd server-side test. We should probably
# migrate more of the existing ChromiumOSUpdater functionality to it as we
# expand non-CrOS support in other tests.
class ChromiumOSUpdater(object):
    """Chromium OS specific DUT update functionality."""

    def __init__(self, update_url, host=None, interactive=True,
                 use_quick_provision=False):
        """Initializes the object.

        @param update_url: The URL we want the update to use.
        @param host: A client.common_lib.hosts.Host implementation.
        @param interactive: Bool whether we are doing an interactive update.
        @param use_quick_provision: Whether we should attempt to perform
            the update using the quick-provision script.
        """
        self.update_url = update_url
        self.host = host
        self.interactive = interactive
        self.update_version = _url_to_version(update_url)
        self._use_quick_provision = use_quick_provision


    def _run(self, cmd, *args, **kwargs):
        """Abbreviated form of self.host.run(...)"""
        return self.host.run(cmd, *args, **kwargs)


    def check_update_status(self):
        """Returns the current update engine state.

        We use the `update_engine_client -status' command and parse the line
        indicating the update state, e.g. "CURRENT_OP=UPDATE_STATUS_IDLE".
        """
        update_status = self.host.run(command='%s -status | grep CURRENT_OP' %
                                      _UPDATER_BIN)
        return update_status.stdout.strip().split('=')[-1]


    def _rootdev(self, options=''):
        """Returns the stripped output of rootdev <options>.

        @param options: options to run rootdev.

        """
        return self._run('rootdev %s' % options).stdout.strip()


    def get_kernel_state(self):
        """Returns the (<active>, <inactive>) kernel state as a pair.

        @raise RootFSUpdateError if the DUT reports a root partition
                number that isn't one of the known valid values.
        """
        active_root = int(re.findall('\d+\Z', self._rootdev('-s'))[0])
        if active_root == _KERNEL_A['root']:
            return _KERNEL_A, _KERNEL_B
        elif active_root == _KERNEL_B['root']:
            return _KERNEL_B, _KERNEL_A
        else:
            raise RootFSUpdateError(
                    'Encountered unknown root partition: %s' % active_root)


    def _cgpt(self, flag, kernel):
        """Return numeric cgpt value for the specified flag, kernel, device."""
        return int(self._run('cgpt show -n -i %d %s $(rootdev -s -d)' % (
            kernel['kernel'], flag)).stdout.strip())


    def _get_next_kernel(self):
        """Return the kernel that has priority for the next boot."""
        priority_a = self._cgpt('-P', _KERNEL_A)
        priority_b = self._cgpt('-P', _KERNEL_B)
        if priority_a > priority_b:
            return _KERNEL_A
        else:
            return _KERNEL_B


    def _get_kernel_success(self, kernel):
        """Return boolean success flag for the specified kernel.

        @param kernel: information of the given kernel, either _KERNEL_A
            or _KERNEL_B.
        """
        return self._cgpt('-S', kernel) != 0


    def _get_kernel_tries(self, kernel):
        """Return tries count for the specified kernel.

        @param kernel: information of the given kernel, either _KERNEL_A
            or _KERNEL_B.
        """
        return self._cgpt('-T', kernel)


    def _get_last_update_error(self):
        """Get the last autoupdate error code."""
        command_result = self._run(
                 '%s --last_attempt_error' % _UPDATER_BIN)
        return command_result.stdout.strip().replace('\n', ', ')


    def _base_update_handler_no_retry(self, run_args):
        """Base function to handle a remote update ssh call.

        @param run_args: Dictionary of args passed to ssh_host.run function.

        @throws: intercepts and re-throws all exceptions
        """
        try:
            self.host.run(**run_args)
        except Exception as e:
            logging.debug('exception in update handler: %s', e)
            raise e


    def _base_update_handler(self, run_args, err_msg_prefix=None):
        """Handle a remote update ssh call, possibly with retries.

        @param run_args: Dictionary of args passed to ssh_host.run function.
        @param err_msg_prefix: Prefix of the exception error message.
        """
        def exception_handler(e):
            """Examines exceptions and returns True if the update handler
            should be retried.

            @param e: the exception intercepted by the retry util.
            """
            return (isinstance(e, error.AutoservSSHTimeout) or
                    (isinstance(e, error.GenericHostRunError) and
                     hasattr(e, 'description') and
                     (re.search('ERROR_CODE=37', e.description) or
                      re.search('generic error .255.', e.description))))

        try:
            # Try the update twice (arg 2 is max_retry, not including the first
            # call).  Some exceptions may be caught by the retry handler.
            retry_util.GenericRetry(exception_handler, 1,
                                    self._base_update_handler_no_retry,
                                    run_args)
        except Exception as e:
            message = err_msg_prefix + ': ' + str(e)
            raise RootFSUpdateError(message)


    def _wait_for_update_service(self):
        """Ensure that the update engine daemon is running, possibly
        by waiting for it a bit in case the DUT just rebooted and the
        service hasn't started yet.
        """
        def handler(e):
            """Retry exception handler.

            Assumes that the error is due to the update service not having
            started yet.

            @param e: the exception intercepted by the retry util.
            """
            if isinstance(e, error.AutoservRunError):
                logging.debug('update service check exception: %s\n'
                              'retrying...', e)
                return True
            else:
                return False

        # Retry at most three times, every 5s.
        status = retry_util.GenericRetry(handler, 3,
                                         self.check_update_status,
                                         sleep=5)

        # Expect the update engine to be idle.
        if status != UPDATER_IDLE:
            raise RootFSUpdateError(
                    'Update engine status is %s (%s was expected).'
                    % (status, UPDATER_IDLE))


    def _reset_update_engine(self):
        """Resets the host to prepare for a clean update regardless of state."""
        self._run('stop ui || true')
        self._run('stop update-engine || true')
        self._run('start update-engine')
        self._wait_for_update_service()


    def _reset_stateful_partition(self):
        """Clear any pending stateful update request."""
        self._run('%s --stateful_change=reset 2>&1'
                  % self._get_stateful_update_script())
        self._run('rm -f %s' % _TARGET_VERSION)


    def _set_target_version(self):
        """Set the "target version" for the update."""
        version_number = self.update_version.split('-')[1]
        self._run('echo %s > %s' % (version_number, _TARGET_VERSION))


    def _revert_boot_partition(self):
        """Revert the boot partition."""
        part = self._rootdev('-s')
        logging.warning('Reverting update; Boot partition will be %s', part)
        return self._run('/postinst %s 2>&1' % part)


    def _verify_kernel_state(self):
        """Verify that the next kernel to boot is correct for update.

        This tests that the kernel state is correct for a successfully
        downloaded and installed update.  That is, the next kernel to
        boot must be the currently inactive kernel.

        @raise RootFSUpdateError if the DUT next kernel isn't the
                expected next kernel.
        """
        inactive_kernel = self.get_kernel_state()[1]
        next_kernel = self._get_next_kernel()
        if next_kernel != inactive_kernel:
            raise RootFSUpdateError(
                    'Update failed.  The kernel for next boot is %s, '
                    'but %s was expected.'
                    % (next_kernel['name'], inactive_kernel['name']))
        return inactive_kernel


    def _verify_update_completed(self):
        """Verifies that an update has completed.

        @raise RootFSUpdateError if the DUT doesn't indicate that
                download is complete and the DUT is ready for reboot.
        """
        status = self.check_update_status()
        if status != UPDATER_NEED_REBOOT:
            error_msg = ''
            if status == UPDATER_IDLE:
                error_msg = 'Update error: %s' % self._get_last_update_error()
            raise RootFSUpdateError(
                    'Update engine status is %s (%s was expected).  %s'
                    % (status, UPDATER_NEED_REBOOT, error_msg))
        return self._verify_kernel_state()


    def trigger_update(self):
        """Triggers a background update."""
        # If this function is called immediately after reboot (which it
        # can be), there is no guarantee that the update engine is up
        # and running yet, so wait for it.
        self._wait_for_update_service()

        autoupdate_cmd = ('%s --check_for_update --omaha_url=%s' %
                          (_UPDATER_BIN, self.update_url))
        run_args = {'command': autoupdate_cmd}
        err_prefix = 'Failed to trigger an update on %s. ' % self.host.hostname
        logging.info('Triggering update via: %s', autoupdate_cmd)
        metric_fields = {'success': False}
        try:
            self._base_update_handler(run_args, err_prefix)
            metric_fields['success'] = True
        finally:
            c = metrics.Counter('chromeos/autotest/autoupdater/trigger')
            metric_fields.update(_get_metric_fields(self.update_url))
            c.increment(fields=metric_fields)


    def update_image(self):
        """Updates the device root FS and kernel and verifies success."""
        autoupdate_cmd = ('%s --update --omaha_url=%s' %
                          (_UPDATER_BIN, self.update_url))
        if not self.interactive:
            autoupdate_cmd = '%s --interactive=false' % autoupdate_cmd
        run_args = {'command': autoupdate_cmd, 'timeout': 3600}
        err_prefix = ('Failed to install device image using payload at %s '
                      'on %s. ' % (self.update_url, self.host.hostname))
        logging.info('Updating image via: %s', autoupdate_cmd)
        metric_fields = {'success': False}
        try:
            self._base_update_handler(run_args, err_prefix)
            metric_fields['success'] = True
        finally:
            c = metrics.Counter('chromeos/autotest/autoupdater/update')
            metric_fields.update(_get_metric_fields(self.update_url))
            c.increment(fields=metric_fields)
        return self._verify_update_completed()


    def _get_remote_script(self, script_name):
        """Ensure that `script_name` is present on the DUT.

        The given script (e.g. `stateful_update`) may be present in the
        stateful partition under /usr/local/bin, or we may have to
        download it from the devserver.

        Determine whether the script is present or must be downloaded
        and download if necessary.  Then, return a command fragment
        sufficient to run the script from whereever it now lives on the
        DUT.

        @param script_name  The name of the script as expected in
                            /usr/local/bin and on the devserver.
        @return A string with the command (minus arguments) that will
                run the target script.
        """
        remote_script = '/usr/local/bin/%s' % script_name
        if self.host.path_exists(remote_script):
            return remote_script
        remote_tmp_script = '/tmp/%s' % script_name
        server_name = urlparse.urlparse(self.update_url)[1]
        script_url = 'http://%s/static/%s' % (server_name, script_name)
        fetch_script = 'curl -Ss -o %s %s && head -1 %s' % (
            remote_tmp_script, script_url, remote_tmp_script)

        first_line = self._run(fetch_script).stdout.strip()

        if first_line and first_line.startswith('#!'):
            script_interpreter = first_line.lstrip('#!')
            if script_interpreter:
                return '%s %s' % (script_interpreter, remote_tmp_script)
        return None

    def _get_stateful_update_script(self):
        """Returns a command to run the stateful update script.

        Find `stateful_update` on the target or install it, as
        necessary.  If installation fails, raise an exception.

        @raise StatefulUpdateError if the script can't be found or
            installed.
        @return A string that can be joined with arguments to run the
            `stateful_update` command on the DUT.
        """
        script_command = self._get_remote_script(_STATEFUL_UPDATE_SCRIPT)
        if not script_command:
            raise StatefulUpdateError('Could not install %s on DUT'
                                      % _STATEFUL_UPDATE_SCRIPT)
        return script_command


    def rollback_rootfs(self, powerwash):
        """Triggers rollback and waits for it to complete.

        @param powerwash: If true, powerwash as part of rollback.

        @raise RootFSUpdateError if anything went wrong.
        """
        version = self.host.get_release_version()
        # Introduced can_rollback in M36 (build 5772). # etc/lsb-release matches
        # X.Y.Z. This version split just pulls the first part out.
        try:
            build_number = int(version.split('.')[0])
        except ValueError:
            logging.error('Could not parse build number.')
            build_number = 0

        if build_number >= 5772:
            can_rollback_cmd = '%s --can_rollback' % _UPDATER_BIN
            logging.info('Checking for rollback.')
            try:
                self._run(can_rollback_cmd)
            except error.AutoservRunError as e:
                raise RootFSUpdateError("Rollback isn't possible on %s: %s" %
                                        (self.host.hostname, str(e)))

        rollback_cmd = '%s --rollback --follow' % _UPDATER_BIN
        if not powerwash:
            rollback_cmd += ' --nopowerwash'

        logging.info('Performing rollback.')
        try:
            self._run(rollback_cmd)
        except error.AutoservRunError as e:
            raise RootFSUpdateError('Rollback failed on %s: %s' %
                                    (self.host.hostname, str(e)))

        self._verify_update_completed()


    def update_stateful(self, clobber=True):
        """Updates the stateful partition.

        @param clobber: If True, a clean stateful installation.

        @raise StatefulUpdateError if the update script fails to
                complete successfully.
        """
        logging.info('Updating stateful partition...')
        statefuldev_url = self.update_url.replace('update', 'static')

        # Attempt stateful partition update; this must succeed so that the newly
        # installed host is testable after update.
        statefuldev_cmd = [self._get_stateful_update_script(), statefuldev_url]
        if clobber:
            statefuldev_cmd.append('--stateful_change=clean')

        statefuldev_cmd.append('2>&1')
        try:
            self._run(' '.join(statefuldev_cmd), timeout=1200)
        except error.AutoservRunError:
            raise StatefulUpdateError(
                    'Failed to perform stateful update on %s' %
                    self.host.hostname)


    def verify_boot_expectations(self, expected_kernel, rollback_message):
        """Verifies that we fully booted given expected kernel state.

        This method both verifies that we booted using the correct kernel
        state and that the OS has marked the kernel as good.

        @param expected_kernel: kernel that we are verifying with,
            i.e. I expect to be booted onto partition 4 etc. See output of
            get_kernel_state.
        @param rollback_message: string include in except message text
            if we booted with the wrong partition.

        @raise NewBuildUpdateError if any of the various checks fail.
        """
        # Figure out the newly active kernel.
        active_kernel = self.get_kernel_state()[0]

        # Check for rollback due to a bad build.
        if active_kernel != expected_kernel:

            # Kernel crash reports should be wiped between test runs, but
            # may persist from earlier parts of the test, or from problems
            # with provisioning.
            #
            # Kernel crash reports will NOT be present if the crash happened
            # before encrypted stateful is mounted.
            #
            # TODO(dgarrett): Integrate with server/crashcollect.py at some
            # point.
            kernel_crashes = glob.glob('/var/spool/crash/kernel.*.kcrash')
            if kernel_crashes:
                rollback_message += ': kernel_crash'
                logging.debug('Found %d kernel crash reports:',
                              len(kernel_crashes))
                # The crash names contain timestamps that may be useful:
                #   kernel.20131207.005945.0.kcrash
                for crash in kernel_crashes:
                    logging.debug('  %s', os.path.basename(crash))

            # Print out some information to make it easier to debug
            # the rollback.
            logging.debug('Dumping partition table.')
            self._run('cgpt show $(rootdev -s -d)')
            logging.debug('Dumping crossystem for firmware debugging.')
            self._run('crossystem --all')
            raise NewBuildUpdateError(self.update_version, rollback_message)

        # Make sure chromeos-setgoodkernel runs.
        try:
            utils.poll_for_condition(
                lambda: (self._get_kernel_tries(active_kernel) == 0
                         and self._get_kernel_success(active_kernel)),
                exception=RootFSUpdateError(),
                timeout=_KERNEL_UPDATE_TIMEOUT, sleep_interval=5)
        except RootFSUpdateError:
            services_status = self._run('status system-services').stdout
            if services_status != 'system-services start/running\n':
                event = NewBuildUpdateError.CHROME_FAILURE
            else:
                event = NewBuildUpdateError.UPDATE_ENGINE_FAILURE
            raise NewBuildUpdateError(self.update_version, event)


    def _prepare_host(self):
        """Make sure the target DUT is working and ready for update.

        Initially, the target DUT's state is unknown.  The DUT is
        expected to be online, but we strive to be forgiving if Chrome
        and/or the update engine aren't fully functional.
        """
        # Summary of work, and the rationale:
        #  1. Reboot, because it's a good way to clear out problems.
        #  2. Touch the PROVISION_FAILED file, to allow repair to detect
        #     failure later.
        #  3. Run the hook for host class specific preparation.
        #  4. Stop Chrome, because the system is designed to eventually
        #     reboot if Chrome is stuck in a crash loop.
        #  5. Force `update-engine` to start, because if Chrome failed
        #     to start properly, the status of the `update-engine` job
        #     will be uncertain.
        if not self.host.is_up():
            raise HostUpdateError(self.host.hostname,
                                  HostUpdateError.DUT_DOWN)
        self._reset_stateful_partition()
        self.host.reboot(timeout=self.host.REBOOT_TIMEOUT)
        self._run('touch %s' % PROVISION_FAILED)
        self.host.prepare_for_update()
        self._reset_update_engine()
        logging.info('Updating from version %s to %s.',
                     self.host.get_release_version(),
                     self.update_version)


    def _install_via_update_engine(self):
        """Install an updating using the production AU flow.

        This uses the standard AU flow and the `stateful_update` script
        to download and install a root FS, kernel and stateful
        filesystem content.

        @return The kernel expected to be booted next.
        """
        logging.info('Installing image using update_engine.')
        expected_kernel = self.update_image()
        self.update_stateful()
        self._set_target_version()
        return expected_kernel


    def _quick_provision_with_gs_cache(self, provision_command, devserver_name,
                                       image_name):
        """Run quick_provision using GsCache server.

        @param provision_command: The path of quick_provision command.
        @param devserver_name: The devserver name and port (optional).
        @param image_name: The image to be installed.
        """
        logging.info('Try quick provision with gs_cache.')
        # If enabled, GsCache server listion on different port on the
        # devserver.
        gs_cache_server = devserver_name.replace(DEVSERVER_PORT, GS_CACHE_PORT)
        gs_cache_url = ('http://%s/download/chromeos-image-archive'
                        % gs_cache_server)

        # Check if GS_Cache server is enabled on the server.
        self._run('curl -s -o /dev/null %s' % gs_cache_url)

        command = '%s --noreboot %s %s' % (provision_command, image_name,
                                           gs_cache_url)
        self._run(command)
        metrics.Counter(_metric_name('quick_provision')).increment(
                fields={'devserver': devserver_name, 'gs_cache': True})


    def _quick_provision_with_devserver(self, provision_command,
                                        devserver_name, image_name):
        """Run quick_provision using legacy devserver.

        @param provision_command: The path of quick_provision command.
        @param devserver_name: The devserver name and port (optional).
        @param image_name: The image to be installed.
        """
        logging.info('Try quick provision with devserver.')
        ds = dev_server.ImageServer('http://%s' % devserver_name)
        try:
            ds.stage_artifacts(image_name, ['quick_provision', 'stateful'])
        except dev_server.DevServerException as e:
            raise error.TestFail, str(e), sys.exc_info()[2]

        static_url = 'http://%s/static' % devserver_name
        command = '%s --noreboot %s %s' % (provision_command, image_name,
                                           static_url)
        self._run(command)
        metrics.Counter(_metric_name('quick_provision')).increment(
                fields={'devserver': devserver_name, 'gs_cache': False})


    def _install_via_quick_provision(self):
        """Install an updating using the `quick-provision` script.

        This uses the `quick-provision` script to download and install
        a root FS, kernel and stateful filesystem content.

        @return The kernel expected to be booted next.
        """
        if not self._use_quick_provision:
            return None
        image_name = url_to_image_name(self.update_url)
        logging.info('Installing image using quick-provision.')
        provision_command = self._get_remote_script(_QUICK_PROVISION_SCRIPT)
        server_name = urlparse.urlparse(self.update_url)[1]
        try:
            try:
                self._quick_provision_with_gs_cache(provision_command,
                                                    server_name, image_name)
            except Exception:
                self._quick_provision_with_devserver(provision_command,
                                                     server_name, image_name)

            self._set_target_version()
            return self._verify_kernel_state()
        except Exception:
            # N.B.  We handle only `Exception` here.  Non-Exception
            # classes (such as KeyboardInterrupt) are handled by our
            # caller.
            logging.exception('quick-provision script failed; '
                              'will fall back to update_engine.')
            self._revert_boot_partition()
            self._reset_stateful_partition()
            self._reset_update_engine()
            return None


    def _install_update(self):
        """Install the requested image on the DUT, but don't start it.

        This downloads and installs a root FS, kernel and stateful
        filesystem content.  This does not reboot the DUT, so the update
        is merely pending when the method returns.

        @return The kernel expected to be booted next.
        """
        logging.info('Installing image at %s onto %s',
                     self.update_url, self.host.hostname)
        try:
            return (self._install_via_quick_provision()
                    or self._install_via_update_engine())
        except:
            # N.B. This handling code includes non-Exception classes such
            # as KeyboardInterrupt.  We need to clean up, but we also must
            # re-raise.
            self._revert_boot_partition()
            self._reset_stateful_partition()
            self._reset_update_engine()
            # Collect update engine logs in the event of failure.
            if self.host.job:
                logging.info('Collecting update engine logs due to failure...')
                self.host.get_file(
                        _UPDATER_LOGS, self.host.job.sysinfo.sysinfodir,
                        preserve_perm=False)
            _list_image_dir_contents(self.update_url)
            raise


    def _complete_update(self, expected_kernel):
        """Finish the update, and confirm that it succeeded.

        Initial condition is that the target build has been downloaded
        and installed on the DUT, but has not yet been booted.  This
        function is responsible for rebooting the DUT, and checking that
        the new build is running successfully.

        @param expected_kernel: kernel expected to be active after reboot.
        """
        # Regarding the 'crossystem' command below: In some cases,
        # the update flow puts the TPM into a state such that it
        # fails verification.  We don't know why.  However, this
        # call papers over the problem by clearing the TPM during
        # the reboot.
        #
        # We ignore failures from 'crossystem'.  Although failure
        # here is unexpected, and could signal a bug, the point of
        # the exercise is to paper over problems; allowing this to
        # fail would defeat the purpose.
        self._run('crossystem clear_tpm_owner_request=1',
                  ignore_status=True)
        self.host.reboot(timeout=self.host.REBOOT_TIMEOUT)

        # Touch the lab machine file to leave a marker that
        # distinguishes this image from other test images.
        # Afterwards, we must re-run the autoreboot script because
        # it depends on the _LAB_MACHINE_FILE.
        autoreboot_cmd = ('FILE="%s" ; [ -f "$FILE" ] || '
                          '( touch "$FILE" ; start autoreboot )')
        self._run(autoreboot_cmd % _LAB_MACHINE_FILE)
        self.verify_boot_expectations(
                expected_kernel, NewBuildUpdateError.ROLLBACK_FAILURE)

        logging.debug('Cleaning up old autotest directories.')
        try:
            installed_autodir = autotest.Autotest.get_installed_autodir(
                    self.host)
            self._run('rm -rf ' + installed_autodir)
        except autotest.AutodirNotFoundError:
            logging.debug('No autotest installed directory found.')


    def run_update(self):
        """Perform a full update of a DUT in the test lab.

        This downloads and installs the root FS and stateful partition
        content needed for the update specified in `self.host` and
        `self.update_url`.  The update is performed according to the
        requirements for provisioning a DUT for testing the requested
        build.

        At the end of the procedure, metrics are reported describing the
        outcome of the operation.

        @returns A tuple of the form `(image_name, attributes)`, where
            `image_name` is the name of the image installed, and
            `attributes` is new attributes to be applied to the DUT.
        """
        server_name = dev_server.get_resolved_hostname(self.update_url)
        metrics.Counter(_metric_name('install')).increment(
                fields={'devserver': server_name})

        try:
            self._prepare_host()
        except _AttributedUpdateError:
            raise
        except Exception as e:
            logging.exception('Failure preparing host prior to update.')
            raise HostUpdateError(self.host.hostname, str(e))

        try:
            expected_kernel = self._install_update()
        except _AttributedUpdateError:
            raise
        except Exception as e:
            logging.exception('Failure during download and install.')
            raise ImageInstallError(self.host.hostname, server_name, str(e))

        try:
            self._complete_update(expected_kernel)
        except _AttributedUpdateError:
            raise
        except Exception as e:
            logging.exception('Failure from build after update.')
            raise NewBuildUpdateError(self.update_version, str(e))

        image_name = url_to_image_name(self.update_url)
        # update_url is different from devserver url needed to stage autotest
        # packages, therefore, resolve a new devserver url here.
        devserver_url = dev_server.ImageServer.resolve(
                image_name, self.host.hostname).url()
        repo_url = tools.get_package_url(devserver_url, image_name)
        return image_name, {ds_constants.JOB_REPO_URL: repo_url}
