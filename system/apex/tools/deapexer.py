#!/usr/bin/env python
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""deapexer is a tool that prints out content of an APEX.

To print content of an APEX to stdout:
  deapexer list foo.apex

To extract content of an APEX to the given directory:
  deapexer extract foo.apex dest
"""
from __future__ import print_function

import argparse
import os
import shutil
import sys
import subprocess
import tempfile
import zipfile
import apex_manifest

class ApexImageEntry(object):

  def __init__(self, name, base_dir, permissions, size, is_directory=False, is_symlink=False):
    self._name = name
    self._base_dir = base_dir
    self._permissions = permissions
    self._size = size
    self._is_directory = is_directory
    self._is_symlink = is_symlink

  @property
  def name(self):
    return self._name

  @property
  def full_path(self):
    return os.path.join(self._base_dir, self._name)

  @property
  def is_directory(self):
    return self._is_directory

  @property
  def is_symlink(self):
    return self._is_symlink

  @property
  def is_regular_file(self):
    return not self.is_directory and not self.is_symlink

  @property
  def permissions(self):
    return self._permissions

  @property
  def size(self):
    return self._size

  def __str__(self):
    ret = ''
    if self._is_directory:
      ret += 'd'
    elif self._is_symlink:
      ret += 'l'
    else:
      ret += '-'

    def mask_as_string(m):
      ret = 'r' if m & 4 == 4 else '-'
      ret += 'w' if m & 2 == 2 else '-'
      ret += 'x' if m & 1 == 1 else '-'
      return ret

    ret += mask_as_string(self._permissions >> 6)
    ret += mask_as_string((self._permissions >> 3) & 7)
    ret += mask_as_string(self._permissions & 7)

    return ret + ' ' + self._size + ' ' + self._name


class ApexImageDirectory(object):

  def __init__(self, path, entries, apex):
    self._path = path
    self._entries = sorted(entries, key=lambda e: e.name)
    self._apex = apex

  def list(self, is_recursive=False):
    for e in self._entries:
      yield e
      if e.is_directory and e.name != '.' and e.name != '..':
        for ce in self.enter_subdir(e).list(is_recursive):
          yield ce

  def enter_subdir(self, entry):
    return self._apex._list(self._path + entry.name + '/')

  def extract(self, dest):
    path = self._path
    self._apex._extract(self._path, dest)


class Apex(object):

  def __init__(self, args):
    self._debugfs = args.debugfs_path
    self._apex = args.apex
    self._tempdir = tempfile.mkdtemp()
    # TODO(b/139125405): support flattened APEXes.
    with zipfile.ZipFile(self._apex, 'r') as zip_ref:
      self._payload = zip_ref.extract('apex_payload.img', path=self._tempdir)
    self._cache = {}

  def __del__(self):
    shutil.rmtree(self._tempdir)

  def __enter__(self):
    return self._list('./')

  def __exit__(self, type, value, traceback):
    pass

  def _list(self, path):
    if path in self._cache:
      return self._cache[path]
    process = subprocess.Popen([self._debugfs, '-R', 'ls -l -p %s' % path, self._payload],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True)
    stdout, _ = process.communicate()
    res = str(stdout)
    entries = []
    for line in res.split('\n'):
      if not line:
        continue
      parts = line.split('/')
      if len(parts) != 8:
        continue
      name = parts[5]
      if not name:
        continue
      bits = parts[2]
      size = parts[6]
      entries.append(ApexImageEntry(name, base_dir=path, permissions=int(bits[3:], 8), size=size,
                                    is_directory=bits[1]=='4', is_symlink=bits[1]=='2'))
    return ApexImageDirectory(path, entries, self)

  def _extract(self, path, dest):
    process = subprocess.Popen([self._debugfs, '-R', 'rdump %s %s' % (path, dest), self._payload],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True)
    _, stderr = process.communicate()
    if process.returncode != 0:
      print(stderr, file=sys.stderr)


def RunList(args):
  with Apex(args) as apex:
    for e in apex.list(is_recursive=True):
      if e.is_directory:
        continue
      if args.size:
        print(e.size, e.full_path)
      else:
        print(e.full_path)


def RunExtract(args):
  with Apex(args) as apex:
    if not os.path.exists(args.dest):
      os.makedirs(args.dest, mode=0o755)
    apex.extract(args.dest)
    shutil.rmtree(os.path.join(args.dest, "lost+found"))


def RunInfo(args):
  manifest = apex_manifest.fromApex(args.apex)
  print(apex_manifest.toJsonString(manifest))


def main(argv):
  parser = argparse.ArgumentParser()

  debugfs_default = 'debugfs'  # assume in PATH by default
  if 'ANDROID_HOST_OUT' in os.environ:
    debugfs_default = '%s/bin/debugfs_static' % os.environ['ANDROID_HOST_OUT']
  parser.add_argument('--debugfs_path', help='The path to debugfs binary', default=debugfs_default)

  subparsers = parser.add_subparsers(required=True, dest='cmd')

  parser_list = subparsers.add_parser('list', help='prints content of an APEX to stdout')
  parser_list.add_argument('apex', type=str, help='APEX file')
  parser_list.add_argument('--size', help='also show the size of the files', action="store_true")
  parser_list.set_defaults(func=RunList)

  parser_extract = subparsers.add_parser('extract', help='extracts content of an APEX to the given '
                                                         'directory')
  parser_extract.add_argument('apex', type=str, help='APEX file')
  parser_extract.add_argument('dest', type=str, help='Directory to extract content of APEX to')
  parser_extract.set_defaults(func=RunExtract)

  parser_info = subparsers.add_parser('info', help='prints APEX manifest')
  parser_info.add_argument('apex', type=str, help='APEX file')
  parser_info.set_defaults(func=RunInfo)

  args = parser.parse_args(argv)

  args.func(args)


if __name__ == '__main__':
  main(sys.argv[1:])
