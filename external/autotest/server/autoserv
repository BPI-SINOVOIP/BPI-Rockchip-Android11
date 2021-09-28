#!/usr/bin/python2 -u
# Copyright 2007-2008 Martin J. Bligh <mbligh@google.com>, Google Inc.
# Released under the GPL v2

"""
Run a control file through the server side engine
"""

import datetime
import contextlib
import getpass
import logging
import os
import re
import shutil
import signal
import socket
import sys
import traceback
import time
import urllib2

import common
from autotest_lib.client.bin.result_tools import utils as result_utils
from autotest_lib.client.bin.result_tools import view as result_view
from autotest_lib.client.common_lib import control_data
from autotest_lib.client.common_lib import enum
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.common_lib import host_queue_entry_states
from autotest_lib.client.common_lib import host_states
from autotest_lib.server import results_mocker
from autotest_lib.server.cros.dynamic_suite import suite

try:
    from chromite.lib import metrics
    from chromite.lib import cloud_trace
except ImportError:
    from autotest_lib.client.common_lib import utils as common_utils
    metrics = common_utils.metrics_mock
    import mock
    cloud_trace = mock.MagicMock()

_CONFIG = global_config.global_config

# Number of seconds to wait before returning if testing mode is enabled
TESTING_MODE_SLEEP_SECS = 1


from autotest_lib.server import frontend
from autotest_lib.server import server_logging_config
from autotest_lib.server import server_job, utils, autoserv_parser, autotest
from autotest_lib.server import utils as server_utils
from autotest_lib.server import site_utils
from autotest_lib.server.cros.dynamic_suite import frontend_wrappers
from autotest_lib.site_utils import job_directories
from autotest_lib.site_utils import lxc
from autotest_lib.site_utils.lxc import utils as lxc_utils
from autotest_lib.client.common_lib import pidfile, logging_manager


# Control segment to stage server-side package.
STAGE_SERVER_SIDE_PACKAGE_CONTROL_FILE = server_job._control_segment_path(
        'stage_server_side_package')

# Command line to start servod in a moblab.
START_SERVOD_CMD = 'sudo start servod BOARD=%s PORT=%s'
STOP_SERVOD_CMD = 'sudo stop servod'

_AUTOTEST_ROOT = os.path.realpath(os.path.join(os.path.dirname(__file__), '..'))
_CONTROL_FILE_FROM_CONTROL_NAME = 'control.from_control_name'

_LXC_JOB_FOLDER = 'lxc_job_folder'

def log_alarm(signum, frame):
    logging.error("Received SIGALARM. Ignoring and continuing on.")
    sys.exit(1)


def _get_machines(parser):
    """Get a list of machine names from command line arg -m or a file.

    @param parser: Parser for the command line arguments.

    @return: A list of machine names from command line arg -m or the
             machines file specified in the command line arg -M.
    """
    if parser.options.machines:
        machines = parser.options.machines.replace(',', ' ').strip().split()
    else:
        machines = []
    machines_file = parser.options.machines_file
    if machines_file:
        machines = []
        for m in open(machines_file, 'r').readlines():
            # remove comments, spaces
            m = re.sub('#.*', '', m).strip()
            if m:
                machines.append(m)
        logging.debug('Read list of machines from file: %s', machines_file)
        logging.debug('Machines: %s', ','.join(machines))

    if machines:
        for machine in machines:
            if not machine or re.search('\s', machine):
                parser.parser.error("Invalid machine: %s" % str(machine))
        machines = list(set(machines))
        machines.sort()
    return machines


