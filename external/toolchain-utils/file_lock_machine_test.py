#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""lock_machine.py related unit-tests.

MachineManagerTest tests MachineManager.
"""

from __future__ import print_function

__author__ = 'asharif@google.com (Ahmad Sharif)'

from multiprocessing import Process
import time
import unittest

import file_lock_machine


def LockAndSleep(machine):
  file_lock_machine.Machine(machine, '/tmp', auto=True).Lock(exclusive=True)
  time.sleep(1)


class MachineTest(unittest.TestCase):
  """Class for testing machine locking."""

  def setUp(self):
    pass

  def testRepeatedUnlock(self):
    mach = file_lock_machine.Machine('qqqraymes.mtv', '/tmp')
    for _ in range(10):
      self.assertTrue(mach.Unlock())
    mach = file_lock_machine.Machine('qqqraymes.mtv', '/tmp', auto=True)
    for _ in range(10):
      self.assertTrue(mach.Unlock())

  def testLockUnlock(self):
    mach = file_lock_machine.Machine('otter.mtv', '/tmp')
    for _ in range(10):
      self.assertTrue(mach.Lock(exclusive=True))
      self.assertTrue(mach.Unlock(exclusive=True))

    mach = file_lock_machine.Machine('otter.mtv', '/tmp', True)
    for _ in range(10):
      self.assertTrue(mach.Lock(exclusive=True))
      self.assertTrue(mach.Unlock(exclusive=True))

  def testSharedLock(self):
    mach = file_lock_machine.Machine('chrotomation.mtv', '/tmp')
    for _ in range(10):
      self.assertTrue(mach.Lock(exclusive=False))
    for _ in range(10):
      self.assertTrue(mach.Unlock(exclusive=False))
    self.assertTrue(mach.Lock(exclusive=True))
    self.assertTrue(mach.Unlock(exclusive=True))

    mach = file_lock_machine.Machine('chrotomation.mtv', '/tmp', auto=True)
    for _ in range(10):
      self.assertTrue(mach.Lock(exclusive=False))
    for _ in range(10):
      self.assertTrue(mach.Unlock(exclusive=False))
    self.assertTrue(mach.Lock(exclusive=True))
    self.assertTrue(mach.Unlock(exclusive=True))

  def testExclusiveLock(self):
    mach = file_lock_machine.Machine('atree.mtv', '/tmp')
    self.assertTrue(mach.Lock(exclusive=True))
    for _ in range(10):
      self.assertFalse(mach.Lock(exclusive=True))
      self.assertFalse(mach.Lock(exclusive=False))
    self.assertTrue(mach.Unlock(exclusive=True))

    mach = file_lock_machine.Machine('atree.mtv', '/tmp', auto=True)
    self.assertTrue(mach.Lock(exclusive=True))
    for _ in range(10):
      self.assertFalse(mach.Lock(exclusive=True))
      self.assertFalse(mach.Lock(exclusive=False))
    self.assertTrue(mach.Unlock(exclusive=True))

  def testExclusiveState(self):
    mach = file_lock_machine.Machine('testExclusiveState', '/tmp')
    self.assertTrue(mach.Lock(exclusive=True))
    for _ in range(10):
      self.assertFalse(mach.Lock(exclusive=False))
    self.assertTrue(mach.Unlock(exclusive=True))

    mach = file_lock_machine.Machine('testExclusiveState', '/tmp', auto=True)
    self.assertTrue(mach.Lock(exclusive=True))
    for _ in range(10):
      self.assertFalse(mach.Lock(exclusive=False))
    self.assertTrue(mach.Unlock(exclusive=True))

  def testAutoLockGone(self):
    mach = file_lock_machine.Machine('lockgone', '/tmp', auto=True)
    p = Process(target=LockAndSleep, args=('lockgone',))
    p.start()
    time.sleep(1.1)
    p.join()
    self.assertTrue(mach.Lock(exclusive=True))

  def testAutoLockFromOther(self):
    mach = file_lock_machine.Machine('other_lock', '/tmp', auto=True)
    p = Process(target=LockAndSleep, args=('other_lock',))
    p.start()
    time.sleep(0.5)
    self.assertFalse(mach.Lock(exclusive=True))
    p.join()
    time.sleep(0.6)
    self.assertTrue(mach.Lock(exclusive=True))

  def testUnlockByOthers(self):
    mach = file_lock_machine.Machine('other_unlock', '/tmp', auto=True)
    p = Process(target=LockAndSleep, args=('other_unlock',))
    p.start()
    time.sleep(0.5)
    self.assertTrue(mach.Unlock(exclusive=True))
    self.assertTrue(mach.Lock(exclusive=True))


if __name__ == '__main__':
  unittest.main()
