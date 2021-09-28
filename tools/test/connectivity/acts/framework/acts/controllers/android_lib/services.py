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


from acts.controllers.android_lib import errors
from acts.controllers.android_lib import events as android_events
from acts.event import event_bus


class AndroidService(object):
    """The base class for Android long-running services.

    The _start method is registered to an AndroidStartServicesEvent, and
    the _stop method is registered to an AndroidStopServicesEvent.

    Attributes:
        ad: The AndroidDevice instance associated with the service.
        serial: The serial of the device.
        _registration_ids: List of registration IDs for the event subscriptions.
    """

    def __init__(self, ad):
        self.ad = ad
        self._registration_ids = []

    @property
    def serial(self):
        return self.ad.serial

    def register(self):
        """Registers the _start and _stop methods to their corresponding
        events.
        """
        def check_serial(event):
            return self.serial == event.ad.serial

        self._registration_ids = [
            event_bus.register(android_events.AndroidStartServicesEvent,
                               self._start, filter_fn=check_serial),
            event_bus.register(android_events.AndroidStopServicesEvent,
                               self._stop, filter_fn=check_serial)]

    def unregister(self):
        """Unregisters all subscriptions in this service."""
        event_bus.unregister_all(from_list=self._registration_ids)
        self._registration_ids.clear()

    def _start(self, start_event):
        """Start the service. Called upon an AndroidStartServicesEvent.

        Args:
            start_event: The AndroidStartServicesEvent instance.
        """
        raise NotImplementedError

    def _stop(self, stop_event):
        """Stop the service. Called upon an AndroidStopServicesEvent.

        Args:
            stop_event: The AndroidStopServicesEvent instance.
        """
        raise NotImplementedError


class AdbLogcatService(AndroidService):
    """Service for adb logcat."""

    def _start(self, _):
        self.ad.start_adb_logcat()

    def _stop(self, _):
        self.ad.stop_adb_logcat()


class Sl4aService(AndroidService):
    """Service for SL4A."""

    def _start(self, start_event):
        if self.ad.skip_sl4a:
            return

        if not self.ad.is_sl4a_installed():
            self.ad.log.error('sl4a.apk is not installed')
            raise errors.AndroidDeviceError(
                'The required sl4a.apk is not installed',
                serial=self.serial)
        if not self.ad.ensure_screen_on():
            self.ad.log.error("User window cannot come up")
            raise errors.AndroidDeviceError(
                "User window cannot come up", serial=self.serial)

        droid, ed = self.ad.get_droid()
        ed.start()

    def _stop(self, _):
        self.ad.terminate_all_sessions()
        self.ad._sl4a_manager.stop_service()
        self.ad.stop_sl4a()