def _stage_ssp(parser, resultsdir):
    """Stage server-side package.

    This function calls a control segment to stage server-side package based on
    the job and autoserv command line option. The detail implementation could
    be different for each host type. Currently, only CrosHost has
    stage_server_side_package function defined.
    The script returns None if no server-side package is available. However,
    it may raise exception if it failed for reasons other than artifact (the
    server-side package) not found.

    @param parser: Command line arguments parser passed in the autoserv process.
    @param resultsdir: Folder to store results. This could be different from
            parser.options.results: parser.options.results  can be set to None
            for results to be stored in a temp folder. resultsdir can be None
            for autoserv run requires no logging.

    @return: url to the autotest server-side package. None in case of errors.
    """
    machines_list = _get_machines(parser)
    machines_list = server_job.get_machine_dicts(
            machine_names=machines_list,
            store_dir=os.path.join(resultsdir, parser.options.host_info_subdir),
            in_lab=parser.options.lab,
            use_shadow_store=not parser.options.local_only_host_info,
            host_attributes=parser.options.host_attributes,
    )

    namespace = {'machines': machines_list,
                 'isolate_hash': parser.options.isolate,
                 'image': parser.options.test_source_build}
    script_locals = {}
    execfile(STAGE_SERVER_SIDE_PACKAGE_CONTROL_FILE, namespace, script_locals)
    ssp_url = script_locals['ssp_url']
    if not ssp_url:
        logging.error('Failed to stage SSP package: %s',
                      script_locals['error_msg'])
        logging.error('This job will fail later, when attempting to run with'
                      ' SSP')
    return ssp_url


def _run_with_ssp(job, container_id, job_id, results, parser, ssp_url,
                  machines):
    """Run the server job with server-side packaging.

    @param job: The server job object.
    @param container_id: ID of the container to run the test.
    @param job_id: ID of the test job.
    @param results: Folder to store results. This could be different from
                    parser.options.results:
                    parser.options.results  can be set to None for results to be
                    stored in a temp folder.
                    results can be None for autoserv run requires no logging.
    @param parser: Command line parser that contains the options.
    @param ssp_url: url of the staged server-side package.
    @param machines: A list of machines to run the test.
    """
    if not ssp_url:
        job.record('FAIL', None, None,
                   'Failed to stage server-side package')
        raise error.AutoservError('Failed to stage server-side package')

    bucket = lxc.ContainerBucket()
    control = (parser.args[0] if len(parser.args) > 0 and parser.args[0] != ''
               else None)
    try:
        dut_name = machines[0] if len(machines) >= 1 else None
        test_container = bucket.setup_test(container_id, job_id, ssp_url,
                                           results, control=control,
                                           job_folder=_LXC_JOB_FOLDER,
                                           dut_name=dut_name,
                                           isolate_hash=parser.options.isolate)
    except Exception as e:
        job.record('FAIL', None, None,
                   'Failed to setup container for test: %s. Check logs in '
                   'ssp_logs folder for more details.' % e)
        raise

    args = sys.argv[:]
    args.remove('--require-ssp')
    # --parent_job_id is only useful in autoserv running in host, not in
    # container. Include this argument will cause test to fail for builds before
    # CL 286265 was merged.
    if '--parent_job_id' in args:
        index = args.index('--parent_job_id')
        args.remove('--parent_job_id')
        # Remove the actual parent job id in command line arg.
        del args[index]

    # A dictionary of paths to replace in the command line. Key is the path to
    # be replaced with the one in value.
    paths_to_replace = {}
    # Replace the control file path with the one in container.
    if control:
        container_control_filename = os.path.join(
                lxc.CONTROL_TEMP_PATH, os.path.basename(control))
        paths_to_replace[control] = container_control_filename
    # Update result directory with the one in container.
    container_result_dir = os.path.join(lxc.RESULT_DIR_FMT % _LXC_JOB_FOLDER)
    if parser.options.results:
        paths_to_replace[parser.options.results] = container_result_dir
    args = [paths_to_replace.get(arg, arg) for arg in args]

    # Apply --use-existing-results, results directory is aready created and
    # mounted in container. Apply this arg to avoid exception being raised.
    if not '--use-existing-results' in args:
        args.append('--use-existing-results')

    # Make sure autoserv running in container using a different pid file.
    if not '--pidfile-label' in args:
        args.extend(['--pidfile-label', 'container_autoserv'])

    cmd_line = ' '.join(["'%s'" % arg if ' ' in arg else arg for arg in args])
    logging.info('Run command in container: %s', cmd_line)
    success = False
    try:
        test_container.attach_run(cmd_line)
        success = True
    except Exception as e:
        # If the test run inside container fails without generating any log,
        # write a message to status.log to help troubleshooting.
        debug_files = os.listdir(os.path.join(results, 'debug'))
        if not debug_files:
            job.record('FAIL', None, None,
                       'Failed to run test inside the container: %s. Check '
                       'logs in ssp_logs folder for more details.' % e)
        raise
    finally:
        metrics.Counter(
            'chromeos/autotest/experimental/execute_job_in_ssp').increment(
                fields={'success': success})
        test_container.destroy()


