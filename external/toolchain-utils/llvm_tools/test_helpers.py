# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions for unit testing."""

from __future__ import print_function

from contextlib import contextmanager
from tempfile import mkstemp
import json
import os


class ArgsOutputTest(object):
  """Testing class to simulate a argument parser object."""

  def __init__(self, svn_option='google3'):
    self.chroot_path = '/abs/path/to/chroot'
    self.last_tested = '/abs/path/to/last_tested_file.json'
    self.llvm_version = svn_option
    self.verbose = False
    self.extra_change_lists = None
    self.options = ['latest-toolchain']
    self.builders = ['some-builder']


# FIXME: Migrate modules with similar helper to use this module.
def CallCountsToMockFunctions(mock_function):
  """A decorator that passes a call count to the function it decorates.

  Examples:
    @CallCountsToMockFunctions
    def foo(call_count):
      return call_count
    ...
    ...
    [foo(), foo(), foo()]
    [0, 1, 2]
  """

  counter = [0]

  def Result(*args, **kwargs):
    # For some values of `counter`, the mock function would simulate raising
    # an exception, so let the test case catch the exception via
    # `unittest.TestCase.assertRaises()` and to also handle recursive functions.
    prev_counter = counter[0]
    counter[0] += 1

    ret_value = mock_function(prev_counter, *args, **kwargs)

    return ret_value

  return Result


def WritePrettyJsonFile(file_name, json_object):
  """Writes the contents of the file to the json object.

  Args:
    file_name: The file that has contents to be used for the json object.
    json_object: The json object to write to.
  """

  json.dump(file_name, json_object, indent=4, separators=(',', ': '))


def CreateTemporaryJsonFile():
  """Makes a temporary .json file."""

  return CreateTemporaryFile(suffix='.json')


@contextmanager
def CreateTemporaryFile(suffix=''):
  """Makes a temporary file."""

  fd, temp_file_path = mkstemp(suffix=suffix)

  os.close(fd)

  try:
    yield temp_file_path

  finally:
    if os.path.isfile(temp_file_path):
      os.remove(temp_file_path)
