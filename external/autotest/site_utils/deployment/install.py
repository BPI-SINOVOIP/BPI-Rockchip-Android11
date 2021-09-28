#!/usr/bin/env python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install an initial test image on a set of DUTs.

The methods in this module are meant for two nominally distinct use
cases that share a great deal of code internally.  The first use
case is for deployment of DUTs that have just been placed in the lab
for the first time.  The second use case is for use after repairing
a servo.

Newly deployed DUTs may be in a somewhat anomalous state:
  * The DUTs are running a production base image, not a test image.
    By extension, the DUTs aren't reachable over SSH.
  * The DUTs are not necessarily in the AFE database.  DUTs that
    _are_ in the database should be locked.  Either way, the DUTs
    cannot be scheduled to run tests.
  * The servos for the DUTs need not be configured with the proper
    overlay.

More broadly, it's not expected that the DUT will be working at the
start of this operation.  If the DUT isn't working at the end of the
operation, an error will be reported.

The script performs the following functions:
  * Configure the servo for the target overlay, and test that the
    servo is generally in good order.
  * For the full deployment case, install dev-signed RO firmware
    from the designated stable test image for the DUTs.
  * For both cases, use servo to install the stable test image from
    USB.
  * If the DUT isn't in the AFE database, add it.

The script imposes these preconditions:
  * Every DUT has a properly connected servo.
  * Every DUT and servo have proper DHCP and DNS configurations.
  * Every servo host is up and running, and accessible via SSH.
  * There is a known, working test image that can be staged and
    installed on the target DUTs via servo.
  * Every DUT has the same board and model.
  * For the full deployment case, every DUT must be in dev mode,
    and configured to allow boot from USB with ctrl+U.

The implementation uses the `multiprocessing` module to run all
installations in parallel, separate processes.

