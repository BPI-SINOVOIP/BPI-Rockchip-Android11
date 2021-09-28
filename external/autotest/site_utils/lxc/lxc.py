# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import tempfile

import common
from autotest_lib.client.bin import utils as common_utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.client.common_lib.cros import retry
from autotest_lib.server import utils as server_utils
from autotest_lib.site_utils.lxc import constants

try:
    from chromite.lib import metrics
except ImportError:
    metrics = common_utils.metrics_mock


def get_container_info(container_path, **filters):
    """Get a collection of container information in the given container path.

    This method parse the output of lxc-ls to get a list of container
    information. The lxc-ls command output looks like:
    NAME      STATE    IPV4       IPV6  AUTOSTART  PID   MEMORY  RAM     SWAP
    --------------------------------------------------------------------------
    base      STOPPED  -          -     NO         -     -       -       -
    test_123  RUNNING  10.0.3.27  -     NO         8359  6.28MB  6.28MB  0.0MB

    @param container_path: Path to look for containers.
    @param filters: Key value to filter the containers, e.g., name='base'

    @return: A list of dictionaries that each dictionary has the information of
             a container. The keys are defined in ATTRIBUTES.
    """
    cmd = 'sudo lxc-ls -P %s -f -F %s' % (os.path.realpath(container_path),
                                          ','.join(constants.ATTRIBUTES))
    output = common_utils.run(cmd).stdout
    info_collection = []

    for line in output.splitlines()[1:]:
        # Only LXC 1.x has the second line of '-' as a separator.
        if line.startswith('------'):
            continue
        info_collection.append(dict(zip(constants.ATTRIBUTES, line.split())))
    if filters:
        filtered_collection = []
        for key, value in filters.iteritems():
            for info in info_collection:
                if key in info and info[key] == value:
                    filtered_collection.append(info)
        info_collection = filtered_collection
    return info_collection


def download_extract(url, target, extract_dir):
    """Download the file from given url and save it to the target, then extract.

    @param url: Url to download the file.
    @param target: Path of the file to save to.
    @param extract_dir: Directory to extract the content of the file to.
    """
    remote_url = dev_server.DevServer.get_server_url(url)
    # This can be run in multiple threads, pick a unique tmp_file.name.
    with tempfile.NamedTemporaryFile(prefix=os.path.basename(target) + '_',
                                     delete=False) as tmp_file:
        if remote_url in dev_server.ImageServerBase.servers():
            # TODO(xixuan): Better to only ssh to devservers in lab, and
            # continue using curl for ganeti devservers.
            _download_via_devserver(url, tmp_file.name)
        else:
            _download_via_curl(url, tmp_file.name)
        common_utils.run('sudo mv %s %s' % (tmp_file.name, target))
    common_utils.run('sudo tar -xvf %s -C %s' % (target, extract_dir))


# Make sure retries only happen in the non-timeout case.
@retry.retry((error.CmdError),
             blacklist=[error.CmdTimeoutError],
             timeout_min=3*2,
             delay_sec=10)
def _download_via_curl(url, target_file_path):
    # We do not want to retry on CmdTimeoutError but still retry on
    # CmdError. Hence we can't use curl --timeout=...
    common_utils.run('sudo curl -s %s -o %s' % (url, target_file_path),
                     stderr_tee=common_utils.TEE_TO_LOGS, timeout=3*60)


# Make sure retries only happen in the non-timeout case.
@retry.retry((error.CmdError),
             blacklist=[error.CmdTimeoutError],
             timeout_min=(constants.DEVSERVER_CALL_TIMEOUT *
                          constants.DEVSERVER_CALL_RETRY / 60),
             delay_sec=constants.DEVSERVER_CALL_DELAY)
def _download_via_devserver(url, target_file_path):
    dev_server.ImageServerBase.download_file(
            url, target_file_path, timeout=constants.DEVSERVER_CALL_TIMEOUT)


def _install_package_precheck(packages):
    """If SSP is not enabled or the test is running in chroot (using test_that),
    packages installation should be skipped.

    The check does not raise exception so tests started by test_that or running
    in an Autotest setup with SSP disabled can continue. That assume the running
    environment, chroot or a machine, has the desired packages installed
    already.

    @param packages: A list of names of the packages to install.

    @return: True if package installation can continue. False if it should be
             skipped.

    """
    if not constants.SSP_ENABLED and not common_utils.is_in_container():
        logging.info('Server-side packaging is not enabled. Install package %s '
                     'is skipped.', packages)
        return False

    if server_utils.is_inside_chroot():
        logging.info('Test is running inside chroot. Install package %s is '
                     'skipped.', packages)
        return False

    if not common_utils.is_in_container():
        raise error.ContainerError('Package installation is only supported '
                                   'when test is running inside container.')

    return True



