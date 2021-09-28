#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate board-specific scripts for Go compiler testing."""

from __future__ import print_function

import argparse
import getpass
import os
import sys

from cros_utils import command_executer

SUCCESS = 0
DEBUG = False

ARCH_DATA = {'x86_64': 'amd64', 'arm32': 'arm', 'arm64': 'arm64'}

CROS_TOOLCHAIN_DATA = {
    'x86_64': 'x86_64-cros-linux-gnu',
    'arm32': 'armv7a-cros-linux-gnueabihf',
    'arm64': 'aarch64-cros-linux-gnu'
}

GLIBC_DATA = {'x86_64': 'glibc', 'arm32': 'glibc32', 'arm64': 'glibc'}

CONFIG_TEMPLATE = """
Host %s
    HostName %s
    User root
    UserKnownHostsFile /dev/null
    BatchMode yes
    CheckHostIP no
    StrictHostKeyChecking no
    IdentityFile %%d/.ssh/testing_rsa
"""

BASE_TEMPLATE = """#!/bin/bash

# Invoke the Go cross compiler for %s.
# Uses ../go_target to add PIE flags.
#
# This is just an example for an %s board.

GOOS="linux" GOARCH="%s" CGO_ENABLED="1" \\
        GOROOT="/usr/lib/go/%s" \\
        CC="%s-clang" \\
        CXX="%s-clang++" \\
        exec go_target "$@"
"""

EXEC_TEMPLATE = """#!/bin/bash

# Copy and remotely execute a binary on the %s device.
#
# For this to work, the corresponding entry must exist in
# ~/.ssh/config and the device must already be setup for
# password-less authentication. See setup instructions at
# http://go/chromeos-toolchain-team/go-toolchain

GOOS="linux" GOARCH="%s" \\
      GOLOADER="/tmp/%s/ld.so" \\
      exec go_target_exec %s "$@"
"""


def log(msg):

  if DEBUG:
    print(msg)


def WriteFile(file_content, file_name):
  with open(file_name, 'w', encoding='utf-8') as out_file:
    out_file.write(file_content)


def GenerateGoHelperScripts(ce, arm_board, x86_board, chromeos_root):
  keys = ['x86_64', 'arm32', 'arm64']
  names = {
      'x86_64': x86_board,
      'arm64': arm_board,
      'arm32': ('%s32' % arm_board)
  }

  toolchain_dir = os.path.join(chromeos_root, 'src', 'third_party',
                               'toolchain-utils', 'go', 'chromeos')
  for k in keys:
    name = names[k]
    arch = ARCH_DATA[k]
    toolchain = CROS_TOOLCHAIN_DATA[k]
    glibc = GLIBC_DATA[k]

    base_file = os.path.join(toolchain_dir, ('go_%s' % name))
    base_file_content = BASE_TEMPLATE % (name, arch, arch, toolchain, toolchain,
                                         toolchain)
    WriteFile(base_file_content, base_file)
    cmd = 'chmod 755 %s' % base_file
    ce.RunCommand(cmd)

    exec_file = os.path.join(toolchain_dir, ('go_%s_exec' % name))
    exec_file_content = EXEC_TEMPLATE % (name, arch, glibc, name)
    WriteFile(exec_file_content, exec_file)
    cmd = 'chmod 755 %s' % exec_file
    ce.RunCommand(cmd)

  return 0