"""

import atexit
from collections import namedtuple
import functools
import json
import logging
import multiprocessing
import os
import shutil
import sys
import tempfile
import time
import traceback

import common
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import host_states
from autotest_lib.client.common_lib import time_utils
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros import retry
from autotest_lib.server import afe_utils
from autotest_lib.server import constants
from autotest_lib.server import frontend
from autotest_lib.server import hosts
from autotest_lib.server.cros.dynamic_suite.constants import VERSION_PREFIX
from autotest_lib.server.hosts import afe_store
from autotest_lib.server.hosts import servo_host
from autotest_lib.site_utils.deployment import cmdvalidate
from autotest_lib.site_utils.deployment.prepare import dut as preparedut
from autotest_lib.utils import labellib


_LOG_FORMAT = '%(asctime)s | %(levelname)-10s | %(message)s'

_DEFAULT_POOL = constants.Labels.POOL_PREFIX + 'suites'

_LABSTATION_DEFAULT_POOL = constants.Labels.POOL_PREFIX + 'labstation_main'

_DIVIDER = '\n============\n'

_LOG_BUCKET_NAME = 'chromeos-install-logs'

_OMAHA_STATUS = 'gs://chromeos-build-release-console/omaha_status.json'

# Lock reasons we'll pass when locking DUTs, depending on the
# host's prior state.
_LOCK_REASON_EXISTING = 'Repairing or deploying an existing host'
_LOCK_REASON_NEW_HOST = 'Repairing or deploying a new host'

_ReportResult = namedtuple('_ReportResult', ['hostname', 'message'])


class InstallFailedError(Exception):
    """Generic error raised explicitly in this module."""


class _NoAFEServoPortError(InstallFailedError):
    """Exception when there is no servo port stored in the AFE."""


class _MultiFileWriter(object):

    """Group file objects for writing at once."""

    def __init__(self, files):
        """Initialize _MultiFileWriter.

        @param files  Iterable of file objects for writing.
        """
        self._files = files

    def write(self, s):
        """Write a string to the files.

        @param s  Write this string.
        """
        for file in self._files:
            file.write(s)


def _get_upload_log_path(arguments):
    return 'gs://{bucket}/{name}'.format(
        bucket=_LOG_BUCKET_NAME,
        name=arguments.upload_basename)


def _upload_logs(dirpath, gspath):
    """Upload report logs to Google Storage.

    @param dirpath  Path to directory containing the logs.
    @param gspath   Path to GS bucket.
    """
    utils.run(['gsutil', 'cp', '-r', '--', dirpath, gspath])


def _get_omaha_build(board):
    """Get the currently preferred Beta channel build for `board`.

    Open and read through the JSON file provided by GoldenEye that
    describes what version Omaha is currently serving for all boards
    on all channels.  Find the entry for `board` on the Beta channel,
    and return that version string.

    @param board  The board to look up from GoldenEye.

    @return Returns a Chrome OS version string in standard form
            R##-####.#.#.  Will return `None` if no Beta channel
            entry is found.
    """
    ret = utils.run(['gsutil', 'cat', '--', _OMAHA_STATUS])
    omaha_status = json.loads(ret.stdout)
    omaha_board = board.replace('_', '-')
    for e in omaha_status['omaha_data']:
        if (e['channel'] == 'beta' and
                e['board']['public_codename'] == omaha_board):
            milestone = e['chrome_version'].split('.')[0]
            build = e['chrome_os_version']
            return 'R%s-%s' % (milestone, build)
    return None


def _update_build(afe, report_log, arguments):
    raise RuntimeError("site_utils.deployment::_update_build is intentionally deleted")


def _create_host(hostname, afe, afe_host):
    """Create a CrosHost object for the DUT.

    This host object is used to update AFE label information for the DUT, but
    can not be used for installation image on the DUT. In particular, this host
    object does not have the servo attribute populated.

    @param hostname  Hostname of the target DUT.
    @param afe       A frontend.AFE object.
    @param afe_host  AFE Host object for the DUT.
    """
    machine_dict = {
            'hostname': hostname,
            'afe_host': afe_host,
            'host_info_store': afe_store.AfeStore(hostname, afe),
    }
    return hosts.create_host(machine_dict)


def _try_lock_host(afe_host):
    """Lock a host in the AFE, and report whether it succeeded.

    The lock action is logged regardless of success; failures are
    logged if they occur.

    @param afe_host AFE Host instance to be locked.

    @return `True` on success, or `False` on failure.
    """
    try:
        logging.warning('Locking host now.')
        afe_host.modify(locked=True,
                        lock_reason=_LOCK_REASON_EXISTING)
    except Exception as e:
        logging.exception('Failed to lock: %s', e)
        return False
    return True


def _try_unlock_host(afe_host):
    """Unlock a host in the AFE, and report whether it succeeded.

    The unlock action is logged regardless of success; failures are
    logged if they occur.

    @param afe_host AFE Host instance to be unlocked.

    @return `True` on success, or `False` on failure.
    """
    try:
        logging.warning('Unlocking host.')
        afe_host.modify(locked=False, lock_reason='')
    except Exception as e:
        logging.exception('Failed to unlock: %s', e)
        return False
    return True


def _update_host_attributes(afe, hostname, host_attrs):
    """Update the attributes for a given host.

    @param afe          AFE object for RPC calls.
    @param hostname     Host name of the DUT.
    @param host_attrs   Dictionary with attributes to be applied to the
                        host.
    """
    s_hostname, s_port, s_serial = _extract_servo_attributes(hostname,
                                                             host_attrs)
    afe.set_host_attribute(servo_host.SERVO_HOST_ATTR,
                           s_hostname,
                           hostname=hostname)
    afe.set_host_attribute(servo_host.SERVO_PORT_ATTR,
                           s_port,
                           hostname=hostname)
    if s_serial:
        afe.set_host_attribute(servo_host.SERVO_SERIAL_ATTR,
                               s_serial,
                               hostname=hostname)


def _extract_servo_attributes(hostname, host_attrs):
    """Extract servo attributes from the host attribute dict, setting defaults.

    @return (servo_hostname, servo_port, servo_serial)
    """
    # Grab the servo hostname/port/serial from `host_attrs` if supplied.
    # For new servo V4 deployments, we require the user to supply the
    # attributes (because there are no appropriate defaults).  So, if
    # none are supplied, we assume it can't be V4, and apply the
    # defaults for servo V3.
    s_hostname = (host_attrs.get(servo_host.SERVO_HOST_ATTR) or
                  servo_host.make_servo_hostname(hostname))
    s_port = (host_attrs.get(servo_host.SERVO_PORT_ATTR) or
              str(servo_host.ServoHost.DEFAULT_PORT))
    s_serial = host_attrs.get(servo_host.SERVO_SERIAL_ATTR)
    return s_hostname, s_port, s_serial


def _wait_for_idle(afe, host_id):
    """Helper function for `_ensure_host_idle`.

    Poll the host with the given `host_id` via `afe`, waiting for it
    to become idle.  Run forever; the caller takes care of timing out.

    @param afe        AFE object for RPC calls.
    @param host_id    Id of the host that's expected to become idle.
    """
    while True:
        afe_host = afe.get_hosts(id=host_id)[0]
        if afe_host.status in host_states.IDLE_STATES:
            return
        # Let's not spam our server.
        time.sleep(0.2)


def _ensure_host_idle(afe, afe_host):
    """Abort any special task running on `afe_host`.

    The given `afe_host` is currently locked.  If there's a special task
    running on the given `afe_host`, abort it, then wait for the host to
    show up as idle, return whether the operation succeeded.

    @param afe        AFE object for RPC calls.
    @param afe_host   Host to be aborted.

    @return A true value if the host is idle at return, or a false value
        if the host wasn't idle after some reasonable time.
    """
    # We need to talk to the shard, not the master, for at least two
    # reasons:
    #   * The `abort_special_tasks` RPC doesn't forward from the master
    #     to the shard, and only the shard has access to the special
    #     tasks.
    #   * Host status on the master can lag actual status on the shard
    #     by several minutes.  Only the shard can provide status
    #     guaranteed to post-date the call to lock the DUT.
    if afe_host.shard:
        afe = frontend.AFE(server=afe_host.shard)
    afe_host = afe.get_hosts(id=afe_host.id)[0]
    if afe_host.status in host_states.IDLE_STATES:
        return True
    afe.run('abort_special_tasks', host_id=afe_host.id, is_active=1)
    return not retry.timeout(_wait_for_idle, (afe, afe_host.id),
                             timeout_sec=5.0)[0]


def _get_afe_host(afe, hostname, host_attrs, arguments):
    """Get an AFE Host object for the given host.

    If the host is found in the database, return the object
    from the RPC call with the updated attributes in host_attr_dict.

    If no host is found, create one with appropriate servo
    attributes and the given board label.

    @param afe          AFE object for RPC calls.
    @param hostname     Host name of the DUT.
    @param host_attrs   Dictionary with attributes to be applied to the
                        host.
    @param arguments    Command line arguments with options.

    @return A tuple of the afe_host, plus a flag. The flag indicates
            whether the Host should be unlocked if subsequent operations
            fail.  (Hosts are always unlocked after success).
    """
    hostlist = afe.get_hosts([hostname])
    unlock_on_failure = False
    if hostlist:
        afe_host = hostlist[0]
        if not afe_host.locked:
            if _try_lock_host(afe_host):
                unlock_on_failure = True
            else:
                raise Exception('Failed to lock host')
        if not _ensure_host_idle(afe, afe_host):
            if unlock_on_failure and not _try_unlock_host(afe_host):
                raise Exception('Failed to abort host, and failed to unlock it')
            raise Exception('Failed to abort task on host')
        # This host was pre-existing; if the user didn't supply
        # attributes, don't update them, because the defaults may
        # not be correct.
        if host_attrs and not arguments.labstation:
            _update_host_attributes(afe, hostname, host_attrs)
    else:
        afe_host = afe.create_host(hostname,
                                   locked=True,
                                   lock_reason=_LOCK_REASON_NEW_HOST)

        if not arguments.labstation:
            _update_host_attributes(afe, hostname, host_attrs)

    # Correct board/model label is critical to installation. Always ensure user
    # supplied board/model matches the AFE information.
    _ensure_label_in_afe(afe_host, 'board', arguments.board)
    _ensure_label_in_afe(afe_host, 'model', arguments.model)

    afe_host = afe.get_hosts([hostname])[0]
    return afe_host, unlock_on_failure


def _ensure_label_in_afe(afe_host, label_name, label_value):
    """Add the given board label, only if one doesn't already exist.

    @params label_name  name of the label, e.g. 'board', 'model', etc.
    @params label_value value of the label.

    @raises InstallFailedError if supplied board  is different from existing
            board in AFE.
    """
    if not label_value:
        return

    labels = labellib.LabelsMapping(afe_host.labels)
    if label_name not in labels:
        afe_host.add_labels(['%s:%s' % (label_name, label_value)])
        return

    existing_value = labels[label_name]
    if label_value != existing_value:
        raise InstallFailedError(
                'provided %s %s does not match the %s %s for host %s' %
                (label_name, label_value, label_name, existing_value,
                 afe_host.hostname))


def _create_host_for_installation(host, arguments):
    """Creates a context manager of hosts.CrosHost object for installation.

    The host object yielded by the returned context manager is agnostic of the
    infrastructure environment. In particular, it does not have any references
    to the AFE.

    @param host: A server.hosts.CrosHost object.
    @param arguments: Parsed commandline arguments for this script.

    @return a context manager which yields hosts.CrosHost object.
    """
    info = host.host_info_store.get()
    s_host, s_port, s_serial = _extract_servo_attributes(host.hostname,
                                                         info.attributes)
    return preparedut.create_cros_host(host.hostname, arguments.board,
                                       arguments.model, s_host, s_port,
                                       s_serial, arguments.logdir)


def _install_test_image(host, arguments):
    """Install a test image to the DUT.

    Install a stable test image on the DUT using the full servo
    repair flow.

    @param host       Host instance for the DUT being installed.
    @param arguments  Command line arguments with options.
    """
    repair_image = _get_cros_repair_image_name(host)
    logging.info('Using repair image %s', repair_image)
    if arguments.dry_run:
        return
    if arguments.stageusb:
        try:
            preparedut.download_image_to_servo_usb(host, repair_image)
        except Exception as e:
            logging.exception('Failed to stage image on USB: %s', e)
            raise Exception('USB staging failed')
    if arguments.install_test_image:
        try:
            preparedut.install_test_image(host)
        except error.AutoservRunError as e:
            logging.exception('Failed to install: %s', e)
            raise Exception('chromeos-install failed')
    if arguments.install_firmware:
        try:
            if arguments.using_servo:
                logging.debug('Install FW using servo.')
                preparedut.flash_firmware_using_servo(host, repair_image)
            else:
                logging.debug('Install FW by chromeos-firmwareupdate.')
                preparedut.install_firmware(host)
        except error.AutoservRunError as e:
            logging.exception('Firmware update failed: %s', e)
            msg = '%s failed' % (
                    'Flashing firmware using servo' if arguments.using_servo
                    else 'chromeos-firmwareupdate')
            raise Exception(msg)
    if arguments.reinstall_test_image:
        try:
            preparedut.reinstall_test_image(host)
        except error.AutoservRunError as e:
            logging.exception('Failed to install: %s', e)
            raise Exception('chromeos-install failed')
    if arguments.install_test_image and arguments.install_firmware:
        # we need to verify that DUT can successfully boot in to recovery mode
        # if it's initial deploy.
        try:
            preparedut.verify_boot_into_rec_mode(host)
        except error.AutoservRunError as e:
            logging.exception('Failed to validate DUT can boot from '
                              'recovery mode: %s', e)
            raise Exception('recovery mode validation failed')


def _install_and_update_afe(afe, hostname, host_attrs, arguments):
    """Perform all installation and AFE updates.

    First, lock the host if it exists and is unlocked.  Then,
    install the test image on the DUT.  At the end, unlock the
    DUT, unless the installation failed and the DUT was locked
    before we started.

    If installation succeeds, make sure the DUT is in the AFE,
    and make sure that it has basic labels.

    @param afe          AFE object for RPC calls.
    @param hostname     Host name of the DUT.
    @param host_attrs   Dictionary with attributes to be applied to the
                        host.
    @param arguments    Command line arguments with options.
    """
    afe_host, unlock_on_failure = _get_afe_host(afe, hostname, host_attrs,
                                                arguments)
    host = None
    try:
        host = _create_host(hostname, afe, afe_host)
        if arguments.labstation:
            _setup_labstation(host)
        else:
            with _create_host_for_installation(host, arguments) as target_host:
                _install_test_image(target_host, arguments)
                _update_servo_type_attribute(target_host, host)

        if ((arguments.install_test_image or arguments.reinstall_test_image)
            and not arguments.dry_run):
            host.labels.update_labels(host)
            platform_labels = afe.get_labels(
                    host__hostname=hostname, platform=True)
            if not platform_labels:
                platform = host.get_platform()
                new_labels = afe.get_labels(name=platform)
                if not new_labels:
                    afe.create_label(platform, platform=True)
                afe_host.add_labels([platform])
        version = [label for label in afe_host.labels
                       if label.startswith(VERSION_PREFIX)]
        if version and not arguments.dry_run:
            afe_host.remove_labels(version)
    except Exception as e:
        if unlock_on_failure and not _try_unlock_host(afe_host):
            logging.error('Failed to unlock host!')
        raise
    finally:
        if host is not None:
            host.close()

    if not _try_unlock_host(afe_host):
        raise Exception('Install succeeded, but failed to unlock the DUT.')


def _install_dut(arguments, host_attr_dict, hostname):
    """Deploy or repair a single DUT.

    @param arguments       Command line arguments with options.
    @param host_attr_dict  Dict mapping hostnames to attributes to be
                           stored in the AFE.
    @param hostname        Host name of the DUT to install on.

    @return On success, return `None`.  On failure, return a string
            with an error message.
    """
    # In some cases, autotest code that we call during install may
    # put stuff onto stdout with 'print' statements.  Most notably,
    # the AFE frontend may print 'FAILED RPC CALL' (boo, hiss).  We
    # want nothing from this subprocess going to the output we
    # inherited from our parent, so redirect stdout and stderr, before
    # we make any AFE calls.  Note that this is reasonable because we're
    # in a subprocess.

    logpath = os.path.join(arguments.logdir, hostname + '.log')
    logfile = open(logpath, 'w')
    sys.stderr = sys.stdout = logfile
    _configure_logging_to_file(logfile)

    afe = frontend.AFE(server=arguments.web)
    try:
        _install_and_update_afe(afe, hostname,
                                host_attr_dict.get(hostname, {}),
                                arguments)
    except Exception as e:
        logging.exception('Original exception: %s', e)
        return str(e)
    return None


def _report_hosts(report_log, heading, host_results_list):
    """Report results for a list of hosts.

    To improve visibility, results are preceded by a header line,
    followed by a divider line.  Then results are printed, one host
    per line.

    @param report_log         File-like object for logging report
                              output.
    @param heading            The header string to be printed before
                              results.
    @param host_results_list  A list of _ReportResult tuples
                              to be printed one per line.
    """
    if not host_results_list:
        return
    report_log.write(heading)
    report_log.write(_DIVIDER)
    for result in host_results_list:
        report_log.write('{result.hostname:30} {result.message}\n'
                         .format(result=result))
    report_log.write('\n')


def _report_results(afe, report_log, hostnames, results, arguments):
    """Gather and report a summary of results from installation.

    Segregate results into successes and failures, reporting
    each separately.  At the end, report the total of successes
    and failures.

    @param afe          AFE object for RPC calls.
    @param report_log   File-like object for logging report output.
    @param hostnames    List of the hostnames that were tested.
    @param results      List of error messages, in the same order
                        as the hostnames.  `None` means the
                        corresponding host succeeded.
    @param arguments  Command line arguments with options.
    """
    successful_hosts = []
    success_reports = []
    failure_reports = []
    for result, hostname in zip(results, hostnames):
        if result is None:
            successful_hosts.append(hostname)
        else:
            failure_reports.append(_ReportResult(hostname, result))
    if successful_hosts:
        afe.repair_hosts(hostnames=successful_hosts)
        for h in afe.get_hosts(hostnames=successful_hosts):
            for label in h.labels:
                if label.startswith(constants.Labels.POOL_PREFIX):
                    result = _ReportResult(h.hostname,
                                           'Host already in %s' % label)
                    success_reports.append(result)
                    break
            else:
                if arguments.labstation:
                    target_pool = _LABSTATION_DEFAULT_POOL
                else:
                    target_pool = _DEFAULT_POOL
                h.add_labels([target_pool])
                result = _ReportResult(h.hostname,
                                       'Host added to %s' % target_pool)
                success_reports.append(result)
    report_log.write(_DIVIDER)
    _report_hosts(report_log, 'Successes', success_reports)
    _report_hosts(report_log, 'Failures', failure_reports)
    report_log.write(
        'Installation complete:  %d successes, %d failures.\n' %
        (len(success_reports), len(failure_reports)))


def _clear_root_logger_handlers():
    """Remove all handlers from root logger."""
    root_logger = logging.getLogger()
    for h in root_logger.handlers:
        root_logger.removeHandler(h)


def _configure_logging_to_file(logfile):
    """Configure the logging module for `install_duts()`.

    @param log_file  Log file object.
    """
    _clear_root_logger_handlers()
    handler = logging.StreamHandler(logfile)
    formatter = logging.Formatter(_LOG_FORMAT, time_utils.TIME_FMT)
    handler.setFormatter(formatter)
    root_logger = logging.getLogger()
    root_logger.addHandler(handler)


def _get_used_servo_ports(servo_hostname, afe):
    """
    Return a list of used servo ports for the given servo host.

    @param servo_hostname:  Hostname of the servo host to check for.
    @param afe:             AFE instance.

    @returns a list of used ports for the given servo host.
    """
    used_ports = []
    host_list = afe.get_hosts_by_attribute(
            attribute=servo_host.SERVO_HOST_ATTR, value=servo_hostname)
    for host in host_list:
        afe_host = afe.get_hosts(hostname=host)
        if afe_host:
            servo_port = afe_host[0].attributes.get(servo_host.SERVO_PORT_ATTR)
            if servo_port:
                used_ports.append(int(servo_port))
    return used_ports


def _get_free_servo_port(servo_hostname, used_servo_ports, afe):
    """
    Get a free servo port for the servo_host.

    @param servo_hostname:    Hostname of the servo host.
    @param used_servo_ports:  Dict of dicts that contain the list of used ports
                              for the given servo host.
    @param afe:               AFE instance.

    @returns a free servo port if servo_hostname is non-empty, otherwise an
        empty string.
    """
    used_ports = []
    servo_port = servo_host.ServoHost.DEFAULT_PORT
    # If no servo hostname was specified we can assume we're dealing with a
    # servo v3 or older deployment since the servo hostname can be
    # inferred from the dut hostname (by appending '-servo' to it).  We only
    # need to find a free port if we're using a servo v4 since we can use the
    # default port for v3 and older.
    if not servo_hostname:
        return ''
    # If we haven't checked this servo host yet, check the AFE if other duts
    # used this servo host and grab the ports specified for them.
    elif servo_hostname not in used_servo_ports:
        used_ports = _get_used_servo_ports(servo_hostname, afe)
    else:
        used_ports = used_servo_ports[servo_hostname]
    used_ports.sort()
    if used_ports:
        # Range is taken from servod.py in hdctools.
        start_port = servo_host.ServoHost.DEFAULT_PORT
        end_port = start_port - 99
        # We'll choose first port available in descending order.
        for port in xrange(start_port, end_port - 1, -1):
            if port not in used_ports:
              servo_port = port
              break
    used_ports.append(servo_port)
    used_servo_ports[servo_hostname] = used_ports
    return servo_port


def _get_afe_servo_port(host_info, afe):
    """
    Get the servo port from the afe if it matches the same servo host hostname.

    @param host_info   HostInfo tuple (hostname, host_attr_dict).

    @returns Servo port (int) if servo host hostname matches the one specified
    host_info.host_attr_dict, otherwise None.

    @raises _NoAFEServoPortError: When there is no stored host info or servo
        port host attribute in the AFE for the given host.
    """
    afe_hosts = afe.get_hosts(hostname=host_info.hostname)
    if not afe_hosts:
        raise _NoAFEServoPortError

    servo_port = afe_hosts[0].attributes.get(servo_host.SERVO_PORT_ATTR)
    afe_servo_host = afe_hosts[0].attributes.get(servo_host.SERVO_HOST_ATTR)
    host_info_servo_host = host_info.host_attr_dict.get(
        servo_host.SERVO_HOST_ATTR)

    if afe_servo_host == host_info_servo_host and servo_port:
        return int(servo_port)
    else:
        raise _NoAFEServoPortError


def _get_host_attributes(host_info_list, afe):
    """
    Get host attributes if a hostname_file was supplied.

    @param host_info_list   List of HostInfo tuples (hostname, host_attr_dict).

    @returns Dict of attributes from host_info_list.
    """
    host_attributes = {}
    # We need to choose servo ports for these hosts but we need to make sure
    # we don't choose ports already used. We'll store all used ports in a
    # dict of lists where the key is the servo_host and the val is a list of
    # ports used.
    used_servo_ports = {}
    for host_info in host_info_list:
        host_attr_dict = host_info.host_attr_dict
        # If the host already has an entry in the AFE that matches the same
        # servo host hostname and the servo port is set, use that port.
        try:
            host_attr_dict[servo_host.SERVO_PORT_ATTR] = _get_afe_servo_port(
                host_info, afe)
        except _NoAFEServoPortError:
            host_attr_dict[servo_host.SERVO_PORT_ATTR] = _get_free_servo_port(
                host_attr_dict[servo_host.SERVO_HOST_ATTR], used_servo_ports,
                afe)
        host_attributes[host_info.hostname] = host_attr_dict
    return host_attributes


def _get_cros_repair_image_name(host):
    """Get the CrOS repair image name for given host.

    @param host: hosts.CrosHost object. This object need not have an AFE
                 reference.
    """
    info = host.host_info_store.get()
    if not info.board:
        raise InstallFailedError('Unknown board for given host')
    return afe_utils.get_stable_cros_image_name_v2(info)


def install_duts(arguments):
    """Install a test image on DUTs, and deploy them.

    This handles command line parsing for both the repair and
    deployment commands.  The two operations are largely identical;
    the main difference is that full deployment includes flashing
    dev-signed firmware on the DUT prior to installing the test
    image.

    @param arguments    Command line arguments with options, as
                        returned by `argparse.Argparser`.
    """
    arguments = cmdvalidate.validate_arguments(arguments)
    if arguments is None:
        sys.exit(1)
    sys.stderr.write('Installation output logs in %s\n' % arguments.logdir)

    # Override tempfile.tempdir.  Some of the autotest code we call
    # will create temporary files that don't get cleaned up.  So, we
    # put the temp files in our results directory, so that we can
    # clean up everything at one fell swoop.
    tempfile.tempdir = tempfile.mkdtemp()
    atexit.register(shutil.rmtree, tempfile.tempdir)

    # We don't want to distract the user with logging output, so we catch
    # logging output in a file.
    logging_file_path = os.path.join(arguments.logdir, 'debug.log')
    logfile = open(logging_file_path, 'w')
    _configure_logging_to_file(logfile)

    report_log_path = os.path.join(arguments.logdir, 'report.log')
    with open(report_log_path, 'w') as report_log_file:
        report_log = _MultiFileWriter([report_log_file, sys.stdout])
        afe = frontend.AFE(server=arguments.web)
        if arguments.dry_run:
            report_log.write('Dry run - installation and most testing '
                             'will be skipped.\n')
        host_attr_dict = _get_host_attributes(arguments.host_info_list, afe)
        install_pool = multiprocessing.Pool(len(arguments.hostnames))
        install_function = functools.partial(_install_dut, arguments,
                                             host_attr_dict)
        results_list = install_pool.map(install_function, arguments.hostnames)
        _report_results(afe, report_log, arguments.hostnames, results_list,
                        arguments)

    if arguments.upload:
        try:
            gspath = _get_upload_log_path(arguments)
            sys.stderr.write('Logs will be uploaded to %s\n' % (gspath,))
            _upload_logs(arguments.logdir, gspath)
        except Exception:
            upload_failure_log_path = os.path.join(arguments.logdir,
                                                   'gs_upload_failure.log')
            with open(upload_failure_log_path, 'w') as file_:
                traceback.print_exc(limit=None, file=file_)
            sys.stderr.write('Failed to upload logs;'
                             ' failure details are stored in {}.\n'
                             .format(upload_failure_log_path))


def _update_servo_type_attribute(host, host_to_update):
    """Update servo_type attribute for the DUT.

    @param host              A CrOSHost with a initialized servo property.
    @param host_to_update    A CrOSHost with AfeStore as its host_info_store.

    """
    info = host_to_update.host_info_store.get()
    if 'servo_type' not in info.attributes:
        logging.info("Collecting and adding servo_type attribute.")
        info.attributes['servo_type'] = host.servo.get_servo_version()
        host_to_update.host_info_store.commit(info)


def _setup_labstation(host):
    """Do initial setup for labstation host.

    @param host    A LabstationHost object.

    """
    try:
        if not host.is_labstation():
            raise InstallFailedError('Current OS on host %s is not a labstation'
                                     ' image.', host.hostname)
    except AttributeError:
        raise InstallFailedError('Unable to verify host has a labstation image,'
                                 ' this can be caused by host is unsshable.')

    try:
        # TODO: we should setup hwid and serial number for DUT in deploy script
        #  as well, which is currently obtained from repair job.
        info = host.host_info_store.get()
        hwid = host.run('crossystem hwid', ignore_status=True).stdout
        if hwid:
            info.attributes['HWID'] = hwid

        serial_number = host.run('vpd -g serial_number',
                                 ignore_status=True).stdout
        if serial_number:
            info.attributes['serial_number'] = serial_number
        if info != host.host_info_store.get():
            host.host_info_store.commit(info)
    except Exception as e:
        raise InstallFailedError('Failed to get HWID & Serial Number for host'
                                 ' %s: %s' % (host.hostname, str(e)))

    host.labels.update_labels(host)
