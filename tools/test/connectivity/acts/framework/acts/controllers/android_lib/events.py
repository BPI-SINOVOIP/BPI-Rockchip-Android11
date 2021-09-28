#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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

from acts.event.event import Event


class AndroidEvent(Event):
    """The base class for AndroidDevice-related events."""

    def __init__(self, android_device):
        self.android_device = android_device

    @property
    def ad(self):
        return self.android_device


class AndroidStartServicesEvent(AndroidEvent):
    """The event posted when an AndroidDevice begins its services."""


class AndroidStopServicesEvent(AndroidEvent):
    """The event posted when an AndroidDevice ends its services."""


class AndroidRebootEvent(AndroidEvent):
    """The event posted when an AndroidDevice has rebooted."""


class AndroidDisconnectEvent(AndroidEvent):
    """The event posted when an AndroidDevice has disconnected."""


class AndroidReconnectEvent(AndroidEvent):
    """The event posted when an AndroidDevice has disconnected."""


class AndroidBugReportEvent(AndroidEvent):
    """The event posted when an AndroidDevice captures a bugreport."""

    def __init__(self, android_device, bugreport_dir):
        super().__init__(android_device)
        self.bugreport_dir = bugreport_dir
