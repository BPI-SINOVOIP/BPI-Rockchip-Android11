#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Returns the latest LLVM version's hash."""

from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager

import git_llvm_rev
from subprocess_helpers import CheckCommand, check_output

_LLVM_GIT_URL = ('https://chromium.googlesource.com/external/github.com/llvm'
                 '/llvm-project')

KNOWN_HASH_SOURCES = {'google3', 'google3-unstable', 'tot'}


def GetVersionFrom(src_dir, git_hash):
  """Obtain an SVN-style version number based on the LLVM git hash passed in.

  Args:
    src_dir: LLVM's source directory.
    git_hash: The git hash.

  Returns:
    An SVN-style version number associated with the git hash.
  """

  version = git_llvm_rev.translate_sha_to_rev(
      git_llvm_rev.LLVMConfig(remote='origin', dir=src_dir), git_hash)
  # Note: branches aren't supported
  assert version.branch == 'master', version.branch
  return version.number


def GetGitHashFrom(src_dir, version):
  """Finds the commit hash(es) of the LLVM version in the git log history.

  Args:
    src_dir: The LLVM source tree.
    version: The version number.

  Returns:
    A git hash string corresponding to the version number.

  Raises:
    subprocess.CalledProcessError: Failed to find a git hash.
  """

  return git_llvm_rev.translate_rev_to_sha(
      git_llvm_rev.LLVMConfig(remote='origin', dir=src_dir),
      git_llvm_rev.Rev(branch='master', number=version))


@contextmanager
def CreateTempLLVMRepo(temp_dir):
  """Adds a LLVM worktree to 'temp_dir'.

  Creating a worktree because the LLVM source tree in
  '../toolchain-utils/llvm_tools/llvm-project-copy' should not be modified.

  This is useful for applying patches to a source tree but do not want to modify
  the actual LLVM source tree in 'llvm-project-copy'.

  Args:
    temp_dir: An absolute path to the temporary directory to put the worktree in
    (obtained via 'tempfile.mkdtemp()').

  Returns:
    The absolute path to 'temp_dir'.

  Raises:
    subprocess.CalledProcessError: Failed to remove the worktree.
    ValueError: Failed to add a worktree.
  """

  abs_path_to_llvm_project_dir = GetAndUpdateLLVMProjectInLLVMTools()

  add_worktree_cmd = [
      'git', '-C', abs_path_to_llvm_project_dir, 'worktree', 'add', '--detach',
      temp_dir, 'master'
  ]

  CheckCommand(add_worktree_cmd)

  try:
    yield temp_dir
  finally:
    if os.path.isdir(temp_dir):
      check_output([
          'git', '-C', abs_path_to_llvm_project_dir, 'worktree', 'remove', '-f',
          temp_dir
      ])


def GetAndUpdateLLVMProjectInLLVMTools():
  """Gets the absolute path to 'llvm-project-copy' directory in 'llvm_tools'.

  The intent of this function is to avoid cloning the LLVM repo and then
  discarding the contents of the repo. The function will create a directory
  in '../toolchain-utils/llvm_tools' called 'llvm-project-copy' if this
  directory does not exist yet. If it does not exist, then it will use the
  LLVMHash() class to clone the LLVM repo into 'llvm-project-copy'. Otherwise,
  it will clean the contents of that directory and then fetch from the chromium
  LLVM mirror. In either case, this function will return the absolute path to
  'llvm-project-copy' directory.

  Raises:
    ValueError: LLVM repo (in 'llvm-project-copy' dir.) has changes or failed to
    checkout to master or failed to fetch from chromium mirror of LLVM.
  """

  abs_path_to_llvm_tools_dir = os.path.dirname(os.path.abspath(__file__))

  abs_path_to_llvm_project_dir = os.path.join(abs_path_to_llvm_tools_dir,
                                              'llvm-project-copy')

  if not os.path.isdir(abs_path_to_llvm_project_dir):
    print(
        'Checking out LLVM from scratch. This could take a while...',
        file=sys.stderr)
    os.mkdir(abs_path_to_llvm_project_dir)

    LLVMHash().CloneLLVMRepo(abs_path_to_llvm_project_dir)
  else:
    # `git status` has a '-s'/'--short' option that shortens the output.
    # With the '-s' option, if no changes were made to the LLVM repo, then the
    # output (assigned to 'repo_status') would be empty.
    repo_status = check_output(
        ['git', '-C', abs_path_to_llvm_project_dir, 'status', '-s'])

    if repo_status.rstrip():
      raise ValueError('LLVM repo in %s has changes, please remove.' %
                       abs_path_to_llvm_project_dir)

    checkout_to_master_cmd = [
        'git', '-C', abs_path_to_llvm_project_dir, 'checkout', 'master'
    ]

    CheckCommand(checkout_to_master_cmd)

    update_master_cmd = ['git', '-C', abs_path_to_llvm_project_dir, 'pull']

    CheckCommand(update_master_cmd)

  return abs_path_to_llvm_project_dir


def GetGoogle3LLVMVersion(stable):
  """Gets the latest google3 LLVM version.

  Returns:
    The latest LLVM SVN version as an integer.

  Raises:
    subprocess.CalledProcessError: An invalid path has been provided to the
    `cat` command.
  """

  subdir = 'stable' if stable else 'llvm_unstable'

  # Cmd to get latest google3 LLVM version.
  cmd = [
      'cat',
      os.path.join('/google/src/head/depot/google3/third_party/crosstool/v18',
                   subdir, 'installs/llvm/git_origin_rev_id')
  ]

  # Get latest version.
  git_hash = check_output(cmd)

  # Change type to an integer
  return GetVersionFrom(GetAndUpdateLLVMProjectInLLVMTools(), git_hash.rstrip())


