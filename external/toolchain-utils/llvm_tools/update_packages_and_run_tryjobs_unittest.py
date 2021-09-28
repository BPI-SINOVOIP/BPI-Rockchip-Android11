#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for running tryjobs after updating packages."""

from __future__ import print_function

import json
import unittest
import unittest.mock as mock

from test_helpers import ArgsOutputTest
from test_helpers import CreateTemporaryFile
from update_chromeos_llvm_next_hash import CommitContents
import update_chromeos_llvm_next_hash
import update_packages_and_run_tryjobs


class UpdatePackagesAndRunTryjobsTest(unittest.TestCase):
  """Unittests when running tryjobs after updating packages."""

  def testNoLastTestedFile(self):
    self.assertEqual(
        update_packages_and_run_tryjobs.GetLastTestedSVNVersion(None), None)

  def testFailedToGetIntegerFromLastTestedFile(self):
    # Create a temporary file to simulate the behavior of the last tested file
    # when the file does not have a SVN version (i.e. int() failed).
    with CreateTemporaryFile() as temp_file:
      self.assertEqual(
          update_packages_and_run_tryjobs.GetLastTestedSVNVersion(temp_file),
          None)

  def testLastTestFileDoesNotExist(self):
    # Simulate 'open()' on a lasted tested file that does not exist.
    mock_open = mock.mock_open(read_data='')

    self.assertEqual(
        update_packages_and_run_tryjobs.GetLastTestedSVNVersion(
            '/some/file/that/does/not/exist.txt'), None)

  def testSuccessfullyRetrievedLastTestedSVNVersion(self):
    with CreateTemporaryFile() as temp_file:
      # Simulate behavior when the last tested file contains a SVN version.
      with open(temp_file, 'w') as svn_file:
        svn_file.write('1234')

      self.assertEqual(
          update_packages_and_run_tryjobs.GetLastTestedSVNVersion(temp_file),
          1234)

  def testGetTryJobCommandWithNoExtraInformation(self):
    test_change_list = 1234

    test_builder = 'nocturne'

    expected_tryjob_cmd_list = [
        'cros', 'tryjob', '--yes', '--json', '-g',
        '%d' % test_change_list, test_builder
    ]

    self.assertEqual(
        update_packages_and_run_tryjobs.GetTryJobCommand(
            test_change_list, None, None, test_builder),
        expected_tryjob_cmd_list)

  def testGetTryJobCommandWithExtraInformation(self):
    test_change_list = 4321
    test_extra_cls = [1000, 10]
    test_options = ['report_error', 'delete_tryjob']
    test_builder = 'kevin'

    expected_tryjob_cmd_list = [
        'cros',
        'tryjob',
        '--yes',
        '--json',
        '-g',
        '%d' % test_change_list,
        '-g',
        '%d' % test_extra_cls[0],
        '-g',
        '%d' % test_extra_cls[1],
        test_builder,
        '--%s' % test_options[0],
        '--%s' % test_options[1],
    ]

    self.assertEqual(
        update_packages_and_run_tryjobs.GetTryJobCommand(
            test_change_list, test_extra_cls, test_options, test_builder),
        expected_tryjob_cmd_list)

  # Simulate `datetime.datetime.utcnow()` when retrieving the current time when
  # submitted a tryjob.
  @mock.patch.object(
      update_packages_and_run_tryjobs,
      'GetCurrentTimeInUTC',
      return_value='2019-09-09')
  # Simulate the behavior of `AddTryjobLinkToCL()` when successfully added the
  # tryjob url to the CL that was uploaded to Gerrit for review.
  @mock.patch.object(update_packages_and_run_tryjobs, 'AddTryjobLinkToCL')
  # Simulate behavior of `ChrootRunCommand()` when successfully submitted a
  # tryjob via `cros tryjob`.
  @mock.patch.object(update_packages_and_run_tryjobs, 'ChrootRunCommand')
  def testSuccessfullySubmittedTryJob(
      self, mock_chroot_cmd, mock_add_tryjob_link_to_cl, mock_launch_time):

    expected_tryjob_cmd_list = [
        'cros', 'tryjob', '--yes', '--json', '-g',
        '%d' % 900, '-g',
        '%d' % 1200, 'builder1', '--some_option'
    ]

    buildbucket_id = '1234'
    url = 'https://some_tryjob_url.com'

    tryjob_launch_contents = [{'buildbucket_id': buildbucket_id, 'url': url}]

    mock_chroot_cmd.return_value = json.dumps(tryjob_launch_contents)

    extra_cls = [1200]
    tryjob_options = ['some_option']
    builder_list = ['builder1']
    chroot_path = '/some/path/to/chroot'
    cl_to_launch_tryjob = 900
    verbose = False

    tryjob_results_list = update_packages_and_run_tryjobs.RunTryJobs(
        cl_to_launch_tryjob, extra_cls, tryjob_options, builder_list,
        chroot_path, verbose)

    expected_tryjob_dict = {
        'launch_time': '2019-09-09',
        'link': url,
        'buildbucket_id': int(buildbucket_id),
        'extra_cls': extra_cls,
        'options': tryjob_options,
        'builder': builder_list
    }

    self.assertEqual(tryjob_results_list, [expected_tryjob_dict])

    mock_chroot_cmd.assert_called_once_with(
        chroot_path, expected_tryjob_cmd_list, verbose=False)

    mock_add_tryjob_link_to_cl.assert_called_once()

    mock_launch_time.assert_called_once()

  # Simulate behavior of `ExecCommandAndCaptureOutput()` when successfully added
  # the tryjob link to the CL via `gerrit message <CL> <message>`.
  @mock.patch.object(
      update_packages_and_run_tryjobs,
      'ExecCommandAndCaptureOutput',
      return_value=None)
  def testSuccessfullyAddedTryjobLinkToCL(self, mock_exec_cmd):
    chroot_path = '/abs/path/to/chroot'

    test_cl_number = 1000

    tryjob_result = [{'link': 'https://some_tryjob_link.com'}]

    update_packages_and_run_tryjobs.AddTryjobLinkToCL(
        tryjob_result, test_cl_number, chroot_path)

    expected_gerrit_message = [
        '%s/chromite/bin/gerrit' % chroot_path, 'message',
        str(test_cl_number),
        'Started the following tryjobs:\n%s' % tryjob_result[0]['link']
    ]

    mock_exec_cmd.assert_called_once_with(expected_gerrit_message)

  # Simulate behavior of `GetCommandLineArgs()` when successfully parsed the
  # command line for the optional/required arguments for the script.
  @mock.patch.object(update_packages_and_run_tryjobs, 'GetCommandLineArgs')
  # Simulate behavior of `GetLLVMHashAndVersionFromSVNOption()` when
  # successfully retrieved the LLVM hash and version for google3.
  @mock.patch.object(update_packages_and_run_tryjobs,
                     'GetLLVMHashAndVersionFromSVNOption')
  # Simulate behavior of `GetLastTestedSVNVersion()` when successfully retrieved
  # the last tested revision from the last tested file.
  @mock.patch.object(
      update_packages_and_run_tryjobs,
      'GetLastTestedSVNVersion',
      return_value=100)
  # Simulate behavior of `VerifyOutsideChroot()` when successfully invoked the
  # script outside of the chroot.
  @mock.patch.object(
      update_packages_and_run_tryjobs, 'VerifyOutsideChroot', return_value=True)
  def testLastTestSVNVersionMatchesSVNVersion(
      self, mock_outside_chroot, mock_get_last_tested_version,
      mock_get_hash_and_version, mock_get_commandline_args):

    args_output_obj = ArgsOutputTest()

    mock_get_commandline_args.return_value = args_output_obj

    mock_get_hash_and_version.return_value = ('a123testhash1', 100)

    update_packages_and_run_tryjobs.main()

    mock_outside_chroot.assert_called_once()

    mock_get_commandline_args.assert_called_once()

    mock_get_last_tested_version.assert_called_once_with(
        args_output_obj.last_tested)

    mock_get_hash_and_version.assert_called_once_with(
        args_output_obj.llvm_version)

  # Simulate the behavior of `RunTryJobs()` when successfully submitted a
  # tryjob.
  @mock.patch.object(update_packages_and_run_tryjobs, 'RunTryJobs')
  # Simulate behavior of `UpdatePackages()` when successfully updated the
  # packages and uploaded a CL for review.
  @mock.patch.object(update_chromeos_llvm_next_hash, 'UpdatePackages')
  # Simulate behavior of `GetCommandLineArgs()` when successfully parsed the
  # command line for the optional/required arguments for the script.
  @mock.patch.object(update_packages_and_run_tryjobs, 'GetCommandLineArgs')
  # Simulate behavior of `GetLLVMHashAndVersionFromSVNOption()` when
  # successfully retrieved the LLVM hash and version for google3.
  @mock.patch.object(update_packages_and_run_tryjobs,
                     'GetLLVMHashAndVersionFromSVNOption')
  # Simulate behavior of `GetLastTestedSVNVersion()` when successfully retrieved
  # the last tested revision from the last tested file.
  @mock.patch.object(
      update_packages_and_run_tryjobs,
      'GetLastTestedSVNVersion',
      return_value=100)
  # Simulate behavior of `VerifyOutsideChroot()` when successfully invoked the
  # script outside of the chroot.
  @mock.patch.object(
      update_packages_and_run_tryjobs, 'VerifyOutsideChroot', return_value=True)
  def testUpdatedLastTestedFileWithNewTestedRevision(
      self, mock_outside_chroot, mock_get_last_tested_version,
      mock_get_hash_and_version, mock_get_commandline_args,
      mock_update_packages, mock_run_tryjobs):

    mock_get_hash_and_version.return_value = ('a123testhash2', 200)

    test_cl_url = 'https://some_cl_url.com'

    test_cl_number = 12345

    mock_update_packages.return_value = CommitContents(
        url=test_cl_url, cl_number=test_cl_number)

    tryjob_test_results = [{
        'link': 'https://some_tryjob_url.com',
        'buildbucket_id': 1234
    }]

    mock_run_tryjobs.return_value = tryjob_test_results

    # Create a temporary file to simulate the last tested file that contains a
    # revision.
    with CreateTemporaryFile() as last_tested_file:
      args_output_obj = ArgsOutputTest(svn_option=200)
      args_output_obj.last_tested = last_tested_file

      mock_get_commandline_args.return_value = args_output_obj

      update_packages_and_run_tryjobs.main()

      # Verify that the lasted tested file has been updated to the new LLVM
      # revision.
      with open(last_tested_file) as update_revision:
        new_revision = update_revision.readline()

        self.assertEqual(int(new_revision.rstrip()), 200)

    mock_outside_chroot.assert_called_once()

    mock_get_commandline_args.assert_called_once()

    mock_get_last_tested_version.assert_called_once()

    mock_get_hash_and_version.assert_called_once()

    mock_run_tryjobs.assert_called_once()

    mock_update_packages.assert_called_once()


if __name__ == '__main__':
  unittest.main()
