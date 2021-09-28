#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A manager for patches."""

from __future__ import print_function

import argparse
import json
import os
import subprocess
import sys
from collections import namedtuple

import get_llvm_hash
from failure_modes import FailureModes
from subprocess_helpers import check_call
from subprocess_helpers import check_output


def is_directory(dir_path):
  """Validates that the argument passed into 'argparse' is a directory."""

  if not os.path.isdir(dir_path):
    raise ValueError('Path is not a directory: %s' % dir_path)

  return dir_path


def is_patch_metadata_file(patch_metadata_file):
  """Valides the argument into 'argparse' is a patch file."""

  if not os.path.isfile(patch_metadata_file):
    raise ValueError(
        'Invalid patch metadata file provided: %s' % patch_metadata_file)

  if not patch_metadata_file.endswith('.json'):
    raise ValueError(
        'Patch metadata file does not end in ".json": %s' % patch_metadata_file)

  return patch_metadata_file


def is_valid_failure_mode(failure_mode):
  """Validates that the failure mode passed in is correct."""

  cur_failure_modes = [mode.value for mode in FailureModes]

  if failure_mode not in cur_failure_modes:
    raise ValueError('Invalid failure mode provided: %s' % failure_mode)

  return failure_mode


def EnsureBisectModeAndSvnVersionAreSpecifiedTogether(failure_mode,
                                                      good_svn_version):
  """Validates that 'good_svn_version' is passed in only for bisection."""

  if failure_mode != FailureModes.BISECT_PATCHES.value and good_svn_version:
    raise ValueError('"good_svn_version" is only available for bisection.')
  elif failure_mode == FailureModes.BISECT_PATCHES.value and \
      not good_svn_version:
    raise ValueError('A good SVN version is required for bisection (used by'
                     '"git bisect start".')


def GetCommandLineArgs():
  """Get the required arguments from the command line."""

  # Create parser and add optional command-line arguments.
  parser = argparse.ArgumentParser(description='A manager for patches.')

  # Add argument for the last good SVN version which is required by
  # `git bisect start` (only valid for bisection mode).
  parser.add_argument(
      '--good_svn_version',
      type=int,
      help='INTERNAL USE ONLY... (used for bisection.)')

  # Add argument for the number of patches it iterate. Only used when performing
  # `git bisect run`.
  parser.add_argument(
      '--num_patches_to_iterate', type=int, help=argparse.SUPPRESS)

  # Add argument for whether bisection should continue. Only used for
  # 'bisect_patches.'
  parser.add_argument(
      '--continue_bisection',
      type=bool,
      default=False,
      help='Determines whether bisection should continue after successfully '
      'bisecting a patch (default: %(default)s) - only used for '
      '"bisect_patches"')

  # Trust src_path HEAD and svn_version.
  parser.add_argument(
      '--use_src_head',
      action='store_true',
      help='Use the HEAD of src_path directory as is, not necessarily the same '
      'as the svn_version of upstream.')

  # Add argument for the LLVM version to use for patch management.
  parser.add_argument(
      '--svn_version',
      type=int,
      required=True,
      help='the LLVM svn version to use for patch management (determines '
      'whether a patch is applicable)')

  # Add argument for the patch metadata file that is in $FILESDIR.
  parser.add_argument(
      '--patch_metadata_file',
      required=True,
      type=is_patch_metadata_file,
      help='the absolute path to the .json file in "$FILESDIR/" of the '
      'package which has all the patches and their metadata if applicable')

  # Add argument for the absolute path to the ebuild's $FILESDIR path.
  # Example: '.../sys-devel/llvm/files/'.
  parser.add_argument(
      '--filesdir_path',
      required=True,
      type=is_directory,
      help='the absolute path to the ebuild "files/" directory')

  # Add argument for the absolute path to the unpacked sources.
  parser.add_argument(
      '--src_path',
      required=True,
      type=is_directory,
      help='the absolute path to the unpacked LLVM sources')

  # Add argument for the mode of the patch manager when handling failing
  # applicable patches.
  parser.add_argument(
      '--failure_mode',
      default=FailureModes.FAIL.value,
      type=is_valid_failure_mode,
      help='the mode of the patch manager when handling failed patches ' \
          '(default: %(default)s)')

  # Parse the command line.
  args_output = parser.parse_args()

  EnsureBisectModeAndSvnVersionAreSpecifiedTogether(
      args_output.failure_mode, args_output.good_svn_version)

  return args_output


