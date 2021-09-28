#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""This module defines an AVD instance.

TODO:
  The current implementation only initialize an object
  with IP and instance name. A complete implementation
  will include the following features.
  - Connect
  - Disconnect
  - HasApp
  - InstallApp
  - UninstallApp
  - GrantAppPermission
  Merge cloud/android/platform/devinfra/caci/framework/app_manager.py
  with this module and updated any callers.
"""

import logging


logger = logging.getLogger(__name__)


class AndroidVirtualDevice(object):
    """Represent an Android device."""

    def __init__(self, instance_name, ip=None, time_info=None):
        """Initialize.

        Args:
            instance_name: Name of the gce instance, e.g. "instance-1"
            ip: namedtuple (internal, external) that holds IP address of the
                gce instance, e.g. "external:140.110.20.1, internal:10.0.0.1"
            time_info: Dict of time cost information, e.g. {"launch_cvd": 5}
        """
        self._ip = ip
        self._instance_name = instance_name

        # Time info that the AVD device is created with. It will be assigned
        # by the inherited AndroidVirtualDevice. For example:
        # {"artifact": "100",
        #  "launch_cvd": "120"}
        self._time_info = time_info

        # Build info that the AVD device is created with. It will be assigned
        # by the inherited AndroidVirtualDevice. For example:
        # {"branch": "git_master-release",
        #  "build_id": "5325535",
        #  "build_target": "cf_x86_phone-userdebug",
        #  "gcs_bucket_build_id": "AAAA.190220.001-5325535"}
        # It can alo have mixed builds' info. For example:
        # {"branch": "git_master-release",
        #  "build_id": "5325535",
        #  "build_target": "cf_x86_phone-userdebug",
        #  "gcs_bucket_build_id": "AAAA.190220.001-5325535",
        #  "system_branch": "git_master",
        #  "system_build_id": "12345",
        #  "system_build_target": "cf_x86_phone-userdebug",
        #  "system_gcs_bucket_build_id": "12345"}
        self._build_info = {}

    @property
    def ip(self):
        """Getter of _ip."""
        if not self._ip:
            raise ValueError(
                "IP of instance %s is unknown yet." % self._instance_name)
        return self._ip

    @ip.setter
    def ip(self, value):
        self._ip = value

    @property
    def instance_name(self):
        """Getter of _instance_name."""
        return self._instance_name

    @property
    def build_info(self):
        """Getter of _build_info."""
        return self._build_info

    @property
    def time_info(self):
        """Getter of _time_info."""
        return self._time_info

    @build_info.setter
    def build_info(self, value):
        self._build_info = value

    def __str__(self):
        """Return a string representation."""
        return "<ip: (internal: %s, external: %s), instance_name: %s >" % (
            self._ip.internal if self._ip else "",
            self._ip.external if self._ip else "",
            self._instance_name)