def is_svn_option(svn_option):
  """Validates whether the argument (string) is a git hash option.

  The argument is used to find the git hash of LLVM.

  Args:
    svn_option: The option passed in as a command line argument.

  Raises:
    ValueError: Invalid svn option provided.
  """

  if svn_option.lower() in KNOWN_HASH_SOURCES:
    return svn_option.lower()

  try:
    svn_version = int(svn_option)

    return svn_version

  # Unable to convert argument to an int, so the option is invalid.
  #
  # Ex: 'one'.
  except ValueError:
    pass

  raise ValueError('Invalid LLVM git hash option provided: %s' % svn_option)


def GetLLVMHashAndVersionFromSVNOption(svn_option):
  """Gets the LLVM hash and LLVM version based off of the svn option.

  Args:
    svn_option: A valid svn option obtained from the command line.
      Ex: 'google3', 'tot', or <svn_version> such as 365123.

  Returns:
    A tuple that is the LLVM git hash and LLVM version.
  """

  new_llvm_hash = LLVMHash()

  # Determine which LLVM git hash to retrieve.
  if svn_option == 'tot':
    git_hash = new_llvm_hash.GetTopOfTrunkGitHash()
    version = GetVersionFrom(GetAndUpdateLLVMProjectInLLVMTools(), git_hash)
  elif isinstance(svn_option, int):
    version = svn_option
    git_hash = GetGitHashFrom(GetAndUpdateLLVMProjectInLLVMTools(), version)
  else:
    assert svn_option in ('google3', 'google3-unstable')
    version = GetGoogle3LLVMVersion(stable=svn_option == 'google3')

    git_hash = GetGitHashFrom(GetAndUpdateLLVMProjectInLLVMTools(), version)

  return git_hash, version


class LLVMHash(object):
  """Provides methods to retrieve a LLVM hash."""

  @staticmethod
  @contextmanager
  def CreateTempDirectory():
    temp_dir = tempfile.mkdtemp()

    try:
      yield temp_dir
    finally:
      if os.path.isdir(temp_dir):
        shutil.rmtree(temp_dir, ignore_errors=True)

  def CloneLLVMRepo(self, temp_dir):
    """Clones the LLVM repo.

    Args:
      temp_dir: The temporary directory to clone the repo to.

    Raises:
      ValueError: Failed to clone the LLVM repo.
    """

    clone_cmd = ['git', 'clone', _LLVM_GIT_URL, temp_dir]

    clone_cmd_obj = subprocess.Popen(clone_cmd, stderr=subprocess.PIPE)
    _, stderr = clone_cmd_obj.communicate()

    if clone_cmd_obj.returncode:
      raise ValueError('Failed to clone the LLVM repo: %s' % stderr)

  def GetLLVMHash(self, version):
    """Retrieves the LLVM hash corresponding to the LLVM version passed in.

    Args:
      version: The LLVM version to use as a delimiter.

    Returns:
      The hash as a string that corresponds to the LLVM version.
    """

    hash_value = GetGitHashFrom(GetAndUpdateLLVMProjectInLLVMTools(), version)
    return hash_value

  def GetGoogle3LLVMHash(self):
    """Retrieves the google3 LLVM hash."""

    return self.GetLLVMHash(GetGoogle3LLVMVersion(stable=True))

  def GetGoogle3UnstableLLVMHash(self):
    """Retrieves the LLVM hash of google3's unstable compiler."""
    return self.GetLLVMHash(GetGoogle3LLVMVersion(stable=False))

  def GetTopOfTrunkGitHash(self):
    """Gets the latest git hash from top of trunk of LLVM."""

    path_to_master_branch = 'refs/heads/master'

    llvm_tot_git_hash_cmd = [
        'git', 'ls-remote', _LLVM_GIT_URL, path_to_master_branch
    ]

    llvm_tot_git_hash = check_output(llvm_tot_git_hash_cmd)

    return llvm_tot_git_hash.rstrip().split()[0]


def main():
  """Prints the git hash of LLVM.

  Parses the command line for the optional command line
  arguments.
  """

  # Create parser and add optional command-line arguments.
  parser = argparse.ArgumentParser(description='Finds the LLVM hash.')
  parser.add_argument(
      '--llvm_version',
      type=is_svn_option,
      required=True,
      help='which git hash of LLVM to find. Either a svn revision, or one '
      'of %s' % sorted(KNOWN_HASH_SOURCES))

  # Parse command-line arguments.
  args_output = parser.parse_args()

  cur_llvm_version = args_output.llvm_version

  new_llvm_hash = LLVMHash()

  if isinstance(cur_llvm_version, int):
    # Find the git hash of the specific LLVM version.
    print(new_llvm_hash.GetLLVMHash(cur_llvm_version))
  elif cur_llvm_version == 'google3':
    print(new_llvm_hash.GetGoogle3LLVMHash())
  elif cur_llvm_version == 'google3-unstable':
    print(new_llvm_hash.GetGoogle3UnstableLLVMHash())
  else:
    assert cur_llvm_version == 'tot'
    print(new_llvm_hash.GetTopOfTrunkGitHash())


if __name__ == '__main__':
  main()
