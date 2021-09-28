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
import logging
import os
import unittest

import mock

from acts import context
from acts.libs.logging import log_stream
from acts.libs.logging.log_stream import AlsoToLogHandler
from acts.libs.logging.log_stream import InvalidStyleSetError
from acts.libs.logging.log_stream import LogStyles
from acts.libs.logging.log_stream import _LogStream


class TestClass(object):
    """Dummy class for TestEvents"""

    def __init__(self):
        self.test_name = self.test_case.__name__

    def test_case(self):
        """Dummy test case for test events."""


class LogStreamTest(unittest.TestCase):
    """Tests the _LogStream class in acts.libs.logging.log_stream."""

    @staticmethod
    def patch(imported_name, *args, **kwargs):
        return mock.patch('acts.libs.logging.log_stream.%s' % imported_name,
                          *args, **kwargs)

    @classmethod
    def setUpClass(cls):
        # logging.log_path only exists if logger._setup_test_logger is called.
        # Here we set it to a value that is likely to not exist so file IO is
        # not executed (an error is raised instead of creating the file).
        logging.log_path = '/f/a/i/l/p/a/t/h'

    def setUp(self):
        log_stream._log_streams = dict()

    # __init__

    @mock.patch('os.makedirs')
    def test_init_adds_null_handler(self, *_):
        """Tests that a NullHandler is added to the logger upon initialization.
        This ensures that no log output is generated when a test class is not
        running.
        """
        debug_monolith_log = LogStyles.LOG_DEBUG | LogStyles.MONOLITH_LOG
        with self.patch('MovableFileHandler'):
            log = log_stream.create_logger(self._testMethodName,
                                           log_styles=debug_monolith_log)

        self.assertTrue(isinstance(log.handlers[0], logging.NullHandler))

    # __validate_style

    @mock.patch('os.makedirs')
    def test_validate_styles_raises_when_same_location_set_multiple_times(
            self, *_):
        """Tests that a style is invalid if it sets the same handler twice.

        If the error is NOT raised, then a LogStream can create a Logger that
        has multiple LogHandlers trying to write to the same file.
        """
        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(
                self._testMethodName,
                log_styles=[LogStyles.LOG_DEBUG | LogStyles.MONOLITH_LOG,
                            LogStyles.LOG_DEBUG | LogStyles.MONOLITH_LOG])
        self.assertTrue(
            'has been set multiple' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

    @mock.patch('os.makedirs')
    def test_validate_styles_raises_when_multiple_file_outputs_set(self, *_):
        """Tests that a style is invalid if more than one of MONOLITH_LOG,
        TESTCLASS_LOG, and TESTCASE_LOG is set for the same log level.

        If the error is NOT raised, then a LogStream can create a Logger that
        has multiple LogHandlers trying to write to the same file.
        """
        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(
                self._testMethodName,
                log_styles=[LogStyles.LOG_DEBUG | LogStyles.TESTCASE_LOG,
                            LogStyles.LOG_DEBUG | LogStyles.TESTCLASS_LOG])
        self.assertTrue(
            'More than one of' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(
                self._testMethodName,
                log_styles=[LogStyles.LOG_DEBUG | LogStyles.TESTCASE_LOG,
                            LogStyles.LOG_DEBUG | LogStyles.MONOLITH_LOG])
        self.assertTrue(
            'More than one of' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(
                self._testMethodName,
                log_styles=[LogStyles.LOG_DEBUG | LogStyles.TESTCASE_LOG,
                            LogStyles.LOG_DEBUG | LogStyles.TESTCLASS_LOG,
                            LogStyles.LOG_DEBUG | LogStyles.MONOLITH_LOG])
        self.assertTrue(
            'More than one of' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

    @mock.patch('os.makedirs')
    def test_validate_styles_raises_when_no_level_exists(self, *_):
        """Tests that a style is invalid if it does not contain a log level.

        If the style does not contain a log level, then there is no way to
        pass the information coming from the logger to the correct file.
        """
        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(self._testMethodName,
                                     log_styles=[LogStyles.MONOLITH_LOG])

        self.assertTrue(
            'log level' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

    @mock.patch('os.makedirs')
    def test_validate_styles_raises_when_no_location_exists(self, *_):
        """Tests that a style is invalid if it does not contain a log level.

        If the style does not contain a log level, then there is no way to
        pass the information coming from the logger to the correct file.
        """
        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(self._testMethodName,
                                     log_styles=[LogStyles.LOG_INFO])

        self.assertTrue(
            'log location' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

    @mock.patch('os.makedirs')
    def test_validate_styles_raises_when_rotate_logs_no_file_handler(self, *_):
        """Tests that a LogStyle cannot set ROTATE_LOGS without *_LOG flag.

        If the LogStyle contains ROTATE_LOGS, it must be associated with a log
        that is rotatable. TO_ACTS_LOG and TO_STDOUT are not rotatable logs,
        since those are both controlled by another object/process. The user
        must specify MONOLITHIC_LOG or TESTCASE_LOG.
        """
        with self.assertRaises(InvalidStyleSetError) as catch:
            log_stream.create_logger(
                self._testMethodName,
                # Added LOG_DEBUG here to prevent the no_level_exists raise from
                # occurring.
                log_styles=[LogStyles.LOG_DEBUG + LogStyles.ROTATE_LOGS])

        self.assertTrue(
            'log type' in catch.exception.args[0],
            msg='__validate_styles did not raise the expected error message')

    # __handle_style

    @mock.patch('os.makedirs')
    def test_handle_style_to_acts_log_creates_handler(self, *_):
        """Tests that using the flag TO_ACTS_LOG creates an AlsoToLogHandler."""
        info_acts_log = LogStyles.LOG_INFO + LogStyles.TO_ACTS_LOG

        log = log_stream.create_logger(self._testMethodName,
                                       log_styles=info_acts_log)

        self.assertTrue(isinstance(log.handlers[1], AlsoToLogHandler))

    @mock.patch('os.makedirs')
    def test_handle_style_to_acts_log_creates_handler_is_lowest_level(self, *_):
        """Tests that using the flag TO_ACTS_LOG creates an AlsoToLogHandler
        that is set to the lowest LogStyles level."""
        info_acts_log = (LogStyles.LOG_DEBUG + LogStyles.LOG_INFO +
                         LogStyles.TO_ACTS_LOG)

        log = log_stream.create_logger(self._testMethodName,
                                       log_styles=info_acts_log)

        self.assertTrue(isinstance(log.handlers[1], AlsoToLogHandler))
        self.assertEqual(log.handlers[1].level, logging.DEBUG)

    @mock.patch('os.makedirs')
    def test_handle_style_to_stdout_creates_stream_handler(self, *_):
        """Tests that using the flag TO_STDOUT creates a StreamHandler."""
        info_acts_log = LogStyles.LOG_INFO + LogStyles.TO_STDOUT

        log = log_stream.create_logger(self._testMethodName,
                                       log_styles=info_acts_log)

        self.assertTrue(isinstance(log.handlers[1], logging.StreamHandler))

    @mock.patch('os.makedirs')
    def test_handle_style_creates_file_handler(self, *_):
        """Tests handle_style creates a MovableFileHandler for the MONOLITH_LOG."""
        info_acts_log = LogStyles.LOG_INFO + LogStyles.MONOLITH_LOG

        expected = mock.MagicMock()
        with self.patch('MovableFileHandler', return_value=expected):
            log = log_stream.create_logger(self._testMethodName,
                                           log_styles=info_acts_log)

        self.assertEqual(log.handlers[1], expected)

    @mock.patch('os.makedirs')
    def test_handle_style_creates_rotating_file_handler(self, *_):
        """Tests handle_style creates a MovableFileHandler for the ROTATE_LOGS."""
        info_acts_log = (LogStyles.LOG_INFO + LogStyles.ROTATE_LOGS +
                         LogStyles.MONOLITH_LOG)

        expected = mock.MagicMock()
        with self.patch('MovableRotatingFileHandler', return_value=expected):
            log = log_stream.create_logger(self._testMethodName,
                                           log_styles=info_acts_log)

        self.assertEqual(log.handlers[1], expected)

    # __create_rotating_file_handler

    def test_create_rotating_file_handler_does_what_it_says_it_does(self):
        """Tests that __create_rotating_file_handler does exactly that."""
        expected = mock.MagicMock()

        with self.patch('MovableRotatingFileHandler', return_value=expected):
            # Through name-mangling, this function is automatically renamed. See
            # https://docs.python.org/3/tutorial/classes.html#private-variables
            fh = _LogStream._LogStream__create_rotating_file_handler('')

        self.assertEqual(expected, fh,
                         'The function did not return a MovableRotatingFileHandler.')

    # __get_file_handler_creator

    def test_get_file_handler_creator_returns_rotating_file_handler(self):
        """Tests the function returns a MovableRotatingFileHandler when the log_style
        has LogStyle.ROTATE_LOGS."""
        expected = mock.MagicMock()

        with self.patch('_LogStream._LogStream__create_rotating_file_handler',
                        return_value=expected):
            # Through name-mangling, this function is automatically renamed. See
            # https://docs.python.org/3/tutorial/classes.html#private-variables
            fh_creator = _LogStream._LogStream__get_file_handler_creator(
                LogStyles.ROTATE_LOGS)

        self.assertEqual(expected, fh_creator('/d/u/m/m/y/p/a/t/h'),
                         'The function did not return a MovableRotatingFileHandler.')

    def test_get_file_handler_creator_returns_file_handler(self):
        """Tests the function returns a MovableFileHandler when the log_style does NOT
        have LogStyle.ROTATE_LOGS."""
        expected = mock.MagicMock()

        with self.patch('MovableFileHandler', return_value=expected):
            # Through name-mangling, this function is automatically renamed. See
            # https://docs.python.org/3/tutorial/classes.html#private-variables
            handler = _LogStream._LogStream__get_file_handler_creator(
                LogStyles.NONE)()

        self.assertTrue(isinstance(handler, mock.Mock))

    # __get_lowest_log_level

    def test_get_lowest_level_gets_lowest_level(self):
        """Tests __get_lowest_level returns the lowest LogStyle level given."""
        level = _LogStream._LogStream__get_lowest_log_level(
            LogStyles.ALL_LEVELS)
        self.assertEqual(level, LogStyles.LOG_DEBUG)

    # __get_current_output_dir

    @mock.patch('os.makedirs')
    def test_get_current_output_dir_gets_correct_path(self, *_):
        """Tests __get_current_output_dir gets the correct path from the context
        """
        info_monolith_log = LogStyles.LOG_INFO + LogStyles.MONOLITH_LOG

        base_path = "BASEPATH"
        subcontext = "SUBCONTEXT"
        with self.patch('MovableFileHandler'):
            logstream = log_stream._LogStream(
                self._testMethodName, log_styles=info_monolith_log,
                base_path=base_path, subcontext=subcontext)

        expected = os.path.join(base_path, subcontext)
        self.assertEqual(
            logstream._LogStream__get_current_output_dir(), expected)

    # __create_handler

    @mock.patch('os.makedirs')
    def test_create_handler_creates_handler_at_correct_path(self, *_):
        """Tests that __create_handler calls the handler creator with the
        correct absolute path to the log file.
        """
        info_monolith_log = LogStyles.LOG_INFO + LogStyles.MONOLITH_LOG
        base_path = 'BASEPATH'
        with self.patch('MovableFileHandler') as file_handler:
            log_stream.create_logger(
                self._testMethodName, log_styles=info_monolith_log,
                base_path=base_path)
            expected = os.path.join(
                base_path, '%s_%s.txt' % (self._testMethodName, 'info'))
            file_handler.assert_called_with(expected)

    # __remove_handler

    @mock.patch('os.makedirs')
    def test_remove_handler_removes_a_handler(self, *_):
        """Tests that __remove_handler removes the handler from the logger and
        closes the handler.
        """
        dummy_obj = mock.Mock()
        dummy_obj.logger = mock.Mock()
        handler = mock.Mock()
        _LogStream._LogStream__remove_handler(dummy_obj, handler)

        self.assertTrue(dummy_obj.logger.removeHandler.called)
        self.assertTrue(handler.close.called)

    # update_handlers

    @mock.patch('os.makedirs')
    def test_update_handlers_updates_filehandler_target(self, _):
        """Tests that update_handlers invokes the underlying
        MovableFileHandler.set_file method on the correct path.
        """
        info_testclass_log = LogStyles.LOG_INFO + LogStyles.TESTCLASS_LOG
        file_name = 'FILENAME'
        with self.patch('MovableFileHandler'):
            log = log_stream.create_logger(
                self._testMethodName, log_styles=info_testclass_log)
            handler = log.handlers[-1]
            handler.baseFilename = file_name
            stream = log_stream._log_streams[log.name]
            stream._LogStream__get_current_output_dir = (
                lambda: 'BASEPATH/TestClass'
            )

            stream.update_handlers(context.NewTestClassContextEvent())

            handler.set_file.assert_called_with('BASEPATH/TestClass/FILENAME')

    # cleanup

    @mock.patch('os.makedirs')
    def test_cleanup_removes_all_handlers(self, *_):
        """ Tests that cleanup removes all handlers in the logger, except
        the NullHandler.
        """
        info_testcase_log = LogStyles.LOG_INFO + LogStyles.MONOLITH_LOG
        with self.patch('MovableFileHandler'):
            log_stream.create_logger(self._testMethodName,
                                     log_styles=info_testcase_log)

        created_log_stream = log_stream._log_streams[self._testMethodName]
        created_log_stream.cleanup()

        self.assertEqual(len(created_log_stream.logger.handlers), 1)


class LogStreamModuleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # logging.log_path only exists if logger._setup_test_logger is called.
        # Here we set it to a value that is likely to not exist so file IO is
        # not executed (an error is raised instead of creating the file).
        logging.log_path = '/f/a/i/l/p/a/t/h'

    def setUp(self):
        log_stream._log_streams = {}

    # _update_handlers

    @staticmethod
    def create_new_context_event():
        return context.NewContextEvent()

    def test_update_handlers_delegates_calls_to_log_streams(self):
        """Tests _update_handlers calls update_handlers on each log_stream.
        """
        log_stream._log_streams = {
            'a': mock.Mock(),
            'b': mock.Mock()
        }

        log_stream._update_handlers(self.create_new_context_event())

        self.assertTrue(log_stream._log_streams['a'].update_handlers.called)
        self.assertTrue(log_stream._log_streams['b'].update_handlers.called)

    # _set_logger

    def test_set_logger_overwrites_previous_logger(self):
        """Tests that calling set_logger overwrites the previous logger within
        log_stream._log_streams.
        """
        previous = mock.Mock()
        log_stream._log_streams = {
            'a': previous
        }
        expected = mock.Mock()
        expected.name = 'a'
        log_stream._set_logger(expected)

        self.assertEqual(log_stream._log_streams['a'], expected)


if __name__ == '__main__':
    unittest.main()