def correct_results_folder_permission(results):
    """Make sure the results folder has the right permission settings.

    For tests running with server-side packaging, the results folder has the
    owner of root. This must be changed to the user running the autoserv
    process, so parsing job can access the results folder.
    TODO(dshi): crbug.com/459344 Remove this function when test container can be
    unprivileged container.

    @param results: Path to the results folder.

    """
    if not results:
        return

    utils.run('sudo -n chown -R %s "%s"' % (os.getuid(), results))
    utils.run('sudo -n chgrp -R %s "%s"' % (os.getgid(), results))


def _start_servod(machine):
    """Try to start servod in moblab if it's not already running or running with
    different board or port.

    @param machine: Name of the dut used for test.
    """
    if not utils.is_moblab():
        return

    logging.debug('Trying to start servod.')
    try:
        afe = frontend.AFE()
        board = server_utils.get_board_from_afe(machine, afe)
        hosts = afe.get_hosts(hostname=machine)
        servo_host = hosts[0].attributes.get('servo_host', None)
        servo_port = hosts[0].attributes.get('servo_port', 9999)
        if not servo_host in ['localhost', '127.0.0.1']:
            logging.warn('Starting servod is aborted. The dut\'s servo_host '
                         'attribute is not set to localhost.')
            return
    except (urllib2.HTTPError, urllib2.URLError):
        # Ignore error if RPC failed to get board
        logging.error('Failed to get board name from AFE. Start servod is '
                      'aborted')
        return

    try:
        pid = utils.run('pgrep servod').stdout
        cmd_line = utils.run('ps -fp %s' % pid).stdout
        if ('--board %s' % board in cmd_line and
            '--port %s' % servo_port in cmd_line):
            logging.debug('Servod is already running with given board and port.'
                          ' There is no need to restart servod.')
            return
        logging.debug('Servod is running with different board or port. '
                      'Stopping existing servod.')
        utils.run('sudo stop servod')
    except error.CmdError:
        # servod is not running.
        pass

    try:
        utils.run(START_SERVOD_CMD % (board, servo_port))
        logging.debug('Servod is started')
    except error.CmdError as e:
        logging.error('Servod failed to be started, error: %s', e)


def _control_path_on_disk(control_name):
    """Find the control file corresponding to the given control name, on disk.

    @param control_name: NAME attribute of the control file to fetch.
    @return: Path to the control file.
    """
    cf_getter = suite.create_fs_getter(_AUTOTEST_ROOT)
    control_name_predicate = suite.test_name_matches_pattern_predicate(
            '^%s$' % control_name)
    tests = suite.find_and_parse_tests(cf_getter, control_name_predicate)
    if not tests:
        raise error.AutoservError(
                'Failed to find any control files with NAME %s' % control_name)
    if len(tests) > 1:
        logging.error('Found more than one control file with NAME %s: %s',
                      control_name, [t.path for t in tests])
        raise error.AutoservError(
                'Found more than one control file with NAME %s' % control_name)
    return tests[0].path


def _stage_control_file(control_name, results_dir):
    """Stage the control file to execute from local autotest checkout.

    @param control_name: Name of the control file to stage.
    @param results_dir: Results directory to stage the control file into.
    @return: Absolute path to the staged control file.
    """
    control_path = _control_path_on_disk(control_name)
    new_control = os.path.join(results_dir, _CONTROL_FILE_FROM_CONTROL_NAME)
    shutil.copy2(control_path, new_control)
    return new_control


