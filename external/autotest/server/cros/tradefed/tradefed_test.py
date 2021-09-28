# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# repohooks/pre-upload.py currently does not run pylint. But for developers who
# want to check their code manually we disable several harmless pylint warnings
# which just distract from more serious remaining issues.
#
# The instance variables _host and _install_paths are not defined in __init__().
# pylint: disable=attribute-defined-outside-init
#
# Many short variable names don't follow the naming convention.
# pylint: disable=invalid-name
#
# _parse_result() and _dir_size() don't access self and could be functions.
# pylint: disable=no-self-use

from collections import namedtuple
import errno
import glob
import hashlib
import logging
import os
import pipes
import re
import shutil
import stat
import tempfile
import urlparse

from autotest_lib.client.bin import utils as client_utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.server import test
from autotest_lib.server import utils
from autotest_lib.server.cros.tradefed import cts_expected_failure_parser
from autotest_lib.server.cros.tradefed import tradefed_chromelogin as login
from autotest_lib.server.cros.tradefed import tradefed_constants as constants
from autotest_lib.server.cros.tradefed import tradefed_utils
from autotest_lib.server.cros.tradefed import tradefed_prerequisite

# TODO(kinaba): Move to tradefed_utils together with the setup/cleanup methods.
MediaAsset = namedtuple('MediaAssetInfo', ['uri', 'localpath'])


