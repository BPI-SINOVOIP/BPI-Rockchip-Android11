#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""
Script for verifying SL4F is running on a Fuchsia device and
can communicate to ACTS successfully.

"""
from acts.base_test import BaseTestClass

import os
import uuid

from acts import signals
from acts.test_utils.tel.tel_test_utils import setup_droid_properties


class Sl4fSanityTest(BaseTestClass):

    def setup_class(self):
        super().setup_class()

        success_str = ("Congratulations! Fuchsia controllers have been "
                       "initialized successfully!")
        err_str = ("Sorry, please try verifying FuchsiaDevice is in your "
                   "config file and try again.")
        if len(self.fuchsia_devices) > 0:
            self.log.info(success_str)
        else:
            raise signals.TestAbortClass("err_str")

    def test_example(self):
        self.log.info("Congratulations! You've run your first test.")
        return True

