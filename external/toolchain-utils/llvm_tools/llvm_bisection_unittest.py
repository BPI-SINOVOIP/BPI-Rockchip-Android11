#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for LLVM bisection."""

from __future__ import print_function

import get_llvm_hash
import json
import llvm_bisection
import unittest
import unittest.mock as mock

from get_llvm_hash import LLVMHash
from test_helpers import ArgsOutputTest
from test_helpers import CallCountsToMockFunctions
from test_helpers import CreateTemporaryJsonFile
from test_helpers import WritePrettyJsonFile


class LLVMBisectionTest(unittest.TestCase):
  """Unittests for LLVM bisection."""

  def testStartAndEndDoNotMatchJsonStartAndEnd(self):
    start = 100
    end = 150

    json_start = 110
    json_end = 150

    # Verify the exception is raised when the start and end revision for LLVM
    # bisection do not match the .JSON's 'start' and 'end' values.
    with self.assertRaises(ValueError) as err:
      llvm_bisection._ValidateStartAndEndAgainstJSONStartAndEnd(
          start, end, json_start, json_end)

    expected_error_message = ('The start %d or the end %d version provided is '
                              'different than "start" %d or "end" %d in the '
                              '.JSON file' % (start, end, json_start, json_end))

    self.assertEqual(str(err.exception), expected_error_message)

  def testStartAndEndMatchJsonStartAndEnd(self):
    start = 100
    end = 150

    json_start = 100
    json_end = 150

    llvm_bisection._ValidateStartAndEndAgainstJSONStartAndEnd(
        start, end, json_start, json_end)

  def testTryjobStatusIsMissing(self):
    start = 100
    end = 150

    test_tryjobs = [{
        'rev': 105,
        'status': 'good',
        'link': 'https://some_tryjob_1_url.com'
    }, {
        'rev': 120,
        'status': None,
        'link': 'https://some_tryjob_2_url.com'
    }, {
        'rev': 140,
        'status': 'bad',
        'link': 'https://some_tryjob_3_url.com'
    }]

    # Verify the exception is raised when a tryjob does not have a value for
    # the 'status' key or the 'status' key is missing.
    with self.assertRaises(ValueError) as err:
      llvm_bisection.GetStartAndEndRevision(start, end, test_tryjobs)

    expected_error_message = (
        '"status" is missing or has no value, please '
        'go to %s and update it' % test_tryjobs[1]['link'])

    self.assertEqual(str(err.exception), expected_error_message)

  def testGoodRevisionGreaterThanBadRevision(self):
    start = 100
    end = 150

    test_tryjobs = [{
        'rev': 110,
        'status': 'bad',
        'link': 'https://some_tryjob_1_url.com'
    }, {
        'rev': 125,
        'status': 'skip',
        'link': 'https://some_tryjob_2_url.com'
    }, {
        'rev': 140,
        'status': 'good',
        'link': 'https://some_tryjob_3_url.com'
    }]

    # Verify the exception is raised when the new 'start' revision is greater
    # than the new 'bad' revision for bisection (i.e. bisection is broken).
    with self.assertRaises(AssertionError) as err:
      llvm_bisection.GetStartAndEndRevision(start, end, test_tryjobs)

    expected_error_message = (
        'Bisection is broken because %d (good) is >= '
        '%d (bad)' % (test_tryjobs[2]['rev'], test_tryjobs[0]['rev']))

    self.assertEqual(str(err.exception), expected_error_message)

  def testSuccessfullyGetNewStartAndNewEndRevision(self):
    start = 100
    end = 150

    test_tryjobs = [{
        'rev': 110,
        'status': 'good',
        'link': 'https://some_tryjob_1_url.com'
    }, {
        'rev': 120,
        'status': 'good',
        'link': 'https://some_tryjob_2_url.com'
    }, {
        'rev': 130,
        'status': 'pending',
        'link': 'https://some_tryjob_3_url.com'
    }, {
        'rev': 135,
        'status': 'skip',
        'link': 'https://some_tryjob_4_url.com'
    }, {
        'rev': 140,
        'status': 'bad',
        'link': 'https://some_tryjob_5_url.com'
    }]

    # Tuple consists of the new good revision, the new bad revision, a set of
    # 'pending' revisions, and a set of 'skip' revisions.
    expected_revisions_tuple = 120, 140, {130}, {135}

    self.assertTupleEqual(
        llvm_bisection.GetStartAndEndRevision(start, end, test_tryjobs),
        expected_revisions_tuple)

  @mock.patch.object(get_llvm_hash, 'GetGitHashFrom')
  def testNoRevisionsBetweenStartAndEnd(self, mock_get_git_hash):
    start = 100
    end = 110

    test_pending_revisions = {107}
    test_skip_revisions = {101, 102, 103, 104, 108, 109}

    # Simulate behavior of `GetGitHashFrom()` when the revision does not
    # exist in the LLVM source tree.
    def MockGetGitHashForRevisionRaiseException(src_path, revision):
      raise ValueError('Revision does not exist')

    mock_get_git_hash.side_effect = MockGetGitHashForRevisionRaiseException

    parallel = 3

    abs_path_to_src = '/abs/path/to/src'

    self.assertListEqual(
        llvm_bisection.GetRevisionsBetweenBisection(
            start, end, parallel, abs_path_to_src, test_pending_revisions,
            test_skip_revisions), [])

  # Assume llvm_bisection module has imported GetGitHashFrom
  @mock.patch.object(get_llvm_hash, 'GetGitHashFrom')
  def testSuccessfullyRetrievedRevisionsBetweenStartAndEnd(
      self, mock_get_git_hash):

    start = 100
    end = 110

    test_pending_revisions = set()
    test_skip_revisions = {101, 102, 103, 104, 106, 108, 109}

    parallel = 3

    abs_path_to_src = '/abs/path/to/src'

    # Valid revision that exist in the LLVM source tree between 'start' and
    # 'end' and were not in the 'pending' set or 'skip' set.
    expected_revisions_between_start_and_end = [105, 107]

    self.assertListEqual(
        llvm_bisection.GetRevisionsBetweenBisection(
            start, end, parallel, abs_path_to_src, test_pending_revisions,
            test_skip_revisions), expected_revisions_between_start_and_end)

    self.assertEqual(mock_get_git_hash.call_count, 2)

  # Simulate behavior of `GetGitHashFrom()` when successfully retrieved
  # a list git hashes for each revision in the revisions list.
  # Assume llvm_bisection module has imported GetGitHashFrom
  @mock.patch.object(get_llvm_hash, 'GetGitHashFrom')
  # Simulate behavior of `GetRevisionsBetweenBisection()` when successfully
  # retrieved a list of valid revisions between 'start' and 'end'.
  @mock.patch.object(llvm_bisection, 'GetRevisionsBetweenBisection')
  # Simulate behavior of `CreatTempLLVMRepo()` when successfully created a
  # worktree when a source path was not provided.
  @mock.patch.object(llvm_bisection, 'CreateTempLLVMRepo')
  def testSuccessfullyGetRevisionsListAndHashList(
      self, mock_create_temp_llvm_repo, mock_get_revisions_between_bisection,
      mock_get_git_hash):

    expected_revisions_and_hash_tuple = ([102, 105, 108], [
        'a123testhash1', 'a123testhash2', 'a123testhash3'
    ])

    @CallCountsToMockFunctions
    def MockGetGitHashForRevision(call_count, src_path, rev):
      # Simulate retrieving the git hash for the revision.
      if call_count < 3:
        return expected_revisions_and_hash_tuple[1][call_count]

      assert False, 'Called `GetGitHashFrom()` more than expected.'

    temp_worktree = '/abs/path/to/tmpDir'

    mock_create_temp_llvm_repo.return_value.__enter__.return_value.name = \
        temp_worktree

    # Simulate the valid revisions list.
    mock_get_revisions_between_bisection.return_value = \
        expected_revisions_and_hash_tuple[0]

    # Simulate behavior of `GetGitHashFrom()` by using the testing
    # function.
    mock_get_git_hash.side_effect = MockGetGitHashForRevision

    start = 100
    end = 110
    parallel = 3
    src_path = None
    pending_revisions = {103, 104}
    skip_revisions = {101, 106, 107, 109}

    self.assertTupleEqual(
        llvm_bisection.GetRevisionsListAndHashList(
            start, end, parallel, src_path, pending_revisions, skip_revisions),
        expected_revisions_and_hash_tuple)

    mock_get_revisions_between_bisection.assert_called_once()

    self.assertEqual(mock_get_git_hash.call_count, 3)

  def testSuccessfullyDieWithNoRevisionsError(self):
    start = 100
    end = 110

    pending_revisions = {105, 108}
    skip_revisions = {101, 102, 103, 104, 106, 107, 109}

    expected_no_revisions_message = ('No revisions between start %d and end '
                                     '%d to create tryjobs' % (start, end))

    expected_no_revisions_message += '\nThe following tryjobs are pending:\n' \
        + '\n'.join(str(rev) for rev in pending_revisions)

    expected_no_revisions_message += '\nThe following tryjobs were skipped:\n' \
        + '\n'.join(str(rev) for rev in skip_revisions)

    # Verify that an exception is raised when there are no revisions to launch
    # tryjobs for between 'start' and 'end' and some tryjobs are 'pending'.
    with self.assertRaises(ValueError) as err:
      llvm_bisection.DieWithNoRevisionsError(start, end, skip_revisions,
                                             pending_revisions)

    self.assertEqual(str(err.exception), expected_no_revisions_message)

  # Simulate behavior of `FindTryjobIndex()` when the index of the tryjob was
  # found.
  @mock.patch.object(llvm_bisection, 'FindTryjobIndex', return_value=0)
  def testTryjobExistsInRevisionsToLaunch(self, mock_find_tryjob_index):
    test_existing_jobs = [{'rev': 102, 'status': 'good'}]

    revision_to_launch = [102]

    expected_revision_that_exists = 102

    with self.assertRaises(ValueError) as err:
      llvm_bisection.CheckForExistingTryjobsInRevisionsToLaunch(
          revision_to_launch, test_existing_jobs)

    expected_found_tryjob_index_error_message = (
        'Revision %d exists already '
        'in "jobs"' % expected_revision_that_exists)

    self.assertEqual(
        str(err.exception), expected_found_tryjob_index_error_message)

    mock_find_tryjob_index.assert_called_once()

  @mock.patch.object(llvm_bisection, 'AddTryjob')
  def testSuccessfullyUpdatedStatusFileWhenExceptionIsRaised(
      self, mock_add_tryjob):

    git_hash_list = ['a123testhash1', 'a123testhash2', 'a123testhash3']
    revisions_list = [102, 104, 106]

    # Simulate behavior of `AddTryjob()` when successfully launched a tryjob for
    # the updated packages.
    @CallCountsToMockFunctions
    def MockAddTryjob(call_count, packages, git_hash, revision, chroot_path,
                      patch_file, extra_cls, options, builder, verbose,
                      svn_revision):

      if call_count < 2:
        return {'rev': revisions_list[call_count], 'status': 'pending'}

      # Simulate an exception happened along the way when updating the
      # packages' `LLVM_NEXT_HASH`.
      if call_count == 2:
        raise ValueError('Unable to launch tryjob')

      assert False, 'Called `AddTryjob()` more than expected.'

    # Use the test function to simulate `AddTryjob()`.
    mock_add_tryjob.side_effect = MockAddTryjob

    start = 100
    end = 110

    bisection_contents = {'start': start, 'end': end, 'jobs': []}

    args_output = ArgsOutputTest()

    packages = ['sys-devel/llvm']
    patch_file = '/abs/path/to/PATCHES.json'

    # Create a temporary .JSON file to simulate a status file for bisection.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisection_contents, f)

      # Verify that the status file is updated when an exception happened when
      # attempting to launch a revision (i.e. progress is not lost).
      with self.assertRaises(ValueError) as err:
        llvm_bisection.UpdateBisection(
            revisions_list, git_hash_list, bisection_contents, temp_json_file,
            packages, args_output.chroot_path, patch_file,
            args_output.extra_change_lists, args_output.options,
            args_output.builders, args_output.verbose)

      expected_bisection_contents = {
          'start':
              start,
          'end':
              end,
          'jobs': [{
              'rev': revisions_list[0],
              'status': 'pending'
          }, {
              'rev': revisions_list[1],
              'status': 'pending'
          }]
      }

      # Verify that the launched tryjobs were added to the status file when
      # an exception happened.
      with open(temp_json_file) as f:
        json_contents = json.load(f)

        self.assertDictEqual(json_contents, expected_bisection_contents)

    self.assertEqual(str(err.exception), 'Unable to launch tryjob')

    self.assertEqual(mock_add_tryjob.call_count, 3)

  # Simulate behavior of `GetGitHashFrom()` when successfully retrieved
  # the git hash of the bad revision. Assume llvm_bisection has imported
  # GetGitHashFrom
  @mock.patch.object(
      get_llvm_hash, 'GetGitHashFrom', return_value='a123testhash4')
  def testCompletedBisectionWhenProvidedSrcPath(self, mock_get_git_hash):
    last_tested = '/some/last/tested_file.json'

    src_path = '/abs/path/to/src/path'

    # The bad revision.
    end = 150

    llvm_bisection._NoteCompletedBisection(last_tested, src_path, end)

    mock_get_git_hash.assert_called_once()

  # Simulate behavior of `GetLLVMHash()` when successfully retrieved
  # the git hash of the bad revision.
  @mock.patch.object(LLVMHash, 'GetLLVMHash', return_value='a123testhash5')
  def testCompletedBisectionWhenNotProvidedSrcPath(self, mock_get_git_hash):
    last_tested = '/some/last/tested_file.json'

    src_path = None

    # The bad revision.
    end = 200

    llvm_bisection._NoteCompletedBisection(last_tested, src_path, end)

    mock_get_git_hash.assert_called_once()

  def testSuccessfullyLoadedStatusFile(self):
    start = 100
    end = 150

    test_bisect_contents = {'start': start, 'end': end, 'jobs': []}

    # Simulate that the status file exists.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(test_bisect_contents, f)

      self.assertDictEqual(
          llvm_bisection.LoadStatusFile(temp_json_file, start, end),
          test_bisect_contents)

  def testLoadedStatusFileThatDoesNotExist(self):
    start = 200
    end = 250

    expected_bisect_contents = {'start': start, 'end': end, 'jobs': []}

    last_tested = '/abs/path/to/file_that_does_not_exist.json'

    self.assertDictEqual(
        llvm_bisection.LoadStatusFile(last_tested, start, end),
        expected_bisect_contents)

  # Simulate behavior of `_NoteCompletedBisection()` when there are no more
  # tryjobs to launch between start and end, so bisection is complete.
  @mock.patch.object(llvm_bisection, '_NoteCompletedBisection')
  @mock.patch.object(llvm_bisection, 'GetRevisionsListAndHashList')
  @mock.patch.object(llvm_bisection, 'GetStartAndEndRevision')
  # Simulate behavior of `_ValidateStartAndEndAgainstJSONStartAndEnd()` when
  # both start and end revisions match.
  @mock.patch.object(llvm_bisection,
                     '_ValidateStartAndEndAgainstJSONStartAndEnd')
  @mock.patch.object(llvm_bisection, 'LoadStatusFile')
  # Simulate behavior of `VerifyOutsideChroot()` when successfully invoked the
  # script outside of the chroot.
  @mock.patch.object(llvm_bisection, 'VerifyOutsideChroot', return_value=True)
  def testSuccessfullyBisectedLLVM(
      self, mock_outside_chroot, mock_load_status_file,
      mock_validate_start_and_end, mock_get_start_and_end_revision,
      mock_get_revision_and_hash_list, mock_note_completed_bisection):

    start = 500
    end = 502

    bisect_contents = {
        'start': start,
        'end': end,
        'jobs': [{
            'rev': 501,
            'status': 'skip'
        }]
    }

    skip_revisions = {501}
    pending_revisions = {}

    # Simulate behavior of `LoadStatusFile()` when successfully loaded the
    # status file.
    mock_load_status_file.return_value = bisect_contents

    # Simulate behavior of `GetStartAndEndRevision()` when successfully found
    # the new start and end revision of the bisection.
    #
    # Returns new start revision, new end revision, a set of pending revisions,
    # and a set of skip revisions.
    mock_get_start_and_end_revision.return_value = (start, end,
                                                    pending_revisions,
                                                    skip_revisions)

    # Simulate behavior of `GetRevisionsListAndHashList()` when successfully
    # retrieved valid revisions (along with their git hashes) between start and
    # end (in this case, none).
    mock_get_revision_and_hash_list.return_value = [], []

    args_output = ArgsOutputTest()
    args_output.start_rev = start
    args_output.end_rev = end
    args_output.parallel = 3
    args_output.src_path = None

    self.assertEqual(
        llvm_bisection.main(args_output),
        llvm_bisection.BisectionExitStatus.BISECTION_COMPLETE.value)

    mock_outside_chroot.assert_called_once()

    mock_load_status_file.assert_called_once()

    mock_validate_start_and_end.assert_called_once()

    mock_get_start_and_end_revision.assert_called_once()

    mock_get_revision_and_hash_list.assert_called_once()

    mock_note_completed_bisection.assert_called_once()

  @mock.patch.object(llvm_bisection, 'DieWithNoRevisionsError')
  # Simulate behavior of `_NoteCompletedBisection()` when there are no more
  # tryjobs to launch between start and end, so bisection is complete.
  @mock.patch.object(llvm_bisection, 'GetRevisionsListAndHashList')
  @mock.patch.object(llvm_bisection, 'GetStartAndEndRevision')
  # Simulate behavior of `_ValidateStartAndEndAgainstJSONStartAndEnd()` when
  # both start and end revisions match.
  @mock.patch.object(llvm_bisection,
                     '_ValidateStartAndEndAgainstJSONStartAndEnd')
  @mock.patch.object(llvm_bisection, 'LoadStatusFile')
  # Simulate behavior of `VerifyOutsideChroot()` when successfully invoked the
  # script outside of the chroot.
  @mock.patch.object(llvm_bisection, 'VerifyOutsideChroot', return_value=True)
  def testNoMoreTryjobsToLaunch(
      self, mock_outside_chroot, mock_load_status_file,
      mock_validate_start_and_end, mock_get_start_and_end_revision,
      mock_get_revision_and_hash_list, mock_die_with_no_revisions_error):

    start = 500
    end = 502

    bisect_contents = {
        'start': start,
        'end': end,
        'jobs': [{
            'rev': 501,
            'status': 'pending'
        }]
    }

    skip_revisions = {}
    pending_revisions = {501}

    no_revisions_error_message = ('No more tryjobs to launch between %d and '
                                  '%d' % (start, end))

    def MockNoRevisionsErrorException(start, end, skip, pending):
      raise ValueError(no_revisions_error_message)

    # Simulate behavior of `LoadStatusFile()` when successfully loaded the
    # status file.
    mock_load_status_file.return_value = bisect_contents

    # Simulate behavior of `GetStartAndEndRevision()` when successfully found
    # the new start and end revision of the bisection.
    #
    # Returns new start revision, new end revision, a set of pending revisions,
    # and a set of skip revisions.
    mock_get_start_and_end_revision.return_value = (start, end,
                                                    pending_revisions,
                                                    skip_revisions)

    # Simulate behavior of `GetRevisionsListAndHashList()` when successfully
    # retrieved valid revisions (along with their git hashes) between start and
    # end (in this case, none).
    mock_get_revision_and_hash_list.return_value = [], []

    # Use the test function to simulate `DieWithNoRevisionsWithError()`
    # behavior.
    mock_die_with_no_revisions_error.side_effect = MockNoRevisionsErrorException

    # Simulate behavior of arguments passed into the command line.
    args_output = ArgsOutputTest()
    args_output.start_rev = start
    args_output.end_rev = end
    args_output.parallel = 3
    args_output.src_path = None

    # Verify the exception is raised when there are no more tryjobs to launch
    # between start and end when there are tryjobs that are 'pending', so
    # the actual bad revision can change when those tryjobs's 'status' are
    # updated.
    with self.assertRaises(ValueError) as err:
      llvm_bisection.main(args_output)

    self.assertEqual(str(err.exception), no_revisions_error_message)

    mock_outside_chroot.assert_called_once()

    mock_load_status_file.assert_called_once()

    mock_validate_start_and_end.assert_called_once()

    mock_get_start_and_end_revision.assert_called_once()

    mock_get_revision_and_hash_list.assert_called_once()

    mock_die_with_no_revisions_error.assert_called_once()


if __name__ == '__main__':
  unittest.main()
