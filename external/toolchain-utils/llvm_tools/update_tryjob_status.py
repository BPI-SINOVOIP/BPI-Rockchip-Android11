#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the status of a tryjob."""

from __future__ import print_function

import argparse
import enum
import json
import os
import subprocess
import sys

from assert_not_in_chroot import VerifyOutsideChroot
from subprocess_helpers import ChrootRunCommand
from test_helpers import CreateTemporaryJsonFile


class TryjobStatus(enum.Enum):
  """Values for the 'status' field of a tryjob."""

  GOOD = 'good'
  BAD = 'bad'
  PENDING = 'pending'
  SKIP = 'skip'

  # Executes the script passed into the command line (this script's exit code
  # determines the 'status' value of the tryjob).
  CUSTOM_SCRIPT = 'custom_script'

  # Uses the result returned by 'cros buildresult'.
  AUTO = 'auto'


class BuilderStatus(enum.Enum):
  """Actual values given via 'cros buildresult'."""

  PASS = 'pass'
  FAIL = 'fail'
  RUNNING = 'running'


class CustomScriptStatus(enum.Enum):
  """Exit code values of a custom script."""

  # NOTE: Not using 1 for 'bad' because the custom script can raise an
  # exception which would cause the exit code of the script to be 1, so the
  # tryjob's 'status' would be updated when there is an exception.
  #
  # Exit codes are as follows:
  #   0: 'good'
  #   124: 'bad'
  #   125: 'skip'
  GOOD = 0
  BAD = 124
  SKIP = 125


custom_script_exit_value_mapping = {
    CustomScriptStatus.GOOD.value: TryjobStatus.GOOD.value,
    CustomScriptStatus.BAD.value: TryjobStatus.BAD.value,
    CustomScriptStatus.SKIP.value: TryjobStatus.SKIP.value
}

builder_status_mapping = {
    BuilderStatus.PASS.value: TryjobStatus.GOOD.value,
    BuilderStatus.FAIL.value: TryjobStatus.BAD.value,
    BuilderStatus.RUNNING.value: TryjobStatus.PENDING.value
}


def GetCommandLineArgs():
  """Parses the command line for the command line arguments."""

  # Default absoute path to the chroot if not specified.
  cros_root = os.path.expanduser('~')
  cros_root = os.path.join(cros_root, 'chromiumos')

  # Create parser and add optional command-line arguments.
  parser = argparse.ArgumentParser(
      description='Updates the status of a tryjob.')

  # Add argument for the JSON file to use for the update of a tryjob.
  parser.add_argument(
      '--status_file',
      required=True,
      help='The absolute path to the JSON file that contains the tryjobs used '
      'for bisecting LLVM.')

  # Add argument that sets the 'status' field to that value.
  parser.add_argument(
      '--set_status',
      required=True,
      choices=[tryjob_status.value for tryjob_status in TryjobStatus],
      help='Sets the "status" field of the tryjob.')

  # Add argument that determines which revision to search for in the list of
  # tryjobs.
  parser.add_argument(
      '--revision',
      required=True,
      type=int,
      help='The revision to set its status.')

  # Add argument for a specific chroot path.
  parser.add_argument(
      '--chroot_path',
      default=cros_root,
      help='the path to the chroot (default: %(default)s)')

  # Add argument for the custom script to execute for the 'custom_script'
  # option in '--set_status'.
  parser.add_argument(
      '--custom_script',
      help='The absolute path to the custom script to execute (its exit code '
      'should be %d for "good", %d for "bad", or %d for "skip")' %
      (CustomScriptStatus.GOOD.value, CustomScriptStatus.BAD.value,
       CustomScriptStatus.SKIP.value))

  args_output = parser.parse_args()

  if not os.path.isfile(args_output.status_file) or \
      not args_output.status_file.endswith('.json'):
    raise ValueError('File does not exist or does not ending in ".json" '
                     ': %s' % args_output.status_file)

  if args_output.set_status == TryjobStatus.CUSTOM_SCRIPT.value and \
      not args_output.custom_script:
    raise ValueError('Please provide the absolute path to the script to '
                     'execute.')

  return args_output


def FindTryjobIndex(revision, tryjobs_list):
  """Searches the list of tryjob dictionaries to find 'revision'.

  Uses the key 'rev' for each dictionary and compares the value against
  'revision.'

  Args:
    revision: The revision to search for in the tryjobs.
    tryjobs_list: A list of tryjob dictionaries of the format:
      {
        'rev' : [REVISION],
        'url' : [URL_OF_CL],
        'cl' : [CL_NUMBER],
        'link' : [TRYJOB_LINK],
        'status' : [TRYJOB_STATUS],
        'buildbucket_id': [BUILDBUCKET_ID]
      }

  Returns:
    The index within the list or None to indicate it was not found.
  """

  for cur_index, cur_tryjob_dict in enumerate(tryjobs_list):
    if cur_tryjob_dict['rev'] == revision:
      return cur_index

  return None


def GetStatusFromCrosBuildResult(chroot_path, buildbucket_id):
  """Retrieves the 'status' using 'cros buildresult'."""

  get_buildbucket_id_cmd = [
      'cros', 'buildresult', '--buildbucket-id',
      str(buildbucket_id), '--report', 'json'
  ]

  tryjob_json = ChrootRunCommand(chroot_path, get_buildbucket_id_cmd)

  tryjob_contents = json.loads(tryjob_json)

  return str(tryjob_contents['%d' % buildbucket_id]['status'])


