#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for modifying a tryjob."""

from __future__ import print_function

import json
import unittest
import unittest.mock as mock

from test_helpers import ArgsOutputTest
from test_helpers import CreateTemporaryJsonFile
from test_helpers import WritePrettyJsonFile
import modify_a_tryjob


class ModifyATryjobTest(unittest.TestCase):
  """Unittests for modifying a tryjob."""

  def testNoTryjobsInStatusFile(self):
    bisect_test_contents = {'start': 369410, 'end': 369420, 'jobs': []}

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      revision_to_modify = 369411

      args_output = ArgsOutputTest()
      args_output.builders = None
      args_output.options = None

      # Verify the exception is raised there are no tryjobs in the status file
      # and the mode is not to 'add' a tryjob.
      with self.assertRaises(SystemExit) as err:
        modify_a_tryjob.PerformTryjobModification(
            revision_to_modify, modify_a_tryjob.ModifyTryjob.REMOVE,
            temp_json_file, args_output.extra_change_lists, args_output.options,
            args_output.builders, args_output.chroot_path, args_output.verbose)

      self.assertEqual(str(err.exception), 'No tryjobs in %s' % temp_json_file)

  # Simulate the behavior of `FindTryjobIndex()` when the index of the tryjob
  # was not found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=None)
  def testNoTryjobIndexFound(self, mock_find_tryjob_index):
    bisect_test_contents = {
        'start': 369410,
        'end': 369420,
        'jobs': [{
            'rev': 369411,
            'status': 'pending',
            'buildbucket_id': 1200
        }]
    }

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      revision_to_modify = 369412

      args_output = ArgsOutputTest()
      args_output.builders = None
      args_output.options = None

      # Verify the exception is raised when the index of the tryjob was not
      # found in the status file and the mode is not to 'add' a tryjob.
      with self.assertRaises(ValueError) as err:
        modify_a_tryjob.PerformTryjobModification(
            revision_to_modify, modify_a_tryjob.ModifyTryjob.REMOVE,
            temp_json_file, args_output.extra_change_lists, args_output.options,
            args_output.builders, args_output.chroot_path, args_output.verbose)

      self.assertEqual(
          str(err.exception), 'Unable to find tryjob for %d in %s' %
          (revision_to_modify, temp_json_file))

    mock_find_tryjob_index.assert_called_once()

  # Simulate the behavior of `FindTryjobIndex()` when the index of the tryjob
  # was found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=0)
  def testSuccessfullyRemovedTryjobInStatusFile(self, mock_find_tryjob_index):
    bisect_test_contents = {
        'start': 369410,
        'end': 369420,
        'jobs': [{
            'rev': 369414,
            'status': 'pending',
            'buildbucket_id': 1200
        }]
    }

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      revision_to_modify = 369414

      args_output = ArgsOutputTest()
      args_output.builders = None
      args_output.options = None

      modify_a_tryjob.PerformTryjobModification(
          revision_to_modify, modify_a_tryjob.ModifyTryjob.REMOVE,
          temp_json_file, args_output.extra_change_lists, args_output.options,
          args_output.builders, args_output.chroot_path, args_output.verbose)

      # Verify that the tryjob was removed from the status file.
      with open(temp_json_file) as status_file:
        bisect_contents = json.load(status_file)

        expected_file_contents = {'start': 369410, 'end': 369420, 'jobs': []}

        self.assertDictEqual(bisect_contents, expected_file_contents)

    mock_find_tryjob_index.assert_called_once()

  # Simulate the behavior of `RunTryJobs()` when successfully submitted a
  # tryjob.
  @mock.patch.object(modify_a_tryjob, 'RunTryJobs')
  # Simulate the behavior of `FindTryjobIndex()` when the index of the tryjob
  # was found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=0)
  def testSuccessfullyRelaunchedTryjob(self, mock_find_tryjob_index,
                                       mock_run_tryjob):

    bisect_test_contents = {
        'start':
            369410,
        'end':
            369420,
        'jobs': [{
            'rev': 369411,
            'status': 'bad',
            'link': 'https://some_tryjob_link.com',
            'buildbucket_id': 1200,
            'cl': 123,
            'extra_cls': None,
            'options': None,
            'builder': ['some-builder-tryjob']
        }]
    }

    tryjob_result = [{
        'link': 'https://some_new_tryjob_link.com',
        'buildbucket_id': 20
    }]

    mock_run_tryjob.return_value = tryjob_result

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      revision_to_modify = 369411

      args_output = ArgsOutputTest()
      args_output.builders = None
      args_output.options = None

      modify_a_tryjob.PerformTryjobModification(
          revision_to_modify, modify_a_tryjob.ModifyTryjob.RELAUNCH,
          temp_json_file, args_output.extra_change_lists, args_output.options,
          args_output.builders, args_output.chroot_path, args_output.verbose)

      # Verify that the tryjob's information was updated after submtting the
      # tryjob.
      with open(temp_json_file) as status_file:
        bisect_contents = json.load(status_file)

        expected_file_contents = {
            'start':
                369410,
            'end':
                369420,
            'jobs': [{
                'rev': 369411,
                'status': 'pending',
                'link': 'https://some_new_tryjob_link.com',
                'buildbucket_id': 20,
                'cl': 123,
                'extra_cls': None,
                'options': None,
                'builder': ['some-builder-tryjob']
            }]
        }

        self.assertDictEqual(bisect_contents, expected_file_contents)

    mock_find_tryjob_index.assert_called_once()

    mock_run_tryjob.assert_called_once()

  # Simulate the behavior of `FindTryjobIndex()` when the index of the tryjob
  # was found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=0)
  def testAddingTryjobThatAlreadyExists(self, mock_find_tryjob_index):
    bisect_test_contents = {
        'start': 369410,
        'end': 369420,
        'jobs': [{
            'rev': 369411,
            'status': 'bad',
            'builder': ['some-builder']
        }]
    }

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      revision_to_add = 369411

      # Index of the tryjob in 'jobs' list.
      tryjob_index = 0

      args_output = ArgsOutputTest()
      args_output.options = None

      # Verify the exception is raised when the tryjob that is going to added
      # already exists in the status file (found its index).
      with self.assertRaises(ValueError) as err:
        modify_a_tryjob.PerformTryjobModification(
            revision_to_add, modify_a_tryjob.ModifyTryjob.ADD, temp_json_file,
            args_output.extra_change_lists, args_output.options,
            args_output.builders, args_output.chroot_path, args_output.verbose)

      self.assertEqual(
          str(err.exception), 'Tryjob already exists (index is %d) in %s.' %
          (tryjob_index, temp_json_file))

    mock_find_tryjob_index.assert_called_once()

  # Simulate the behavior of `FindTryjobIndex()` when the tryjob was not found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=None)
  def testSuccessfullyDidNotAddTryjobOutsideOfBisectionBounds(
      self, mock_find_tryjob_index):

    bisect_test_contents = {
        'start': 369410,
        'end': 369420,
        'jobs': [{
            'rev': 369411,
            'status': 'bad'
        }]
    }

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      # Add a revision that is outside of 'start' and 'end'.
      revision_to_add = 369450

      args_output = ArgsOutputTest()
      args_output.options = None

      # Verify the exception is raised when adding a tryjob that does not exist
      # and is not within 'start' and 'end'.
      with self.assertRaises(ValueError) as err:
        modify_a_tryjob.PerformTryjobModification(
            revision_to_add, modify_a_tryjob.ModifyTryjob.ADD, temp_json_file,
            args_output.extra_change_lists, args_output.options,
            args_output.builders, args_output.chroot_path, args_output.verbose)

      self.assertEqual(
          str(err.exception), 'Failed to add tryjob to %s' % temp_json_file)

    mock_find_tryjob_index.assert_called_once()

  # Simulate the behavior of `AddTryjob()` when successfully submitted the
  # tryjob and constructed the tryjob information (a dictionary).
  @mock.patch.object(modify_a_tryjob, 'AddTryjob')
  # Simulate the behavior of `GetLLVMHashAndVersionFromSVNOption()` when
  # successfully retrieved the git hash of the revision to launch a tryjob for.
  @mock.patch.object(
      modify_a_tryjob,
      'GetLLVMHashAndVersionFromSVNOption',
      return_value=('a123testhash1', 369418))
  # Simulate the behavior of `FindTryjobIndex()` when the tryjob was not found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=None)
  def testSuccessfullyAddedTryjob(self, mock_find_tryjob_index,
                                  mock_get_llvm_hash, mock_add_tryjob):

    bisect_test_contents = {
        'start': 369410,
        'end': 369420,
        'jobs': [{
            'rev': 369411,
            'status': 'bad'
        }]
    }

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      # Add a revision that is outside of 'start' and 'end'.
      revision_to_add = 369418

      args_output = ArgsOutputTest()
      args_output.options = None

      new_tryjob_info = {
          'rev': revision_to_add,
          'status': 'pending',
          'options': args_output.options,
          'extra_cls': args_output.extra_change_lists,
          'builder': args_output.builders
      }

      mock_add_tryjob.return_value = new_tryjob_info

      modify_a_tryjob.PerformTryjobModification(
          revision_to_add, modify_a_tryjob.ModifyTryjob.ADD, temp_json_file,
          args_output.extra_change_lists, args_output.options,
          args_output.builders, args_output.chroot_path, args_output.verbose)

      # Verify that the tryjob was added to the status file.
      with open(temp_json_file) as status_file:
        bisect_contents = json.load(status_file)

        expected_file_contents = {
            'start': 369410,
            'end': 369420,
            'jobs': [{
                'rev': 369411,
                'status': 'bad'
            }, new_tryjob_info]
        }

        self.assertDictEqual(bisect_contents, expected_file_contents)

    mock_find_tryjob_index.assert_called_once()

    mock_get_llvm_hash.assert_called_once_with(revision_to_add)

    mock_add_tryjob.assert_called_once()

  # Simulate the behavior of `FindTryjobIndex()` when the tryjob was found.
  @mock.patch.object(modify_a_tryjob, 'FindTryjobIndex', return_value=0)
  def testModifyATryjobOptionDoesNotExist(self, mock_find_tryjob_index):
    bisect_test_contents = {
        'start': 369410,
        'end': 369420,
        'jobs': [{
            'rev': 369414,
            'status': 'bad'
        }]
    }

    # Create a temporary .JSON file to simulate a .JSON file that has bisection
    # contents.
    with CreateTemporaryJsonFile() as temp_json_file:
      with open(temp_json_file, 'w') as f:
        WritePrettyJsonFile(bisect_test_contents, f)

      # Add a revision that is outside of 'start' and 'end'.
      revision_to_modify = 369414

      args_output = ArgsOutputTest()
      args_output.builders = None
      args_output.options = None

      # Verify the exception is raised when the modify a tryjob option does not
      # exist.
      with self.assertRaises(ValueError) as err:
        modify_a_tryjob.PerformTryjobModification(
            revision_to_modify, 'remove_link', temp_json_file,
            args_output.extra_change_lists, args_output.options,
            args_output.builders, args_output.chroot_path, args_output.verbose)

      self.assertEqual(
          str(err.exception),
          'Invalid "modify_tryjob" option provided: remove_link')

      mock_find_tryjob_index.assert_called_once()


if __name__ == '__main__':
  unittest.main()