def run_autoserv(pid_file_manager, results, parser, ssp_url, use_ssp):
    """Run server job with given options.

    @param pid_file_manager: PidFileManager used to monitor the autoserv process
    @param results: Folder to store results.
    @param parser: Parser for the command line arguments.
    @param ssp_url: Url to server-side package.
    @param use_ssp: Set to True to run with server-side packaging.
    """
    # send stdin to /dev/null
    dev_null = os.open(os.devnull, os.O_RDONLY)
    os.dup2(dev_null, sys.stdin.fileno())
    os.close(dev_null)

    # Create separate process group if the process is not a process group
    # leader. This allows autoserv process to keep running after the caller
    # process (drone manager call) exits.
    if os.getpid() != os.getpgid(0):
        os.setsid()

    # Container name is predefined so the container can be destroyed in
    # handle_sigterm.
    job_or_task_id = job_directories.get_job_id_or_task_id(
            parser.options.results)
    container_id = lxc.ContainerId(job_or_task_id, time.time(), os.getpid())

    # Implement SIGTERM handler
    def handle_sigterm(signum, frame):
        logging.debug('Received SIGTERM')
        if pid_file_manager:
            pid_file_manager.close_file(1, signal.SIGTERM)
        logging.debug('Finished writing to pid_file. Killing process.')

        # Update results folder's file permission. This needs to be done ASAP
        # before the parsing process tries to access the log.
        if use_ssp and results:
            correct_results_folder_permission(results)

        # This sleep allows the pending output to be logged before the kill
        # signal is sent.
        time.sleep(.1)
        if use_ssp:
            logging.debug('Destroy container %s before aborting the autoserv '
                          'process.', container_id)
            try:
                bucket = lxc.ContainerBucket()
                container = bucket.get_container(container_id)
                if container:
                    container.destroy()
                    logging.debug("Container %s destroyed.", container_id)
                else:
                    logging.debug('Container %s is not found.', container_id)
                    bucket.scrub_container_location(container_id)
            except:
                # Handle any exception so the autoserv process can be aborted.
                logging.exception('Failed to destroy container %s.',
                                  container_id)
            # Try to correct the result file permission again after the
            # container is destroyed, as the container might have created some
            # new files in the result folder.
            if results:
                correct_results_folder_permission(results)

        os.killpg(os.getpgrp(), signal.SIGKILL)

    # Set signal handler
    signal.signal(signal.SIGTERM, handle_sigterm)

    # faulthandler is only needed to debug in the Lab and is not avaliable to
    # be imported in the chroot as part of VMTest, so Try-Except it.
    try:
        import faulthandler
        faulthandler.register(signal.SIGTERM, all_threads=True, chain=True)
        logging.debug('faulthandler registered on SIGTERM.')
    except ImportError:
        sys.exc_clear()

    # Ignore SIGTTOU's generated by output from forked children.
    signal.signal(signal.SIGTTOU, signal.SIG_IGN)

    # If we received a SIGALARM, let's be loud about it.
    signal.signal(signal.SIGALRM, log_alarm)

    # Server side tests that call shell scripts often depend on $USER being set
    # but depending on how you launch your autotest scheduler it may not be set.
    os.environ['USER'] = getpass.getuser()

    label = parser.options.label
    group_name = parser.options.group_name
    user = parser.options.user
    client = parser.options.client
    server = parser.options.server
    verify = parser.options.verify
    repair = parser.options.repair
    cleanup = parser.options.cleanup
    provision = parser.options.provision
    reset = parser.options.reset
    job_labels = parser.options.job_labels
    no_tee = parser.options.no_tee
    execution_tag = parser.options.execution_tag
    ssh_user = parser.options.ssh_user
    ssh_port = parser.options.ssh_port
    ssh_pass = parser.options.ssh_pass
    collect_crashinfo = parser.options.collect_crashinfo
    control_filename = parser.options.control_filename
    verify_job_repo_url = parser.options.verify_job_repo_url
    skip_crash_collection = parser.options.skip_crash_collection
    ssh_verbosity = int(parser.options.ssh_verbosity)
    ssh_options = parser.options.ssh_options
    no_use_packaging = parser.options.no_use_packaging
    in_lab = bool(parser.options.lab)

    # can't be both a client and a server side test
    if client and server:
        parser.parser.error("Can not specify a test as both server and client!")

    if provision and client:
        parser.parser.error("Cannot specify provisioning and client!")

    is_special_task = (verify or repair or cleanup or collect_crashinfo or
                       provision or reset)
    use_client_trampoline = False
    if parser.options.control_name:
        if use_ssp:
            # When use_ssp is True, autoserv will be re-executed inside a
            # container preserving the --control-name argument. Control file
            # will be staged inside the rexecuted autoserv.
            control = None
        else:
            try:
                control = _stage_control_file(parser.options.control_name,
                                              results)
            except error.AutoservError as e:
                logging.info("Using client trampoline because of: %s", e)
                control = parser.options.control_name
                use_client_trampoline = True

    elif parser.args:
        control = parser.args[0]
    else:
        if not is_special_task:
            parser.parser.error("Missing argument: control file")
        control = None

    if ssh_verbosity > 0:
        # ssh_verbosity is an integer between 0 and 3, inclusive
        ssh_verbosity_flag = '-' + 'v' * ssh_verbosity
    else:
        ssh_verbosity_flag = ''

    machines = _get_machines(parser)
    if group_name and len(machines) < 2:
        parser.parser.error('-G %r may only be supplied with more than one '
                            'machine.' % group_name)

    logging.debug("Parser.args is %r", parser.args)
    try:
      logging.debug("Parser.options.args is %r", parser.options.args)
    except AttributeError:
      logging.debug("No Parser.options.args.")

    job_kwargs = {
            'control': control,
            'args': parser.args[1:],
            'resultdir': results,
            'label': label,
            'user': user,
            'machines': machines,
            'machine_dict_list': server_job.get_machine_dicts(
                    machine_names=machines,
                    store_dir=os.path.join(results,
                                           parser.options.host_info_subdir),
                    in_lab=in_lab,
                    use_shadow_store=not parser.options.local_only_host_info,
                    host_attributes=parser.options.host_attributes,
            ),
            'client': client,
            'ssh_user': ssh_user,
            'ssh_port': ssh_port,
            'ssh_pass': ssh_pass,
            'ssh_verbosity_flag': ssh_verbosity_flag,
            'ssh_options': ssh_options,
            'group_name': group_name,
            'tag': execution_tag,
            'disable_sysinfo': parser.options.disable_sysinfo,
            'in_lab': in_lab,
            'use_client_trampoline': use_client_trampoline,
    }
    if parser.options.parent_job_id:
        job_kwargs['parent_job_id'] = int(parser.options.parent_job_id)
    if control_filename:
        job_kwargs['control_filename'] = control_filename
    job = server_job.server_job(**job_kwargs)

    job.logging.start_logging()

    # perform checks
    job.precheck()

    # run the job
    exit_code = 0
    auto_start_servod = _CONFIG.get_config_value(
            'AUTOSERV', 'auto_start_servod', type=bool, default=False)

    site_utils.SetupTsMonGlobalState('autoserv', indirect=False,
                                     short_lived=True)
    try:
        try:
            if repair:
                if auto_start_servod and len(machines) == 1:
                    _start_servod(machines[0])
                job.repair(job_labels)
            elif verify:
                job.verify(job_labels)
            elif provision:
                job.provision(job_labels)
            elif reset:
                job.reset(job_labels)
            elif cleanup:
                job.cleanup(job_labels)
            else:
                if auto_start_servod and len(machines) == 1:
                    _start_servod(machines[0])
                if use_ssp:
                    try:
                        _run_with_ssp(job, container_id, job_or_task_id,
                                        results, parser, ssp_url, machines)
                    finally:
                        # Update the ownership of files in result folder.
                        correct_results_folder_permission(results)
                else:
                    if collect_crashinfo:
                        # Update the ownership of files in result folder. If the
                        # job to collect crashinfo was running inside container
                        # (SSP) and crashed before correcting folder permission,
                        # the result folder might have wrong permission setting.
                        try:
                            correct_results_folder_permission(results)
                        except:
                            # Ignore any error as the user may not have root
                            # permission to run sudo command.
                            pass
                    metric_name = ('chromeos/autotest/experimental/'
                                   'autoserv_job_run_duration')
                    f = {'in_container': utils.is_in_container(),
                         'success': False}
                    with metrics.SecondsTimer(metric_name, fields=f) as c:
                        job.run(verify_job_repo_url=verify_job_repo_url,
                                only_collect_crashinfo=collect_crashinfo,
                                skip_crash_collection=skip_crash_collection,
                                job_labels=job_labels,
                                use_packaging=(not no_use_packaging))
                        c['success'] = True

        finally:
            job.close()
            # Special task doesn't run parse, so result summary needs to be
            # built here.
            if results and (repair or verify or reset or cleanup or provision):
                # Throttle the result on the server side.
                try:
                    result_utils.execute(
                            results, control_data.DEFAULT_MAX_RESULT_SIZE_KB)
                except:
                    logging.exception(
                            'Non-critical failure: Failed to throttle results '
                            'in directory %s.', results)
                # Build result view and report metrics for result sizes.
                site_utils.collect_result_sizes(results)
    except:
        exit_code = 1
        traceback.print_exc()
    finally:
        metrics.Flush()

    sys.exit(exit_code)


