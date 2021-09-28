#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a tryjob/tryjobs after updating the packages."""

from __future__ import print_function

import argparse
import datetime
import json
import os

from assert_not_in_chroot import VerifyOutsideChroot
from failure_modes import FailureModes
from get_llvm_hash import GetLLVMHashAndVersionFromSVNOption
from get_llvm_hash import is_svn_option
from subprocess_helpers import ChrootRunCommand
from subprocess_helpers import ExecCommandAndCaptureOutput
import update_chromeos_llvm_next_hash


def GetCommandLineArgs():
  """Parses the command line for the command line arguments.

  Returns:
    The log level to use when retrieving the LLVM hash or google3 LLVM version,
    the chroot path to use for executing chroot commands,
    a list of a package or packages to update their LLVM next hash,
    and the LLVM version to use when retrieving the LLVM hash.
  """

  # Default path to the chroot if a path is not specified.
  cros_root = os.path.expanduser('~')
  cros_root = os.path.join(cros_root, 'chromiumos')

  # Create parser and add optional command-line arguments.
  parser = argparse.ArgumentParser(
      description='Runs a tryjob if successfully updated packages\''
      '"LLVM_NEXT_HASH".')

  # Add argument for the absolute path to the file that contains information on
  # the previous tested svn version.
  parser.add_argument(
      '--last_tested',
      help='the absolute path to the file that contains the last tested '
      'svn version')

  # Add argument for other change lists that want to run alongside the tryjob
  # which has a change list of updating a package's git hash.
  parser.add_argument(
      '--extra_change_lists',
      type=int,
      nargs='+',
      help='change lists that would like to be run alongside the change list '
      'of updating the packages')

  # Add argument for custom options for the tryjob.
  parser.add_argument(
      '--options',
      required=False,
      nargs='+',
      help='options to use for the tryjob testing')

  # Add argument for builders for the tryjob.
  parser.add_argument(
      '--builders',
      required=True,
      nargs='+',
      help='builders to use for the tryjob testing')

  # Add argument for the description of the tryjob.
  parser.add_argument(
      '--description',
      required=False,
      nargs='+',
      help='the description of the tryjob')

  # Add argument for a specific chroot path.
  parser.add_argument(
      '--chroot_path',
      default=cros_root,
      help='the path to the chroot (default: %(default)s)')

  # Add argument for whether to display command contents to `stdout`.
  parser.add_argument(
      '--verbose',
      action='store_true',
      help='display contents of a command to the terminal '
      '(default: %(default)s)')

  # Add argument for the LLVM version to use.
  parser.add_argument(
      '--llvm_version',
      type=is_svn_option,
      required=True,
      help='which git hash of LLVM to find '
      '{google3, ToT, <svn_version>} '
      '(default: finds the git hash of the google3 LLVM '
      'version)')

  args_output = parser.parse_args()

  return args_output


def GetLastTestedSVNVersion(last_tested_file):
  """Gets the lasted tested svn version from the file.

  Args:
    last_tested_file: The absolute path to the file that contains the last
    tested svn version.

  Returns:
    The last tested svn version or 'None' if the file did not have a last tested
    svn version (the file exists, but failed to convert the contents to an
    integer) or the file does not exist.
  """

  if not last_tested_file:
    return None

  last_svn_version = None

  # Get the last tested svn version if the file exists.
  try:
    with open(last_tested_file) as file_obj:
      # For now, the first line contains the last tested svn version.
      return int(file_obj.read().rstrip())

  except IOError:
    pass
  except ValueError:
    pass

  return last_svn_version


def GetTryJobCommand(change_list, extra_change_lists, options, builder):
  """Constructs the 'tryjob' command.

  Args:
    change_list: The CL obtained from updating the packages.
    extra_change_lists: Extra change lists that would like to be run alongside
    the change list of updating the packages.
    options: Options to be passed into the tryjob command.
    builder: The builder to be passed into the tryjob command.

  Returns:
    The 'tryjob' command with the change list of updating the packages and
    any extra information that was passed into the command line.
  """

  tryjob_cmd = ['cros', 'tryjob', '--yes', '--json', '-g', '%d' % change_list]

  if extra_change_lists:
    for extra_cl in extra_change_lists:
      tryjob_cmd.extend(['-g', '%d' % extra_cl])

  tryjob_cmd.append(builder)

  if options:
    tryjob_cmd.extend('--%s' % option for option in options)

  return tryjob_cmd


def GetCurrentTimeInUTC():
  """Returns the current time via `datetime.datetime.utcnow()`."""
  return datetime.datetime.utcnow()