def GetHEADSVNVersion(src_path):
  """Gets the SVN version of HEAD in the src tree."""

  cmd = ['git', '-C', src_path, 'rev-parse', 'HEAD']

  git_hash = check_output(cmd)

  version = get_llvm_hash.GetVersionFrom(src_path, git_hash.rstrip())

  return version


def VerifyHEADIsTheSameAsSVNVersion(src_path, svn_version):
  """Verifies that HEAD's SVN version matches 'svn_version'."""

  head_svn_version = GetHEADSVNVersion(src_path)

  if head_svn_version != svn_version:
    raise ValueError('HEAD\'s SVN version %d does not match "svn_version"'
                     ' %d, please move HEAD to "svn_version"s\' git hash.' %
                     (head_svn_version, svn_version))


def GetPathToPatch(filesdir_path, rel_patch_path):
  """Gets the absolute path to a patch in $FILESDIR.

  Args:
    filesdir_path: The absolute path to $FILESDIR.
    rel_patch_path: The relative path to the patch in '$FILESDIR/'.

  Returns:
    The absolute path to the patch in $FILESDIR.

  Raises:
    ValueError: Unable to find the path to the patch in $FILESDIR.
  """

  if not os.path.isdir(filesdir_path):
    raise ValueError('Invalid path to $FILESDIR provided: %s' % filesdir_path)

  # Combine $FILESDIR + relative path of patch to $FILESDIR.
  patch_path = os.path.join(filesdir_path, rel_patch_path)

  if not os.path.isfile(patch_path):
    raise ValueError('The absolute path %s to the patch %s does not exist' %
                     (patch_path, rel_patch_path))

  return patch_path


def GetPatchMetadata(patch_dict):
  """Gets the patch's metadata.

  Args:
    patch_dict: A dictionary that has the patch metadata.

  Returns:
    A tuple that contains the metadata values.
  """

  # Get the metadata values of a patch if possible.
  start_version = patch_dict.get('start_version', 0)
  end_version = patch_dict.get('end_version', None)
  is_critical = patch_dict.get('is_critical', False)

  return start_version, end_version, is_critical


def ApplyPatch(src_path, patch_path):
  """Attempts to apply the patch.

  Args:
    src_path: The absolute path to the unpacked sources of the package.
    patch_path: The absolute path to the patch in $FILESDIR/

  Returns:
    A boolean where 'True' means that the patch applied fine or 'False' means
    that the patch failed to apply.
  """

  if not os.path.isdir(src_path):
    raise ValueError('Invalid src path provided: %s' % src_path)

  if not os.path.isfile(patch_path):
    raise ValueError('Invalid patch file provided: %s' % patch_path)

  # Test the patch with '--dry-run' before actually applying the patch.
  test_patch_cmd = [
      'patch', '--dry-run', '-d', src_path, '-f', '-p1', '-E',
      '--no-backup-if-mismatch', '-i', patch_path
  ]

  # Cmd to apply a patch in the src unpack path.
  apply_patch_cmd = [
      'patch', '-d', src_path, '-f', '-p1', '-E', '--no-backup-if-mismatch',
      '-i', patch_path
  ]

  try:
    check_output(test_patch_cmd)

  # If the mode is 'continue', then catching the exception makes sure that
  # the program does not exit on the first failed applicable patch.
  except subprocess.CalledProcessError:
    # Test run on the patch failed to apply.
    return False

  # Test run succeeded on the patch.
  check_output(apply_patch_cmd)

  return True


def UpdatePatchMetadataFile(patch_metadata_file, patches):
  """Updates the .json file with unchanged and at least one changed patch.

  Args:
    patch_metadata_file: The absolute path to the .json file that has all the
    patches and its metadata.
    patches: A list of patches whose metadata were or were not updated.

  Raises:
    ValueError: The patch metadata file does not have the correct extension.
  """

  if not patch_metadata_file.endswith('.json'):
    raise ValueError('File does not end in ".json": %s' % patch_metadata_file)

  with open(patch_metadata_file, 'w') as patch_file:
    json.dump(patches, patch_file, indent=4, separators=(',', ': '))