def UpdateChrootSshConfig(ce, arm_board, arm_dut, x86_board, x86_dut,
                          chromeos_root):
  log('Entering UpdateChrootSshConfig')
  # Copy testing_rsa to .ssh and set file protections properly.
  user = getpass.getuser()
  ssh_dir = os.path.join(chromeos_root, 'chroot', 'home', user, '.ssh')
  dest_file = os.path.join(ssh_dir, 'testing_rsa')
  src_file = os.path.join(chromeos_root, 'src', 'scripts',
                          'mod_for_test_scripts', 'testing_rsa')
  if not os.path.exists(dest_file):
    if os.path.exists(src_file):
      cmd = 'cp %s %s' % (src_file, dest_file)
      ret = ce.RunCommand(cmd)
      if ret != SUCCESS:
        print('Error executing "%s". Exiting now...' % cmd)
        sys.exit(1)
      cmd = 'chmod 600 %s' % dest_file
      ret = ce.RunCommand(cmd)
      if ret != SUCCESS:
        print('Error executing %s; may need to re-run this manually.' % cmd)
    else:
      print('Cannot find %s; you will need to update testing_rsa by hand.' %
            src_file)
  else:
    log('testing_rsa exists already.')

  # Save ~/.ssh/config file, if not already done.
  config_file = os.path.expanduser('~/.ssh/config')
  saved_config_file = os.path.join(
      os.path.expanduser('~/.ssh'), 'config.save.go-scripts')
  if not os.path.exists(saved_config_file):
    cmd = 'cp %s %s' % (config_file, saved_config_file)
    ret = ce.RunCommand(cmd)
    if ret != SUCCESS:
      print('Error making save copy of ~/.ssh/config. Exiting...')
      sys.exit(1)

  # Update ~/.ssh/config file
  log('Reading ssh config file')
  with open(config_file, 'r') as input_file:
    config_lines = input_file.read()

  x86_host_config = CONFIG_TEMPLATE % (x86_board, x86_dut)
  arm_names = '%s %s32' % (arm_board, arm_board)
  arm_host_config = CONFIG_TEMPLATE % (arm_names, arm_dut)

  config_lines += x86_host_config
  config_lines += arm_host_config

  log('Writing ~/.ssh/config')
  WriteFile(config_lines, config_file)

  return 0


def CleanUp(ce, x86_board, arm_board, chromeos_root):
  # Find and remove go helper scripts
  keys = ['x86_64', 'arm32', 'arm64']
  names = {
      'x86_64': x86_board,
      'arm64': arm_board,
      'arm32': ('%s32' % arm_board)
  }

  toolchain_dir = os.path.join(chromeos_root, 'src', 'third_party',
                               'toolchain-utils', 'go', 'chromeos')
  for k in keys:
    name = names[k]
    base_file = os.path.join(toolchain_dir, ('go_%s' % name))
    exec_file = os.path.join(toolchain_dir, ('go_%s_exec' % name))
    cmd = ('rm -f %s; rm -f %s' % (base_file, exec_file))
    ce.RunCommand(cmd)

  # Restore saved config_file
  config_file = os.path.expanduser('~/.ssh/config')
  saved_config_file = os.path.join(
      os.path.expanduser('~/.ssh'), 'config.save.go-scripts')
  if not os.path.exists(saved_config_file):
    print('Could not find file: %s; unable to restore ~/.ssh/config .' %
          saved_config_file)
  else:
    cmd = 'mv %s %s' % (saved_config_file, config_file)
    ce.RunCommand(cmd)

  return 0


def Main(argv):
  # pylint: disable=global-statement
  global DEBUG

  parser = argparse.ArgumentParser()
  parser.add_argument('-a', '--arm64_board', dest='arm_board', required=True)
  parser.add_argument(
      '-b', '--x86_64_board', dest='x86_64_board', required=True)
  parser.add_argument(
      '-c', '--chromeos_root', dest='chromeos_root', required=True)
  parser.add_argument('-x', '--x86_64_dut', dest='x86_64_dut', required=True)
  parser.add_argument('-y', '--arm64_dut', dest='arm_dut', required=True)
  parser.add_argument(
      '-z', '--cleanup', dest='cleanup', default=False, action='store_true')
  parser.add_argument(
      '-v', '--verbose', dest='verbose', default=False, action='store_true')

  options = parser.parse_args(argv[1:])

  if options.verbose:
    DEBUG = True

  if not os.path.exists(options.chromeos_root):
    print('Invalid ChromeOS Root: %s' % options.chromeos_root)

  ce = command_executer.GetCommandExecuter()
  all_good = True
  for m in (options.x86_64_dut, options.arm_dut):
    cmd = 'ping -c 3 %s > /dev/null' % m
    ret = ce.RunCommand(cmd)
    if ret != SUCCESS:
      print('Machine %s is currently not responding to ping.' % m)
      all_good = False

  if not all_good:
    return 1

  if not options.cleanup:
    UpdateChrootSshConfig(ce, options.arm_board, options.arm_dut,
                          options.x86_64_board, options.x86_64_dut,
                          options.chromeos_root)
    GenerateGoHelperScripts(ce, options.arm_board, options.x86_64_board,
                            options.chromeos_root)
  else:
    CleanUp(ce, options.x86_64_board, options.arm_board, options.chromeos_root)

  return 0


if __name__ == '__main__':
  val = Main(sys.argv)
  sys.exit(val)
