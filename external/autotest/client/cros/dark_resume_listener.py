# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import threading

import dbus
import dbus.mainloop.glib
import gobject
import os

from autotest_lib.client.cros.input_playback import keyboard

DARK_SUSPEND_DELAY_MILLISECONDS = 50000

class DarkResumeListener(object):
    """Server which listens for dark resume-related DBus signals to count how
    many dark resumes we have seen since instantiation."""

    SIGNAL_NAME = 'DarkSuspendImminent'


    def __init__(self):
        dbus.mainloop.glib.threads_init()
        gobject.threads_init()

        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        self._bus = dbus.SystemBus()
        self._count = 0
        self._stop_resuspend = False

        self._bus.add_signal_receiver(handler_function=self._saw_dark_resume,
                                      signal_name=self.SIGNAL_NAME)

        def loop_runner():
            """Handles DBus events on the system bus using the mainloop."""
            # If we just call run on this loop, the listener will hang and the test
            # will never finish. Instead, we process events as they come in. This
            # thread is set to daemon below, which means that the program will exit
            # when the main thread exits.
            loop = gobject.MainLoop()
            context = loop.get_context()
            while True:
                context.iteration(True)
        thread = threading.Thread(None, loop_runner)
        thread.daemon = True
        thread.start()
        logging.debug('Dark resume listener started')

        def register_dark_delay():
            """Register a new client with powerd to delay dark suspend."""
            # Powerd resuspends on dark resume once all clients are ready. Once
            # resuspended the device might not be reachable. Thus this test on
            # seeing a dark resume injects an input event by creating a virtual
            # input device so that powerd bails out of resuspend process. But we
            # need to delay the resuspend long enough for powerd to detect new
            # input device and read the input event. This needs to run on a
            # seperate thread as powerd will automatically unregister this
            # client's suspend delay when it disconnects from D-Bus. Thus this
            # thread is set to daemon below, which means that the program will
            # exit when the main thread exits.
            command = ('/usr/bin/suspend_delay_sample --delay_ms=%d '
                       '--timeout_ms=%d --dark_suspend_delay' %
                       (DARK_SUSPEND_DELAY_MILLISECONDS,
                        DARK_SUSPEND_DELAY_MILLISECONDS))
            logging.info("Running '%s'", command)
            os.system(command)

        suspend_delay_thread = threading.Thread(None, register_dark_delay)
        suspend_delay_thread.daemon = True
        suspend_delay_thread.start()
        logging.debug('Dark suspend delay registered')


    @property
    def count(self):
        """Number of DarkSuspendImminent events this listener has seen since its
        creation."""
        return self._count


    def _saw_dark_resume(self, unused):
        self._count += 1
        if self._stop_resuspend:
            # Inject input event to stop re-suspend.
            with keyboard.Keyboard() as keys:
                    keys.press_key('f4')



    def stop_resuspend(self, should_stop):
        """
        Whether to stop suspend after seeing a dark resume.

        @param should_stop: Whether to stop system from re-suspending.
        """
        self._stop_resuspend = should_stop