def _remove_banned_packages(packages, banned_packages):
    """Filter out packages.

    @param packages: A set of packages names that have been requested.
    @param items: A list of package names that are not to be installed.

    @return: A sanatized set of packages names to install.
    """
    return {package for package in packages if package not in banned_packages}


def _ensure_pip(target_setting):
    """ Ensure pip is installed, if not install it.

    @param target_setting: target command param specifying the path to where
                           python packages should be installed.
    """
    try:
        import pip
    except ImportError:
        common_utils.run(
            'wget https://bootstrap.pypa.io/get-pip.py -O /tmp/get-pip.py')
        common_utils.run('python /tmp/get-pip.py %s' % target_setting)


@metrics.SecondsTimerDecorator(
    '%s/install_packages_duration' % constants.STATS_KEY)
@retry.retry(error.CmdError, timeout_min=30)
def install_packages(packages=[], python_packages=[], force_latest=False):
    """Install the given package inside container.

    !!! WARNING !!!
    This call may introduce several minutes of delay in test run. The best way
    to avoid such delay is to update the base container used for the test run.
    File a bug for infra deputy to update the base container with the new
    package a test requires.

    @param packages: A list of names of the packages to install.
    @param python_packages: A list of names of the python packages to install
                            using pip.
    @param force_latest: True to force to install the latest version of the
                         package. Default to False, which means skip installing
                         the package if it's installed already, even with an old
                         version.

    @raise error.ContainerError: If package is attempted to be installed outside
                                 a container.
    @raise error.CmdError: If the package doesn't exist or failed to install.

    """
    if not _install_package_precheck(packages or python_packages):
        return

    # If force_latest is False, only install packages that are not already
    # installed.
    if not force_latest:
        packages = [p for p in packages
                    if not common_utils.is_package_installed(p)]
        python_packages = [p for p in python_packages
                           if not common_utils.is_python_package_installed(p)]
        if not packages and not python_packages:
            logging.debug('All packages are installed already, skip reinstall.')
            return

    # Always run apt-get update before installing any container. The base
    # container may have outdated cache.
    common_utils.run('sudo apt-get update')

    # Make sure the lists are not None for iteration.
    packages = [] if not packages else packages
    # Remove duplicates.
    packages = set(packages)

    # Ubuntu distribution of pip is very old, do not use it as it causes
    # segmentation faults.  Some tests request these packages, ensure they
    # are not installed.
    packages = _remove_banned_packages(packages, ['python-pip', 'python-dev'])

    if packages:
        common_utils.run(
            'sudo DEBIAN_FRONTEND=noninteractive apt-get install %s -y '
            '--force-yes' % ' '.join(packages))
        logging.debug('Packages are installed: %s.', packages)

    target_setting = ''
    # For containers running in Moblab, /usr/local/lib/python2.7/dist-packages/
    # is a readonly mount from the host. Therefore, new python modules have to
    # be installed in /usr/lib/python2.7/dist-packages/
    # Containers created in Moblab does not have autotest/site-packages folder.
    if not os.path.exists('/usr/local/autotest/site-packages'):
        target_setting = '--target="/usr/lib/python2.7/dist-packages/"'
    # Pip should be installed in the base container, if not install it.
    if python_packages:
        _ensure_pip(target_setting)
        common_utils.run('python -m pip install pip --upgrade')
        common_utils.run('python -m pip install %s %s' % (target_setting,
                                              ' '.join(python_packages)))
        logging.debug('Python packages are installed: %s.', python_packages)


@retry.retry(error.CmdError, timeout_min=20)
def install_package(package):
    """Install the given package inside container.

    This function is kept for backwards compatibility reason. New code should
    use function install_packages for better performance.

    @param package: Name of the package to install.

    @raise error.ContainerError: If package is attempted to be installed outside
                                 a container.
    @raise error.CmdError: If the package doesn't exist or failed to install.

    """
    logging.warn('This function is obsoleted, please use install_packages '
                 'instead.')
    install_packages(packages=[package])


@retry.retry(error.CmdError, timeout_min=20)
def install_python_package(package):
    """Install the given python package inside container using pip.

    This function is kept for backwards compatibility reason. New code should
    use function install_packages for better performance.

    @param package: Name of the python package to install.

    @raise error.CmdError: If the package doesn't exist or failed to install.
    """
    logging.warn('This function is obsoleted, please use install_packages '
                 'instead.')
    install_packages(python_packages=[package])