# Job breakdown statuses
_hs = host_states.Status
_qs = host_queue_entry_states.Status
_status_list = [
        _qs.QUEUED, _qs.RESETTING, _qs.VERIFYING,
        _qs.PROVISIONING, _hs.REPAIRING, _qs.CLEANING,
        _qs.RUNNING, _qs.GATHERING, _qs.PARSING]
_JOB_OVERHEAD_STATUS = enum.Enum(*_status_list, string_values=True)


def get_job_status(options):
    """Returns the HQE Status for this run.

    @param options: parser options.
    """
    s = _JOB_OVERHEAD_STATUS
    task_mapping = {
            'reset': s.RESETTING, 'verify': s.VERIFYING,
            'provision': s.PROVISIONING, 'repair': s.REPAIRING,
            'cleanup': s.CLEANING, 'collect_crashinfo': s.GATHERING}
    match = [task for task in task_mapping if getattr(options, task, False)]
    return task_mapping[match[0]] if match else s.RUNNING


def _require_ssp_from_control(control_name):
    """Read the value of REQUIRE_SSP from test control file.

    This reads the control file from the prod checkout of autotest and uses that
    to determine whether to even stage the SSP package on a devserver.

    This means:
    [1] Any change in REQUIRE_SSP directive in a test requires a prod-push to go
    live.
    [2] This function may find that the control file does not exist but the SSP
    package may contain the test file. This function conservatively returns True
    in that case.

    This function is called very early in autoserv, before logging is setup.
    """
    if not control_name:
        return True
    try:
        path = _control_path_on_disk(control_name)
    except error.AutoservError as e:
        sys.stderr.write("autoserv: Could not determine control file path,"
                         " assuming we need SSP: %s\n" % e)
        sys.stderr.flush()
        return True
    if not os.path.isfile(path):
        return True
    control = control_data.parse_control(path)
    # There must be explicit directive in the control file to disable SSP.
    if not control or control.require_ssp is None:
        return True
    return control.require_ssp


