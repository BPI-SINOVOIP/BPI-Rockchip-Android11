#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the status of all tryjobs to the result of `cros buildresult`."""

from __future__ import print_function

import argparse
import json
import os

from assert_not_in_chroot import VerifyOutsideChroot
from update_tryjob_status import GetAutoResult
from update_tryjob_status import TryjobStatus


def GetPathToUpdateAllTryjobsWithAutoScript():
  """Returns the absolute path to this script."""

  return os.path.abspath(__file__)


def GetCommandLineArgs():
  """Parses the command line for the command line arguments."""

  # Default absoute path to the chroot if not specified.
  cros_root = os.path.expanduser('~')
  cros_root = os.path.join(cros_root, 'chromiumos')

  # Create parser and add optional command-line arguments.
  parser = argparse.ArgumentParser(description=__doc__)

  # Add argument for the JSON file to use for the update of a tryjob.
  parser.add_argument(
      '--last_tested',
      required=True,
      help='The absolute path to the JSON file that contains the tryjobs used '
      'for bisecting LLVM.')

  # Add argument for a specific chroot path.
  parser.add_argument(
      '--chroot_path',
      default=cros_root,
      help='the path to the chroot (default: %(default)s)')

  args_output = parser.parse_args()

  if not os.path.isfile(args_output.last_tested) or \
      not args_output.last_tested.endswith('.json'):
    raise ValueError('File does not exist or does not ending in ".json" '
                     ': %s' % args_output.last_tested)

  return args_output


def main():
  """Updates the status of a tryjob."""

  VerifyOutsideChroot()

  args_output = GetCommandLineArgs()

  with open(args_output.last_tested) as tryjobs:
    bisect_contents = json.load(tryjobs)

  for tryjob in bisect_contents['jobs']:
    if tryjob['status'] == TryjobStatus.PENDING.value:
      tryjob['status'] = GetAutoResult(args_output.chroot_path,
                                       tryjob['buildbucket_id'])

  new_file = '%s.new' % args_output.last_tested
  with open(new_file, 'w') as update_tryjobs:
    json.dump(bisect_contents, update_tryjobs, indent=4, separators=(',', ': '))
  os.rename(new_file, args_output.last_tested)


if __name__ == '__main__':
  main()
