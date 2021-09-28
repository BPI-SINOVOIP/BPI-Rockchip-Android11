# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module provides some utilities used by LXC and its tools.
"""

import logging
import os
import re
import shutil
import tempfile
import unittest
from contextlib import contextmanager

import common
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import interface
from autotest_lib.client.common_lib import global_config
from autotest_lib.site_utils.lxc import constants
from autotest_lib.site_utils.lxc import unittest_setup


def path_exists(path):
    """Check if path exists.

    If the process is not running with root user, os.path.exists may fail to
    check if a path owned by root user exists. This function uses command
    `test -e` to check if path exists.

    @param path: Path to check if it exists.

    @return: True if path exists, otherwise False.
    """
    try:
        utils.run('sudo test -e "%s"' % path)
        return True
    except error.CmdError:
        return False


def get_host_ip():
    """Get the IP address of the host running containers on lxcbr*.

    This function gets the IP address on network interface lxcbr*. The
    assumption is that lxc uses the network interface started with "lxcbr".

    @return: IP address of the host running containers.
    """
    # The kernel publishes symlinks to various network devices in /sys.
    result = utils.run('ls /sys/class/net', ignore_status=True)
    # filter out empty strings
    interface_names = [x for x in result.stdout.split() if x]

    lxc_network = None
    for name in interface_names:
        if name.startswith('lxcbr'):
            lxc_network = name
            break
    if not lxc_network:
        raise error.ContainerError('Failed to find network interface used by '
                                   'lxc. All existing interfaces are: %s' %
                                   interface_names)
    netif = interface.Interface(lxc_network)
    return netif.ipv4_address

def is_vm():
    """Check if the process is running in a virtual machine.

    @return: True if the process is running in a virtual machine, otherwise
             return False.
    """
    try:
        virt = utils.run('sudo -n virt-what').stdout.strip()
        logging.debug('virt-what output: %s', virt)
        return bool(virt)
    except error.CmdError:
        logging.warn('Package virt-what is not installed, default to assume '
                     'it is not a virtual machine.')
        return False


def destroy(path, name,
            force=True, snapshots=False, ignore_status=False, timeout=-1):
  """
  Destroy an LXC container.

  @param force: Destroy even if running. Default true.
  @param snapshots: Destroy all snapshots based on the container. Default false.
  @param ignore_status: Ignore return code of command. Default false.
  @param timeout: Seconds to wait for completion. No timeout imposed if the
    value is negative. Default -1 (no timeout).

  @returns: CmdResult object from the shell command
  """
  cmd = 'sudo lxc-destroy -P %s -n %s' % (path, name)
  if force:
    cmd += ' -f'
  if snapshots:
    cmd += ' -s'
  if timeout >= 0:
    return utils.run(cmd, ignore_status=ignore_status, timeout=timeout)
  else:
    return utils.run(cmd, ignore_status=ignore_status)

def clone(lxc_path, src_name, new_path, dst_name, snapshot):
    """Clones a container.

    @param lxc_path: The LXC path of the source container.
    @param src_name: The name of the source container.
    @param new_path: The LXC path of the destination container.
    @param dst_name: The name of the destination container.
    @param snapshot: Whether or not to create a snapshot clone.
    """
    snapshot_arg = '-s' if snapshot and constants.SUPPORT_SNAPSHOT_CLONE else ''
    # overlayfs is the default clone backend storage. However it is not
    # supported in Ganeti yet. Use aufs as the alternative.
    aufs_arg = '-B aufs' if is_vm() and snapshot else ''
    cmd = (('sudo lxc-copy --lxcpath {lxcpath} --newpath {newpath} '
                    '--name {name} --newname {newname} {snapshot} {backing}')
           .format(
               lxcpath = lxc_path,
               newpath = new_path,
               name = src_name,
               newname = dst_name,
               snapshot = snapshot_arg,
               backing = aufs_arg
           ))
    utils.run(cmd)


@contextmanager
def TempDir(*args, **kwargs):
    """Context manager for creating a temporary directory."""
    tmpdir = tempfile.mkdtemp(*args, **kwargs)
    try:
        yield tmpdir
    finally:
        shutil.rmtree(tmpdir)


class BindMount(object):
    """Manages setup and cleanup of bind-mounts."""
    def __init__(self, spec):
        """Sets up a new bind mount.

        Do not call this directly, use the create or from_existing class
        methods.

        @param spec: A two-element tuple (dir, mountpoint) where dir is the
                     location of an existing directory, and mountpoint is the
                     path under that directory to the desired mount point.
        """
        self.spec = spec


    def __eq__(self, rhs):
        if isinstance(rhs, self.__class__):
            return self.spec == rhs.spec
        return NotImplemented


    def __ne__(self, rhs):
        return not (self == rhs)


    @classmethod
    def create(cls, src, dst, rename=None, readonly=False):
        """Creates a new bind mount.

        @param src: The path of the source file/dir.
        @param dst: The destination directory.  The new mount point will be
                    ${dst}/${src} unless renamed.  If the mount point does not
                    already exist, it will be created.
        @param rename: An optional path to rename the mount.  If provided, the
                       mount point will be ${dst}/${rename} instead of
                       ${dst}/${src}.
        @param readonly: If True, the mount will be read-only.  False by
                         default.

        @return An object representing the bind-mount, which can be used to
                clean it up later.
        """
        spec = (dst, (rename if rename else src).lstrip(os.path.sep))
        full_dst = os.path.join(*list(spec))

        if not path_exists(full_dst):
            utils.run('sudo mkdir -p %s' % full_dst)

        utils.run('sudo mount --bind %s %s' % (src, full_dst))
        if readonly:
            utils.run('sudo mount -o remount,ro,bind %s' % full_dst)

        return cls(spec)


    @classmethod
    def from_existing(cls, host_dir, mount_point):
        """Creates a BindMount for an existing mount point.

        @param host_dir: Path of the host dir hosting the bind-mount.
        @param mount_point: Full path to the mount point (including the host
                            dir).

        @return An object representing the bind-mount, which can be used to
                clean it up later.
        """
        spec = (host_dir, os.path.relpath(mount_point, host_dir))
        return cls(spec)


    def cleanup(self):
        """Cleans up the bind-mount.

        Unmounts the destination, and deletes it if possible. If it was mounted
        alongside important files, it will not be deleted.
        """
        full_dst = os.path.join(*list(self.spec))
        utils.run('sudo umount %s' % full_dst)
        # Ignore errors because bind mount locations are sometimes nested
        # alongside actual file content (e.g. SSPs install into
        # /usr/local/autotest so rmdir -p will fail for any mounts located in
        # /usr/local/autotest).
        utils.run('sudo bash -c "cd %s; rmdir -p --ignore-fail-on-non-empty %s"'
                  % self.spec)


def is_subdir(parent, subdir):
    """Determines whether the given subdir exists under the given parent dir.

    @param parent: The parent directory.
    @param subdir: The subdirectory.

    @return True if the subdir exists under the parent dir, False otherwise.
    """
    # Append a trailing path separator because commonprefix basically just
    # performs a prefix string comparison.
    parent = os.path.join(parent, '')
    return os.path.commonprefix([parent, subdir]) == parent


def sudo_commands(commands):
    """Takes a list of bash commands and executes them all with one invocation
    of sudo. Saves ~400 ms per command.

    @param commands: The bash commands, as strings.

    @return The return code of the sudo call.
    """

    combine = global_config.global_config.get_config_value(
        'LXC_POOL','combine_sudos', type=bool, default=False)

    if combine:
        with tempfile.NamedTemporaryFile() as temp:
            temp.write("set -e\n")
            temp.writelines([command+"\n" for command in commands])
            logging.info("Commands to run: %s", str(commands))
            return utils.run("sudo bash %s" % temp.name)
    else:
        for command in commands:
            result = utils.run("sudo %s" % command)


def get_lxc_version():
    """Gets the current version of lxc if available."""
    cmd = 'sudo lxc-info --version'
    result = utils.run(cmd)
    if result and result.exit_status == 0:
        version = re.split("[.-]", result.stdout.strip())
        if len(version) < 3:
            logging.error("LXC version is not expected format %s.",
                          result.stdout.strip())
            return None
        return_value = []
        for a in version[:3]:
            try:
                return_value.append(int(a))
            except ValueError:
                logging.error(("LXC version contains non numerical version "
                               "number %s (%s)."), a, result.stdout.strip())
                return None
        return return_value
    else:
        logging.error("Unable to determine LXC version.")
        return None

class LXCTests(unittest.TestCase):
    """Thin wrapper to call correct setup for LXC tests."""

    @classmethod
    def setUpClass(cls):
        unittest_setup.setup()
