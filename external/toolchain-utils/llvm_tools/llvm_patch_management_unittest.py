#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests when creating the arguments for the patch manager."""

from __future__ import print_function
from collections import namedtuple
from failure_modes import FailureModes

import get_llvm_hash
import llvm_patch_management
import os
import patch_manager
import subprocess
import unittest
import unittest.mock as mock



class LlvmPatchManagementTest(unittest.TestCase):
  """Test class when constructing the arguments for the patch manager."""

  # Simulate the behavior of `os.path.isdir()` when the chroot path does not
  # exist or is not a directory.
  @mock.patch.object(os.path, 'isdir', return_value=False)
  def testInvalidChrootPathWhenGetPathToFilesDir(self, mock_isdir):
    chroot_path = '/some/path/to/chroot'
    package = 'sys-devel/llvm'

    # Verify the exception is raised when an invalid absolute path to the chroot
    # is passed in.
    with self.assertRaises(ValueError) as err:
      llvm_patch_management.GetPathToFilesDirectory(chroot_path, package)

    self.assertEqual(
        str(err.exception), 'Invalid chroot provided: %s' % chroot_path)

    mock_isdir.assert_called_once()

  # Simulate the behavior of 'os.path.isdir()' when a valid chroot path is
  # passed in.
  @mock.patch.object(os.path, 'isdir', return_value=True)
  @mock.patch.object(llvm_patch_management, 'ChrootRunCommand')
  @mock.patch.object(llvm_patch_management, '_GetRelativePathOfChrootPath')
  def testSuccessfullyGetPathToFilesDir(
      self, mock_get_relative_path_of_chroot_path, mock_chroot_cmd, mock_isdir):

    package_chroot_path = '/mnt/host/source/path/to/llvm/llvm.ebuild'

    # Simulate behavior of 'ChrootRunCommand()' when successfully
    # retrieved the absolute chroot path to the package's ebuild.
    mock_chroot_cmd.return_value = package_chroot_path

    # Simulate behavior of '_GetRelativePathOfChrootPath()' when successfully
    # removed '/mnt/host/source' of the absolute chroot path to the package's
    # ebuild.
    #
    # Returns relative path after '/mnt/host/source/'.
    mock_get_relative_path_of_chroot_path.return_value = 'path/to/llvm'

    chroot_path = '/some/path/to/chroot'

    package = 'sys-devel/llvm'

    self.assertEqual(
        llvm_patch_management.GetPathToFilesDirectory(chroot_path, package),
        '/some/path/to/chroot/path/to/llvm/files/')

    mock_isdir.assert_called_once()

    mock_chroot_cmd.assert_called_once()

    mock_get_relative_path_of_chroot_path.assert_called_once_with(
        '/mnt/host/source/path/to/llvm')

  def testInvalidPrefixForChrootPath(self):
    package_chroot_path = '/path/to/llvm'

    # Verify the exception is raised when the chroot path does not start with
    # '/mnt/host/source/'.
    with self.assertRaises(ValueError) as err:
      llvm_patch_management._GetRelativePathOfChrootPath(package_chroot_path)

    self.assertEqual(
        str(err.exception),
        'Invalid prefix for the chroot path: %s' % package_chroot_path)

  def testValidPrefixForChrootPath(self):
    package_chroot_path = '/mnt/host/source/path/to/llvm'

    package_rel_path = 'path/to/llvm'

    self.assertEqual(
        llvm_patch_management._GetRelativePathOfChrootPath(package_chroot_path),
        package_rel_path)

  # Simulate behavior of 'os.path.isfile()' when the patch metadata file does
  # not exist.
  @mock.patch.object(os.path, 'isfile', return_value=False)
  def testInvalidFileForPatchMetadataPath(self, mock_isfile):
    abs_path_to_patch_file = '/abs/path/to/files/test.json'

    # Verify the exception is raised when the absolute path to the patch
    # metadata file does not exist.
    with self.assertRaises(ValueError) as err:
      llvm_patch_management._CheckPatchMetadataPath(abs_path_to_patch_file)

    self.assertEqual(
        str(err.exception),
        'Invalid file provided: %s' % abs_path_to_patch_file)

    mock_isfile.assert_called_once()

  # Simulate behavior of 'os.path.isfile()' when the absolute path to the
  # patch metadata file exists.
  @mock.patch.object(os.path, 'isfile', return_value=True)
  def testPatchMetadataFileDoesNotEndInJson(self, mock_isfile):
    abs_path_to_patch_file = '/abs/path/to/files/PATCHES'

    # Verify the exception is raised when the patch metadata file does not end
    # in '.json'.
    with self.assertRaises(ValueError) as err:
      llvm_patch_management._CheckPatchMetadataPath(abs_path_to_patch_file)

    self.assertEqual(
        str(err.exception),
        'File does not end in ".json": %s' % abs_path_to_patch_file)

    mock_isfile.assert_called_once()

  @mock.patch.object(os.path, 'isfile')
  def testValidPatchMetadataFile(self, mock_isfile):
    abs_path_to_patch_file = '/abs/path/to/files/PATCHES.json'

    # Simulate behavior of 'os.path.isfile()' when the absolute path to the
    # patch metadata file exists.
    mock_isfile.return_value = True

    llvm_patch_management._CheckPatchMetadataPath(abs_path_to_patch_file)

    mock_isfile.assert_called_once()

  # Simulate `GetGitHashFrom()` when successfully retrieved the git hash
  # of the version passed in.
  @mock.patch.object(
      get_llvm_hash, 'GetGitHashFrom', return_value='a123testhash1')
  # Simulate `CreateTempLLVMRepo()` when successfully created a work tree from
  # the LLVM repo copy in `llvm_tools` directory.
  @mock.patch.object(llvm_patch_management, 'CreateTempLLVMRepo')
  # Simulate behavior of `_MoveSrcTreeHEADToGitHash()` when successfully moved
  # the head pointer to the git hash of the revision.
  @mock.patch.object(llvm_patch_management, '_MoveSrcTreeHEADToGitHash')
  @mock.patch.object(llvm_patch_management, 'GetPathToFilesDirectory')
  @mock.patch.object(llvm_patch_management, '_CheckPatchMetadataPath')
  def testExceptionIsRaisedWhenUpdatingAPackagesMetadataFile(
      self, mock_check_patch_metadata_path, mock_get_filesdir_path,
      mock_move_head_pointer, mock_create_temp_llvm_repo, mock_get_git_hash):

    abs_path_to_patch_file = \
        '/some/path/to/chroot/some/path/to/filesdir/PATCHES'

    # Simulate the behavior of '_CheckPatchMetadataPath()' when the patch
    # metadata file in $FILESDIR does not exist or does not end in '.json'.
    def InvalidPatchMetadataFile(patch_metadata_path):
      self.assertEqual(patch_metadata_path, abs_path_to_patch_file)

      raise ValueError(
          'File does not end in ".json": %s' % abs_path_to_patch_file)

    # Use the test function to simulate behavior of '_CheckPatchMetadataPath()'.
    mock_check_patch_metadata_path.side_effect = InvalidPatchMetadataFile

    abs_path_to_filesdir = '/some/path/to/chroot/some/path/to/filesdir'

    # Simulate the behavior of 'GetPathToFilesDirectory()' when successfully
    # constructed the absolute path to $FILESDIR of a package.
    mock_get_filesdir_path.return_value = abs_path_to_filesdir

    temp_work_tree = '/abs/path/to/tmpWorkTree'

    # Simulate the behavior of returning the absolute path to a worktree via
    # `git worktree add`.
    mock_create_temp_llvm_repo.return_value.__enter__.return_value.name = \
        temp_work_tree

    chroot_path = '/some/path/to/chroot'
    revision = 1000
    patch_file_name = 'PATCHES'
    package_name = 'test-package/package1'

    # Verify the exception is raised when a package is constructing the
    # arguments for the patch manager to update its patch metadata file and an
    # exception is raised in the process.
    with self.assertRaises(ValueError) as err:
      llvm_patch_management.UpdatePackagesPatchMetadataFile(
          chroot_path, revision, patch_file_name, [package_name],
          FailureModes.FAIL)

    self.assertEqual(
        str(err.exception),
        'File does not end in ".json": %s' % abs_path_to_patch_file)

    mock_get_filesdir_path.assert_called_once_with(chroot_path, package_name)

    mock_get_git_hash.assert_called_once()

    mock_check_patch_metadata_path.assert_called_once()

    mock_move_head_pointer.assert_called_once()

    mock_create_temp_llvm_repo.assert_called_once()

  # Simulate `CleanSrcTree()` when successfully removed changes from the
  # worktree.
  @mock.patch.object(patch_manager, 'CleanSrcTree')
  # Simulate `GetGitHashFrom()` when successfully retrieved the git hash
  # of the version passed in.
  @mock.patch.object(
      get_llvm_hash, 'GetGitHashFrom', return_value='a123testhash1')
  # Simulate `CreateTempLLVMRepo()` when successfully created a work tree from
  # the LLVM repo copy in `llvm_tools` directory.
  @mock.patch.object(llvm_patch_management, 'CreateTempLLVMRepo')
  # Simulate behavior of `_MoveSrcTreeHEADToGitHash()` when successfully moved
  # the head pointer to the git hash of the revision.
  @mock.patch.object(llvm_patch_management, '_MoveSrcTreeHEADToGitHash')
  @mock.patch.object(llvm_patch_management, 'GetPathToFilesDirectory')
  @mock.patch.object(llvm_patch_management, '_CheckPatchMetadataPath')
  @mock.patch.object(patch_manager, 'HandlePatches')
  def testSuccessfullyRetrievedPatchResults(
      self, mock_handle_patches, mock_check_patch_metadata_path,
      mock_get_filesdir_path, mock_move_head_pointer,
      mock_create_temp_llvm_repo, mock_get_git_hash, mock_clean_src_tree):

    abs_path_to_filesdir = '/some/path/to/chroot/some/path/to/filesdir'

    abs_path_to_patch_file = \
        '/some/path/to/chroot/some/path/to/filesdir/PATCHES.json'

    # Simulate the behavior of 'GetPathToFilesDirectory()' when successfully
    # constructed the absolute path to $FILESDIR of a package.
    mock_get_filesdir_path.return_value = abs_path_to_filesdir

    PatchInfo = namedtuple('PatchInfo', [
        'applied_patches', 'failed_patches', 'non_applicable_patches',
        'disabled_patches', 'removed_patches', 'modified_metadata'
    ])

    # Simulate the behavior of 'HandlePatches()' when successfully iterated
    # through every patch in the patch metadata file and a dictionary is
    # returned that contains information about the patches' status.
    mock_handle_patches.return_value = PatchInfo(
        applied_patches=['fixes_something.patch'],
        failed_patches=['disables_output.patch'],
        non_applicable_patches=[],
        disabled_patches=[],
        removed_patches=[],
        modified_metadata=None)

    temp_work_tree = '/abs/path/to/tmpWorkTree'

    # Simulate the behavior of returning the absolute path to a worktree via
    # `git worktree add`.
    mock_create_temp_llvm_repo.return_value.__enter__.return_value.name = \
        temp_work_tree

    expected_patch_results = {
        'applied_patches': ['fixes_something.patch'],
        'failed_patches': ['disables_output.patch'],
        'non_applicable_patches': [],
        'disabled_patches': [],
        'removed_patches': [],
        'modified_metadata': None
    }

    chroot_path = '/some/path/to/chroot'
    revision = 1000
    patch_file_name = 'PATCHES.json'
    package_name = 'test-package/package2'

    patch_info = llvm_patch_management.UpdatePackagesPatchMetadataFile(
        chroot_path, revision, patch_file_name, [package_name],
        FailureModes.CONTINUE)

    self.assertDictEqual(patch_info, {package_name: expected_patch_results})

    mock_get_filesdir_path.assert_called_once_with(chroot_path, package_name)

    mock_check_patch_metadata_path.assert_called_once_with(
        abs_path_to_patch_file)

    mock_handle_patches.assert_called_once()

    mock_create_temp_llvm_repo.assert_called_once()

    mock_get_git_hash.assert_called_once()

    mock_move_head_pointer.assert_called_once()

    mock_clean_src_tree.assert_called_once()


if __name__ == '__main__':
  unittest.main()