class TradefedTest(test.test):
    """Base class to prepare DUT to run tests via tradefed."""
    version = 1

    # Default and upperbounds of max_retry, based on board and revision
    # after branching (that is, 'y' of R74-12345.y.z).
    #
    # By default, 0<=y<1 does 5 retries and 1<=y does 10. The |max_retry|
    # parameter in control files can override the count, within the
    # _BRANCH_MAX_RETRY limit below.
    _BRANCH_DEFAULT_RETRY = [(0, 5), (1, 10)]  # dev=5, beta=stable=10
    _BRANCH_MAX_RETRY = [(0, 5), (1, 30),      # dev=5, beta=30, stable=99
        (constants.APPROXIMATE_STABLE_BRANCH_NUMBER, 99)]
    # TODO(kinaba): betty-arcnext
    _BOARD_MAX_RETRY = {'betty': 0}

    _SHARD_CMD = None
    _board_arch = None
    _board_name = None
    _release_branch_number = None  # The 'y' of OS version Rxx-xxxxx.y.z
    _android_version = None
    _num_media_bundles = 0
    _abilist = []

    def _log_java_version(self):
        """Quick sanity and spew of java version installed on the server."""
        utils.run(
            'java',
            args=('-version',),
            ignore_status=False,
            verbose=True,
            stdout_tee=utils.TEE_TO_LOGS,
            stderr_tee=utils.TEE_TO_LOGS)

    def initialize(self,
                   bundle=None,
                   uri=None,
                   host=None,
                   hosts=None,
                   max_retry=None,
                   load_waivers=True,
                   retry_manual_tests=False,
                   warn_on_test_retry=True,
                   hard_reboot_on_failure=False):
        """Sets up the tools and binary bundles for the test."""
        self._install_paths = []
        # TODO(pwang): Remove host if we enable multiple hosts everywhere.
        self._hosts = [host] if host else hosts
        for host in self._hosts:
            logging.info('Hostname: %s', host.host_port)
        self._verify_hosts()

        self._max_retry = self._get_max_retry(max_retry)
        self._warn_on_test_retry = warn_on_test_retry
        # Tests in the lab run within individual lxc container instances.
        if utils.is_in_container():
            cache_root = constants.TRADEFED_CACHE_CONTAINER
        else:
            cache_root = constants.TRADEFED_CACHE_LOCAL

        # TODO(ihf): reevaluate this again when we run out of memory. We could
        # for example use 32 bit java on the first run but not during retries.
        # b/62895114. If select_32bit_java gets deleted for good also remove it
        # from the base image.
        # Try to save server memory (crbug.com/717413).
        # select_32bit_java()

        # The content of the cache survives across jobs.
        self._safe_makedirs(cache_root)
        self._tradefed_cache = os.path.join(cache_root, 'cache')
        self._tradefed_cache_lock = os.path.join(cache_root, 'lock')
        self._tradefed_cache_dirty = os.path.join(cache_root, 'dirty')
        # The content of the install location does not survive across jobs and
        # is isolated (by using a unique path)_against other autotest instances.
        # This is not needed for the lab, but if somebody wants to run multiple
        # TradedefTest instance.
        self._tradefed_install = tempfile.mkdtemp(
            prefix=constants.TRADEFED_PREFIX)
        # Under lxc the cache is shared between multiple autotest/tradefed
        # instances. We need to synchronize access to it. All binaries are
        # installed through the (shared) cache into the local (unshared)
        # lxc/autotest instance storage.
        # If clearing the cache it must happen before all downloads.
        self._clean_download_cache_if_needed()
        # Set permissions (rwxr-xr-x) to the executable binaries.
        permission = (
            stat.S_IRWXU | stat.S_IRGRP | stat.S_IXGRP | stat.S_IROTH
            | stat.S_IXOTH)
        self._install_files(constants.ADB_DIR, constants.ADB_FILES,
                            permission)
        self._install_files(constants.SDK_TOOLS_DIR,
                            constants.SDK_TOOLS_FILES, permission)

        # Install the tradefed bundle.
        bundle_install_path = self._install_bundle(
            uri or self._get_default_bundle_url(bundle))
        self._repository = os.path.join(bundle_install_path,
                                        self._get_tradefed_base_dir())

        # Load expected test failures to exclude them from re-runs.
        self._waivers = set()
        if load_waivers:
            self._waivers.update(
                    self._get_expected_failures('expectations', bundle))
        if not retry_manual_tests:
            self._waivers.update(
                    self._get_expected_failures('manual_tests', bundle))

        # Load modules with no tests.
        self._notest_modules = self._get_expected_failures('notest_modules',
                bundle)
        self._hard_reboot_on_failure = hard_reboot_on_failure

    def postprocess(self):
        """Postprocess: output performance values."""
        path = tradefed_utils.get_test_result_xml_path(
            os.path.join(self.resultsdir,
                         self._get_tradefed_base_dir()))
        if path:
            for metric in tradefed_utils.get_perf_metrics_from_test_result_xml(
                path, self.resultsdir):
                self.output_perf_value(**metric)

    def cleanup(self):
        """Cleans up any dirtied state."""
        # Kill any lingering adb servers.
        for host in self._hosts:
            try:
                self._run_adb_cmd(host, verbose=True, args=('kill-server',))
            except (error.CmdError, AttributeError):
                pass

        if hasattr(self, '_tradefed_install'):
            logging.info('Cleaning up %s.', self._tradefed_install)
            try:
                shutil.rmtree(self._tradefed_install)
            except IOError:
                pass

    def _verify_hosts(self):
        """Verify all hosts' ChromeOS consistency."""
        # Check release builder path. E.g. cave-release/R66-10435.0.0
        release_builder_path = set(host.get_release_builder_path()
                                   for host in self._hosts)
        if len(release_builder_path) > 1:
            raise error.TestFail('Hosts\' CHROMEOS_RELEASE_BUILDER_PATH is '
                                 'different: %s', release_builder_path)

        # Check ChromeOS ARC VERSION. E.g.
        arc_version = set(host.get_arc_version() for host in self._hosts)
        if len(arc_version) > 1:
            raise error.TestFail('Hosts\' CHROMEOS_ARC_VERSION is different: '
                                 '%s', arc_version)

        # Check ChromeOS model for unibuild.
        # TODO(pwang): Adding a check if we found how to detect host's model.

    def _verify_arc_hosts(self):
        """Verify all hosts' Android configuration consistency.

        This method should only be called after all hosts' Android has been
        successfully booted up."""
        # Check all hosts have same Android fingerprint.
        fingerprint = set(self._run_adb_cmd(
            host,
            args=('shell', 'getprop', 'ro.build.fingerprint')).stdout
            for host in self._hosts)
        if len(fingerprint) > 1:
            raise error.TestFail('Hosts\' supported fingerprint is different: '
                                 '%s', fingerprint)

    def _calculate_test_count_factor(self, bundle):
        """ Calculate the multiplicative factor for the test case number.

        The value equals to the times each test case is run, which is determined
        by the intersection of the supported ABIs of the CTS/GTS bundle and that
        of the tested device."""
        arm_abis = set(('armeabi-v7a', 'arm64-v8a'))
        x86_abis = set(('x86', 'x86_64'))
        if bundle == 'arm':
            tradefed_abis = arm_abis
        elif bundle == 'x86':
            tradefed_abis = x86_abis
        else:
            tradefed_abis = arm_abis | x86_abis
        self._test_count_factor = len(set(self._get_abilist()) & tradefed_abis)
        # Avoid setting timeout=0 (None) in any cases.
        self._timeout_factor = max(1, self._test_count_factor)

    def _get_adb_targets(self):
        """Get a list of adb targets."""
        return [self._get_adb_target(host) for host in self._hosts]

    def _get_adb_target(self, host):
        """Get the adb target format.

        This method is slightly different from host.host_port as we need to
        explicitly specify the port so the serial name of adb target would
        match."""
        return '{}:{}'.format(host.hostname, host.port)

    def _run_adb_cmd(self, host=None, **kwargs):
        """Running adb command.

        @param host: DUT that want to connect to. (None if the adb command is
                     intended to run in the server. eg. keygen)
        """
        # As of N, tradefed could not specify which adb socket to use, which use
        # tcp:localhost:5037 by default.
        adb_global_option = ('-H', 'localhost', '-P', '5037')
        if host:
            host_port = self._get_adb_target(host)
            adb_global_option = ('-s', host_port)
        kwargs['args'] = adb_global_option + kwargs.get('args', ())
        result = self._run('adb', **kwargs)
        logging.info('adb %s:\n%s', ' '.join(kwargs.get('args')),
                     result.stdout + result.stderr)
        return result

    def _try_adb_connect(self, host):
        """Attempts to connect to adb on the DUT.

        @param host: DUT that need to be connected.
        @return boolean indicating if adb connected successfully.
        """
        # Add ADB_TRACE=all for debugging adb connection failures.
        env = os.environ.copy()
        env['ADB_TRACE'] = 'all'
        try:
            # This may fail return failure due to a race condition in adb
            # connect (b/29370989). If adb is already connected, this command
            # will immediately return success.
            host_port = self._get_adb_target(host)
            result = self._run_adb_cmd(
                host, args=('connect', host_port), verbose=True, env=env,
                ignore_status=True,
                timeout=constants.ADB_CONNECT_TIMEOUT_SECONDS)
            if result.exit_status != 0:
                return False

            result = self._run_adb_cmd(host, args=('devices',), env=env,
                timeout=constants.ADB_CONNECT_TIMEOUT_SECONDS)
            if not re.search(r'{}\s+(device|unauthorized)'.format(
                    re.escape(host_port)), result.stdout):
                logging.info('No result found in with pattern: %s',
                             r'{}\s+(device|unauthorized)'.format(
                                 re.escape(host_port)))
                return False

            # Actually test the connection with an adb command as there can be
            # a race between detecting the connected device and actually being
            # able to run a commmand with authenticated adb.
            result = self._run_adb_cmd(
                host, args=('shell', 'exit'), env=env, ignore_status=True,
                timeout=constants.ADB_CONNECT_TIMEOUT_SECONDS)
            return result.exit_status == 0
        except error.CmdTimeoutError as e:
            logging.warning(e)
            return False

    def _android_shell(self, host, command):
        """Run a command remotely on the device in an android shell

        This function is strictly for internal use only, as commands do not run
        in a fully consistent Android environment. Prefer adb shell instead.
        """
        host.run('android-sh -c ' + pipes.quote(command))

    def _write_android_file(self, host, filename, data):
        """Writes a file to a location relative to the android container.

        This is an internal function used to bootstrap adb.
        Tests should use adb push to write files.
        """
        android_cmd = 'echo %s > %s' % (pipes.quote(data),
                                        pipes.quote(filename))
        self._android_shell(host, android_cmd)

    def _connect_adb(self, host, pubkey):
        """Sets up ADB connection to the ARC container.

        @param host: DUT that should be connected to.
        @param pubkey: public key that adb keygen && adb pubkey generated.
        """
        logging.info('Setting up adb connection.')
        # Push keys for adb.
        self._write_android_file(host, constants.ANDROID_ADB_KEYS_PATH, pubkey)
        self._android_shell(
            host, 'restorecon ' + pipes.quote(constants.ANDROID_ADB_KEYS_PATH))

        # This starts adbd.
        self._android_shell(host, 'setprop sys.usb.config adb')

        # Also let it be automatically started upon reboot.
        self._android_shell(host, 'setprop persist.sys.usb.config adb')

        # adbd may take some time to come up. Repeatedly try to connect to adb.
        utils.poll_for_condition(
            lambda: self._try_adb_connect(host),
            timeout=constants.ADB_READY_TIMEOUT_SECONDS,
            sleep_interval=constants.ADB_POLLING_INTERVAL_SECONDS)

        logging.info('Successfully setup adb connection.')

    def _wait_for_arc_boot(self, host):
        """Wait until ARC is fully booted.

        Tests for the presence of the intent helper app to determine whether ARC
        has finished booting.
        @param host: DUT that need to be connected to.
        """

        def _intent_helper_running():
            result = self._run_adb_cmd(
                host,
                args=('shell', 'pgrep', '-f', 'org.chromium.arc.intent_helper'),
                ignore_status=True)
            return bool(result.stdout)

        utils.poll_for_condition(
            _intent_helper_running,
            exception=error.TestFail(
                'Error: Timed out waiting for intent helper.'),
            timeout=constants.ARC_READY_TIMEOUT_SECONDS,
            sleep_interval=constants.ARC_POLLING_INTERVAL_SECONDS)

    def _disable_adb_install_dialog(self, host):
        """Disables a dialog shown on adb install execution.

        By default, on adb install execution, "Allow Google to regularly check
        device activity ... " dialog is shown. It requires manual user action
        so that tests are blocked at the point.
        This method disables it.
        """
        logging.info('Disabling the adb install dialog.')
        result = self._run_adb_cmd(
            host,
            verbose=True,
            args=('shell', 'settings', 'put', 'global',
                  'verifier_verify_adb_installs', '0'))
        logging.info('Disable adb dialog: %s', result.stdout)

        # Android "RescueParty" feature can reset the above settings when the
        # device crashes often. Disable the rescue during testing.
        self._android_shell(host, 'setprop persist.sys.disable_rescue true')

    def _ready_arc(self):
        """Ready ARC and adb in parallel for running tests via tradefed."""
        # Generate the adb keys on server.
        key_path = os.path.join(self.tmpdir, 'test_key')
        self._run_adb_cmd(verbose=True, args=('keygen', pipes.quote(key_path)))
        os.environ['ADB_VENDOR_KEYS'] = key_path
        pubkey = self._run_adb_cmd(verbose=True,
                args=('pubkey', pipes.quote(key_path))).stdout

        for _ in range(2):
            try:
                # Kill existing adb server to ensure that the env var is picked
                # up, and reset any previous bad state.
                self._run_adb_cmd(verbose=True, args=('kill-server',))

                # TODO(pwang): connect_adb takes 10+ seconds on a single DUT.
                #              Parallelize it if it becomes a bottleneck.
                for host in self._hosts:
                    self._connect_adb(host, pubkey)
                    self._disable_adb_install_dialog(host)
                    self._wait_for_arc_boot(host)
                self._verify_arc_hosts()
                return
            except (utils.TimeoutError, error.CmdTimeoutError):
                logging.error('Failed to set up adb connection. Retrying...')
        raise error.TestFail('Error: Failed to set up adb connection')

    def _safe_makedirs(self, path):
        """Creates a directory at |path| and its ancestors.

        Unlike os.makedirs(), ignore errors even if directories exist.
        """
        try:
            os.makedirs(path)
        except OSError as e:
            if not (e.errno == errno.EEXIST and os.path.isdir(path)):
                raise

    def _unzip(self, filename):
        """Unzip the file.

        The destination directory name will be the stem of filename.
        E.g., _unzip('foo/bar/baz.zip') will create directory at
        'foo/bar/baz', and then will inflate zip's content under the directory.
        If here is already a directory at the stem, that directory will be used.

        @param filename: Path to the zip archive.
        @return Path to the inflated directory.
        """
        destination = os.path.splitext(filename)[0]
        if os.path.isdir(destination):
            logging.info('Skipping unzip %s, reusing content of %s', filename,
                         destination)
            return destination
        tmp = tempfile.mkdtemp(dir=os.path.dirname(filename))
        logging.info('Begin unzip %s', filename)
        try:
            utils.run('unzip', args=('-d', tmp, filename))
        except:
            logging.error('Failed unzip, cleaning up.')
            # Clean up just created files.
            shutil.rmtree(tmp, ignore_errors=True)
            raise
        logging.info('End unzip %s', filename)
        try:
            os.renames(tmp, destination)
        except:
            logging.error('Failed rename, cleaning up.')
            shutil.rmtree(destination, ignore_errors=True)
            shutil.rmtree(tmp, ignore_errors=True)
            raise
        return destination

    def _dir_size(self, directory):
        """Compute recursive size in bytes of directory."""
        size = 0
        for root, _, files in os.walk(directory):
            for name in files:
                try:
                    size += os.path.getsize(os.path.join(root, name))
                except OSError:
                    logging.error('Inaccessible path (crbug/793696): %s/%s',
                                  root, name)
        return size

    def _invalidate_download_cache(self):
        """Marks the download cache for deferred deletion.

        Used to make cache file operations atomic across failures and reboots.
        The caller is responsible to hold the lock to the cache.
        """
        if not os.path.exists(self._tradefed_cache_dirty):
            os.mkdir(self._tradefed_cache_dirty)

    def _validate_download_cache(self):
        """Validates and unmarks the download cache from deletion.

        Used to make cache file operations atomic across failures and reboots.
        The caller is responsible to hold the lock to the cache.
        """
        shutil.rmtree(self._tradefed_cache_dirty, ignore_errors=True)

    def _clean_download_cache_if_needed(self, force=False):
        """Invalidates cache to prevent it from growing too large."""
        # If the cache is large enough to hold a working set, we can simply
        # delete everything without thrashing.
        # TODO(ihf): Investigate strategies like LRU.
        clean = force
        with tradefed_utils.lock(self._tradefed_cache_lock):
            size = self._dir_size(self._tradefed_cache)
            if size > constants.TRADEFED_CACHE_MAX_SIZE:
                logging.info(
                    'Current cache size=%d got too large. Clearing %s.', size,
                    self._tradefed_cache)
                clean = True
            else:
                logging.info('Current cache size=%d of %s.', size,
                             self._tradefed_cache)
            if os.path.exists(self._tradefed_cache_dirty):
                logging.info('Found dirty cache.')
                clean = True
            if clean:
                logging.warning('Cleaning download cache.')
                shutil.rmtree(self._tradefed_cache, ignore_errors=True)
                self._safe_makedirs(self._tradefed_cache)
                shutil.rmtree(self._tradefed_cache_dirty, ignore_errors=True)

    def _download_to_cache(self, uri):
        """Downloads the uri from the storage server.

        It always checks the cache for available binaries first and skips
        download if binaries are already in cache.

        The caller of this function is responsible for holding the cache lock.

        @param uri: The Google Storage or dl.google.com uri.
        @return Path to the downloaded object, name.
        """
        # We are hashing the uri instead of the binary. This is acceptable, as
        # the uris are supposed to contain version information and an object is
        # not supposed to be changed once created.
        output_dir = os.path.join(self._tradefed_cache,
                                  hashlib.md5(uri).hexdigest())
        # Check for existence of cache entry. We check for directory existence
        # instead of file existence, so that _install_bundle can delete original
        # zip files to save disk space.
        if os.path.exists(output_dir):
            # TODO(crbug.com/800657): Mitigation for the invalid state. Normally
            # this should not happen, but when a lock is force borken due to
            # high IO load, multiple processes may enter the critical section
            # and leave a bad state permanently.
            if os.listdir(output_dir):
                logging.info('Skipping download of %s, reusing content of %s.',
                             uri, output_dir)
                return os.path.join(output_dir,
                    os.path.basename(urlparse.urlparse(uri).path))
            logging.error('Empty cache entry detected %s', output_dir)
        return self._download_to_dir(uri, output_dir)

    def _download_to_dir(self, uri, output_dir):
        """Downloads the gs|http|https uri from the storage server.

        @param uri: The Google Storage or dl.google.com uri.
        @output_dir: The directory where the downloaded file should be placed.
        @return Path to the downloaded object, name.
        """
        # Split uri into 3 pieces for use by gsutil and also by wget.
        parsed = urlparse.urlparse(uri)
        filename = os.path.basename(parsed.path)
        output = os.path.join(output_dir, filename)

        self._safe_makedirs(output_dir)
        if parsed.scheme not in ['gs', 'http', 'https']:
            raise error.TestFail(
                'Error: Unknown download scheme %s' % parsed.scheme)
        if parsed.scheme in ['http', 'https']:
            logging.info('Using wget to download %s to %s.', uri, output_dir)
            # We are downloading 1 file at a time, hence using -O over -P.
            utils.run(
                'wget',
                args=('--report-speed=bits', '-O', output, uri),
                verbose=True)
            return output

        if not client_utils.is_moblab():
            # If the machine can access to the storage server directly,
            # defer to "gsutil" for downloading.
            logging.info('Not in lab. Downloading %s directly to %s.',
                         uri, output)
            # b/17445576: gsutil rsync of individual files is not implemented.
            utils.run('gsutil', args=('cp', uri, output), verbose=True)
            return output

        # We are in the moblab. Because the machine cannot access the storage
        # server directly, use dev server to proxy.
        logging.info('In lab. Downloading %s by staging to %s.',
                     uri, output)

        dirname = os.path.dirname(parsed.path)
        archive_url = '%s://%s%s' % (parsed.scheme, parsed.netloc, dirname)

        # First, request the devserver to download files into the lab network.
        # TODO(ihf): Switch stage_artifacts to honor rsync. Then we don't have
        # to shuffle files inside of tarballs.
        info = self._hosts[0].host_info_store.get()
        ds = dev_server.ImageServer.resolve(info.build)
        ds.stage_artifacts(
            info.build, files=[filename], archive_url=archive_url)

        # Then download files from the dev server.
        # TODO(ihf): use rsync instead of wget. Are there 3 machines involved?
        # Itself, dev_server plus DUT? Or is there just no rsync in moblab?
        ds_src = '/'.join([ds.url(), 'static', dirname, filename])
        logging.info('dev_server URL: %s', ds_src)
        # Calls into DUT to pull uri from dev_server.
        utils.run(
            'wget',
            args=('--report-speed=bits', '-O', output, ds_src),
            verbose=True)
        return output

    def _instance_copyfile(self, cache_path):
        """Makes a copy of a file from the (shared) cache to a wholy owned
        local instance. Also copies one level of cache directoy (MD5 named).
        """
        filename = os.path.basename(cache_path)
        dirname = os.path.basename(os.path.dirname(cache_path))
        instance_dir = os.path.join(self._tradefed_install, dirname)
        # Make sure destination directory is named the same.
        self._safe_makedirs(instance_dir)
        instance_path = os.path.join(instance_dir, filename)
        shutil.copyfile(cache_path, instance_path)
        return instance_path

    def _instance_copytree(self, cache_path):
        """Makes a copy of a directory from the (shared and writable) cache to
        a wholy owned local instance.

        TODO(ihf): Consider using cp -al to only copy links. Not sure if this
        is really a benefit across the container boundary, but it is risky due
        to the possibility of corrupting the original files by an lxc instance.
        """
        # We keep the top 2 names from the cache_path = .../dir1/dir2.
        dir2 = os.path.basename(cache_path)
        dir1 = os.path.basename(os.path.dirname(cache_path))
        instance_path = os.path.join(self._tradefed_install, dir1, dir2)
        logging.info('Copying %s to instance %s', cache_path, instance_path)
        shutil.copytree(cache_path, instance_path)
        return instance_path

    def _install_bundle(self, gs_uri):
        """Downloads a zip file, installs it and returns the local path.

        @param gs_uri: GS bucket that contains the necessary files.
        """
        if not gs_uri.endswith('.zip'):
            raise error.TestFail('Error: Not a .zip file %s.', gs_uri)
        # Atomic write through of file.
        with tradefed_utils.lock(self._tradefed_cache_lock):
            # Atomic operations.
            self._invalidate_download_cache()
            # Download is lazy (cache_path may not actually exist if
            # cache_unzipped does).
            cache_path = self._download_to_cache(gs_uri)
            # Unzip is lazy as well (but cache_unzipped guaranteed to
            # exist).
            cache_unzipped = self._unzip(cache_path)
            # To save space we delete the original zip file. This works as
            # _download only checks existence of the cache directory for
            # lazily skipping download, and unzip itself will bail if the
            # unzipped destination exists. Hence we don't need the original
            # anymore.
            if os.path.exists(cache_path):
                logging.info('Deleting original %s', cache_path)
                os.remove(cache_path)
            # Erase dirty marker from disk.
            self._validate_download_cache()
            # We always copy files to give tradefed a clean copy of the
            # bundle.
            unzipped_local = self._instance_copytree(cache_unzipped)
        return unzipped_local

    def _install_files(self, gs_dir, files, permission):
        """Installs binary tools."""
        for filename in files:
            gs_uri = os.path.join(gs_dir, filename)
            # Atomic write through of file.
            with tradefed_utils.lock(self._tradefed_cache_lock):
                # We don't want to leave a corrupt cache for other jobs.
                self._invalidate_download_cache()
                cache_path = self._download_to_cache(gs_uri)
                # Mark cache as clean again.
                self._validate_download_cache()
                # This only affects the current job, so not part of cache
                # validation.
                local = self._instance_copyfile(cache_path)
            os.chmod(local, permission)
            # Keep track of PATH.
            self._install_paths.append(os.path.dirname(local))

    def _prepare_media(self, media_asset):
        """Downloads and offers the cached media files to tradefed."""
        if media_asset.uri:
            media = self._install_bundle(media_asset.uri)
            if os.path.islink(media_asset.localpath):
                os.unlink(media_asset.localpath)
            if os.path.isdir(media_asset.localpath):
                shutil.rmtree(media_asset.localpath)
            self._safe_makedirs(os.path.dirname(media_asset.localpath))
            os.symlink(media, media_asset.localpath)

            logging.info('Offered %s as a media directory in %s',
                    media, media_asset.localpath)

        # Records the number of existing media bundles, to check later.
        if os.path.isdir(media_asset.localpath):
            self._num_media_bundles = len(
                    os.listdir(media_asset.localpath))

    def _cleanup_media(self, media_asset):
        """Clean up the local copy of cached media files."""
        self._fail_on_unexpected_media_download(media_asset)
        if os.path.islink(media_asset.localpath):
            path = os.readlink(media_asset.localpath)
            os.unlink(media_asset.localpath)
            if os.path.isdir(path):
                logging.info('Cleaning up media files in %s', path)
                shutil.rmtree(path)

    def _fail_on_unexpected_media_download(self, media_asset):
        if os.path.isdir(media_asset.localpath):
            contents = os.listdir(media_asset.localpath)
            if len(contents) > self._num_media_bundles:
                raise error.TestFail(
                    'Failed: Unexpected media bundle was added %s' % contents)

    def _run(self, *args, **kwargs):
        """Executes the given command line.

        To support SDK tools, such as adb or aapt, this adds _install_paths
        to the extra_paths. Before invoking this, ensure _install_files() has
        been called.
        """
        kwargs['extra_paths'] = (
            kwargs.get('extra_paths', []) + self._install_paths)
        return utils.run(*args, **kwargs)

    def _collect_tradefed_global_log(self, result, destination):
        """Collects the tradefed global log.

        @param result: The result object from utils.run.
        @param destination: Autotest result directory (destination of logs).
        """
        match = re.search(r'Saved log to /tmp/(tradefed_global_log_.*\.txt)',
                          result.stdout)
        if not match:
            logging.error('no tradefed_global_log file is found')
            return

        name = match.group(1)
        dest = os.path.join(destination, 'logs', 'tmp')
        self._safe_makedirs(dest)
        shutil.copy(os.path.join('/tmp', name), os.path.join(dest, name))

    def _get_expected_failures(self, directory, bundle_abi):
        """Return a list of expected failures or no test module.

        @param directory: A directory with expected no tests or failures files.
        @param bundle_abi: 'arm' or 'x86' if the test is for the particular ABI.
                           None otherwise (like GTS, built for multi-ABI.)
        @return: A list of expected failures or no test modules for the current
                 testing device.
        """
        # Load waivers and manual tests so TF doesn't re-run them.
        expected_fail_files = []
        test_board = self._get_board_name()
        test_arch = self._get_board_arch()
        sdk_ver = self._get_android_version()
        expected_fail_dir = os.path.join(self.bindir, directory)
        if os.path.exists(expected_fail_dir):
            expected_fail_files += glob.glob(expected_fail_dir + '/*.yaml')

        waivers = cts_expected_failure_parser.ParseKnownCTSFailures(
            expected_fail_files)
        return waivers.find_waivers(test_arch, test_board, bundle_abi, sdk_ver)

    def _get_abilist(self):
        """Return the abilist supported by calling adb command.

        This method should only be called after the android environment is
        successfully initialized."""
        if not self._abilist:
            for _ in range(3):
                abilist_str = self._run_adb_cmd(
                    self._hosts[0],
                    args=('shell', 'getprop',
                          'ro.product.cpu.abilist')).stdout.strip()
                if abilist_str:
                    self._abilist = abilist_str.split(',')
                    break
                else:
                    # TODO(kinaba): Sometimes getprop returns an empty string.
                    # Investigate why. For now we mitigate the bug by retries.
                    logging.error('Empty abilist.')
        return self._abilist

    def _get_release_branch_number(self):
        """Returns the DUT branch number (z of Rxx-yyyyy.z.w) or 0 on error."""
        if not self._release_branch_number:
            ver = (self._hosts[0].get_release_version() or '').split('.')
            self._release_branch_number = (int(ver[1]) if len(ver) >= 3 else 0)
        return self._release_branch_number

    def _get_board_arch(self):
        """Return target DUT arch name."""
        if not self._board_arch:
            self._board_arch = ('arm' if self._hosts[0].get_cpu_arch() == 'arm'
                else 'x86')
        return self._board_arch

    def _get_board_name(self):
        """Return target DUT board name."""
        if not self._board_name:
            self._board_name = self._hosts[0].get_board().split(':')[1]
        return self._board_name

    def _get_android_version(self):
        """Return target DUT Android SDK version"""
        # TODO(kinaba): factor this out to server/hosts/cros_host.py
        if not self._android_version:
            self._android_version = self._hosts[0].run(
                'grep ANDROID_SDK /etc/lsb-release',
                ignore_status=True).stdout.rstrip().split('=')[1]
        return self._android_version

    def _get_max_retry(self, max_retry):
        """Return the maximum number of retries.

        @param max_retry: max_retry specified in the control file.
        @return: number of retries for this specific host.
        """
        if max_retry is None:
            max_retry = self._get_branch_retry(self._BRANCH_DEFAULT_RETRY)
        candidate = [max_retry]
        candidate.append(self._get_board_retry())
        candidate.append(self._get_branch_retry(self._BRANCH_MAX_RETRY))
        return min(x for x in candidate if x is not None)

    def _get_board_retry(self):
        """Return the maximum number of retries for DUT board name.

        @return: number of max_retry or None.
        """
        board = self._get_board_name()
        if board in self._BOARD_MAX_RETRY:
            return self._BOARD_MAX_RETRY[board]
        logging.info('No board retry specified for board: %s', board)
        return None

    def _get_branch_retry(self, table):
        """Returns the retry count for DUT branch number defined in |table|."""
        number = self._get_release_branch_number()
        for lowerbound, retry in reversed(table):
            if lowerbound <= number:
                return retry
        logging.warning('Could not establish channel. Using retry=0.')
        return 0

    def _run_commands(self, commands, **kwargs):
        """Run commands on all the hosts."""
        for host in self._hosts:
            for command in commands:
                logging.info('RUN: %s\n', command)
                output = host.run(command, **kwargs)
                logging.info('END: %s\n', command)
                logging.debug(output)

    def _override_powerd_prefs(self):
        """Overrides powerd prefs to prevent screen from turning off, complying
        with CTS requirements.

        This is a remote version of PowerPrefChanger which ensures overrided
        policies won't persist across reboots by bind-mounting onto the config
        directory.
        """
        pref_dir = constants.POWERD_PREF_DIR
        temp_dir = constants.POWERD_TEMP_DIR
        commands = (
                'cp -r %s %s' % (pref_dir, temp_dir),
                'echo 1 > %s/ignore_external_policy' % temp_dir,
                'echo 0 | tee %s/{,un}plugged_{dim,off,suspend}_ms' % temp_dir,
                'mount --bind %s %s' % (temp_dir, pref_dir),
                'restart powerd',
        )
        try:
            self._run_commands(commands)
        except (error.AutoservRunError, error.AutoservSSHTimeout):
            logging.warning('Failed to override powerd policy, tests depending '
                            'on screen being always on may fail.')

    def _restore_powerd_prefs(self):
        """Restores powerd prefs overrided by _override_powerd_prefs()."""
        pref_dir = constants.POWERD_PREF_DIR
        temp_dir = constants.POWERD_TEMP_DIR
        commands = (
                'umount %s' % pref_dir,
                'restart powerd',
                'rm -rf %s' % temp_dir,
        )
        try:
            self._run_commands(commands)
        except (error.AutoservRunError, error.AutoservSSHTimeout):
            logging.warning('Failed to restore powerd policy, overrided policy '
                            'will persist until device reboot.')

    def _run_and_parse_tradefed(self, command):
        """Kick off the tradefed command.

        @param command: Lists of command tokens.
        @raise TestFail: when a test failure is detected.
        @return: tuple of (tests, pass, fail, notexecuted) counts.
        """
        target_argument = []
        for host in self._hosts:
            target_argument += ['-s', self._get_adb_target(host)]
        shard_argument = []
        if len(self._hosts) > 1:
            if self._SHARD_CMD:
                shard_argument = [self._SHARD_CMD, str(len(self._hosts))]
            else:
                logging.warning('cts-tradefed shard command isn\'t defined, '
                                'falling back to use single device.')
        command = command + target_argument + shard_argument

        try:
            output = self._run_tradefed(command)
        except Exception as e:
            self._log_java_version()
            if not isinstance(e, error.CmdTimeoutError):
                # In case this happened due to file corruptions, try to
                # force to recreate the cache.
                logging.error('Failed to run tradefed! Cleaning up now.')
                self._clean_download_cache_if_needed(force=True)
            raise

        result_destination = os.path.join(self.resultsdir,
                                          self._get_tradefed_base_dir())
        # Gather the global log first. Datetime parsing below can abort the test
        # if tradefed startup had failed. Even then the global log is useful.
        self._collect_tradefed_global_log(output, result_destination)
        # Result parsing must come after all other essential operations as test
        # warnings, errors and failures can be raised here.
        return tradefed_utils.parse_tradefed_result(output.stdout,
                                                    self._waivers)

    def _setup_result_directories(self):
        """Sets up the results and logs directories for tradefed.

        Tradefed saves the logs and results at:
          self._repository/results/$datetime/
          self._repository/results/$datetime.zip
          self._repository/logs/$datetime/
        Because other tools rely on the currently chosen Google storage paths
        we need to keep destination_results in:
          self.resultsdir/android-cts/results/$datetime/
          self.resultsdir/android-cts/results/$datetime.zip
          self.resultsdir/android-cts/results/logs/$datetime/
        To bridge between them, create symlinks from the former to the latter.
        """
        logging.info('Setting up tradefed results and logs directories.')

        results_destination = os.path.join(self.resultsdir,
                                           self._get_tradefed_base_dir())
        logs_destination = os.path.join(results_destination, 'logs')
        directory_mapping = [
            (os.path.join(self._repository, 'results'), results_destination),
            (os.path.join(self._repository, 'logs'), logs_destination),
        ]

        for (tradefed_path, final_path) in directory_mapping:
            if os.path.exists(tradefed_path):
                shutil.rmtree(tradefed_path)
            self._safe_makedirs(final_path)
            os.symlink(final_path, tradefed_path)

    def _install_plan(self, subplan):
        """Copy test subplan to CTS-TF.

        @param subplan: CTS subplan to be copied into TF.
        """
        logging.info('Install subplan: %s', subplan)
        subplans_tf_dir = os.path.join(self._repository, 'subplans')
        if not os.path.exists(subplans_tf_dir):
            os.makedirs(subplans_tf_dir)
        test_subplan_file = os.path.join(self.bindir, 'subplans',
                                         '%s.xml' % subplan)
        try:
            shutil.copy(test_subplan_file, subplans_tf_dir)
        except (shutil.Error, OSError, IOError) as e:
            raise error.TestFail(
                'Error: failed to copy test subplan %s to CTS bundle. %s' %
                (test_subplan_file, e))

    def _should_skip_test(self, _bundle):
        """Some tests are expected to fail and are skipped.

        Subclasses should override with specific details.
        """
        return False

    def _should_reboot(self, steps):
        """Oracle to decide if DUT should reboot or just restart Chrome.

        For now we will not reboot after the first two iterations, but on all
        iterations afterward as before. In particular this means that most CTS
        tests will now not get a "clean" machine, but one on which tests ran
        before. But we will still reboot after persistent failures, hopefully
        not causing too many flakes down the line.
        """
        if steps < 3:
            return False
        return True

    def _copy_extra_artifacts_dut(self, extra_artifacts, host, output_dir):
        """ Upload the custom artifacts """
        self._safe_makedirs(output_dir)

        for artifact in extra_artifacts:
            logging.info('Copying extra artifacts from "%s" to "%s".',
                         artifact, output_dir)
            try:
                self._run_adb_cmd(host, verbose=True, timeout=120,
                                  args=('pull', artifact, output_dir))
            except:
                # Maybe ADB connection failed, or the artifacts don't exist.
                logging.exception('Copying extra artifacts failed.')

    def _copy_extra_artifacts_host(self, extra_artifacts, host, output_dir):
        """ Upload the custom artifacts """
        self._safe_makedirs(output_dir)

        for artifact in extra_artifacts:
            logging.info('Copying extra artifacts from "%s" to "%s".',
                         artifact, output_dir)
            for extracted_path in glob.glob(artifact):
                logging.info('... %s', extracted_path)
                # Move it not to collect it again in future retries.
                shutil.move(extracted_path, output_dir)

    def _run_tradefed_list_results(self):
        """Run the `tradefed list results` command.

        @return: tuple of the last (session_id, pass, fail, all_done?).
        """

        # Fix b/143580192: We set the timeout to 3 min. It never takes more than
        # 10s on light disk load.
        output = self._run_tradefed_with_timeout(['list', 'results'], 180)

        # Parses the last session from the output that looks like:
        #
        # Session  Pass  Fail  Modules Complete ...
        # 0        90    10    1 of 2
        # 1        199   1     2 of 2
        # ...
        lastmatch = None
        for m in re.finditer(r'^(\d+)\s+(\d+)\s+(\d+)\s+(\d+) of (\d+)',
                             output.stdout, re.MULTILINE):
            session, passed, failed, done, total = map(int,
                                                       m.group(1, 2, 3, 4, 5))
            lastmatch = (session, passed, failed, done == total)
        return lastmatch

    def _tradefed_retry_command(self, template, session_id):
        raise NotImplementedError('Subclass should override this function')

    def _tradefed_run_command(self, template):
        raise NotImplementedError('Subclass should override this function')

    def _tradefed_cmd_path(self):
        raise NotImplementedError('Subclass should override this function')

    def _tradefed_env(self):
        return None

    def _run_tradefed_with_timeout(self, command, timeout):
        tradefed = self._tradefed_cmd_path()
        with tradefed_utils.adb_keepalive(self._get_adb_targets(),
                                          self._install_paths):
            logging.info('RUN(timeout=%d): %s', timeout,
                         ' '.join([tradefed] + command))
            output = self._run(
                tradefed,
                args=tuple(command),
                env=self._tradefed_env(),
                timeout=timeout,
                verbose=True,
                ignore_status=False,
                # Make sure to tee tradefed stdout/stderr to autotest logs
                # continuously during the test run.
                stdout_tee=utils.TEE_TO_LOGS,
                stderr_tee=utils.TEE_TO_LOGS)
            logging.info('END: %s\n', ' '.join([tradefed] + command))
        return output

    def _run_tradefed(self, command):
        timeout = self._timeout * self._timeout_factor
        return self._run_tradefed_with_timeout(command, timeout)

    def _run_tradefed_with_retries(self,
                                   test_name,
                                   run_template,
                                   retry_template,
                                   timeout,
                                   media_asset=None,
                                   enable_default_apps=False,
                                   target_module=None,
                                   target_plan=None,
                                   executable_test_count=None,
                                   bundle=None,
                                   extra_artifacts=[],
                                   extra_artifacts_host=[],
                                   cts_uri=None,
                                   login_precondition_commands=[],
                                   precondition_commands=[],
                                   prerequisites=[]):
        """Run CTS/GTS with retry logic.

        We first kick off the specified module. Then rerun just the failures
        on the next MAX_RETRY iterations.
        """
        for prereq in prerequisites:
            result = tradefed_prerequisite.check(prereq, self._hosts)
            if not result[0]:
                raise error.TestError(result[1])

        # On dev and beta channels timeouts are sharp, lenient on stable.
        self._timeout = timeout
        if (self._get_release_branch_number() >=
                constants.APPROXIMATE_STABLE_BRANCH_NUMBER):
            self._timeout += 3600

        if self._should_skip_test(bundle):
            logging.warning('Skipped test %s', ' '.join(test_name))
            return

        steps = -1  # For historic reasons the first iteration is not counted.
        self.summary = ''
        accurate = []
        board = self._get_board_name()
        session_id = None

        self._setup_result_directories()
        self._prepare_media(media_asset)

        # This loop retries failures. For this reason please do not raise
        # TestFail in this loop if you suspect the failure might be fixed
        # in the next loop iteration.
        while steps < self._max_retry:
            steps += 1
            keep_media = media_asset and media_asset.uri and steps >= 1
            self._run_commands(login_precondition_commands, ignore_status=True)
            with login.login_chrome(
                    hosts=self._hosts,
                    board=board,
                    dont_override_profile=keep_media,
                    enable_default_apps=enable_default_apps) as current_logins:
                if self._should_reboot(steps):
                    # TODO(rohitbm): Evaluate if power cycle really helps with
                    # Bluetooth test failures, and then make the implementation
                    # more strict by first running complete restart and reboot
                    # retries and then perform power cycle.
                    #
                    # Currently, (steps + 1 == self._max_retry) means that
                    # hard_reboot is attempted after "this" cycle failed. Then,
                    # the last remaining 1 step will be run on the rebooted DUT.
                    hard_reboot = (self._hard_reboot_on_failure
                        and steps + 1 == self._max_retry)
                    for current_login in current_logins:
                        current_login.need_reboot(hard_reboot=hard_reboot)
                self._ready_arc()
                self._calculate_test_count_factor(bundle)
                self._run_commands(precondition_commands, ignore_status=True)

                # Run tradefed.
                if session_id == None:
                    if target_plan is not None:
                        self._install_plan(target_plan)

                    logging.info('Running %s:', test_name)
                    command = self._tradefed_run_command(run_template)
                else:
                    logging.info('Retrying failures of %s with session_id %d:',
                                 test_name, session_id)
                    command = self._tradefed_retry_command(retry_template,
                                                           session_id)

                # TODO(pwang): Evaluate if it is worth it to get the number of
                #              not-excecuted, for instance, by collecting all
                #              tests on startup (very expensive, may take 30
                #              minutes).
                # TODO(b/137917339): Only prevent screen from turning off for
                # media tests. Remove this check once the GPU issue is fixed.
                if media_asset and media_asset.uri:
                    self._override_powerd_prefs()
                try:
                    waived_tests, acc = self._run_and_parse_tradefed(command)
                finally:
                    # TODO(b/137917339): ditto
                    if media_asset and media_asset.uri:
                        self._restore_powerd_prefs()
                self._fail_on_unexpected_media_download(media_asset)
                result = self._run_tradefed_list_results()
                if not result:
                    logging.error('Did not find any test results. Retry.')
                    for current_login in current_logins:
                        current_login.need_reboot()
                    continue

                waived = len(waived_tests)
                last_session_id, passed, failed, all_done = result

                if failed > waived or not utils.is_in_container():
                    for host in self._hosts:
                        dir_name = "%s-step%02d" % (host.hostname, steps)
                        output_dir = os.path.join(
                            self.resultsdir, 'extra_artifacts', dir_name)
                        self._copy_extra_artifacts_dut(
                            extra_artifacts, host, output_dir)
                        self._copy_extra_artifacts_host(
                            extra_artifacts_host, host, output_dir)

                if passed + failed > 0:
                    # At least one test had run, which means the media push step
                    # of tradefed didn't fail. To free up the storage earlier,
                    # delete the copy on the server side. See crbug.com/970881
                    self._cleanup_media(media_asset)

                # If the result is |acc|urate according to the log, or the
                # inaccuracy is recognized by tradefed (not all_done), then
                # it is fine.
                accurate.append(acc or not all_done)
                if failed < waived:
                    logging.error(
                        'Error: Internal waiver bookkeeping has become '
                        'inconsistent (f=%d, w=%d)', failed, waived)

                msg = 'run' if session_id == None else ' retry'
                msg += '(p=%s, f=%s, w=%s)' % (passed, failed, waived)
                self.summary += msg
                logging.info('RESULT: %s %s', msg, result)

                # Check for no-test modules. We use the "all_done" indicator
                # provided by list_results to decide if there are outstanding
                # modules to iterate over (similar to missing tests just on a
                # per-module basis).
                notest = (passed + failed == 0 and all_done)
                if target_module in self._notest_modules:
                    if notest:
                        logging.info('Package has no tests as expected.')
                        return
                    else:
                        # We expected no tests, but the new bundle drop must
                        # have added some for us. Alert us to the situation.
                        raise error.TestFail(
                            'Failed: Remove module %s from '
                            'notest_modules directory!' % target_module)
                elif notest:
                    logging.error('Did not find any tests in module. Hoping '
                                  'this is transient. Retry after reboot.')
                    for current_login in current_logins:
                        current_login.need_reboot()
                    continue

                session_id = last_session_id
                if (not all_done and executable_test_count != None and
                        (passed + failed ==
                         executable_test_count * self._test_count_factor)):
                    logging.warning('Overwriting all_done as True, since the '
                                    'explicitly set executable_test_count '
                                    'tests have run.')
                    all_done = True

                # Check if all the tests passed.
                if failed <= waived and all_done:
                    break

                # TODO(b/127908450) Tradefed loses track of not-executed tests
                # when the commandline pattern included '*', and retry run for
                # them wrongly declares all tests passed. This is misleading.
                # Rather, we give up the retry and report the result as FAIL.
                if not all_done and '*' in ''.join(run_template):
                    break

        if session_id == None:
            raise error.TestFail('Error: Could not find any tests in module.')

        if failed <= waived and all_done:
            if not all(accurate):
                raise error.TestFail(
                    'Failed: Not all tests were executed. After %d '
                    'retries passing %d tests, waived=%d. %s' % (
                        steps, passed, waived, self.summary))
            # TODO(ihf): Make this error.TestPass('...') once
            # available.
            if steps > 0 and self._warn_on_test_retry:
                raise error.TestWarn(
                    'Passed: after %d retries passing %d tests, '
                    'waived=%d. %s' % (steps, passed, waived,
                                       self.summary))
            return

        raise error.TestFail(
            'Failed: after %d retries giving up. '
            'passed=%d, failed=%d, waived=%d%s%s. %s' %
            (steps, passed, failed, waived, '' if all_done else ', notexec>=1',
             '' if all(accurate) else ', Tests may not be accurate.',
             self.summary))