def main():
    start_time = datetime.datetime.now()
    parser = autoserv_parser.autoserv_parser
    parser.parse_args()

    if len(sys.argv) == 1:
        parser.parser.print_help()
        sys.exit(1)

    if parser.options.no_logging:
        results = None
    else:
        results = parser.options.results
        if not results:
            results = 'results.' + time.strftime('%Y-%m-%d-%H.%M.%S')
        results = os.path.abspath(results)
        resultdir_exists = False
        for filename in ('control.srv', 'status.log', '.autoserv_execute'):
            if os.path.exists(os.path.join(results, filename)):
                resultdir_exists = True
        if not parser.options.use_existing_results and resultdir_exists:
            error = "Error: results directory already exists: %s\n" % results
            sys.stderr.write(error)
            sys.exit(1)

        # Now that we certified that there's no leftover results dir from
        # previous jobs, lets create the result dir since the logging system
        # needs to create the log file in there.
        if not os.path.isdir(results):
            os.makedirs(results)

    if parser.options.require_ssp:
        # This is currently only used for skylab (i.e., when --control-name is
        # used).
        use_ssp = _require_ssp_from_control(parser.options.control_name)
    else:
        use_ssp = False


    if use_ssp:
        log_dir = os.path.join(results, 'ssp_logs') if results else None
        if log_dir and not os.path.exists(log_dir):
            os.makedirs(log_dir)
    else:
        log_dir = results

    logging_manager.configure_logging(
            server_logging_config.ServerLoggingConfig(),
            results_dir=log_dir,
            use_console=not parser.options.no_tee,
            verbose=parser.options.verbose,
            no_console_prefix=parser.options.no_console_prefix)

    logging.debug('autoserv is running in drone %s.', socket.gethostname())
    logging.debug('autoserv environment: %r', os.environ)
    logging.debug('autoserv command was: %s', ' '.join(sys.argv))
    logging.debug('autoserv parsed options: %s', parser.options)

    if use_ssp:
        ssp_url = _stage_ssp(parser, results)
    else:
        ssp_url = None

    if results:
        logging.info("Results placed in %s" % results)

        # wait until now to perform this check, so it get properly logged
        if (parser.options.use_existing_results and not resultdir_exists and
            not utils.is_in_container()):
            logging.error("No existing results directory found: %s", results)
            sys.exit(1)

    if parser.options.write_pidfile and results:
        pid_file_manager = pidfile.PidFileManager(parser.options.pidfile_label,
                                                  results)
        pid_file_manager.open_file()
    else:
        pid_file_manager = None

    autotest.Autotest.set_install_in_tmpdir(
        parser.options.install_in_tmpdir)

    exit_code = 0
    # TODO(beeps): Extend this to cover different failure modes.
    # Testing exceptions are matched against labels sent to autoserv. Eg,
    # to allow only the hostless job to run, specify
    # testing_exceptions: test_suite in the shadow_config. To allow both
    # the hostless job and dummy_Pass to run, specify
    # testing_exceptions: test_suite,dummy_Pass. You can figure out
    # what label autoserv is invoked with by looking through the logs of a test
    # for the autoserv command's -l option.
    testing_exceptions = _CONFIG.get_config_value(
            'AUTOSERV', 'testing_exceptions', type=list, default=[])
    test_mode = _CONFIG.get_config_value(
            'AUTOSERV', 'testing_mode', type=bool, default=False)
    test_mode = (results_mocker and test_mode and not
                 any([ex in parser.options.label
                      for ex in testing_exceptions]))
    is_task = (parser.options.verify or parser.options.repair or
               parser.options.provision or parser.options.reset or
               parser.options.cleanup or parser.options.collect_crashinfo)

    trace_labels = {
            'job_id': job_directories.get_job_id_or_task_id(
                    parser.options.results)
    }
    trace = cloud_trace.SpanStack(
            labels=trace_labels,
            global_context=parser.options.cloud_trace_context)
    trace.enabled = parser.options.cloud_trace_context_enabled == 'True'
    try:
        try:
            if test_mode:
                # The parser doesn't run on tasks anyway, so we can just return
                # happy signals without faking results.
                if not is_task:
                    machine = parser.options.results.split('/')[-1]

                    # TODO(beeps): The proper way to do this would be to
                    # refactor job creation so we can invoke job.record
                    # directly. To do that one needs to pipe the test_name
                    # through run_autoserv and bail just before invoking
                    # the server job. See the comment in
                    # puppylab/results_mocker for more context.
                    results_mocker.ResultsMocker(
                            'unknown-test', parser.options.results, machine
                            ).mock_results()
                return
            else:
                with trace.Span(get_job_status(parser.options)):
                    run_autoserv(pid_file_manager, results, parser, ssp_url,
                                 use_ssp)
        except SystemExit as e:
            exit_code = e.code
            if exit_code:
                logging.exception('Uncaught SystemExit with code %s', exit_code)
        except Exception:
            # If we don't know what happened, we'll classify it as
            # an 'abort' and return 1.
            logging.exception('Uncaught Exception, exit_code = 1.')
            exit_code = 1
    finally:
        if pid_file_manager:
            pid_file_manager.close_file(exit_code)
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