def GetCommitHashesForBisection(src_path, good_svn_version, bad_svn_version):
  """Gets the good and bad commit hashes required by `git bisect start`."""

  bad_commit_hash = get_llvm_hash.GetGitHashFrom(src_path, bad_svn_version)

  good_commit_hash = get_llvm_hash.GetGitHashFrom(src_path, good_svn_version)

  return good_commit_hash, bad_commit_hash


def PerformBisection(src_path, good_commit, bad_commit, svn_version,
                     patch_metadata_file, filesdir_path, num_patches):
  """Performs bisection to determine where a patch stops applying."""

  start_cmd = [
      'git', '-C', src_path, 'bisect', 'start', bad_commit, good_commit
  ]

  check_output(start_cmd)

  run_cmd = [
      'git', '-C', src_path, 'bisect', 'run',
      os.path.abspath(__file__), '--svn_version',
      '%d' % svn_version, '--patch_metadata_file', patch_metadata_file,
      '--filesdir_path', filesdir_path, '--src_path', src_path,
      '--failure_mode', 'internal_bisection', '--num_patches_to_iterate',
      '%d' % num_patches
  ]

  check_call(run_cmd)

  # Successfully bisected the patch, so retrieve the SVN version from the
  # commit message.
  get_bad_commit_hash_cmd = [
      'git', '-C', src_path, 'rev-parse', 'refs/bisect/bad'
  ]

  git_hash = check_output(get_bad_commit_hash_cmd)

  end_cmd = ['git', '-C', src_path, 'bisect', 'reset']

  check_output(end_cmd)

  # `git bisect run` returns the bad commit hash and the commit message.
  version = get_llvm_hash.GetVersionFrom(src_path, git_hash.rstrip())

  return version


def CleanSrcTree(src_path):
  """Cleans the source tree of the changes made in 'src_path'."""

  reset_src_tree_cmd = ['git', '-C', src_path, 'reset', 'HEAD', '--hard']

  check_output(reset_src_tree_cmd)

  clean_src_tree_cmd = ['git', '-C', src_path, 'clean', '-fd']

  check_output(clean_src_tree_cmd)


def SaveSrcTreeState(src_path):
  """Stashes the changes made so far to the source tree."""

  save_src_tree_cmd = ['git', '-C', src_path, 'stash', '-a']

  check_output(save_src_tree_cmd)


def RestoreSrcTreeState(src_path, bad_commit_hash):
  """Restores the changes made to the source tree."""

  checkout_cmd = ['git', '-C', src_path, 'checkout', bad_commit_hash]

  check_output(checkout_cmd)

  get_changes_cmd = ['git', '-C', src_path, 'stash', 'pop']

  check_output(get_changes_cmd)


