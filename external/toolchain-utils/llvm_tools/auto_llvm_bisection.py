#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performs bisection on LLVM based off a .JSON file."""

from __future__ import print_function

import os
import subprocess
import sys
import time
import traceback

from assert_not_in_chroot import VerifyOutsideChroot
from update_all_tryjobs_with_auto import GetPathToUpdateAllTryjobsWithAutoScript
from llvm_bisection import BisectionExitStatus
import llvm_bisection

# Used to re-try for 'llvm_bisection.py' to attempt to launch more tryjobs.
BISECTION_RETRY_TIME_SECS = 10 * 60

# Wait time to then poll each tryjob whose 'status' value is 'pending'.
POLL_RETRY_TIME_SECS = 30 * 60

# The number of attempts for 'llvm_bisection.py' to launch more tryjobs.
#
# It is reset (break out of the `for` loop/ exit the program) if successfully
# launched more tryjobs or bisection is finished (no more revisions between
# start and end of the bisection).
BISECTION_ATTEMPTS = 3

# The limit for updating all tryjobs whose 'status' is 'pending'.
#
# If the time that has passed for polling exceeds this value, then the program
# will exit with the appropriate exit code.
POLLING_LIMIT_SECS = 18 * 60 * 60


def main():
  """Bisects LLVM using the result of `cros buildresult` of each tryjob.

  Raises:
    AssertionError: The script was run inside the chroot.
  """

  VerifyOutsideChroot()

  args_output = llvm_bisection.GetCommandLineArgs()

  exec_update_tryjobs = [
      GetPathToUpdateAllTryjobsWithAutoScript(), '--chroot_path',
      args_output.chroot_path, '--last_tested', args_output.last_tested
  ]

  if os.path.isfile(args_output.last_tested):
    print('Resuming bisection for %s' % args_output.last_tested)
  else:
    print('Starting a new bisection for %s' % args_output.last_tested)

  while True:
    if os.path.isfile(args_output.last_tested):
      update_start_time = time.time()

      # Update all tryjobs whose status is 'pending' to the result of `cros
      # buildresult`.
      while True:
        print('\nAttempting to update all tryjobs whose "status" is '
              '"pending":')
        print('-' * 40)

        update_ret = subprocess.call(exec_update_tryjobs)

        print('-' * 40)

        # Successfully updated all tryjobs whose 'status' was 'pending'/ no
        # updates were needed (all tryjobs already have been updated).
        if update_ret == 0:
          break

        delta_time = time.time() - update_start_time

        if delta_time > POLLING_LIMIT_SECS:
          print('Unable to update tryjobs whose status is "pending" to '
                'the result of `cros buildresult`.')

          # Something is wrong with updating the tryjobs's 'status' via
          # `cros buildresult` (e.g. network issue, etc.).
          sys.exit(1)

        print('Sleeping for %d minutes.' % (POLL_RETRY_TIME_SECS // 60))
        time.sleep(POLL_RETRY_TIME_SECS)

    # Launch more tryjobs if possible to narrow down the bad commit/revision or
    # terminate the bisection because the bad commit/revision was found.
    for cur_try in range(1, BISECTION_ATTEMPTS + 1):
      try:
        print('\nAttempting to launch more tryjobs if possible:')
        print('-' * 40)

        bisection_ret = llvm_bisection.main(args_output)

        print('-' * 40)

        # Exit code 126 means that there are no more revisions to test between
        # 'start' and 'end', so bisection is complete.
        if bisection_ret == BisectionExitStatus.BISECTION_COMPLETE.value:
          sys.exit(0)

        # Successfully launched more tryjobs.
        break
      except Exception:
        traceback.print_exc()

        print('-' * 40)

        # Exceeded the number of times to launch more tryjobs.
        if cur_try == BISECTION_ATTEMPTS:
          print('Unable to continue bisection.')

          sys.exit(1)

        num_retries_left = BISECTION_ATTEMPTS - cur_try

        print('Retries left to continue bisection %d.' % num_retries_left)

        print('Sleeping for %d minutes.' % (BISECTION_RETRY_TIME_SECS // 60))
        time.sleep(BISECTION_RETRY_TIME_SECS)


if __name__ == '__main__':
  main()