def GetAutoResult(chroot_path, buildbucket_id):
  """Returns the conversion of the result of 'cros buildresult'."""

  # Calls 'cros buildresult' to get the status of the tryjob.
  build_result = GetStatusFromCrosBuildResult(chroot_path, buildbucket_id)

  # The string returned by 'cros buildresult' might not be in the mapping.
  if build_result not in builder_status_mapping:
    raise ValueError(
        '"cros buildresult" return value is invalid: %s' % build_result)

  return builder_status_mapping[build_result]


def GetCustomScriptResult(custom_script, status_file, tryjob_contents):
  """Returns the conversion of the exit code of the custom script.

  Args:
    custom_script: Absolute path to the script to be executed.
    status_file: Absolute path to the file that contains information about the
    bisection of LLVM.
    tryjob_contents: A dictionary of the contents of the tryjob (e.g. 'status',
    'url', 'link', 'buildbucket_id', etc.).

  Returns:
    The exit code conversion to either return 'good', 'bad', or 'skip'.

  Raises:
    ValueError: The custom script failed to provide the correct exit code.
  """

  # Create a temporary file to write the contents of the tryjob at index
  # 'tryjob_index' (the temporary file path will be passed into the custom
  # script as a command line argument).
  with CreateTemporaryJsonFile() as temp_json_file:
    with open(temp_json_file, 'w') as tryjob_file:
      json.dump(tryjob_contents, tryjob_file, indent=4, separators=(',', ': '))

    exec_script_cmd = [custom_script, temp_json_file]

    # Execute the custom script to get the exit code.
    exec_script_cmd_obj = subprocess.Popen(
        exec_script_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    _, stderr = exec_script_cmd_obj.communicate()

    # Invalid exit code by the custom script.
    if exec_script_cmd_obj.returncode not in custom_script_exit_value_mapping:
      # Save the .JSON file to the directory of 'status_file'.
      name_of_json_file = os.path.join(
          os.path.dirname(status_file), os.path.basename(temp_json_file))

      os.rename(temp_json_file, name_of_json_file)

      raise ValueError(
          'Custom script %s exit code %d did not match '
          'any of the expected exit codes: %d for "good", %d '
          'for "bad", or %d for "skip".\nPlease check %s for information '
          'about the tryjob: %s' %
          (custom_script, exec_script_cmd_obj.returncode,
           CustomScriptStatus.GOOD.value, CustomScriptStatus.BAD.value,
           CustomScriptStatus.SKIP.value, name_of_json_file, stderr))

  return custom_script_exit_value_mapping[exec_script_cmd_obj.returncode]


def UpdateTryjobStatus(revision, set_status, status_file, chroot_path,
                       custom_script):
  """Updates a tryjob's 'status' field based off of 'set_status'.

  Args:
    revision: The revision associated with the tryjob.
    set_status: What to update the 'status' field to.
      Ex: TryjobStatus.Good, TryjobStatus.BAD, TryjobStatus.PENDING, or
      TryjobStatus.AUTO where TryjobStatus.AUTO uses the result of
      'cros buildresult'.
    status_file: The .JSON file that contains the tryjobs.
    chroot_path: The absolute path to the chroot (used by 'cros buildresult').
    custom_script: The absolute path to a script that will be executed which
    will determine the 'status' value of the tryjob.
  """

  # Format of 'bisect_contents':
  # {
  #   'start': [START_REVISION_OF_BISECTION]
  #   'end': [END_REVISION_OF_BISECTION]
  #   'jobs' : [
  #       {[TRYJOB_INFORMATION]},
  #       {[TRYJOB_INFORMATION]},
  #       ...,
  #       {[TRYJOB_INFORMATION]}
  #   ]
  # }
  with open(status_file) as tryjobs:
    bisect_contents = json.load(tryjobs)

  if not bisect_contents['jobs']:
    sys.exit('No tryjobs in %s' % status_file)

  tryjob_index = FindTryjobIndex(revision, bisect_contents['jobs'])

  # 'FindTryjobIndex()' returns None if the revision was not found.
  if tryjob_index is None:
    raise ValueError(
        'Unable to find tryjob for %d in %s' % (revision, status_file))

  # Set 'status' depending on 'set_status' for the tryjob.
  if set_status == TryjobStatus.GOOD:
    bisect_contents['jobs'][tryjob_index]['status'] = TryjobStatus.GOOD.value
  elif set_status == TryjobStatus.BAD:
    bisect_contents['jobs'][tryjob_index]['status'] = TryjobStatus.BAD.value
  elif set_status == TryjobStatus.PENDING:
    bisect_contents['jobs'][tryjob_index]['status'] = TryjobStatus.PENDING.value
  elif set_status == TryjobStatus.AUTO:
    bisect_contents['jobs'][tryjob_index]['status'] = GetAutoResult(
        chroot_path, bisect_contents['jobs'][tryjob_index]['buildbucket_id'])
  elif set_status == TryjobStatus.SKIP:
    bisect_contents['jobs'][tryjob_index]['status'] = TryjobStatus.SKIP.value
  elif set_status == TryjobStatus.CUSTOM_SCRIPT:
    bisect_contents['jobs'][tryjob_index]['status'] = GetCustomScriptResult(
        custom_script, status_file, bisect_contents['jobs'][tryjob_index])
  else:
    raise ValueError('Invalid "set_status" option provided: %s' % set_status)

  with open(status_file, 'w') as update_tryjobs:
    json.dump(bisect_contents, update_tryjobs, indent=4, separators=(',', ': '))


def main():
  """Updates the status of a tryjob."""

  VerifyOutsideChroot()

  args_output = GetCommandLineArgs()

  UpdateTryjobStatus(args_output.revision, TryjobStatus(args_output.set_status),
                     args_output.status_file, args_output.chroot_path,
                     args_output.custom_script)


if __name__ == '__main__':
  main()
