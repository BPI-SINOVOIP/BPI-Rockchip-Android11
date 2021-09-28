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
import unittest

import mock
import subprocess
from acts.libs.proc.process import Process
from acts.libs.proc.process import ProcessError

class FakeThread(object):
    def __init__(self, target=None):
        self.target = target
        self.alive = False

    def _on_start(self):
        pass

    def start(self):
        self.alive = True
        if self._on_start:
            self._on_start()

    def stop(self):
        self.alive = False

    def join(self):
        pass


class ProcessTest(unittest.TestCase):
    """Tests the acts.libs.proc.process.Process class."""

    def setUp(self):
        self._Process__start_process = Process._Process__start_process

    def tearDown(self):
        Process._Process__start_process = self._Process__start_process

    @staticmethod
    def patch(imported_name, *args, **kwargs):
        return mock.patch('acts.libs.proc.process.%s' % imported_name,
                          *args, **kwargs)

    # set_on_output_callback

    def test_set_on_output_callback(self):
        """Tests that set_on_output_callback sets on_output_callback."""
        callback = mock.Mock()

        process = Process('cmd').set_on_output_callback(callback)
        process._on_output_callback()

        self.assertTrue(callback.called)

    # set_on_terminate_callback

    def test_set_on_terminate_callback(self):
        """Tests that set_on_terminate_callback sets _on_terminate_callback."""
        callback = mock.Mock()

        process = Process('cmd').set_on_terminate_callback(callback)
        process._on_terminate_callback()

        self.assertTrue(callback.called)

    # start

    def test_start_raises_if_called_back_to_back(self):
        """Tests that start raises an exception if it has already been called
        prior.

        This is required to prevent references to processes and threads from
        being overwritten, potentially causing ACTS to hang."""
        process = Process('cmd')

        # Here we need the thread to start the process object.
        class FakeThreadImpl(FakeThread):
            def _on_start(self):
                process._process = mock.Mock()

        with self.patch('Thread', FakeThreadImpl):
            process.start()
            expected_msg = 'Process has already started.'
            with self.assertRaisesRegex(ProcessError, expected_msg):
                process.start()

    def test_start_starts_listening_thread(self):
        """Tests that start starts the _exec_popen_loop function."""
        process = Process('cmd')

        # Here we need the thread to start the process object.
        class FakeThreadImpl(FakeThread):
            def _on_start(self):
                process._process = mock.Mock()

        with self.patch('Thread', FakeThreadImpl):
            process.start()

        self.assertTrue(process._listening_thread.alive)
        self.assertEqual(process._listening_thread.target, process._exec_loop)

    # wait

    def test_wait_raises_if_called_back_to_back(self):
        """Tests that wait raises an exception if it has already been called
        prior."""
        process = Process('cmd')
        process._process = mock.Mock()

        process.wait(0)
        expected_msg = 'Process is already being stopped.'
        with self.assertRaisesRegex(ProcessError, expected_msg):
            process.wait(0)

    @mock.patch.object(Process, '_kill_process')
    def test_wait_kills_after_timeout(self, *_):
        """Tests that if a TimeoutExpired error is thrown during wait, the
        process is killed."""
        process = Process('cmd')
        process._process = mock.Mock()
        process._process.wait.side_effect = subprocess.TimeoutExpired('', '')

        process.wait(0)

        self.assertEqual(process._kill_process.called, True)

    @mock.patch.object(Process, '_kill_process')
    def test_wait_sets_stopped_to_true_before_process_kill(self, *_):
        """Tests that stop() sets the _stopped attribute to True.

        This order is required to prevent the _exec_loop from calling
        _on_terminate_callback when the user has killed the process.
        """
        verifier = mock.Mock()
        verifier.passed = False

        def test_call_order():
            self.assertTrue(process._stopped)
            verifier.passed = True

        process = Process('cmd')
        process._process = mock.Mock()
        process._process.poll.return_value = None
        process._process.wait.side_effect = subprocess.TimeoutExpired('', '')
        process._kill_process = test_call_order

        process.wait()

        self.assertEqual(verifier.passed, True)

    def test_wait_joins_listening_thread_if_it_exists(self):
        """Tests wait() joins _listening_thread if it exists."""
        process = Process('cmd')
        process._process = mock.Mock()
        mocked_thread = mock.Mock()
        process._listening_thread = mocked_thread

        process.wait(0)

        self.assertEqual(mocked_thread.join.called, True)

    def test_wait_clears_listening_thread_if_it_exists(self):
        """Tests wait() joins _listening_thread if it exists.

        Threads can only be started once, so after wait has been called, we
        want to make sure we clear the listening thread.
        """
        process = Process('cmd')
        process._process = mock.Mock()
        process._listening_thread = mock.Mock()

        process.wait(0)

        self.assertEqual(process._listening_thread, None)

    def test_wait_joins_redirection_thread_if_it_exists(self):
        """Tests wait() joins _listening_thread if it exists."""
        process = Process('cmd')
        process._process = mock.Mock()
        mocked_thread = mock.Mock()
        process._redirection_thread = mocked_thread

        process.wait(0)

        self.assertEqual(mocked_thread.join.called, True)

    def test_wait_clears_redirection_thread_if_it_exists(self):
        """Tests wait() joins _listening_thread if it exists.

        Threads can only be started once, so after wait has been called, we
        want to make sure we clear the listening thread.
        """
        process = Process('cmd')
        process._process = mock.Mock()
        process._redirection_thread = mock.Mock()

        process.wait(0)

        self.assertEqual(process._redirection_thread, None)

    # stop

    def test_stop_sets_stopped_to_true(self):
        """Tests that stop() sets the _stopped attribute to True."""
        process = Process('cmd')
        process._process = mock.Mock()

        process.stop()

        self.assertTrue(process._stopped)

    def test_stop_sets_stopped_to_true_before_process_kill(self):
        """Tests that stop() sets the _stopped attribute to True.

        This order is required to prevent the _exec_loop from calling
        _on_terminate_callback when the user has killed the process.
        """
        verifier = mock.Mock()
        verifier.passed = False

        def test_call_order():
            self.assertTrue(process._stopped)
            verifier.passed = True

        process = Process('cmd')
        process._process = mock.Mock()
        process._process.poll.return_value = None
        process._kill_process = test_call_order
        process._process.wait.side_effect = subprocess.TimeoutExpired('', '')

        process.stop()

        self.assertEqual(verifier.passed, True)

    def test_stop_calls_wait(self):
        """Tests that stop() also has the functionality of wait()."""
        process = Process('cmd')
        process._process = mock.Mock()
        process.wait = mock.Mock()

        process.stop()

        self.assertEqual(process.wait.called, True)

    # _redirect_output

    def test_redirect_output_feeds_all_lines_to_on_output_callback(self):
        """Tests that _redirect_output loops until all lines are parsed."""
        received_list = []

        def appender(line):
            received_list.append(line)

        process = Process('cmd')
        process.set_on_output_callback(appender)
        process._process = mock.Mock()
        process._process.stdout.readline.side_effect = [b'a\n', b'b\n', b'']

        process._redirect_output()

        self.assertEqual(received_list[0], 'a')
        self.assertEqual(received_list[1], 'b')
        self.assertEqual(len(received_list), 2)

    # __start_process

    def test_start_process_returns_a_popen_object(self):
        """Tests that a Popen object is returned by __start_process."""
        with self.patch('subprocess.Popen', return_value='verification'):
            self.assertEqual(Process._Process__start_process('cmd'),
                             'verification')

    # _exec_loop

    def test_exec_loop_redirections_output(self):
        """Tests that the _exec_loop function calls to redirect the output."""
        process = Process('cmd')
        Process._Process__start_process = mock.Mock()

        with self.patch('Thread', FakeThread):
            process._exec_loop()

        self.assertEqual(process._redirection_thread.target,
                         process._redirect_output)
        self.assertEqual(process._redirection_thread.alive, True)

    def test_exec_loop_waits_for_process(self):
        """Tests that the _exec_loop waits for the process to complete before
        returning."""
        process = Process('cmd')
        Process._Process__start_process = mock.Mock()

        with self.patch('Thread', FakeThread):
            process._exec_loop()

        self.assertEqual(process._process.wait.called, True)

    def test_exec_loop_loops_if_not_stopped(self):
        process = Process('1st')
        Process._Process__start_process = mock.Mock()
        process._on_terminate_callback = mock.Mock(side_effect=[['2nd'], None])

        with self.patch('Thread', FakeThread):
            process._exec_loop()

        self.assertEqual(Process._Process__start_process.call_count, 2)
        self.assertEqual(Process._Process__start_process.call_args_list[0][0],
                         (['1st'], ))
        self.assertEqual(Process._Process__start_process.call_args_list[1][0],
                         (['2nd'], ))

    def test_exec_loop_does_not_loop_if_stopped(self):
        process = Process('1st')
        Process._Process__start_process = mock.Mock()
        process._on_terminate_callback = mock.Mock(
            side_effect=['2nd', None])
        process._stopped = True

        with self.patch('Thread', FakeThread):
            process._exec_loop()

        self.assertEqual(Process._Process__start_process.call_count, 1)
        self.assertEqual(
            Process._Process__start_process.call_args_list[0][0],
            (['1st'],))


if __name__ == '__main__':
    unittest.main()
