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

from mock import Mock
from mock import patch
import unittest
from unittest import TestCase
from acts.metrics.logger import LoggerProxy
from acts.metrics.logger import MetricLogger

COMPILE_IMPORT_PROTO = 'acts.metrics.logger.compile_import_proto'
CREATE_FROM_INSTANCE = (
    'acts.metrics.logger.subscription_bundle.create_from_instance')
LOGGING_ERROR = 'logging.error'
LOGGING_DEBUG = 'logging.debug'
GET_CONTEXT_FOR_EVENT = 'acts.metrics.logger.get_context_for_event'
GET_FILE = 'acts.metrics.logger.inspect.getfile'
MKDTEMP = 'acts.metrics.logger.tempfile.mkdtemp'
PROTO_METRIC_PUBLISHER = 'acts.metrics.logger.ProtoMetricPublisher'
TEST_CASE_LOGGER_PROXY = 'acts.metrics.logger.TestCaseLoggerProxy'
TEST_CLASS_LOGGER_PROXY = 'acts.metrics.logger.TestClassLoggerProxy'


class MetricLoggerTest(TestCase):
    """Unit tests for the MetricLogger class."""

    @patch(TEST_CASE_LOGGER_PROXY)
    def test_for_test_case_returns_test_case_proxy(self, proxy_cls):
        args = (Mock(), )
        kwargs = {'mock' : Mock()}
        logger = MetricLogger.for_test_case(*args, **kwargs)

        proxy_cls.assert_called_once_with(MetricLogger, args, kwargs)

    @patch(TEST_CLASS_LOGGER_PROXY)
    def test_for_test_class_returns_test_class_proxy(self, proxy_cls):
        args = (Mock(),)
        kwargs = {'mock': Mock()}
        logger = MetricLogger.for_test_class(*args, **kwargs)

        proxy_cls.assert_called_once_with(MetricLogger, args, kwargs)

    @patch(TEST_CASE_LOGGER_PROXY)
    def test_for_test_case_works_on_subclases(self, proxy_cls):
        class TestLogger(MetricLogger):
            pass
        args = (Mock(),)
        kwargs = {'mock': Mock()}
        logger = TestLogger.for_test_case(*args, **kwargs)

        proxy_cls.assert_called_once_with(TestLogger, args, kwargs)

    @patch(TEST_CLASS_LOGGER_PROXY)
    def test_for_test_class_works_on_subclases(self, proxy_cls):
        class TestLogger(MetricLogger):
            pass
        args = (Mock(),)
        kwargs = {'mock': Mock()}
        logger = TestLogger.for_test_class(*args, **kwargs)

        proxy_cls.assert_called_once_with(TestLogger, args, kwargs)

    @patch(COMPILE_IMPORT_PROTO)
    @patch(GET_FILE)
    def test_compile_proto_relative_path(self, getfile, compile_import_proto):
        getfile.return_value = '/path/to/class/file.py'
        proto_path = 'dir/my_proto.proto'
        compiler_out = Mock()
        MetricLogger._compile_proto(proto_path, compiler_out=compiler_out)

        full_proto_path = '/path/to/class/dir/my_proto.proto'
        compile_import_proto.assert_called_once_with(
            compiler_out, full_proto_path)

    @patch(COMPILE_IMPORT_PROTO)
    @patch(GET_FILE)
    def test_compile_proto_absolute_path(self, getfile, compile_import_proto):
        proto_path = '/abs/path/to/my_proto.proto'
        compiler_out = Mock()
        MetricLogger._compile_proto(proto_path, compiler_out=compiler_out)

        compile_import_proto.assert_called_once_with(compiler_out, proto_path)
        getfile.assert_not_called()

    @patch(COMPILE_IMPORT_PROTO)
    @patch(GET_FILE)
    @patch(MKDTEMP)
    def test_compile_proto_default_compiler_out(self,
                                                mkdtemp,
                                                getfile,
                                                compile_import_proto):
        compiler_out = Mock()
        mkdtemp.return_value = compiler_out
        proto_path = '/abs/path/to/my_proto.proto'
        MetricLogger._compile_proto(proto_path)

        compile_import_proto.assert_called_once_with(compiler_out, proto_path)


    def test_init_empty(self):
        logger = MetricLogger()

        self.assertIsNone(logger.context)
        self.assertIsNone(logger.publisher)

    def test_init_with_context_and_publisher(self):
        context = Mock()
        publisher = Mock()

        logger = MetricLogger(context=context, publisher=publisher)

        self.assertEqual(logger.context, context)
        self.assertEqual(logger.publisher, publisher)

    @patch(PROTO_METRIC_PUBLISHER)
    @patch(GET_CONTEXT_FOR_EVENT)
    def test_init_with_event(self, get_context, publisher_cls):
        context = Mock()
        publisher = Mock()
        get_context.return_value = context
        publisher_cls.return_value = publisher
        event = Mock()

        logger = MetricLogger(event=event)

        get_context.assert_called_once_with(event)
        publisher_cls.assert_called_once_with(context)
        self.assertEqual(logger.context, context)
        self.assertEqual(logger.publisher, publisher)

    def test_start_has_default_impl(self):
        logger = MetricLogger()
        logger.start(Mock())

    def test_end_has_default_impl(self):
        logger = MetricLogger()
        logger.end(Mock())

    def test_compile_proto(self):
        logger = MetricLogger()


