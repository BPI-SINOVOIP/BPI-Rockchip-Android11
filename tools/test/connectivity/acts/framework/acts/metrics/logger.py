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

import inspect
import logging
import tempfile
import traceback
from os import path

from acts.context import get_context_for_event
from acts.event import event_bus
from acts.event import subscription_bundle
from acts.event.decorators import subscribe
from acts.event.event import TestCaseBeginEvent
from acts.event.event import TestCaseEndEvent
from acts.event.event import TestClassBeginEvent
from acts.event.event import TestClassEndEvent
from acts.libs.proto.proto_utils import compile_import_proto
from acts.metrics.core import ProtoMetricPublisher


class MetricLogger(object):
    """The base class for a logger object that records metric data.

    This is the central component to the ACTS metrics framework. Users should
    extend this class with the functionality needed to log their specific
    metric.

    The public API for this class contains only a start() and end() method,
    intended to bookend the logging process for a particular metric. The timing
    of when those methods are called depends on how the logger is subscribed.
    The canonical use for this class is to use the class methods to
    automatically subscribe the logger to certain test events.

    Example:
        def MyTestClass(BaseTestClass):
            def __init__(self):
                self.my_metric_logger = MyMetricLogger.for_test_case()

    This would subscribe the logger to test case begin and end events. For each
    test case in MyTestClass, a new MyMetricLogger instance will be created,
    and start() and end() will be called at the before and after the test case,
    respectively.

    The self.my_metric_logger object will be a proxy object that points to
    whatever MyMetricLogger is being used in the current context. This means
    that test code can access this logger without worrying about managing
    separate instances for each test case.

    Example:
         def MyMetricLogger(MetricLogger):
             def store_data(self, data):
                 # store data

             def end(self, event):
                 # write out stored data

         def MyTestClass(BaseTestClass):
             def __init__(self):
                 self.my_metric_logger = MyMetricLogger.for_test_case()

             def test_case_a(self):
                 # do some test stuff
                 self.my_metric_logger.store_data(data)
                 # more test stuff

             def test_case_b(self):
                 # do some test stuff
                 self.my_metric_logger.store_data(data)
                 # more test stuff

    In the above example, test_case_a and test_case_b both record data to
    self.my_metric_logger. However, because the MyMetricLogger was subscribed
    to test cases, the proxy object would point to a new instance for each
    test case.


    Attributes:

        context: A MetricContext object describing metadata about how the
                 logger is being run. For example, on a test case metric
                 logger, the context should contain the test class and test
                 case name.
        publisher: A MetricPublisher object that provides an API for publishing
                   metric data, typically to a file.
    """

    @classmethod
    def for_test_case(cls, *args, **kwargs):
        """Registers the logger class for each test case.

        Creates a proxy logger that will instantiate this method's logger class
        for each test case. Any arguments passed to this method will be
        forwarded to the underlying MetricLogger construction by the proxy.

        Returns:
            The proxy logger.
        """
        return TestCaseLoggerProxy(cls, args, kwargs)

    @classmethod
    def for_test_class(cls, *args, **kwargs):
        """Registers the logger class for each test class.

        Creates a proxy logger that will instantiate this method's logger class
        for each test class. Any arguments passed to this method will be
        forwarded to the underlying MetricLogger construction by the proxy.

        Returns:
            The proxy logger.
        """
        return TestClassLoggerProxy(cls, args, kwargs)

    @classmethod
    def _compile_proto(cls, proto_path, compiler_out=None):
        """Compile and return a proto file into a module.

        Args:
            proto_path: the path to the proto file. Can be either relative to
                        the logger class file or absolute.
            compiler_out: the directory in which to write the result of the
                          compilation
        """
        if not compiler_out:
            compiler_out = tempfile.mkdtemp()

        if path.isabs(proto_path):
            abs_proto_path = proto_path
        else:
            classfile = inspect.getfile(cls)
            base_dir = path.dirname(path.realpath(classfile))
            abs_proto_path = path.normpath(path.join(base_dir, proto_path))

        return compile_import_proto(compiler_out, abs_proto_path)

    def __init__(self, context=None, publisher=None, event=None):
        """Initializes a MetricLogger.

        If context or publisher are passed, they are set as attributes to the
        logger. Otherwise, they will be initialized later by an event.

        If event is passed, it is used immediately to populate the context and
        publisher (unless they are explicitly passed as well).

        Args:
             context: the MetricContext in which this logger has been created
             publisher: the MetricPublisher to use
             event: an event triggering the creation of this logger, used to
                    populate context and publisher
        """
        self.context = context
        self.publisher = publisher
        if event:
            self._init_for_event(event)

    def start(self, event):
        """Start the logging process.

        Args:
            event: the event that is triggering this start
        """
        pass

    def end(self, event):
        """End the logging process.

        Args:
            event: the event that is triggering this start
        """
        pass

    def _init_for_event(self, event):
        """Populate unset attributes with default values."""
        if not self.context:
            self.context = self._get_default_context(event)
        if not self.publisher:
            self.publisher = self._get_default_publisher(event)

    def _get_default_context(self, event):
        """Get the default context for the given event."""
        return get_context_for_event(event)

    def _get_default_publisher(self, _):
        """Get the default publisher for the given event."""
        return ProtoMetricPublisher(self.context)


