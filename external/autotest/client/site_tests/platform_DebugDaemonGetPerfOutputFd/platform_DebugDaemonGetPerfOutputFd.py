# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus, os, time

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import debugd_util

class platform_DebugDaemonGetPerfOutputFd(test.test):
    """
    This autotest tests the collection of perf data.  It calls perf indirectly
    through debugd -> quipper -> perf. This also tests stopping the perf
    session.

    The perf data is read from a pipe that is redirected to stdout of the
    quipper process.
    """

    version = 1

    def check_perf_output(self, perf_data):
        """
        Utility function to validate the perf data that was previously read
        from the pipe.
        """
        if len(perf_data) < 10:
            raise error.TestFail('Perf output (%s) too small' % perf_data)

        # Perform basic sanity checks of the perf data: it should contain
        # [kernel.kallsyms] and /usr/bin/perf
        if (perf_data.find('[kernel.kallsyms]') == -1 or
                perf_data.find('/usr/bin/perf') == -1):
            raise error.TestFail('Quipper failed: %s' % perf_data)

    def call_get_perf_output_fd(self, duration):
        """
        Utility function to call DBus method GetPerfOutputFd with the given
        duration.
        """
        pipe_r, pipe_w = os.pipe()
        perf_command = ['perf', 'record', '-a', '-F', '100']

        session_id = self.dbus_iface.GetPerfOutputFd(
            duration, perf_command, dbus.types.UnixFd(pipe_w), signature="uash")

        # pipe_w is dup()'d in calling dbus. Close in this process.
        os.close(pipe_w)

        if session_id == 0:
            raise error.TestFail('Invalid session ID from GetPerfOutputFd')

        # Don't explicitly os.close(pipe_r) since it will be closed
        # automatically when the file object returned by os.fdopen() is closed.
        return session_id, os.fdopen(pipe_r, 'r')

    def call_stop_perf(self, session_id, real_duration):
        """
        Utility function to call DBus method StopPerf to collect perf data
        collected within the given duration.
        """
        # Sleep for real_duration seconds and then stop the perf session.
        time.sleep(real_duration)
        self.dbus_iface.StopPerf(session_id, signature='t')

    def test_full_duration(self):
        """
        Test GetPerfOutpuFd to collect a profile of 2 seconds.
        """

        session_id, result_file = self.call_get_perf_output_fd(2)

        # This performs synchronous read until perf exits.
        result = result_file.read()

        self.check_perf_output(result)

    def test_stop_perf(self):
        """
        Test StopPerf by calling GetPerfOutputFd to collect a profile of 30
        seconds. After the perf session is started for 2 seconds, call StopPerf
        to stop the profiling session. The net result is a profile of 2
        seconds. Verify StopPerf working by timing the test case: the test case
        shouldn't run for 30 seconds or longer.
        """
        start = time.time()

        # Default duration is 30 sec.
        session_id, result_file = self.call_get_perf_output_fd(30)

        # Get a profile of 2 seconds by premature stop.
        self.call_stop_perf(session_id, 2)

        # This performs synchronous read until perf exits.
        result = result_file.read()

        self.check_perf_output(result)

        end = time.time()
        if (end - start) >= 30:
            raise error.TestFail('Unable to stop the perf tool')

    def test_start_after_previous_finished(self):
        """
        Test consecutive GetPerfOutputFd calls that there is no undesirable
        side effect left in the previous profiling session.
        """
        self.test_full_duration()
        self.test_full_duration()

    def test_stop_without_start(self):
        """
        Test unmatched StopPerf call by checking the returned DBusException.
        """
        dbus_message = None
        try:
            self.call_stop_perf(0, 1)
        except dbus.exceptions.DBusException as dbus_exception:
            dbus_message = dbus_exception.get_dbus_message()

        if dbus_message is None:
            raise error.TestFail('DBusException expected')
        if dbus_message.find('Perf tool not started') == -1:
            raise error.TestFail('Unexpected DBus message: %s' % dbus_message)

    def test_stop_using_wrong_id(self):
        """
        Test calling StopPerf with an invalid session ID by checking the
        returned DBusException.
        """
        start = time.time()

        # Default duration is 30 sec.
        session_id, result_file = self.call_get_perf_output_fd(30)

        dbus_message = None
        try:
            # Use session_id - 1 to trigger the error condition.
            self.call_stop_perf(session_id - 1, 1)
        except dbus.exceptions.DBusException as dbus_exception:
            dbus_message = dbus_exception.get_dbus_message()

        if dbus_message is None:
            raise error.TestFail('DBusException expected')
        if dbus_message.find('Invalid profile session id') == -1:
            raise error.TestFail('Unexpected DBus message: %s' % dbus_message)

        # Get a profile of 1 second by premature stop.
        self.call_stop_perf(session_id, 1)

        # This performs synchronous read until perf exits.
        result = result_file.read()

        self.check_perf_output(result)

        end = time.time()
        if (end - start) >= 30:
            raise error.TestFail('Unable to stop the perf tool')

    def test_start_2nd_time(self):
        """
        Test calling GetPerfOutputFd when an existing profiling session is
        running: the 2nd call should yield a DBusException without affecting
        the 1st call.
        """
        # Default duration is 30 sec.
        session_id, result_file = self.call_get_perf_output_fd(30)

        dbus_message = None
        try:
            self.call_get_perf_output_fd(60)
        except dbus.exceptions.DBusException as dbus_exception:
            dbus_message = dbus_exception.get_dbus_message()

        if dbus_message is None:
            raise error.TestFail('DBusException expected')
        if dbus_message.find('Existing perf tool running') == -1:
            raise error.TestFail('Unexpected DBus message: %s' % dbus_message)

        # Get a profile of 1 second by premature stop.
        self.call_stop_perf(session_id, 1)

        # This performs synchronous read until perf exits.
        result = result_file.read()

        self.check_perf_output(result)

    def run_once(self, *args, **kwargs):
        """
        Primary autotest function.
        """
        # Setup.
        self.dbus_iface = debugd_util.iface()

        # Test normal cases.
        self.test_full_duration()
        self.test_start_after_previous_finished()
        self.test_stop_perf()

        # Test error cases.
        self.test_stop_without_start()
        self.test_stop_using_wrong_id()
        self.test_start_2nd_time()