class LoggerProxyTest(TestCase):

    @patch(CREATE_FROM_INSTANCE)
    def test_init(self, create_from_instance):
        logger_cls = Mock()
        logger_args = Mock()
        logger_kwargs = Mock()
        bundle = Mock()
        create_from_instance.return_value = bundle
        proxy = LoggerProxy(logger_cls,
                            logger_args,
                            logger_kwargs)

        self.assertEqual(proxy._logger_cls, logger_cls)
        self.assertEqual(proxy._logger_args, logger_args)
        self.assertEqual(proxy._logger_kwargs, logger_kwargs)
        self.assertIsNone(proxy._logger)
        create_from_instance.assert_called_once_with(proxy)
        bundle.register.assert_called_once_with()

    @patch(CREATE_FROM_INSTANCE)
    def test_setup_proxy(self, create_from_instance):
        logger_cls = Mock()
        logger_args = (Mock(), )
        logger_kwargs = {'mock': Mock()}
        bundle = Mock()
        event = Mock()
        create_from_instance.return_value = bundle
        logger = Mock()
        logger_cls.return_value = logger

        proxy = LoggerProxy(logger_cls,
                            logger_args,
                            logger_kwargs)
        proxy._setup_proxy(event)

        logger_cls.assert_called_once_with(event=event,
                                           *logger_args,
                                           **logger_kwargs)
        logger.start.assert_called_once_with(event)

    @patch(CREATE_FROM_INSTANCE)
    def test_teardown_proxy(self, create_from_instance):
        logger_cls = Mock()
        logger_args = (Mock(),)
        logger_kwargs = {'mock': Mock()}
        bundle = Mock()
        event = Mock()
        create_from_instance.return_value = bundle
        logger = Mock()
        logger_cls.return_value = logger

        proxy = LoggerProxy(logger_cls,
                            logger_args,
                            logger_kwargs)
        proxy._setup_proxy(event)
        proxy._teardown_proxy(event)

        logger.end.assert_called_once_with(event)
        self.assertIsNone(proxy._logger)

    @patch(LOGGING_DEBUG)
    @patch(LOGGING_ERROR)
    @patch(CREATE_FROM_INSTANCE)
    def test_teardown_proxy_logs_upon_exception(self, create_from_instance,
                                                logging_error, logging_debug):
        logger_cls = Mock()
        logger_args = (Mock(),)
        logger_kwargs = {'mock': Mock()}
        bundle = Mock()
        event = Mock()
        create_from_instance.return_value = bundle
        logger = Mock()
        logger.end.side_effect = ValueError('test')
        logger_cls.return_value = logger

        proxy = LoggerProxy(logger_cls,
                            logger_args,
                            logger_kwargs)
        proxy._setup_proxy(event)
        proxy._teardown_proxy(event)

        self.assertTrue(logging_error.called)
        self.assertTrue(logging_debug.called)
        self.assertIsNone(proxy._logger)

    @patch(CREATE_FROM_INSTANCE)
    def test_getattr_forwards_to_logger(self, create_from_instance):
        logger_cls = Mock()
        logger_args = (Mock(),)
        logger_kwargs = {'mock': Mock()}
        bundle = Mock()
        event = Mock()
        create_from_instance.return_value = bundle
        logger = Mock()
        logger_cls.return_value = logger

        proxy = LoggerProxy(logger_cls,
                            logger_args,
                            logger_kwargs)
        proxy._setup_proxy(event)

        self.assertEqual(proxy.some_attr, logger.some_attr)

    @patch(CREATE_FROM_INSTANCE)
    def test_getattr_with_no_logger_raises(self, create_from_instance):
        bundle = Mock()
        create_from_instance.return_value = bundle

        proxy = LoggerProxy(Mock(), Mock(), Mock())

        self.assertRaises(ValueError, lambda: proxy.some_attr)

    @patch(CREATE_FROM_INSTANCE)
    def test_setattr_forwards_to_logger(self, create_from_instance):
        logger_cls = Mock()
        logger_args = (Mock(),)
        logger_kwargs = {'mock': Mock()}
        bundle = Mock()
        event = Mock()
        create_from_instance.return_value = bundle
        logger = Mock()
        logger_cls.return_value = logger
        value = Mock()

        proxy = LoggerProxy(logger_cls,
                            logger_args,
                            logger_kwargs)
        proxy._setup_proxy(event)
        proxy.some_attr = value

        self.assertEqual(logger.some_attr, value)

    @patch(CREATE_FROM_INSTANCE)
    def test_setattr_with_no_logger_raises(self, create_from_instance):
        bundle = Mock()
        create_from_instance.return_value = bundle
        value = Mock()

        proxy = LoggerProxy(Mock(), Mock(), Mock())

        def try_set():
            proxy.some_attr = value
        self.assertRaises(ValueError, try_set)


if __name__ == '__main__':
    unittest.main()
