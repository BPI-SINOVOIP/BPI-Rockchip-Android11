# Copyright (c) 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from autotest_lib.client.bin import test

class dummy_SynchronousOffload(test.test):
  version = 1

  def initialize(self):
    pass

  def run_once(self):
    DIR = os.getenv('SYNCHRONOUS_OFFLOAD_DIR')
    with open(os.path.join(DIR,"test_file"), "w") as f:
      f.write("Test string which should be offloaded")
      logging.debug("Wrote string to test file.")

  def cleanup(self):
    pass