def RunTryJobs(cl_number, extra_change_lists, options, builders, chroot_path,
               verbose):
  """Runs a tryjob/tryjobs.

  Args:
    cl_number: The CL created by updating the packages.
    extra_change_lists: Any extra change lists that would run alongside the CL
    that was created by updating the packages ('cl_number').
    options: Any options to be passed into the 'tryjob' command.
    builders: All the builders to run the 'tryjob' with.
    chroot_path: The absolute path to the chroot.
    verbose: Print command contents to `stdout`.

  Returns:
    A list that contains stdout contents of each tryjob, where stdout is
    information (a hashmap) about the tryjob. The hashmap also contains stderr
    if there was an error when running a tryjob.

  Raises:
    ValueError: Failed to submit a tryjob.
  """

  # Contains the results of each tryjob. The results are retrieved from 'out'
  # which is stdout of the command executer.
  tryjob_results = []

  # For each builder passed into the command line:
  #
  # Run a tryjob with the change list number obtained from updating the
  # packages and append additional changes lists and options obtained from the
  # command line.
  for cur_builder in builders:
    tryjob_cmd = GetTryJobCommand(cl_number, extra_change_lists, options,
                                  cur_builder)

    out = ChrootRunCommand(chroot_path, tryjob_cmd, verbose=verbose)

    tryjob_launch_time = GetCurrentTimeInUTC()

    tryjob_contents = json.loads(out)

    buildbucket_id = int(tryjob_contents[0]['buildbucket_id'])

    new_tryjob = {
        'launch_time': str(tryjob_launch_time),
        'link': str(tryjob_contents[0]['url']),
        'buildbucket_id': buildbucket_id,
        'extra_cls': extra_change_lists,
        'options': options,
        'builder': [cur_builder]
    }

    tryjob_results.append(new_tryjob)

  AddTryjobLinkToCL(tryjob_results, cl_number, chroot_path)

  return tryjob_results


def AddTryjobLinkToCL(tryjobs, cl, chroot_path):
  """Adds the tryjob link(s) to the CL via `gerrit message <CL> <message>`."""

  # NOTE: Invoking `cros_sdk` does not make each tryjob link appear on its own
  # line, so invoking the `gerrit` command directly instead of using `cros_sdk`
  # to do it for us.
  #
  # FIXME: Need to figure out why `cros_sdk` does not add each tryjob link as a
  # newline.
  gerrit_abs_path = os.path.join(chroot_path, 'chromite/bin/gerrit')

  tryjob_links = ['Started the following tryjobs:']
  tryjob_links.extend(tryjob['link'] for tryjob in tryjobs)

  add_message_cmd = [
      gerrit_abs_path, 'message',
      str(cl), '\n'.join(tryjob_links)
  ]

  ExecCommandAndCaptureOutput(add_message_cmd)


def main():
  """Updates the packages' 'LLVM_NEXT_HASH' and submits tryjobs.

  Raises:
    AssertionError: The script was run inside the chroot.
  """

  VerifyOutsideChroot()

  args_output = GetCommandLineArgs()

  last_svn_version = GetLastTestedSVNVersion(args_output.last_tested)

  update_packages = [
      'sys-devel/llvm', 'sys-libs/compiler-rt', 'sys-libs/libcxx',
      'sys-libs/libcxxabi', 'sys-libs/llvm-libunwind'
  ]

  patch_metadata_file = 'PATCHES.json'

  svn_option = args_output.llvm_version

  git_hash, svn_version = GetLLVMHashAndVersionFromSVNOption(svn_option)

  # There is no need to run tryjobs when the SVN version matches the last tested
  # SVN version.
  if last_svn_version == svn_version:
    print('svn version (%d) matches the last tested svn version (%d) in %s' %
          (svn_version, last_svn_version, args_output.last_tested))
    return

  update_chromeos_llvm_next_hash.verbose = args_output.verbose

  change_list = update_chromeos_llvm_next_hash.UpdatePackages(
      update_packages, git_hash, svn_version, args_output.chroot_path,
      patch_metadata_file, FailureModes.DISABLE_PATCHES, svn_option)

  print('Successfully updated packages to %d' % svn_version)
  print('Gerrit URL: %s' % change_list.url)
  print('Change list number: %d' % change_list.cl_number)

  tryjob_results = RunTryJobs(change_list.cl_number,
                              args_output.extra_change_lists,
                              args_output.options, args_output.builders,
                              args_output.chroot_path, args_output.verbose)

  print('Tryjobs:')
  for tryjob in tryjob_results:
    print(tryjob)

  # Updated the packages and submitted tryjobs successfully, so the file will
  # contain 'svn_version' which will now become the last tested svn version.
  if args_output.last_tested:
    with open(args_output.last_tested, 'w') as file_obj:
      file_obj.write(str(svn_version))


if __name__ == '__main__':
  main()
