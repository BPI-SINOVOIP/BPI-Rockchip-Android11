# -*- coding: utf-8 -*-
# Copyright 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A global variable for testing."""

is_test = [False]


def SetTestMode(flag):
  is_test[0] = flag


def GetTestMode():
  return is_test[0]
