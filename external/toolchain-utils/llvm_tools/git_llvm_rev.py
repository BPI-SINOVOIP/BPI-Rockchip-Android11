#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Maps LLVM git SHAs to synthetic revision numbers and back.

Revision numbers are all of the form '(branch_name, r1234)'. As a shorthand,
r1234 is parsed as '(master, 1234)'.
"""

from __future__ import print_function

import argparse
import re
import subprocess
import sys
import typing as t

# Note that after base_llvm_sha, we reach The Wild West(TM) of commits.
# So reasonable input that could break us includes:
#
#   Revert foo
#
#   This reverts foo, which had the commit message:
#
#   bar
#   llvm-svn: 375505
#
# While saddening, this is something we should probably try to handle
# reasonably.
base_llvm_revision = 375505
base_llvm_sha = '186155b89c2d2a2f62337081e3ca15f676c9434b'

# Represents an LLVM git checkout:
#  - |dir| is the directory of the LLVM checkout
#  - |remote| is the name of the LLVM remote. Generally it's "origin".
LLVMConfig = t.NamedTuple('LLVMConfig', (('remote', str), ('dir', str)))


class Rev(t.NamedTuple('Rev', (('branch', str), ('number', int)))):
  """Represents a LLVM 'revision', a shorthand identifies a LLVM commit."""

  @staticmethod
  def parse(rev: str) -> 'Rev':
    """Parses a Rev from the given string.

    Raises a ValueError on a failed parse.
    """
    # Revs are parsed into (${branch_name}, r${commits_since_base_commit})
    # pairs.
    #
    # We support r${commits_since_base_commit} as shorthand for
    # (master, r${commits_since_base_commit}).
    if rev.startswith('r'):
      branch_name = 'master'
      rev_string = rev[1:]
    else:
      match = re.match(r'\((.+), r(\d+)\)', rev)
      if not match:
        raise ValueError("%r isn't a valid revision" % rev)

      branch_name, rev_string = match.groups()

    return Rev(branch=branch_name, number=int(rev_string))

  def __str__(self) -> str:
    branch_name, number = self
    if branch_name == 'master':
      return 'r%d' % number
    return '(%s, r%d)' % (branch_name, number)


def is_git_sha(xs: str) -> bool:
  """Returns whether the given string looks like a valid git commit SHA."""
  return len(xs) > 6 and len(xs) <= 40 and all(
      x.isdigit() or 'a' <= x.lower() <= 'f' for x in xs)


def check_output(command: t.List[str], cwd: str) -> str:
  """Shorthand for subprocess.check_output. Auto-decodes any stdout."""
  result = subprocess.run(
      command,
      cwd=cwd,
      check=True,
      stdin=subprocess.DEVNULL,
      stdout=subprocess.PIPE,
      encoding='utf-8',
  )
  return result.stdout


def translate_prebase_sha_to_rev_number(llvm_config: LLVMConfig,
                                        sha: str) -> int:
  """Translates a sha to a revision number (e.g., "llvm-svn: 1234").

  This function assumes that the given SHA is an ancestor of |base_llvm_sha|.
  """
  commit_message = check_output(
      ['git', 'log', '-n1', '--format=%B', sha],
      cwd=llvm_config.dir,
  )
  last_line = commit_message.strip().splitlines()[-1]
  svn_match = re.match(r'^llvm-svn: (\d+)$', last_line)

  if not svn_match:
    raise ValueError(
        f"No llvm-svn line found for {sha}, which... shouldn't happen?")

  return int(svn_match.group(1))


def translate_sha_to_rev(llvm_config: LLVMConfig, sha_or_ref: str) -> Rev:
  """Translates a sha or git ref to a Rev."""

  if is_git_sha(sha_or_ref):
    sha = sha_or_ref
  else:
    sha = check_output(
        ['git', 'rev-parse', sha_or_ref],
        cwd=llvm_config.dir,
    )
    sha = sha.strip()

  merge_base = check_output(
      ['git', 'merge-base', base_llvm_sha, sha],
      cwd=llvm_config.dir,
  )
  merge_base = merge_base.strip()

  if merge_base == base_llvm_sha:
    result = check_output(
        [
            'git',
            'rev-list',
            '--count',
            '--first-parent',
            f'{base_llvm_sha}..{sha}',
        ],
        cwd=llvm_config.dir,
    )
    count = int(result.strip())
    return Rev(branch='master', number=count + base_llvm_revision)

  # Otherwise, either:
  # - |merge_base| is |sha| (we have a guaranteed llvm-svn number on |sha|)
  # - |merge_base| is neither (we have a guaranteed llvm-svn number on
  #                            |merge_base|, but not |sha|)
  merge_base_number = translate_prebase_sha_to_rev_number(
      llvm_config, merge_base)
  if merge_base == sha:
    return Rev(branch='master', number=merge_base_number)

  distance_from_base = check_output(
      [
          'git',
          'rev-list',
          '--count',
          '--first-parent',
          f'{merge_base}..{sha}',
      ],
      cwd=llvm_config.dir,
  )

  revision_number = merge_base_number + int(distance_from_base.strip())
  branches_containing = check_output(
      ['git', 'branch', '-r', '--contains', sha],
      cwd=llvm_config.dir,
  )

  candidates = []

  prefix = llvm_config.remote + '/'
  for branch in branches_containing.splitlines():
    branch = branch.strip()
    if branch.startswith(prefix):
      candidates.append(branch[len(prefix):])

  if not candidates:
    raise ValueError(
        f'No viable branches found from {llvm_config.remote} with {sha}')

  if len(candidates) != 1:
    raise ValueError(
        f'Ambiguity: multiple branches from {llvm_config.remote} have {sha}: '
        f'{sorted(candidates)}')

  return Rev(branch=candidates[0], number=revision_number)


def parse_git_commit_messages(stream: t.Iterable[str],
                              separator: str) -> t.Iterable[t.Tuple[str, str]]:
  """Parses a stream of git log messages.

  These are expected to be in the format:

  40 character sha
  commit
  message
  body
  separator
  40 character sha
  commit
  message
  body
  separator
  """

  lines = iter(stream)
  while True:
    # Looks like a potential bug in pylint? crbug.com/1041148
    # pylint: disable=stop-iteration-return
    sha = next(lines, None)
    if sha is None:
      return

    sha = sha.strip()
    assert is_git_sha(sha), f'Invalid git SHA: {sha}'

    message = []
    for line in lines:
      if line.strip() == separator:
        break
      message.append(line)

    yield sha, ''.join(message)


def translate_prebase_rev_to_sha(llvm_config: LLVMConfig, rev: Rev) -> str:
  """Translates a Rev to a SHA.

  This function assumes that the given rev refers to a commit that's an
  ancestor of |base_llvm_sha|.
  """
  # Because reverts may include reverted commit messages, we can't just |-n1|
  # and pick that.
  separator = '>!' * 80
  looking_for = f'llvm-svn: {rev.number}'

  git_command = [
      'git', 'log', '--grep', f'^{looking_for}$', f'--format=%H%n%B{separator}',
      base_llvm_sha
  ]

  subp = subprocess.Popen(
      git_command,
      cwd=llvm_config.dir,
      stdin=subprocess.DEVNULL,
      stdout=subprocess.PIPE,
      encoding='utf-8',
  )

  with subp:
    for sha, message in parse_git_commit_messages(subp.stdout, separator):
      last_line = message.splitlines()[-1]
      if last_line.strip() == looking_for:
        subp.terminate()
        return sha

  if subp.returncode:
    raise subprocess.CalledProcessError(subp.returncode, git_command)
  raise ValueError(f'No commit with revision {rev} found')


def translate_rev_to_sha(llvm_config: LLVMConfig, rev: Rev) -> str:
  """Translates a Rev to a SHA.

  Raises a ValueError if the given Rev doesn't exist in the given config.
  """
  branch, number = rev

  if branch == 'master':
    if number < base_llvm_revision:
      return translate_prebase_rev_to_sha(llvm_config, rev)
    base_sha = base_llvm_sha
    base_revision_number = base_llvm_revision
  else:
    base_sha = check_output(
        ['git', 'merge-base', base_llvm_sha, f'{llvm_config.remote}/{branch}'],
        cwd=llvm_config.dir,
    )
    base_sha = base_sha.strip()
    if base_sha == base_llvm_sha:
      base_revision_number = base_llvm_revision
    else:
      base_revision_number = translate_prebase_sha_to_rev_number(
          llvm_config, base_sha)

  # Alternatively, we could |git log --format=%H|, but git is *super* fast
  # about rev walking/counting locally compared to long |log|s, so we walk back
  # twice.
  head = check_output(
      ['git', 'rev-parse', f'{llvm_config.remote}/{branch}'],
      cwd=llvm_config.dir,
  )
  branch_head_sha = head.strip()

  commit_number = number - base_revision_number
  revs_between_str = check_output(
      [
          'git',
          'rev-list',
          '--count',
          '--first-parent',
          f'{base_sha}..{branch_head_sha}',
      ],
      cwd=llvm_config.dir,
  )
  revs_between = int(revs_between_str.strip())

  commits_behind_head = revs_between - commit_number
  if commits_behind_head < 0:
    raise ValueError(
        f'Revision {rev} is past {llvm_config.remote}/{branch}. Try updating '
        'your tree?')

  result = check_output(
      ['git', 'rev-parse', f'{branch_head_sha}~{commits_behind_head}'],
      cwd=llvm_config.dir,
  )

  return result.strip()


def find_root_llvm_dir(root_dir: str = '.') -> str:
  """Finds the root of an LLVM directory starting at |root_dir|.

  Raises a subprocess.CalledProcessError if no git directory is found.
  """
  result = check_output(
      ['git', 'rev-parse', '--show-toplevel'],
      cwd=root_dir,
  )
  return result.strip()


def main(argv: t.List[str]) -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--llvm_dir',
      help='LLVM directory to consult for git history, etc. Autodetected '
      'if cwd is inside of an LLVM tree')
  parser.add_argument(
      '--upstream',
      default='origin',
      help="LLVM upstream's remote name. Defaults to %(default)s.")
  sha_or_rev = parser.add_mutually_exclusive_group(required=True)
  sha_or_rev.add_argument(
      '--sha', help='A git SHA (or ref) to convert to a rev')
  sha_or_rev.add_argument('--rev', help='A rev to convert into a sha')
  opts = parser.parse_args(argv)

  llvm_dir = opts.llvm_dir
  if llvm_dir is None:
    try:
      llvm_dir = find_root_llvm_dir()
    except subprocess.CalledProcessError:
      parser.error("Couldn't autodetect an LLVM tree; please use --llvm_dir")

  config = LLVMConfig(
      remote=opts.upstream,
      dir=opts.llvm_dir or find_root_llvm_dir(),
  )

  if opts.sha:
    rev = translate_sha_to_rev(config, opts.sha)
    print(rev)
  else:
    sha = translate_rev_to_sha(config, Rev.parse(opts.rev))
    print(sha)


if __name__ == '__main__':
  main(sys.argv[1:])