def HandlePatches(svn_version,
                  patch_metadata_file,
                  filesdir_path,
                  src_path,
                  mode,
                  good_svn_version=None,
                  num_patches_to_iterate=None,
                  continue_bisection=False):
  """Handles the patches in the .json file for the package.

  Args:
    svn_version: The LLVM version to use for patch management.
    patch_metadata_file: The absolute path to the .json file in '$FILESDIR/'
    that has all the patches and their metadata.
    filesdir_path: The absolute path to $FILESDIR.
    src_path: The absolute path to the unpacked destination of the package.
    mode: The action to take when an applicable patch failed to apply.
      Ex: 'FailureModes.FAIL'
    good_svn_version: Only used by 'bisect_patches' which tells
    `git bisect start` the good version.
    num_patches_to_iterate: The number of patches to iterate in the .JSON file
    (internal use). Only used by `git bisect run`.
    continue_bisection: Only used for 'bisect_patches' mode. If flag is set,
    then bisection will continue to the next patch when successfully bisected a
    patch.

  Returns:
    Depending on the mode, 'None' would be returned if everything went well or
    the .json file was not updated. Otherwise, a list or multiple lists would
    be returned that indicates what has changed.

  Raises:
    ValueError: The patch metadata file does not exist or does not end with
    '.json' or the absolute path to $FILESDIR does not exist or the unpacked
    path does not exist or if the mode is 'fail', then an applicable patch
    failed to apply.
  """

  # A flag for whether the mode specified would possible modify the patches.
  can_modify_patches = False

  # 'fail' or 'continue' mode would not modify a patch's metadata, so the .json
  # file would stay the same.
  if mode != FailureModes.FAIL and mode != FailureModes.CONTINUE:
    can_modify_patches = True

  # A flag that determines whether at least one patch's metadata was
  # updated due to the mode that is passed in.
  updated_patch = False

  # A list of patches that will be in the updated .json file.
  applicable_patches = []

  # A list of patches that successfully applied.
  applied_patches = []

  # A list of patches that were disabled.
  disabled_patches = []

  # A list of bisected patches.
  bisected_patches = []

  # A list of non applicable patches.
  non_applicable_patches = []

  # A list of patches that will not be included in the updated .json file
  removed_patches = []

  # Whether the patch metadata file was modified where 'None' means that the
  # patch metadata file was not modified otherwise the absolute path to the
  # patch metadata file is stored.
  modified_metadata = None

  # A list of patches that failed to apply.
  failed_patches = []

  with open(patch_metadata_file) as patch_file:
    patch_file_contents = json.load(patch_file)

    if mode == FailureModes.BISECT_PATCHES:
      # A good and bad commit are required by `git bisect start`.
      good_commit, bad_commit = GetCommitHashesForBisection(
          src_path, good_svn_version, svn_version)

    # Patch format:
    # {
    #   "rel_patch_path" : "[REL_PATCH_PATH_FROM_$FILESDIR]"
    #   [PATCH_METADATA] if available.
    # }
    #
    # For each patch, find the path to it in $FILESDIR and get its metadata if
    # available, then check if the patch is applicable.
    for patch_dict_index, cur_patch_dict in enumerate(patch_file_contents):
      # Used by the internal bisection. All the patches in the interval [0, N]
      # have been iterated.
      if num_patches_to_iterate and \
          (patch_dict_index + 1) > num_patches_to_iterate:
        break

      # Get the absolute path to the patch in $FILESDIR.
      path_to_patch = GetPathToPatch(filesdir_path,
                                     cur_patch_dict['rel_patch_path'])

      # Get the patch's metadata.
      #
      # Index information of 'patch_metadata':
      #   [0]: start_version
      #   [1]: end_version
      #   [2]: is_critical
      patch_metadata = GetPatchMetadata(cur_patch_dict)

      if not patch_metadata[1]:
        # Patch does not have an 'end_version' value which implies 'end_version'
        # == 'inf' ('svn_version' will always be less than 'end_version'), so
        # the patch is applicable if 'svn_version' >= 'start_version'.
        patch_applicable = svn_version >= patch_metadata[0]
      else:
        # Patch is applicable if 'svn_version' >= 'start_version' &&
        # "svn_version" < "end_version".
        patch_applicable = (svn_version >= patch_metadata[0] and \
                            svn_version < patch_metadata[1])

      if can_modify_patches:
        # Add to the list only if the mode can potentially modify a patch.
        #
        # If the mode is 'remove_patches', then all patches that are
        # applicable or are from the future will be added to the updated .json
        # file and all patches that are not applicable will be added to the
        # remove patches list which will not be included in the updated .json
        # file.
        if patch_applicable or svn_version < patch_metadata[0] or \
            mode != FailureModes.REMOVE_PATCHES:
          applicable_patches.append(cur_patch_dict)
        elif mode == FailureModes.REMOVE_PATCHES:
          removed_patches.append(path_to_patch)

          if not modified_metadata:
            # At least one patch will be removed from the .json file.
            modified_metadata = patch_metadata_file

      if not patch_applicable:
        non_applicable_patches.append(os.path.basename(path_to_patch))

      # There is no need to apply patches in 'remove_patches' mode because the
      # mode removes patches that do not apply anymore based off of
      # 'svn_version.'
      if patch_applicable and mode != FailureModes.REMOVE_PATCHES:
        patch_applied = ApplyPatch(src_path, path_to_patch)

        if not patch_applied:  # Failed to apply patch.
          failed_patches.append(os.path.basename(path_to_patch))

          # Check the mode to determine what action to take on the failing
          # patch.
          if mode == FailureModes.DISABLE_PATCHES:
            # Set the patch's 'end_version' to 'svn_version' so the patch
            # would not be applicable anymore (i.e. the patch's 'end_version'
            # would not be greater than 'svn_version').

            # Last element in 'applicable_patches' is the current patch.
            applicable_patches[-1]['end_version'] = svn_version

            disabled_patches.append(os.path.basename(path_to_patch))

            if not updated_patch:
              # At least one patch has been modified, so the .json file
              # will be updated with the new patch metadata.
              updated_patch = True

              modified_metadata = patch_metadata_file
          elif mode == FailureModes.BISECT_PATCHES:
            # Figure out where the patch's stops applying and set the patch's
            # 'end_version' to that version.

            # Do not want to overwrite the changes to the current progress of
            # 'bisect_patches' on the source tree.
            SaveSrcTreeState(src_path)

            # Need a clean source tree for `git bisect run` to avoid unnecessary
            # fails for patches.
            CleanSrcTree(src_path)

            print('\nStarting to bisect patch %s for SVN version %d:\n' %
                  (os.path.basename(cur_patch_dict['rel_patch_path']),
                   svn_version))

            # Performs the bisection: calls `git bisect start` and
            # `git bisect run`, where `git bisect run` is going to call this
            # script as many times as needed with specific arguments.
            bad_svn_version = PerformBisection(
                src_path, good_commit, bad_commit, svn_version,
                patch_metadata_file, filesdir_path, patch_dict_index + 1)

            print('\nSuccessfully bisected patch %s, starts to fail to apply '
                  'at %d\n' % (os.path.basename(
                      cur_patch_dict['rel_patch_path']), bad_svn_version))

            # Overwrite the .JSON file with the new 'end_version' for the
            # current failed patch so that if there are other patches that
            # fail to apply, then the 'end_version' for the current patch could
            # be applicable when `git bisect run` is performed on the next
            # failed patch because the same .JSON file is used for `git bisect
            # run`.
            patch_file_contents[patch_dict_index][
                'end_version'] = bad_svn_version
            UpdatePatchMetadataFile(patch_metadata_file, patch_file_contents)

            # Clear the changes made to the source tree by `git bisect run`.
            CleanSrcTree(src_path)

            if not continue_bisection:
              # Exiting program early because 'continue_bisection' is not set.
              sys.exit(0)

            bisected_patches.append(
                '%s starts to fail to apply at %d' % (os.path.basename(
                    cur_patch_dict['rel_patch_path']), bad_svn_version))

            # Continue where 'bisect_patches' left off.
            RestoreSrcTreeState(src_path, bad_commit)

            if not modified_metadata:
              # At least one patch's 'end_version' has been updated.
              modified_metadata = patch_metadata_file

          elif mode == FailureModes.FAIL:
            if applied_patches:
              print('The following patches applied successfully up to the '
                    'failed patch:')
              print('\n'.join(applied_patches))

            # Throw an exception on the first patch that failed to apply.
            raise ValueError(
                'Failed to apply patch: %s' % os.path.basename(path_to_patch))
          elif mode == FailureModes.INTERNAL_BISECTION:
            # Determine the exit status for `git bisect run` because of the
            # failed patch in the interval [0, N].
            #
            # NOTE: `git bisect run` exit codes are as follows:
            #   130: Terminates the bisection.
            #   1: Similar as `git bisect bad`.

            # Some patch in the interval [0, N) failed, so terminate bisection
            # (the patch stack is broken).
            if (patch_dict_index + 1) != num_patches_to_iterate:
              print('\nTerminating bisection due to patch %s failed to apply '
                    'on SVN version %d.\n' % (os.path.basename(
                        cur_patch_dict['rel_patch_path']), svn_version))

              # Man page for `git bisect run` states that any value over 127
              # terminates it.
              sys.exit(130)

            # Changes to the source tree need to be removed, otherwise some
            # patches may fail when applying the patch to the source tree when
            # `git bisect run` calls this script again.
            CleanSrcTree(src_path)

            # The last patch in the interval [0, N] failed to apply, so let
            # `git bisect run` know that the last patch (the patch that failed
            # originally which led to `git bisect run` to be invoked) is bad
            # with exit code 1.
            sys.exit(1)
        else:  # Successfully applied patch
          applied_patches.append(os.path.basename(path_to_patch))

  # All patches in the interval [0, N] applied successfully, so let
  # `git bisect run` know that the program exited with exit code 0 (good).
  if mode == FailureModes.INTERNAL_BISECTION:
    # Changes to the source tree need to be removed, otherwise some
    # patches may fail when applying the patch to the source tree when
    # `git bisect run` calls this script again.
    #
    # Also, if `git bisect run` will NOT call this script again (terminated) and
    # if the source tree changes are not removed, `git bisect reset` will
    # complain that the changes would need to be 'stashed' or 'removed' in
    # order to reset HEAD back to the bad commit's git hash, so HEAD will remain
    # on the last git hash used by `git bisect run`.
    CleanSrcTree(src_path)

    # NOTE: Exit code 0 is similar to `git bisect good`.
    sys.exit(0)

  # Create a namedtuple of the patch results.
  PatchInfo = namedtuple('PatchInfo', [
      'applied_patches', 'failed_patches', 'non_applicable_patches',
      'disabled_patches', 'removed_patches', 'modified_metadata'
  ])

  patch_info = PatchInfo(
      applied_patches=applied_patches,
      failed_patches=failed_patches,
      non_applicable_patches=non_applicable_patches,
      disabled_patches=disabled_patches,
      removed_patches=removed_patches,
      modified_metadata=modified_metadata)

  # Determine post actions after iterating through the patches.
  if mode == FailureModes.REMOVE_PATCHES:
    if removed_patches:
      UpdatePatchMetadataFile(patch_metadata_file, applicable_patches)
  elif mode == FailureModes.DISABLE_PATCHES:
    if updated_patch:
      UpdatePatchMetadataFile(patch_metadata_file, applicable_patches)
  elif mode == FailureModes.BISECT_PATCHES:
    PrintPatchResults(patch_info)
    if modified_metadata:
      print('\nThe following patches have been bisected:')
      print('\n'.join(bisected_patches))

    # Exiting early because 'bisect_patches' will not be called from other
    # scripts, only this script uses 'bisect_patches'. The intent is to provide
    # bisection information on the patches and aid in the bisection process.
    sys.exit(0)

  return patch_info


