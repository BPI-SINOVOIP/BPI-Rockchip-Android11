#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates LLVM_NEXT_HASH and uprevs the build of a package or packages.

For each package, a temporary repo is created and the changes are uploaded
for review.
"""

from __future__ import print_function

import argparse
import os
import re
import subprocess
from collections import namedtuple

from assert_not_in_chroot import VerifyOutsideChroot
from failure_modes import FailureModes
from get_llvm_hash import GetLLVMHashAndVersionFromSVNOption, is_svn_option
import get_llvm_hash
import llvm_patch_management
from subprocess_helpers import ChrootRunCommand, ExecCommandAndCaptureOutput

# If set to `True`, then the contents of `stdout` after executing a command will
# be displayed to the terminal.
verbose = False

CommitContents = namedtuple('CommitContents', ['url', 'cl_number'])


def GetCommandLineArgs():
  """Parses the command line for the optional command line arguments.

  Returns:
    The log level to use when retrieving the LLVM hash or google3 LLVM version,
    the chroot path to use for executing chroot commands,
    a list of a package or packages to update their LLVM next hash,
    and the LLVM version to use when retrieving the LLVM hash.
  """

  # Default path to the chroot if a path is not specified.
  cros_root = os.path.expanduser('~')
  cros_root = os.path.join(cros_root, 'chromiumos')

  # Create parser and add optional command-line arguments.
  parser = argparse.ArgumentParser(
      description="Updates the build's hash for llvm-next.")

  # Add argument for a specific chroot path.
  parser.add_argument(
      '--chroot_path',
      default=cros_root,
      help='the path to the chroot (default: %(default)s)')

  # Add argument for specific builds to uprev and update their llvm-next hash.
  parser.add_argument(
      '--update_packages',
      default=['sys-devel/llvm'],
      required=False,
      nargs='+',
      help='the ebuilds to update their hash for llvm-next ' \
          '(default: %(default)s)')

  # Add argument for whether to display command contents to `stdout`.
  parser.add_argument(
      '--verbose',
      action='store_true',
      help='display contents of a command to the terminal '
      '(default: %(default)s)')

  # Add argument for the LLVM version to use.
  parser.add_argument(
      '--llvm_version',
      type=is_svn_option,
      required=True,
      help='which git hash of LLVM to find. Either a svn revision, or one '
      'of %s' % sorted(get_llvm_hash.KNOWN_HASH_SOURCES))

  # Add argument for the mode of the patch management when handling patches.
  parser.add_argument(
      '--failure_mode',
      default=FailureModes.FAIL.value,
      choices=[FailureModes.FAIL.value, FailureModes.CONTINUE.value,
               FailureModes.DISABLE_PATCHES.value,
               FailureModes.REMOVE_PATCHES.value],
      help='the mode of the patch manager when handling failed patches ' \
          '(default: %(default)s)')

  # Add argument for the patch metadata file.
  parser.add_argument(
      '--patch_metadata_file',
      default='PATCHES.json',
      help='the .json file that has all the patches and their '
      'metadata if applicable (default: PATCHES.json inside $FILESDIR)')

  # Parse the command line.
  args_output = parser.parse_args()

  # FIXME: We shouldn't be using globals here, but until we fix it, make pylint
  # stop complaining about it.
  # pylint: disable=global-statement
  global verbose

  verbose = args_output.verbose

  return args_output


def GetChrootBuildPaths(chromeos_root, package_list):
  """Gets the chroot path(s) of the package(s).

  Args:
    chromeos_root: The absolute path to the chroot to
    use for executing chroot commands.
    package_list: A list of a package/packages to
    be used to find their chroot path.

  Returns:
    A list of a chroot path/chroot paths of the package's ebuild file.

  Raises:
    ValueError: Failed to get the chroot path of a package.
  """

  chroot_paths = []

  # Find the chroot path for each package's ebuild.
  for cur_package in sorted(set(package_list)):
    # Cmd to find the chroot path for the package.
    equery_cmd = ['equery', 'w', cur_package]

    chroot_path = ChrootRunCommand(chromeos_root, equery_cmd, verbose=verbose)

    chroot_paths.append(chroot_path.strip())

  return chroot_paths


def _ConvertChrootPathsToSymLinkPaths(chromeos_root, chroot_file_paths):
  """Converts the chroot path(s) to absolute symlink path(s).

  Args:
    chromeos_root: The absolute path to the chroot.
    chroot_file_paths: A list of a chroot path/chroot paths to convert to
    a absolute symlink path/symlink paths.

  Returns:
    A list of absolute path(s) which are symlinks that point to
    the ebuild of the package(s).

  Raises:
    ValueError: Invalid prefix for the chroot path or
    invalid chroot path(s) were provided.
  """

  symlink_file_paths = []

  chroot_prefix = '/mnt/host/source/'

  # Iterate through the chroot paths.
  #
  # For each chroot file path, remove '/mnt/host/source/' prefix
  # and combine the chroot path with the result and add it to the list.
  for cur_chroot_file_path in chroot_file_paths:
    if not cur_chroot_file_path.startswith(chroot_prefix):
      raise ValueError(
          'Invalid prefix for the chroot path: %s' % cur_chroot_file_path)

    rel_path = cur_chroot_file_path[len(chroot_prefix):]

    # combine the chromeos root path + '/src/...'
    absolute_symlink_path = os.path.join(chromeos_root, rel_path)

    symlink_file_paths.append(absolute_symlink_path)

  return symlink_file_paths


def GetEbuildPathsFromSymLinkPaths(symlinks):
  """Reads the symlink(s) to get the ebuild path(s) to the package(s).

  Args:
    symlinks: A list of absolute path symlink/symlinks that point
    to the package's ebuild.

  Returns:
    A dictionary where the key is the absolute path of the symlink and the value
    is the absolute path to the ebuild that was read from the symlink.

  Raises:
    ValueError: Invalid symlink(s) were provided.
  """

  # A dictionary that holds:
  #   key: absolute symlink path
  #   value: absolute ebuild path
  resolved_paths = {}

  # Iterate through each symlink.
  #
  # For each symlink, check that it is a valid symlink,
  # and then construct the ebuild path, and
  # then add the ebuild path to the dict.
  for cur_symlink in symlinks:
    if not os.path.islink(cur_symlink):
      raise ValueError('Invalid symlink provided: %s' % cur_symlink)

    # Construct the absolute path to the ebuild.
    ebuild_path = os.path.realpath(cur_symlink)

    if cur_symlink not in resolved_paths:
      resolved_paths[cur_symlink] = ebuild_path

  return resolved_paths


def UpdateBuildLLVMNextHash(ebuild_path, llvm_hash, llvm_version):
  """Updates the build's LLVM_NEXT_HASH.

  The build changes are staged for commit in the temporary repo.

  Args:
    ebuild_path: The absolute path to the ebuild.
    llvm_hash: The new LLVM hash to use for LLVM_NEXT_HASH.
    llvm_version: The revision number of 'llvm_hash'.

  Raises:
    ValueError: Invalid ebuild path provided or failed to stage the commit
    of the changes or failed to update the LLVM hash.
  """

  # Iterate through each ebuild.
  #
  # For each ebuild, read the file in
  # advance and then create a temporary file
  # that gets updated with the new LLVM hash
  # and revision number and then the ebuild file
  # gets updated to the temporary file.

  if not os.path.isfile(ebuild_path):
    raise ValueError('Invalid ebuild path provided: %s' % ebuild_path)

  # Create regex that finds 'LLVM_NEXT_HASH'.
  llvm_regex = re.compile('^LLVM_NEXT_HASH=\"[a-z0-9]+\"')

  temp_ebuild_file = '%s.temp' % ebuild_path

  # A flag for whether 'LLVM_NEXT_HASH=...' was updated.
  is_updated = False

  with open(ebuild_path) as ebuild_file:
    # write updates to a temporary file in case of interrupts
    with open(temp_ebuild_file, 'w') as temp_file:
      for cur_line in ReplaceLLVMNextHash(ebuild_file, is_updated, llvm_regex,
                                          llvm_hash, llvm_version):
        temp_file.write(cur_line)

  os.rename(temp_ebuild_file, ebuild_path)

  # Get the path to the parent directory.
  parent_dir = os.path.dirname(ebuild_path)

  # Stage the changes.
  stage_changes_cmd = ['git', '-C', parent_dir, 'add', ebuild_path]

  ExecCommandAndCaptureOutput(stage_changes_cmd, verbose=verbose)


def ReplaceLLVMNextHash(ebuild_lines, is_updated, llvm_regex, llvm_hash,
                        llvm_version):
  """Iterates through the ebuild file and updates the 'LLVM_NEXT_HASH'.

  Args:
    ebuild_lines: The contents of the ebuild file.
    is_updated: A flag for whether 'LLVM_NEXT_HASH' was updated.
    llvm_regex: The regex object for finding 'LLVM_NEXT_HASH=...' when
    iterating through the contents of the file.
    llvm_hash: The new LLVM hash to use for LLVM_NEXT_HASH.
    llvm_version: The revision number of 'llvm_hash'.
  """

  for cur_line in ebuild_lines:
    if not is_updated and llvm_regex.search(cur_line):
      # Update the LLVM next hash and revision number.
      cur_line = 'LLVM_NEXT_HASH=\"%s\" # r%d\n' % (llvm_hash, llvm_version)

      is_updated = True

    yield cur_line

  if not is_updated:  # failed to update 'LLVM_NEXT_HASH'
    raise ValueError('Failed to update the LLVM hash.')


def UprevEbuild(symlink):
  """Uprevs the ebuild's revision number.

  Increases the revision number by 1 and stages the change in
  the temporary repo.

  Args:
    symlink: The absolute path of the symlink that points to
    the ebuild of the package.

  Raises:
    ValueError: Failed to uprev the symlink or failed to stage the changes.
  """

  if not os.path.islink(symlink):
    raise ValueError('Invalid symlink provided: %s' % symlink)

  # Find the revision number and increment it by 1.
  new_symlink, is_changed = re.subn(
      r'r([0-9]+).ebuild',
      lambda match: 'r%s.ebuild' % str(int(match.group(1)) + 1),
      symlink,
      count=1)

  if not is_changed:  # failed to increment the revision number
    raise ValueError('Failed to uprev the ebuild.')

  path_to_symlink_dir = os.path.dirname(symlink)

  # Stage the new symlink for commit.
  stage_symlink_cmd = [
      'git', '-C', path_to_symlink_dir, 'mv', symlink, new_symlink
  ]

  ExecCommandAndCaptureOutput(stage_symlink_cmd, verbose=verbose)


def _CreateRepo(path_to_repo_dir, llvm_hash):
  """Creates a temporary repo for the changes.

  Args:
    path_to_repo_dir: The absolute path to the repo.
    llvm_hash: The LLVM hash to use for the name of the repo.

  Raises:
    ValueError: Failed to create a repo in that directory.
  """

  if not os.path.isdir(path_to_repo_dir):
    raise ValueError('Invalid directory path provided: %s' % path_to_repo_dir)

  reset_changes_cmd = [
      'git',
      '-C',
      path_to_repo_dir,
      'reset',
      'HEAD',
      '--hard',
  ]

  ExecCommandAndCaptureOutput(reset_changes_cmd, verbose=verbose)

  create_repo_cmd = ['repo', 'start', 'llvm-next-update-%s' % llvm_hash]

  ExecCommandAndCaptureOutput(
      create_repo_cmd, cwd=path_to_repo_dir, verbose=verbose)


def _DeleteRepo(path_to_repo_dir, llvm_hash):
  """Deletes the temporary repo.

  Args:
    path_to_repo_dir: The absolute path of the repo.
    llvm_hash: The LLVM hash used for the name of the repo.

  Raises:
    ValueError: Failed to delete the repo in that directory.
  """

  if not os.path.isdir(path_to_repo_dir):
    raise ValueError('Invalid directory path provided: %s' % path_to_repo_dir)

  checkout_to_master_cmd = [
      'git', '-C', path_to_repo_dir, 'checkout', 'cros/master'
  ]

  ExecCommandAndCaptureOutput(checkout_to_master_cmd, verbose=verbose)

  reset_head_cmd = ['git', '-C', path_to_repo_dir, 'reset', 'HEAD', '--hard']

  ExecCommandAndCaptureOutput(reset_head_cmd, verbose=verbose)

  delete_repo_cmd = [
      'git', '-C', path_to_repo_dir, 'branch', '-D',
      'llvm-next-update-%s' % llvm_hash
  ]

  ExecCommandAndCaptureOutput(delete_repo_cmd, verbose=verbose)


def GetGerritRepoUploadContents(repo_upload_contents):
  """Parses 'repo upload' to get the Gerrit commit URL and CL number.

  Args:
    repo_upload_contents: The contents of the 'repo upload' command.

  Returns:
    A nametuple that has two (key, value) pairs, where the first pair is the
    Gerrit commit URL and the second pair is the change list number.

  Raises:
    ValueError: The contents of the 'repo upload' command did not contain a
    Gerrit commit URL.
  """

  found_url = re.search(
      r'https://chromium-review.googlesource.com/c/'
      r'chromiumos/overlays/chromiumos-overlay/\+/([0-9]+)',
      repo_upload_contents)

  if not found_url:
    raise ValueError('Failed to find change list URL.')

  cl_number = int(found_url.group(1))

  return CommitContents(url=found_url.group(0), cl_number=cl_number)


def UploadChanges(path_to_repo_dir, llvm_hash, commit_messages):
  """Uploads the changes (updating LLVM next hash and uprev symlink) for review.

  Args:
    path_to_repo_dir: The absolute path to the repo where changes were made.
    llvm_hash: The LLVM hash used for the name of the repo.
    commit_messages: A string of commit message(s) (i.e. '-m [message]'
    of the changes made.

  Returns:
    A nametuple that has two (key, value) pairs, where the first pair is the
    Gerrit commit URL and the second pair is the change list number.

  Raises:
    ValueError: Failed to create a commit or failed to upload the
    changes for review.
  """

  if not os.path.isdir(path_to_repo_dir):
    raise ValueError('Invalid directory path provided: %s' % path_to_repo_dir)

  commit_cmd = [
      'git',
      'commit',
  ]
  commit_cmd.extend(commit_messages)

  ExecCommandAndCaptureOutput(commit_cmd, cwd=path_to_repo_dir, verbose=verbose)

  # Upload the changes for review.
  upload_change_cmd = (
      'yes | repo upload --br=llvm-next-update-%s --no-verify' % llvm_hash)

  # Pylint currently doesn't lint things in py3 mode, and py2 didn't allow
  # users to specify `encoding`s for Popen. Hence, pylint is "wrong" here.
  # pylint: disable=unexpected-keyword-arg

  # NOTE: Need `shell=True` in order to pipe `yes` into `repo upload ...`.
  #
  # The CL URL is sent to 'stderr', so need to redirect 'stderr' to 'stdout'.
  upload_changes_obj = subprocess.Popen(
      upload_change_cmd,
      cwd=path_to_repo_dir,
      shell=True,
      encoding='UTF-8',
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT)

  out, _ = upload_changes_obj.communicate()

  if upload_changes_obj.returncode:  # Failed to upload changes.
    print(out)
    raise ValueError('Failed to upload changes for review')

  return GetGerritRepoUploadContents(out.rstrip())


def CreatePathDictionaryFromPackages(chroot_path, update_packages):
  """Creates a symlink and ebuild path pair dictionary from the packages.

  Args:
    chroot_path: The absolute path to the chroot.
    update_packages: The filtered packages to be updated.

  Returns:
    A dictionary where the key is the absolute path to the symlink
    of the package and the value is the absolute path to the ebuild of
    the package.
  """

  # Construct a list containing the chroot file paths of the package(s).
  chroot_file_paths = GetChrootBuildPaths(chroot_path, update_packages)

  # Construct a list containing the symlink(s) of the package(s).
  symlink_file_paths = _ConvertChrootPathsToSymLinkPaths(
      chroot_path, chroot_file_paths)

  # Create a dictionary where the key is the absolute path of the symlink to
  # the package and the value is the absolute path to the ebuild of the package.
  return GetEbuildPathsFromSymLinkPaths(symlink_file_paths)


def RemovePatchesFromFilesDir(patches_to_remove):
  """Removes the patches from $FILESDIR of a package.

  Args:
    patches_to_remove: A list where each entry is the absolute path to a patch.

  Raises:
    ValueError: Failed to remove a patch in $FILESDIR.
  """

  for cur_patch in patches_to_remove:
    remove_patch_cmd = [
        'git', '-C',
        os.path.dirname(cur_patch), 'rm', '-f', cur_patch
    ]

    ExecCommandAndCaptureOutput(remove_patch_cmd, verbose=verbose)


def StagePatchMetadataFileForCommit(patch_metadata_file_path):
  """Stages the updated patch metadata file for commit.

  Args:
    patch_metadata_file_path: The absolute path to the patch metadata file.

  Raises:
    ValueError: Failed to stage the patch metadata file for commit or invalid
    patch metadata file.
  """

  if not os.path.isfile(patch_metadata_file_path):
    raise ValueError(
        'Invalid patch metadata file provided: %s' % patch_metadata_file_path)

  # Cmd to stage the patch metadata file for commit.
  stage_patch_file = [
      'git', '-C',
      os.path.dirname(patch_metadata_file_path), 'add', patch_metadata_file_path
  ]

  ExecCommandAndCaptureOutput(stage_patch_file, verbose=verbose)


def StagePackagesPatchResultsForCommit(package_info_dict, commit_messages):
  """Stages the patch results of the packages to the commit message.

  Args:
    package_info_dict: A dictionary where the key is the package name and the
    value is a dictionary that contains information about the patches of the
    package (key).
    commit_messages: The commit message that has the updated ebuilds and
    upreving information.
  """

  # For each package, check if any patches for that package have
  # changed, if so, add which patches have changed to the commit
  # message.
  for package_name, patch_info_dict in package_info_dict.items():
    if patch_info_dict['disabled_patches'] or \
        patch_info_dict['removed_patches'] or \
        patch_info_dict['modified_metadata']:
      cur_package_header = 'For the package %s:' % package_name
      commit_messages.append('-m %s' % cur_package_header)

    # Add to the commit message that the patch metadata file was modified.
    if patch_info_dict['modified_metadata']:
      patch_metadata_path = patch_info_dict['modified_metadata']
      commit_messages.append('-m %s' % 'The patch metadata file %s was '
                             'modified' % os.path.basename(patch_metadata_path))

      StagePatchMetadataFileForCommit(patch_metadata_path)

    # Add each disabled patch to the commit message.
    if patch_info_dict['disabled_patches']:
      commit_messages.append('-m %s' % 'The following patches were disabled:')

      for patch_path in patch_info_dict['disabled_patches']:
        commit_messages.append('-m %s' % os.path.basename(patch_path))

    # Add each removed patch to the commit message.
    if patch_info_dict['removed_patches']:
      commit_messages.append('-m %s' % 'The following patches were removed:')

      for patch_path in patch_info_dict['removed_patches']:
        commit_messages.append('-m %s' % os.path.basename(patch_path))

      RemovePatchesFromFilesDir(patch_info_dict['removed_patches'])

  return commit_messages


def UpdatePackages(packages, llvm_hash, llvm_version, chroot_path,
                   patch_metadata_file, mode, svn_option):
  """Updates the package's LLVM_NEXT_HASH and uprevs the ebuild.

  A temporary repo is created for the changes. The changes are
  then uploaded for review.

  Args:
    packages: A list of all the packages that are going to be updated.
    llvm_hash: The LLVM hash to use for 'LLVM_NEXT_HASH'.
    llvm_version: The LLVM version of the 'llvm_hash'.
    chroot_path: The absolute path to the chroot.
    patch_metadata_file: The name of the .json file in '$FILESDIR/' that has
    the patches and its metadata.
    mode: The mode of the patch manager when handling an applicable patch
    that failed to apply.
      Ex: 'FailureModes.FAIL'
    svn_option: The git hash to use based off of the svn option.
      Ex: 'google3', 'tot', or <svn_version> such as 365123

  Returns:
    A nametuple that has two (key, value) pairs, where the first pair is the
    Gerrit commit URL and the second pair is the change list number.
  """

  # Determines whether to print the result of each executed command.
  llvm_patch_management.verbose = verbose

  # Construct a dictionary where the key is the absolute path of the symlink to
  # the package and the value is the absolute path to the ebuild of the package.
  paths_dict = CreatePathDictionaryFromPackages(chroot_path, packages)

  repo_path = os.path.dirname(next(iter(paths_dict.values())))

  _CreateRepo(repo_path, llvm_hash)

  try:
    if svn_option in get_llvm_hash.KNOWN_HASH_SOURCES:
      commit_message_header = ('llvm-next/%s: upgrade to %s (r%d)' %
                               (svn_option, llvm_hash, llvm_version))
    else:
      commit_message_header = (
          'llvm-next: upgrade to %s (r%d)' % (llvm_hash, llvm_version))

    commit_messages = ['-m %s' % commit_message_header]

    commit_messages.append('-m The following packages have been updated:')

    # Holds the list of packages that are updating.
    packages = []

    # Iterate through the dictionary.
    #
    # For each iteration:
    # 1) Update the ebuild's LLVM_NEXT_HASH.
    # 2) Uprev the ebuild (symlink).
    # 3) Add the modified package to the commit message.
    for symlink_path, ebuild_path in paths_dict.items():
      path_to_ebuild_dir = os.path.dirname(ebuild_path)

      UpdateBuildLLVMNextHash(ebuild_path, llvm_hash, llvm_version)

      UprevEbuild(symlink_path)

      cur_dir_name = os.path.basename(path_to_ebuild_dir)
      parent_dir_name = os.path.basename(os.path.dirname(path_to_ebuild_dir))

      packages.append('%s/%s' % (parent_dir_name, cur_dir_name))

      new_commit_message = '%s/%s' % (parent_dir_name, cur_dir_name)

      commit_messages.append('-m %s' % new_commit_message)

    # Handle the patches for each package.
    package_info_dict = llvm_patch_management.UpdatePackagesPatchMetadataFile(
        chroot_path, llvm_version, patch_metadata_file, packages, mode)

    # Update the commit message if changes were made to a package's patches.
    commit_messages = StagePackagesPatchResultsForCommit(
        package_info_dict, commit_messages)

    change_list = UploadChanges(repo_path, llvm_hash, commit_messages)

  finally:
    _DeleteRepo(repo_path, llvm_hash)

  return change_list


def main():
  """Updates the LLVM next hash for each package.

  Raises:
    AssertionError: The script was run inside the chroot.
  """

  VerifyOutsideChroot()

  args_output = GetCommandLineArgs()

  svn_option = args_output.llvm_version

  llvm_hash, llvm_version = GetLLVMHashAndVersionFromSVNOption(svn_option)

  change_list = UpdatePackages(
      args_output.update_packages, llvm_hash, llvm_version,
      args_output.chroot_path, args_output.patch_metadata_file,
      FailureModes(args_output.failure_mode), svn_option)

  print('Successfully updated packages to %d' % llvm_version)
  print('Gerrit URL: %s' % change_list.url)
  print('Change list number: %d' % change_list.cl_number)


if __name__ == '__main__':
  main()