class LoggerProxy(object):
    """A proxy object to manage and forward calls to an underlying logger.

    The proxy is intended to respond to certain framework events and
    create/discard the underlying logger as appropriate. It should be treated
    as an abstract class, with subclasses specifying what actions to be taken
    based on certain events.

    There is no global registry of proxies, so implementations should be
    inherently self-managing. In particular, they should unregister any
    subscriptions they have once they are finished.

    Attributes:
        _logger_cls: the class object for the underlying logger
        _logger_args: the position args for the logger constructor
        _logger_kwargs: the keyword args for the logger constructor. Note that
                        the triggering even is always passed as a keyword arg.
        __initialized: Whether the class attributes have been initialized. Used
                      by __getattr__ and __setattr__ to prevent infinite
                      recursion.
    """

    def __init__(self, logger_cls, logger_args, logger_kwargs):
        """Constructs a proxy for the given logger class.

        The logger class will later be constructed using the triggering event,
        along with the args and kwargs passed here.

        This will also register any methods decorated with event subscriptions
        that may have been defined in a subclass. It is the subclass's
        responsibility to unregister them once the logger is finished.

        Args:
            logger_cls: The class object for the underlying logger.
            logger_args: The position args for the logger constructor.
            logger_kwargs: The keyword args for the logger constructor.
        """
        self._logger_cls = logger_cls
        self._logger_args = logger_args
        self._logger_kwargs = logger_kwargs
        self._logger = None
        bundle = subscription_bundle.create_from_instance(self)
        bundle.register()
        self.__initialized = True

    def _setup_proxy(self, event):
        """Creates and starts the underlying logger based on the event.

        Args:
            event: The event that triggered this logger.
        """
        self._logger = self._logger_cls(event=event, *self._logger_args,
                                        **self._logger_kwargs)
        self._logger.start(event)

    def _teardown_proxy(self, event):
        """Ends and removes the underlying logger.

        If the underlying logger does not exist, no action is taken. We avoid
        raising an error in this case with the implicit assumption that
        _setup_proxy would have raised one already if logger creation failed.

        Args:
            event: The triggering event.
        """

        # Here, we surround the logger's end() function with a catch-all try
        # statement. This prevents logging failures from crashing the test class
        # before all test cases have completed. Note that this has not been
        # added to _setup_proxy. Failure in teardown is more likely due to
        # failure to receive metric data (e.g., was unable to be gathered), or
        # failure to log to the correct proto (e.g., incorrect format).

        # noinspection PyBroadException
        try:
            if self._logger:
                self._logger.end(event)
        except Exception:
            logging.error('Unable to properly close logger %s.' %
                          self._logger.__class__.__name__)
            logging.debug("\n%s" % traceback.format_exc())
        finally:
            self._logger = None

    def __getattr__(self, attr):
        """Forwards attribute access to the underlying logger.

        Args:
            attr: The name of the attribute to retrieve.

        Returns:
            The attribute with name attr from the underlying logger.

        Throws:
            ValueError: If the underlying logger is not set.
        """
        logger = getattr(self, '_logger', None)
        if not logger:
            raise ValueError('Underlying logger is not initialized.')
        return getattr(logger, attr)

    def __setattr__(self, attr, value):
        """Forwards attribute access to the underlying logger.

        Args:
            attr: The name of the attribute to set.
            value: The value of the attribute to set.

        Throws:
            ValueError: If the underlying logger is not set.
        """
        if not self.__dict__.get('_LoggerProxy__initialized', False):
            return super().__setattr__(attr, value)
        if attr == '_logger':
            return super().__setattr__(attr, value)
        logger = getattr(self, '_logger', None)
        if not logger:
            raise ValueError('Underlying logger is not initialized.')
        return setattr(logger, attr, value)


class TestCaseLoggerProxy(LoggerProxy):
    """A LoggerProxy implementation to subscribe to test case events.

    The underlying logger will be created and destroyed on test case begin and
    end events respectively. The proxy will unregister itself from the event
    bus at the end of the test class.
    """

    def __init__(self, logger_cls, logger_args, logger_kwargs):
        super().__init__(logger_cls, logger_args, logger_kwargs)

    @subscribe(TestCaseBeginEvent)
    def __on_test_case_begin(self, event):
        """Sets up the proxy for a test case."""
        self._setup_proxy(event)

    @subscribe(TestCaseEndEvent)
    def __on_test_case_end(self, event):
        """Tears down the proxy for a test case."""
        self._teardown_proxy(event)

    @subscribe(TestClassEndEvent)
    def __on_test_class_end(self, event):
        """Cleans up the subscriptions at the end of a class."""
        event_bus.unregister(self.__on_test_case_begin)
        event_bus.unregister(self.__on_test_case_end)
        event_bus.unregister(self.__on_test_class_end)


class TestClassLoggerProxy(LoggerProxy):
    """A LoggerProxy implementation to subscribe to test class events.

    The underlying logger will be created and destroyed on test class begin and
    end events respectively. The proxy will also unregister itself from the
    event bus at the end of the test class.
    """

    def __init__(self, logger_cls, logger_args, logger_kwargs):
        super().__init__(logger_cls, logger_args, logger_kwargs)

    @subscribe(TestClassBeginEvent)
    def __on_test_class_begin(self, event):
        """Sets up the proxy for a test class."""
        self._setup_proxy(event)

    @subscribe(TestClassEndEvent)
    def __on_test_class_end(self, event):
        """Tears down the proxy for a test class and removes subscriptions."""
        self._teardown_proxy(event)
        event_bus.unregister(self.__on_test_class_begin)
        event_bus.unregister(self.__on_test_class_end)