def PrintPatchResults(patch_info):
  """Prints the results of handling the patches of a package.

  Args:
    patch_info: A namedtuple that has information on the patches.
  """

  if patch_info.applied_patches:
    print('\nThe following patches applied successfully:')
    print('\n'.join(patch_info.applied_patches))

  if patch_info.failed_patches:
    print('\nThe following patches failed to apply:')
    print('\n'.join(patch_info.failed_patches))

  if patch_info.non_applicable_patches:
    print('\nThe following patches were not applicable:')
    print('\n'.join(patch_info.non_applicable_patches))

  if patch_info.modified_metadata:
    print('\nThe patch metadata file %s has been modified' % os.path.basename(
        patch_info.modified_metadata))

  if patch_info.disabled_patches:
    print('\nThe following patches were disabled:')
    print('\n'.join(patch_info.disabled_patches))

  if patch_info.removed_patches:
    print('\nThe following patches were removed from the patch metadata file:')
    for cur_patch_path in patch_info.removed_patches:
      print('%s' % os.path.basename(cur_patch_path))


def main():
  """Applies patches to the source tree and takes action on a failed patch."""

  args_output = GetCommandLineArgs()

  if args_output.failure_mode != FailureModes.INTERNAL_BISECTION.value:
    # If the SVN version of HEAD is not the same as 'svn_version', then some
    # patches that fail to apply could successfully apply if HEAD's SVN version
    # was the same as 'svn_version'. In other words, HEAD's git hash should be
    # what is being updated to (e.g. LLVM_NEXT_HASH).
    if not args_output.use_src_head:
      VerifyHEADIsTheSameAsSVNVersion(args_output.src_path,
                                      args_output.svn_version)
  else:
    # `git bisect run` called this script.
    #
    # `git bisect run` moves HEAD each time it invokes this script, so set the
    # 'svn_version' to be current HEAD's SVN version so that the previous
    # SVN version is not used in determining whether a patch is applicable.
    args_output.svn_version = GetHEADSVNVersion(args_output.src_path)

  # Get the results of handling the patches of the package.
  patch_info = HandlePatches(
      args_output.svn_version, args_output.patch_metadata_file,
      args_output.filesdir_path, args_output.src_path,
      FailureModes(args_output.failure_mode), args_output.good_svn_version,
      args_output.num_patches_to_iterate, args_output.continue_bisection)

  PrintPatchResults(patch_info)


if __name__ == '__main__':
  main()
