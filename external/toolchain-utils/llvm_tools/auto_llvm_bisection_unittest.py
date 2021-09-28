#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for auto bisection of LLVM."""

from __future__ import print_function

import os
import subprocess
import time
import traceback
import unittest
import unittest.mock as mock

from test_helpers import ArgsOutputTest
from test_helpers import CallCountsToMockFunctions
import auto_llvm_bisection
import llvm_bisection


class AutoLLVMBisectionTest(unittest.TestCase):
  """Unittests for auto bisection of LLVM."""

  # Simulate the behavior of `VerifyOutsideChroot()` when successfully invoking
  # the script outside of the chroot.
  @mock.patch.object(
      auto_llvm_bisection, 'VerifyOutsideChroot', return_value=True)
  # Simulate behavior of `time.sleep()` when waiting for errors to settle caused
  # by `llvm_bisection.main()` (e.g. network issue, etc.).
  @mock.patch.object(time, 'sleep')
  # Simulate behavior of `traceback.print_exc()` when an exception happened in
  # `llvm_bisection.main()`.
  @mock.patch.object(traceback, 'print_exc')
  # Simulate behavior of `llvm_bisection.main()` when failed to launch tryjobs
  # (exception happened along the way, etc.).
  @mock.patch.object(llvm_bisection, 'main')
  # Simulate behavior of `os.path.isfile()` when starting a new bisection.
  @mock.patch.object(os.path, 'isfile', return_value=False)
  # Simulate behavior of `GetPathToUpdateAllTryjobsWithAutoScript()` when
  # returning the absolute path to that script that updates all 'pending'
  # tryjobs to the result of `cros buildresult`.
  @mock.patch.object(
      auto_llvm_bisection,
      'GetPathToUpdateAllTryjobsWithAutoScript',
      return_value='/abs/path/to/update_tryjob.py')
  # Simulate `llvm_bisection.GetCommandLineArgs()` when parsing the command line
  # arguments required by the bisection script.
  @mock.patch.object(
      llvm_bisection, 'GetCommandLineArgs', return_value=ArgsOutputTest())
  def testFailedToStartBisection(
      self, mock_get_args, mock_get_auto_script, mock_is_file,
      mock_llvm_bisection, mock_traceback, mock_sleep, mock_outside_chroot):

    def MockLLVMBisectionRaisesException(args_output):
      raise ValueError('Failed to launch more tryjobs.')

    # Use the test function to simulate the behavior of an exception happening
    # when launching more tryjobs.
    mock_llvm_bisection.side_effect = MockLLVMBisectionRaisesException

    # Verify the exception is raised when the number of attempts to launched
    # more tryjobs is exceeded, so unable to continue
    # bisection.
    with self.assertRaises(SystemExit) as err:
      auto_llvm_bisection.main()

    self.assertEqual(err.exception.code, 1)

    mock_outside_chroot.assert_called_once()
    mock_get_args.assert_called_once()
    mock_get_auto_script.assert_called_once()
    self.assertEqual(mock_is_file.call_count, 2)
    self.assertEqual(mock_llvm_bisection.call_count, 3)
    self.assertEqual(mock_traceback.call_count, 3)
    self.assertEqual(mock_sleep.call_count, 2)

  # Simulate the behavior of `subprocess.call()` when successfully updated all
  # tryjobs whose 'status' value is 'pending'.
  @mock.patch.object(subprocess, 'call', return_value=0)
  # Simulate the behavior of `VerifyOutsideChroot()` when successfully invoking
  # the script outside of the chroot.
  @mock.patch.object(
      auto_llvm_bisection, 'VerifyOutsideChroot', return_value=True)
  # Simulate behavior of `time.sleep()` when waiting for errors to settle caused
  # by `llvm_bisection.main()` (e.g. network issue, etc.).
  @mock.patch.object(time, 'sleep')
  # Simulate behavior of `traceback.print_exc()` when an exception happened in
  # `llvm_bisection.main()`.
  @mock.patch.object(traceback, 'print_exc')
  # Simulate behavior of `llvm_bisection.main()` when failed to launch tryjobs
  # (exception happened along the way, etc.).
  @mock.patch.object(llvm_bisection, 'main')
  # Simulate behavior of `os.path.isfile()` when starting a new bisection.
  @mock.patch.object(os.path, 'isfile')
  # Simulate behavior of `GetPathToUpdateAllTryjobsWithAutoScript()` when
  # returning the absolute path to that script that updates all 'pending'
  # tryjobs to the result of `cros buildresult`.
  @mock.patch.object(
      auto_llvm_bisection,
      'GetPathToUpdateAllTryjobsWithAutoScript',
      return_value='/abs/path/to/update_tryjob.py')
  # Simulate `llvm_bisection.GetCommandLineArgs()` when parsing the command line
  # arguments required by the bisection script.
  @mock.patch.object(
      llvm_bisection, 'GetCommandLineArgs', return_value=ArgsOutputTest())
  def testSuccessfullyBisectedLLVMRevision(
      self, mock_get_args, mock_get_auto_script, mock_is_file,
      mock_llvm_bisection, mock_traceback, mock_sleep, mock_outside_chroot,
      mock_update_tryjobs):

    # Simulate the behavior of `os.path.isfile()` when checking whether the
    # status file provided exists.
    @CallCountsToMockFunctions
    def MockStatusFileCheck(call_count, last_tested):
      # Simulate that the status file does not exist, so the LLVM bisection
      # script would create the status file and launch tryjobs.
      if call_count < 2:
        return False

      # Simulate when the status file exists and `subprocess.call()` executes
      # the script that updates all the 'pending' tryjobs to the result of `cros
      # buildresult`.
      if call_count == 2:
        return True

      assert False, 'os.path.isfile() called more times than expected.'

    # Simulate behavior of `llvm_bisection.main()` when successfully bisected
    # between the good and bad LLVM revision.
    @CallCountsToMockFunctions
    def MockLLVMBisectionReturnValue(call_count, args_output):
      # Simulate that successfully launched more tryjobs.
      if call_count == 0:
        return 0

      # Simulate that failed to launch more tryjobs.
      if call_count == 1:
        raise ValueError('Failed to launch more tryjobs.')

      # Simulate that the bad revision has been found.
      if call_count == 2:
        return llvm_bisection.BisectionExitStatus.BISECTION_COMPLETE.value

      assert False, 'Called `llvm_bisection.main()` more than expected.'

    # Use the test function to simulate the behavior of `llvm_bisection.main()`.
    mock_llvm_bisection.side_effect = MockLLVMBisectionReturnValue

    # Use the test function to simulate the behavior of `os.path.isfile()`.
    mock_is_file.side_effect = MockStatusFileCheck

    # Verify the excpetion is raised when successfully found the bad revision.
    # Uses `sys.exit(0)` to indicate success.
    with self.assertRaises(SystemExit) as err:
      auto_llvm_bisection.main()

    self.assertEqual(err.exception.code, 0)

    mock_outside_chroot.assert_called_once()
    mock_get_args.assert_called_once()
    mock_get_auto_script.assert_called_once()
    self.assertEqual(mock_is_file.call_count, 3)
    self.assertEqual(mock_llvm_bisection.call_count, 3)
    mock_traceback.assert_called_once()
    mock_sleep.assert_called_once()
    mock_update_tryjobs.assert_called_once()

  # Simulate behavior of `subprocess.call()` when failed to update tryjobs to
  # `cros buildresult` (script failed).
  @mock.patch.object(subprocess, 'call', return_value=1)
  # Simulate behavior of `time.time()` when determining the time passed when
  # updating tryjobs whose 'status' is 'pending'.
  @mock.patch.object(time, 'time')
  # Simulate the behavior of `VerifyOutsideChroot()` when successfully invoking
  # the script outside of the chroot.
  @mock.patch.object(
      auto_llvm_bisection, 'VerifyOutsideChroot', return_value=True)
  # Simulate behavior of `time.sleep()` when waiting for errors to settle caused
  # by `llvm_bisection.main()` (e.g. network issue, etc.).
  @mock.patch.object(time, 'sleep')
  # Simulate behavior of `traceback.print_exc()` when resuming bisection.
  @mock.patch.object(os.path, 'isfile', return_value=True)
  # Simulate behavior of `GetPathToUpdateAllTryjobsWithAutoScript()` when
  # returning the absolute path to that script that updates all 'pending'
  # tryjobs to the result of `cros buildresult`.
  @mock.patch.object(
      auto_llvm_bisection,
      'GetPathToUpdateAllTryjobsWithAutoScript',
      return_value='/abs/path/to/update_tryjob.py')
  # Simulate `llvm_bisection.GetCommandLineArgs()` when parsing the command line
  # arguments required by the bisection script.
  @mock.patch.object(
      llvm_bisection, 'GetCommandLineArgs', return_value=ArgsOutputTest())
  def testFailedToUpdatePendingTryJobs(
      self, mock_get_args, mock_get_auto_script, mock_is_file, mock_sleep,
      mock_outside_chroot, mock_time, mock_update_tryjobs):

    # Simulate behavior of `time.time()` for time passed.
    @CallCountsToMockFunctions
    def MockTimePassed(call_count):
      if call_count < 3:
        return call_count

      assert False, 'Called `time.time()` more than expected.'

    # Use the test function to simulate the behavior of `time.time()`.
    mock_time.side_effect = MockTimePassed

    # Reduce the polling limit for the test case to terminate faster.
    auto_llvm_bisection.POLLING_LIMIT_SECS = 1

    # Verify the exception is raised when unable to update tryjobs whose
    # 'status' value is 'pending'.
    with self.assertRaises(SystemExit) as err:
      auto_llvm_bisection.main()

    self.assertEqual(err.exception.code, 1)

    mock_outside_chroot.assert_called_once()
    mock_get_args.assert_called_once()
    mock_get_auto_script.assert_called_once()
    self.assertEqual(mock_is_file.call_count, 2)
    mock_sleep.assert_called_once()
    self.assertEqual(mock_time.call_count, 3)
    self.assertEqual(mock_update_tryjobs.call_count, 2)


if __name__ == '__main__':
  unittest.main()
