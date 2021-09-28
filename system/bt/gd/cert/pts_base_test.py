#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

from acts.base_test import BaseTestClass

import importlib
import logging
import os
import signal
import subprocess


class PTSBaseTestClass(BaseTestClass):

    def __init__(self, configs):
        BaseTestClass.__init__(self, configs)

        gd_devices = self.controller_configs.get("GdDevice")

        self.register_controller(
            importlib.import_module('cert.gd_device'), builtin=True)
