#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for retrieving the LLVM hash."""

from __future__ import print_function

import get_llvm_hash
import subprocess
import unittest
import unittest.mock as mock
import test_helpers

from get_llvm_hash import LLVMHash

# We grab protected stuff from get_llvm_hash. That's OK.
# pylint: disable=protected-access


def MakeMockPopen(return_code):

  def MockPopen(*_args, **_kwargs):
    result = mock.MagicMock()
    result.returncode = return_code

    communicate_result = result.communicate.return_value
    # Communicate returns stdout, stderr.
    communicate_result.__iter__.return_value = (None, 'some stderr')
    return result

  return MockPopen


class TestGetLLVMHash(unittest.TestCase):
  """The LLVMHash test class."""

  @mock.patch.object(subprocess, 'Popen')
  def testCloneRepoSucceedsWhenGitSucceeds(self, popen_mock):
    popen_mock.side_effect = MakeMockPopen(return_code=0)
    llvm_hash = LLVMHash()

    into_tempdir = '/tmp/tmpTest'
    llvm_hash.CloneLLVMRepo(into_tempdir)
    popen_mock.assert_called_with(
        ['git', 'clone', get_llvm_hash._LLVM_GIT_URL, into_tempdir],
        stderr=subprocess.PIPE)

  @mock.patch.object(subprocess, 'Popen')
  def testCloneRepoFailsWhenGitFails(self, popen_mock):
    popen_mock.side_effect = MakeMockPopen(return_code=1)

    with self.assertRaises(ValueError) as err:
      LLVMHash().CloneLLVMRepo('/tmp/tmp1')

    self.assertIn('Failed to clone', str(err.exception.args))
    self.assertIn('some stderr', str(err.exception.args))

  @mock.patch.object(get_llvm_hash, 'GetGitHashFrom')
  def testGetGitHashWorks(self, mock_get_git_hash):
    mock_get_git_hash.return_value = 'a13testhash2'

    self.assertEqual(
        get_llvm_hash.GetGitHashFrom('/tmp/tmpTest', 100), 'a13testhash2')

    mock_get_git_hash.assert_called_once()

  @mock.patch.object(LLVMHash, 'GetLLVMHash')
  @mock.patch.object(get_llvm_hash, 'GetGoogle3LLVMVersion')
  def testReturnGoogle3LLVMHash(self, mock_google3_llvm_version,
                                mock_get_llvm_hash):
    mock_get_llvm_hash.return_value = 'a13testhash3'
    mock_google3_llvm_version.return_value = 1000
    self.assertEqual(LLVMHash().GetGoogle3LLVMHash(), 'a13testhash3')
    mock_get_llvm_hash.assert_called_once_with(1000)

  @mock.patch.object(LLVMHash, 'GetLLVMHash')
  @mock.patch.object(get_llvm_hash, 'GetGoogle3LLVMVersion')
  def testReturnGoogle3UnstableLLVMHash(self, mock_google3_llvm_version,
                                        mock_get_llvm_hash):
    mock_get_llvm_hash.return_value = 'a13testhash3'
    mock_google3_llvm_version.return_value = 1000
    self.assertEqual(LLVMHash().GetGoogle3UnstableLLVMHash(), 'a13testhash3')
    mock_get_llvm_hash.assert_called_once_with(1000)

  @mock.patch.object(subprocess, 'check_output')
  def testSuccessfullyGetGitHashFromToTOfLLVM(self, mock_check_output):
    mock_check_output.return_value = 'a123testhash1 path/to/master\n'
    self.assertEqual(LLVMHash().GetTopOfTrunkGitHash(), 'a123testhash1')
    mock_check_output.assert_called_once()


if __name__ == '__main__':
  unittest.main()
