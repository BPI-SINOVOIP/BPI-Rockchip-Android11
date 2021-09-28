#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for time_line.py."""

from __future__ import print_function

__author__ = 'yunlian@google.com (Yunlian Jiang)'

import time
import unittest

from cros_utils import timeline


class TimeLineTest(unittest.TestCase):
  """Tests for the Timeline class."""

  def testRecord(self):
    tl = timeline.Timeline()
    tl.Record('A')
    t = time.time()
    t1 = tl.events[0].timestamp
    self.assertEqual(int(t1 - t), 0)
    self.assertRaises(AssertionError, tl.Record, 'A')

  def testGetEvents(self):
    tl = timeline.Timeline()
    tl.Record('A')
    e = tl.GetEvents()
    self.assertEqual(e, ['A'])
    tl.Record('B')
    e = tl.GetEvents()
    self.assertEqual(e, ['A', 'B'])

  def testGetEventTime(self):
    tl = timeline.Timeline()
    tl.Record('A')
    t = time.time()
    t1 = tl.GetEventTime('A')
    self.assertEqual(int(t1 - t), 0)
    self.assertRaises(IndexError, tl.GetEventTime, 'B')

  def testGetLastEventTime(self):
    tl = timeline.Timeline()
    self.assertRaises(IndexError, tl.GetLastEventTime)
    tl.Record('A')
    t = time.time()
    t1 = tl.GetLastEventTime()
    self.assertEqual(int(t1 - t), 0)
    time.sleep(2)
    tl.Record('B')
    t = time.time()
    t1 = tl.GetLastEventTime()
    self.assertEqual(int(t1 - t), 0)


if __name__ == '__main__':
  unittest.main()
