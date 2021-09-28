# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test which updates chameleond on the Bluetooth Peer device

This is not a test per se. This 'test' checks if the chameleond version on the
Bluetooth peer device and updates it if it below the expected value.

"""


import logging
import os
import sys
import time

from datetime import datetime

from autotest_lib.client.common_lib import error
from autotest_lib.server import test


# The current chameleond version
CUR_BT_PKG_VERSION='0.0.1'


# The following needs to be kept in sync with values chameleond code
BUNDLE='chameleond-0.0.2.tar.gz' # Name of the chamleond package
BUNDLE_DIR = 'chameleond-0.0.2'
BUNDLE_VERSION = '9999'
CHAMELEON_BOARD = 'fpga_tio'

def run_cmd(peer, cmd):
    """A wrapper around host.run()."""
    try:
        logging.info("executing command %s on peer",cmd)
        result = peer.host.run(cmd)
        logging.info("exit_status is %s", result.exit_status)
        logging.info("stdout is %s stderr is %s", result.stdout, result.stderr)
        output = result.stderr if result.stderr else result.stdout
        if result.exit_status == 0:
            return True, output
        else:
            return False, output
    except error.AutoservRunError as e:
        logging.error("Error while running cmd %s %s", cmd, e)
        return False, None


def cmp_version(v1,v2):
    """ Compare versions.

    Return -1,0,1 depending on whether v1 is less than, equal to or greater
    than v2

    """
    p1 = [int(i) for i in v1.split('.')]
    p2 = [int(i) for i in v2.split('.')]
    for i1,i2 in zip(p1,p2):
        if i1 < i2:
            return -1
        elif i1 > i2:
            return 1
        else:
            continue
    return 0


def check_version_format(v):
    """ Check whether version is in proper format"""
    try:
        parts = v.split('.')
        if len(parts) != 3:
            logging.error('version should be in format x.x.x')
            return False
        return all([i.isdigit() for i in parts])
    except Exception as e:
        logging.error('exception in check_version_format %s', str(e))
        return False


def is_update_needed(peer):
    """ Check if update is required

    Get the version of chameleond on the peer and compare to
    expected version

    @returns: True/False
    """
    try:
        version = peer.get_bt_pkg_version()
    except:
        logging.error("Getting the version failed %s",sys.exc_info())
        raise error.TestFail("Unable to get version")

    logging.info("version on chameleond on the peer device is %s", version)
    logging.info("current chameleond version is %s", CUR_BT_PKG_VERSION)

    if not check_version_format(version):
        logging.error("Version not in proper format")
        raise error.TestFail("Version not in proper format")

    if cmp_version(version, CUR_BT_PKG_VERSION) == -1:
        logging.debug("Current version is lower. Update required")
        return True
    elif cmp_version(version, CUR_BT_PKG_VERSION) == 0:
        logging.debug("Current version is equal. Update not required")
        return False
    else:
        # A lab peer with a higher version should be highlighted
        raise error.TestFail("Peer has a higher version")


def is_tmp_noexec(peer):
    """Check if /tmp partition is not excutable."""
    try:
        cmd = 'cat /proc/mounts |grep "/tmp"'
        status, output = run_cmd(peer, cmd)
        return status and 'noexec' in output
    except Exception as e:
        logging.error('Exception %s while checking status of /tmp', str(e))
        return True

def remount_tmp_exec(peer):
    """Remount /tmp partition with exec flag."""
    try:
        cmd = 'mount -o remount,exec "/tmp"'
        status, output = run_cmd(peer, cmd)
        return status
    except Exception as e:
        logging.error('Exception %s while remounting /tmp', str(e))
        return False



def perform_update(peer, arch):
    """ Update the chameleond on the peer"""

    if arch == 'CHROMEOS':
        logging.info("Check if /tmp partition is executable")
        if is_tmp_noexec(peer):
            if not remount_tmp_exec(peer):
                logging.error("unable to remount /tmp as exec")
                return False
            if is_tmp_noexec(peer):
                logging.error("/tmp partition is still noexec")
                return False
        logging.info("/tmp partition is executable")


    logging.info("copy the file over to the peer")
    try:
        current_dir = os.path.dirname(os.path.realpath(__file__))
        bundle_path = os.path.join(current_dir, BUNDLE)
        logging.debug("package location is %s", bundle_path)
        peer.host.send_file(bundle_path, '/tmp/')
    except:
        logging.error("copying the file failed %s ", sys.exc_info())
        return False

    HOST_NOW = datetime.strftime(datetime.now(), '%Y-%m-%d %H:%M:%S')
    logging.info("running make on peer")
    cmd = ('cd /tmp && rm -rf %s && tar zxf %s &&'
           'cd %s && find -exec touch -c {} \; &&'
           'make install REMOTE_INSTALL=TRUE '
           'HOST_NOW="%s" BUNDLE_VERSION=%s '
           'CHAMELEON_BOARD=%s') % (BUNDLE_DIR, BUNDLE, BUNDLE_DIR,
                                    HOST_NOW, BUNDLE_VERSION,
                                    CHAMELEON_BOARD)
    status, _ = run_cmd(peer, cmd)
    if not status:
        logging.info("make failed")
        return False

    logging.info("chameleond installed on peer")
    return True



def restart_check_chameleond(peer, arch):
    """restart chamleond and make sure it is running."""
    def _check_output(output, arch):
        logging.info("checking output '%s' on %s", output, arch)
        if arch == 'CHROMEOS':
            expected_output = 'chameleond start/running'
        else:
            expected_output = 'chameleond is running'
        return expected_output in output

    if arch == 'CHROMEOS':
        restart_cmd = 'restart chameleond'
        start_cmd = 'start chameleond'
        status_cmd = 'status chameleond'
    else:
        restart_cmd = 'sudo /etc/init.d/chameleond restart'
        start_cmd = 'sudo /etc/init.d/chameleond start'
        status_cmd = 'sudo /etc/init.d/chameleond status'

    status, _ = run_cmd(peer, restart_cmd)
    if not status:
        status, _ = run_cmd(peer, start_cmd)
        if not status:
            logging.error("restarting/starting chamleond failed")

    # Wait till chameleond initialization is complete
    time.sleep(5)

    status, output = run_cmd(peer, status_cmd)
    return status and _check_output(output, arch)



def update_peer(host):
    """Update the chameleond on peer device if requried"""

    if host.chameleon is None:
        raise error.TestError('Bluetooth Peer not present')

    peer = host.chameleon
    arch = host.chameleon.get_platform()

    if arch not in ['CHROMEOS', 'RASPI']:
        raise error.TestNAError("%s architecture not supported" % arch)

    if not is_update_needed(peer):
        raise error.TestNAError("Update not needed")

    if not perform_update(peer, arch):
        raise error.TestFail("Update failed")

    if not restart_check_chameleond(peer, arch):
        raise error.TestFail("Unable to start chameleond")

    if is_update_needed(peer):
        raise error.TestFail("Version not updated after upgrade")

    logging.info("updating chameleond succeded")



class bluetooth_PeerUpdate(test.test):
    """
    This test updates chameleond on Bluetooth peer devices

    """

    version = 1

    def run_once(self, host):
        """ Update Bluetooth peer device

        @param host: the DUT, usually a chromebook
        """

        self.host = host
        update_peer(self.host)
