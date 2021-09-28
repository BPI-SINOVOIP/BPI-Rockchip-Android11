# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Runs a command inside an NsJail sandbox for building Android.

NsJail creates a user namespace sandbox where
Android can be built in an isolated process.
If no command is provided then it will open
an interactive bash shell.
"""

import argparse
import collections
import os
import re
import subprocess
from .overlay import BindMount
from .overlay import BindOverlay

_DEFAULT_META_ANDROID_DIR = 'LINUX/android'
_DEFAULT_COMMAND = '/bin/bash'

_SOURCE_MOUNT_POINT = '/src'
_OUT_MOUNT_POINT = '/src/out'
_DIST_MOUNT_POINT = '/dist'
_META_MOUNT_POINT = '/meta'

_CHROOT_MOUNT_POINTS = [
  'bin', 'sbin',
  'etc/alternatives', 'etc/default' 'etc/perl',
  'etc/ssl', 'etc/xml',
  'lib', 'lib32', 'lib64', 'libx32',
  'usr',
]

def load_rw_whitelist(rw_whitelist_config):
  """Loads a read/write whitelist configuration file.

  The read/write whitelist configuration file is a text file that contains a
  list of source_dir relative paths which should be mounted read/write inside
  the build sandbox. Empty lines and lines begnning with a comment marker ('#')
  will be ignored. An empty whitelist implies that all source paths are mounted
  read-only. An empty rw_whitelist_config argument implies that all source
  paths are mounted read/write.

  Args:
    rw_whitelist_config: A string path to a read/write whitelist file.

  Returns:
    A set of whitelist path strings.
  """
  if not rw_whitelist_config:
    return None

  if not os.path.exists(rw_whitelist_config):
    return None

  ret = set()
  with open(rw_whitelist_config, 'r') as f:
    for p in f.read().splitlines():
      p = p.strip()
      if not p or p.startswith('#'):
        continue
      ret.add(p)

  return ret


def run(command,
        android_target,
        nsjail_bin,
        chroot,
        overlay_config=None,
        rw_whitelist_config=None,
        source_dir=os.getcwd(),
        out_dirname_for_whiteout=None,
        dist_dir=None,
        build_id=None,
        out_dir = None,
        meta_root_dir = None,
        meta_android_dir = _DEFAULT_META_ANDROID_DIR,
        mount_local_device = False,
        max_cpus=None,
        extra_bind_mounts=[],
        readonly_bind_mounts=[],
        extra_nsjail_args=[],
        dry_run=False,
        quiet=False,
        env=[],
        stdout=None,
        stderr=None):
  """Run inside an NsJail sandbox.

  Args:
    command: A list of strings with the command to run.
    android_target: A string with the name of the target to be prepared
      inside the container.
    nsjail_bin: A string with the path to the nsjail binary.
    chroot: A string with the path to the chroot.
    overlay_config: A string path to an overlay configuration file.
    rw_whitelist_config: A string path to a read/write whitelist configuration file.
    source_dir: A string with the path to the Android platform source.
    out_dirname_for_whiteout: The optional name of the folder within
      source_dir that is the Android build out folder *as seen from outside
      the Docker container*.
    dist_dir: A string with the path to the dist directory.
    build_id: A string with the build identifier.
    out_dir: An optional path to the Android build out folder.
    meta_root_dir: An optional path to a folder containing the META build.
    meta_android_dir: An optional path to the location where the META build expects
      the Android build. This path must be relative to meta_root_dir.
    mount_local_device: Whether to mount /dev/usb (and related) trees enabling
      adb to run inside the jail
    max_cpus: An integer with maximum number of CPUs.
    extra_bind_mounts: An array of extra mounts in the 'source' or 'source:dest' syntax.
    readonly_bind_mounts: An array of read only mounts in the 'source' or 'source:dest' syntax.
    extra_nsjail_args: A list of strings that contain extra arguments to nsjail.
    dry_run: If true, the command will be returned but not executed
    quiet: If true, the function will not display the command and
      will pass -quiet argument to nsjail
    env: An array of environment variables to define in the jail in the `var=val` syntax.
    stdout: the standard output for all printed messages. Valid values are None, a file
      descriptor or file object. A None value means sys.stdout is used.
    stderr: the standard error for all printed messages. Valid values are None, a file
      descriptor or file object, and subprocess.STDOUT (which indicates that all stderr
      should be redirected to stdout). A None value means sys.stderr is used.

  Returns:
    A list of strings with the command executed.
  """


  nsjail_command = get_command(
      command=command,
      android_target=android_target,
      nsjail_bin=nsjail_bin,
      chroot=chroot,
      overlay_config=overlay_config,
      rw_whitelist_config=rw_whitelist_config,
      source_dir=source_dir,
      out_dirname_for_whiteout=out_dirname_for_whiteout,
      dist_dir=dist_dir,
      build_id=build_id,
      out_dir=out_dir,
      meta_root_dir=meta_root_dir,
      meta_android_dir=meta_android_dir,
      mount_local_device=mount_local_device,
      max_cpus=max_cpus,
      extra_bind_mounts=extra_bind_mounts,
      readonly_bind_mounts=readonly_bind_mounts,
      extra_nsjail_args=extra_nsjail_args,
      quiet=quiet,
      env=env)

  run_command(
      nsjail_command=nsjail_command,
      mount_local_device=mount_local_device,
      dry_run=dry_run,
      quiet=quiet,
      stdout=stdout,
      stderr=stderr)

  return nsjail_command

def get_command(command,
        android_target,
        nsjail_bin,
        chroot,
        overlay_config=None,
        rw_whitelist_config=None,
        source_dir=os.getcwd(),
        out_dirname_for_whiteout=None,
        dist_dir=None,
        build_id=None,
        out_dir = None,
        meta_root_dir = None,
        meta_android_dir = _DEFAULT_META_ANDROID_DIR,
        mount_local_device = False,
        max_cpus=None,
        extra_bind_mounts=[],
        readonly_bind_mounts=[],
        extra_nsjail_args=[],
        quiet=False,
        env=[]):
  """Get command to run nsjail sandbox.

  Args:
    command: A list of strings with the command to run.
    android_target: A string with the name of the target to be prepared
      inside the container.
    nsjail_bin: A string with the path to the nsjail binary.
    chroot: A string with the path to the chroot.
    overlay_config: A string path to an overlay configuration file.
    rw_whitelist_config: A string path to a read/write whitelist configuration file.
    source_dir: A string with the path to the Android platform source.
    out_dirname_for_whiteout: The optional name of the folder within
      source_dir that is the Android build out folder *as seen from outside
      the Docker container*.
    dist_dir: A string with the path to the dist directory.
    build_id: A string with the build identifier.
    out_dir: An optional path to the Android build out folder.
    meta_root_dir: An optional path to a folder containing the META build.
    meta_android_dir: An optional path to the location where the META build expects
      the Android build. This path must be relative to meta_root_dir.
    max_cpus: An integer with maximum number of CPUs.
    extra_bind_mounts: An array of extra mounts in the 'source' or 'source:dest' syntax.
    readonly_bind_mounts: An array of read only mounts in the 'source' or 'source:dest' syntax.
    extra_nsjail_args: A list of strings that contain extra arguments to nsjail.
    quiet: If true, the function will not display the command and
      will pass -quiet argument to nsjail
    env: An array of environment variables to define in the jail in the `var=val` syntax.

  Returns:
    A list of strings with the command to execute.
  """
  script_dir = os.path.dirname(os.path.abspath(__file__))
  config_file = os.path.join(script_dir, 'nsjail.cfg')

  # Run expects absolute paths
  if out_dir:
    out_dir = os.path.abspath(out_dir)
  if dist_dir:
    dist_dir = os.path.abspath(dist_dir)
  if meta_root_dir:
    meta_root_dir = os.path.abspath(meta_root_dir)
  if source_dir:
    source_dir = os.path.abspath(source_dir)

  if nsjail_bin:
    nsjail_bin = os.path.join(source_dir, nsjail_bin)

  if chroot:
    chroot = os.path.join(source_dir, chroot)

  if meta_root_dir:
    if not meta_android_dir or os.path.isabs(meta_android_dir):
      raise ValueError('error: the provided meta_android_dir is not a path'
          'relative to meta_root_dir.')

  nsjail_command = [nsjail_bin,
    '--env', 'USER=nobody',
    '--config', config_file]

  # By mounting the points individually that we need we reduce exposure and
  # keep the chroot clean from artifacts
  if chroot:
    for mpoints in _CHROOT_MOUNT_POINTS:
      source = os.path.join(chroot, mpoints)
      dest = os.path.join('/', mpoints)
      if os.path.exists(source):
        nsjail_command.extend([
          '--bindmount_ro', '%s:%s' % (source, dest)
        ])

  if build_id:
    nsjail_command.extend(['--env', 'BUILD_NUMBER=%s' % build_id])
  if max_cpus:
    nsjail_command.append('--max_cpus=%i' % max_cpus)
  if quiet:
    nsjail_command.append('--quiet')

  whiteout_list = set()
  if out_dirname_for_whiteout:
    whiteout_list.add(os.path.join(source_dir, out_dirname_for_whiteout))
  if out_dir and (
      os.path.dirname(out_dir) == source_dir) and (
      os.path.basename(out_dir) != 'out'):
    whiteout_list.add(os.path.abspath(out_dir))
    if not os.path.exists(out_dir):
      os.makedirs(out_dir)

  rw_whitelist = load_rw_whitelist(rw_whitelist_config)

  # Apply the overlay for the selected Android target to the source
  # directory if an overlay configuration was provided
  if overlay_config and os.path.exists(overlay_config):
    overlay = BindOverlay(android_target,
                      source_dir,
                      overlay_config,
                      whiteout_list,
                      _SOURCE_MOUNT_POINT,
                      rw_whitelist)
    bind_mounts = overlay.GetBindMounts()
  else:
    bind_mounts = collections.OrderedDict()
    bind_mounts[_SOURCE_MOUNT_POINT] = BindMount(source_dir, False)

  if out_dir:
    bind_mounts[_OUT_MOUNT_POINT] = BindMount(out_dir, False)

  if dist_dir:
    bind_mounts[_DIST_MOUNT_POINT] = BindMount(dist_dir, False)
    nsjail_command.extend([
        '--env', 'DIST_DIR=%s'%_DIST_MOUNT_POINT
    ])

  if meta_root_dir:
    bind_mounts[_META_MOUNT_POINT] = BindMount(meta_root_dir, False)
    bind_mounts[os.path.join(_META_MOUNT_POINT, meta_android_dir)] = BindMount(source_dir, False)
    if out_dir:
      bind_mounts[os.path.join(_META_MOUNT_POINT, meta_android_dir, 'out')] = BindMount(out_dir, False)

  for bind_destination, bind_mount in bind_mounts.items():
    if bind_mount.readonly:
      nsjail_command.extend([
        '--bindmount_ro',  bind_mount.source_dir + ':' + bind_destination
      ])
    else:
      nsjail_command.extend([
        '--bindmount',  bind_mount.source_dir + ':' + bind_destination
      ])

  if mount_local_device:
    # Mount /dev/bus/usb and several /sys/... paths, which adb will examine
    # while attempting to find the attached android device. These paths expose
    # a lot of host operating system device space, so it's recommended to use
    # the mount_local_device option only when you need to use adb (e.g., for
    # atest or some other purpose).
    nsjail_command.extend(['--bindmount', '/dev/bus/usb'])
    nsjail_command.extend(['--bindmount', '/sys/bus/usb/devices'])
    nsjail_command.extend(['--bindmount', '/sys/dev'])
    nsjail_command.extend(['--bindmount', '/sys/devices'])

  for mount in extra_bind_mounts:
    nsjail_command.extend(['--bindmount', mount])
  for mount in readonly_bind_mounts:
    nsjail_command.extend(['--bindmount_ro', mount])

  for var in env:
    nsjail_command.extend(['--env', var])

  nsjail_command.extend(extra_nsjail_args)

  nsjail_command.append('--')
  nsjail_command.extend(command)

  return nsjail_command

def run_command(nsjail_command,
                mount_local_device=False,
                dry_run=False,
                quiet=False,
                stdout=None,
                stderr=None):
  """Run the provided nsjail command.

  Args:
    nsjail_command: A list of strings with the command to run.
    mount_local_device: Whether to mount /dev/usb (and related) trees enabling
      adb to run inside the jail
    dry_run: If true, the command will be returned but not executed
    quiet: If true, the function will not display the command and
      will pass -quiet argument to nsjail
    stdout: the standard output for all printed messages. Valid values are None, a file
      descriptor or file object. A None value means sys.stdout is used.
    stderr: the standard error for all printed messages. Valid values are None, a file
      descriptor or file object, and subprocess.STDOUT (which indicates that all stderr
      should be redirected to stdout). A None value means sys.stderr is used.
  """

  if mount_local_device:
    # A device can only communicate with one adb server at a time, so the adb server is
    # killed on the host machine.
    for line in subprocess.check_output(['ps','-eo','cmd']).decode().split('\n'):
      if re.match(r'adb.*fork-server.*', line):
        print('An adb server is running on your host machine. This server must be '
              'killed to use the --mount_local_device flag.')
        print('Continue? [y/N]: ', end='')
        if input().lower() != 'y':
          exit()
        subprocess.check_call(['adb', 'kill-server'])

  if not quiet:
    print('NsJail command:', file=stdout)
    print(' '.join(nsjail_command), file=stdout)

  if not dry_run:
    subprocess.check_call(nsjail_command, stdout=stdout, stderr=stderr)

def parse_args():
  """Parse command line arguments.

  Returns:
    An argparse.Namespace object.
  """

  # Use the top level module docstring for the help description
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--nsjail_bin',
      required=True,
      help='Path to NsJail binary.')
  parser.add_argument(
      '--chroot',
      help='Path to the chroot to be used for building the Android'
      'platform. This will be mounted as the root filesystem in the'
      'NsJail sandbox.')
  parser.add_argument(
      '--overlay_config',
      help='Path to the overlay configuration file.')
  parser.add_argument(
      '--rw_whitelist_config',
      help='Path to the read/write whitelist configuration file.')
  parser.add_argument(
      '--source_dir',
      default=os.getcwd(),
      help='Path to Android platform source to be mounted as /src.')
  parser.add_argument(
      '--out_dir',
      help='Full path to the Android build out folder. If not provided, uses '
      'the standard \'out\' folder in the current path.')
  parser.add_argument(
      '--meta_root_dir',
      default='',
      help='Full path to META folder. Default to \'\'')
  parser.add_argument(
      '--meta_android_dir',
      default=_DEFAULT_META_ANDROID_DIR,
      help='Relative path to the location where the META build expects '
      'the Android build. This path must be relative to meta_root_dir. '
      'Defaults to \'%s\'' % _DEFAULT_META_ANDROID_DIR)
  parser.add_argument(
      '--out_dirname_for_whiteout',
      help='The optional name of the folder within source_dir that is the '
      'Android build out folder *as seen from outside the Docker '
      'container*.')
  parser.add_argument(
      '--whiteout',
      action='append',
      default=[],
      help='Optional glob filter of directories to add to the whiteout. The '
      'directories will not appear in the container. '
      'Can be specified multiple times.')
  parser.add_argument(
      '--command',
      default=_DEFAULT_COMMAND,
      help='Command to run after entering the NsJail.'
      'If not set then an interactive Bash shell will be launched')
  parser.add_argument(
      '--android_target',
      required=True,
      help='Android target selected for building')
  parser.add_argument(
      '--dist_dir',
      help='Path to the Android dist directory. This is where'
      'Android platform release artifacts will be written.'
      'If unset then the Android platform default will be used.')
  parser.add_argument(
      '--build_id',
      help='Build identifier what will label the Android platform'
      'release artifacts.')
  parser.add_argument(
      '--max_cpus',
      type=int,
      help='Limit of concurrent CPU cores that the NsJail sandbox'
      'can use. Defaults to unlimited.')
  parser.add_argument(
      '--bindmount',
      type=str,
      default=[],
      action='append',
      help='List of mountpoints to be mounted. Can be specified multiple times. '
      'Syntax: \'source\' or \'source:dest\'')
  parser.add_argument(
      '--bindmount_ro',
      type=str,
      default=[],
      action='append',
      help='List of mountpoints to be mounted read-only. Can be specified multiple times. '
      'Syntax: \'source\' or \'source:dest\'')
  parser.add_argument(
      '--dry_run',
      action='store_true',
      help='Prints the command without executing')
  parser.add_argument(
      '--quiet', '-q',
      action='store_true',
      help='Suppress debugging output')
  parser.add_argument(
      '--mount_local_device',
      action='store_true',
      help='If provided, mount locally connected Android USB devices inside '
      'the container. WARNING: Using this flag will cause the adb server to be '
      'killed on the host machine. WARNING: Using this flag exposes parts of '
      'the host /sys/... file system. Use only when you need adb.')
  parser.add_argument(
      '--env', '-e',
      type=str,
      default=[],
      action='append',
      help='Specify an environment variable to the NSJail sandbox. Can be specified '
      'muliple times. Syntax: var_name=value')
  return parser.parse_args()

def run_with_args(args):
  """Run inside an NsJail sandbox.

  Use the arguments from an argspace namespace.

  Args:
    An argparse.Namespace object.

  Returns:
    A list of strings with the commands executed.
  """
  run(chroot=args.chroot,
      nsjail_bin=args.nsjail_bin,
      overlay_config=args.overlay_config,
      rw_whitelist_config=args.rw_whitelist_config,
      source_dir=args.source_dir,
      command=args.command.split(),
      android_target=args.android_target,
      out_dirname_for_whiteout=args.out_dirname_for_whiteout,
      dist_dir=args.dist_dir,
      build_id=args.build_id,
      out_dir=args.out_dir,
      meta_root_dir=args.meta_root_dir,
      meta_android_dir=args.meta_android_dir,
      mount_local_device=args.mount_local_device,
      max_cpus=args.max_cpus,
      extra_bind_mounts=args.bindmount,
      readonly_bind_mounts=args.bindmount_ro,
      dry_run=args.dry_run,
      quiet=args.quiet,
      env=args.env)

def main():
  run_with_args(parse_args())

if __name__ == '__main__':
  main()
