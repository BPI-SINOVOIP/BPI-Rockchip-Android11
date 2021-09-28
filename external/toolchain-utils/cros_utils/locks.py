# -*- coding: utf-8 -*-
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for locking machines."""

from __future__ import print_function

import time

import lock_machine

from cros_utils import logger


def AcquireLock(machines, chromeos_root, timeout=1200):
  """Acquire lock for machine(s) with timeout."""
  start_time = time.time()
  locked = True
  sleep_time = min(10, timeout / 10.0)
  while True:
    try:
      lock_machine.LockManager(machines, False,
                               chromeos_root).UpdateMachines(True)
      break
    except Exception as e:
      if time.time() - start_time > timeout:
        locked = False
        logger.GetLogger().LogWarning(
            'Could not acquire lock on {0} within {1} seconds: {2}'.format(
                repr(machines), timeout, str(e)))
        break
      time.sleep(sleep_time)
  return locked


def ReleaseLock(machines, chromeos_root):
  """Release locked machine(s)."""
  unlocked = True
  try:
    lock_machine.LockManager(machines, False,
                             chromeos_root).UpdateMachines(False)
  except Exception as e:
    unlocked = False
    logger.GetLogger().LogWarning(
        'Could not unlock %s. %s' % (repr(machines), str(e)))
  return unlocked
