#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utility functions."""

DEFAULT_OBJECT_NUMBER = 1238
DEFAULT_BAD_OBJECT_NUMBER = 23
OBJECTS_FILE = 'objects.txt'
WORKING_SET_FILE = 'working_set.txt'


def ReadWorkingSet():
  working_set = []
  with open(WORKING_SET_FILE, 'r', encoding='utf-8') as f:
    for l in f:
      working_set.append(int(l))
  return working_set


def WriteWorkingSet(working_set):
  with open(WORKING_SET_FILE, 'w', encoding='utf-8') as f:
    for o in working_set:
      f.write('{0}\n'.format(o))


def ReadObjectsFile():
  objects_file = []
  with open(OBJECTS_FILE, 'r', encoding='utf-8') as f:
    for l in f:
      objects_file.append(int(l))
  return objects_file


def ReadObjectIndex(filename):
  object_index = []
  with open(filename, 'r', encoding='utf-8') as f:
    for o in f:
      object_index.append(int(o))
  return object_index
