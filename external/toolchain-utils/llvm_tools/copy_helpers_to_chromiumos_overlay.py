#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clones helper scripts into chromiumos-overlay.

Some files in here also need to live in chromiumos-overlay (e.g., the
patch_manager ones). This script simplifies the copying of those around.
"""

# Necessary until crbug.com/1006448 is fixed
from __future__ import print_function

import argparse
import os
import shutil
import sys


def _find_repo_root(script_root):
  repo_root = os.path.abspath(os.path.join(script_root, '../../../../'))
  if not os.path.isdir(os.path.join(repo_root, '.repo')):
    return None
  return repo_root


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--chroot_path',
      help="Path to where CrOS' source tree lives. Will autodetect if you're "
      'running this from inside the CrOS source tree.')
  args = parser.parse_args()

  my_dir = os.path.abspath(os.path.dirname(__file__))

  repo_root = args.chroot_path
  if repo_root is None:
    repo_root = _find_repo_root(my_dir)
    if repo_root is None:
      sys.exit("Couldn't detect the CrOS checkout root; please provide a "
               'value for --chroot_path')

  chromiumos_overlay = os.path.join(repo_root,
                                    'src/third_party/chromiumos-overlay')

  clone_files = [
      'failure_modes.py',
      'get_llvm_hash.py',
      'git_llvm_rev.py',
      'patch_manager.py',
      'subprocess_helpers.py',
  ]

  filesdir = os.path.join(chromiumos_overlay,
                          'sys-devel/llvm/files/patch_manager')
  for f in clone_files:
    source = os.path.join(my_dir, f)
    dest = os.path.join(filesdir, f)
    print('%r => %r' % (source, dest))
    shutil.copyfile(source, dest)


if __name__ == '__main__':
  main()
