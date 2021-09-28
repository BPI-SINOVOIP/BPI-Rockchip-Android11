#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build script that builds a binary from a bundle."""

from __future__ import print_function

import argparse
import os.path
import re
import subprocess
import sys


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--config',
      required=True,
      choices=['cros.hardened', 'cros.nonhardened', 'cros.host', 'android'])
  parser.add_argument('--use_ccache', required=True, choices=['true', 'false'])
  parser.add_argument(
      '--use_llvm_next', required=True, choices=['true', 'false'])
  parser.add_argument('--output_file', required=True, type=str)
  return parser.parse_args()


def calc_go_args(args, version):
  ldFlags = [
      '-X',
      'main.ConfigName=' + args.config,
      '-X',
      'main.UseCCache=' + args.use_ccache,
      '-X',
      'main.UseLlvmNext=' + args.use_llvm_next,
      '-X',
      'main.Version=' + version,
  ]

  # If the wrapper is intended for Chrome OS, we need to use libc's exec.
  extra_args = []
  if 'cros' in args.config:
    extra_args = ['-tags', 'libc_exec']

  return [
      'go', 'build', '-o',
      os.path.abspath(args.output_file), '-ldflags', ' '.join(ldFlags)
  ] + extra_args


def read_version(build_dir):
  version_path = os.path.join(build_dir, 'VERSION')
  if os.path.exists(version_path):
    with open(version_path, 'r') as r:
      return r.read()

  last_commit_msg = subprocess.check_output(
      ['git', '-C', build_dir, 'log', '-1', '--pretty=%B'], encoding='utf-8')
  # Use last found change id to support reverts as well.
  change_ids = re.findall(r'Change-Id: (\w+)', last_commit_msg)
  if not change_ids:
    sys.exit("Couldn't find Change-Id in last commit message.")
  return change_ids[-1]


def main():
  args = parse_args()
  build_dir = os.path.dirname(__file__)
  version = read_version(build_dir)
  # Note: Go does not support using absolute package names.
  # So we run go inside the directory of the the build file.
  sys.exit(subprocess.call(calc_go_args(args, version), cwd=build_dir))


if __name__ == '__main__':
  main()
