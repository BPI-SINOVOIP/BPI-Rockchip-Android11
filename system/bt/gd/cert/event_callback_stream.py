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

from concurrent.futures import ThreadPoolExecutor
from grpc import RpcError
import logging


class EventCallbackStream(object):
    """
    A an object that translate a gRPC stream of events to a Python stream of
    callbacks.

    All callbacks are non-sticky. This means that user will only receive callback
    generated after EventCallbackStream is registered and will not receive any
    callback after EventCallbackStream is unregistered

    You would need a new EventCallbackStream and anything that depends on this
    object once shutdown() is called
    """

    def __init__(self, server_stream_call):
        """
        Construct this object, call the |grpc_lambda| and trigger event_callback on
        the thread used to create this object until |destroy| is called when this
        object can no longer be used
        :param server_stream_call: A server stream call object returned from
                                   calling a gRPC server stream RPC API. The
                                   object must support iterator interface (i.e.
                                   next() method) and the grpc.Call interface
                                   so that we can cancel it
        :param event_callback: callback to be invoked with the only argument as
                               the generated event. The callback will be invoked
                               on a separate thread created within this object
        """
        if server_stream_call is None:
            raise ValueError("server_stream_call must not be None")
        self.server_stream_call = server_stream_call
        self.handlers = []
        self.executor = ThreadPoolExecutor()
        self.future = self.executor.submit(EventCallbackStream._event_loop,
                                           self)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.shutdown()
        if traceback is None:
            return True
        else:
            return False

    def __del__(self):
        self.shutdown()

    def register_callback(self, callback, matcher_fn=None):
        """
        Register a callback to handle events. Event will be handled by callback
        if matcher_fn(event) returns True

        callback and matcher are registered as a tuple. Hence the same callback
        with different matcher are considered two different handler units. Same
        matcher, but different callback are also considered different handling
        unit

        Callback will be invoked on a ThreadPoolExecutor owned by this
        EventCallbackStream

        :param callback: Will be called as callback(event)
        :param matcher_fn: A boolean function that returns True or False when
                           calling matcher_fn(event), if None, all event will
                           be matched
        """
        if callback is None:
            raise ValueError("callback must not be None")
        self.handlers.append((callback, matcher_fn))

    def unregister_callback(self, callback, matcher_fn=None):
        """
        Unregister callback and matcher_fn from the event stream. Both objects
        must match exactly the ones when calling register_callback()

        :param callback: callback used in register_callback()
        :param matcher_fn: matcher_fn used in register_callback()
        :raises ValueError when (callback, matcher_fn) tuple is not found
        """
        if callback is None:
            raise ValueError("callback must not be None")
        self.handlers.remove((callback, matcher_fn))

    def shutdown(self):
        """
        Stop the gRPC lambda so that event_callback will not be invoked after th
        method returns.

        This object will be useless after this call as there is no way to restart
        the gRPC callback. You would have to create a new EventCallbackStream

        :return: None on success, exception object on failure
        """
        while not self.server_stream_call.done():
            self.server_stream_call.cancel()
        exception_for_return = None
        try:
            result = self.future.result()
            if result:
                logging.warning("Inner loop error %s" % result)
                raise result
        except Exception as exp:
            logging.warning("Exception: %s" % (exp))
            exception_for_return = exp
        self.executor.shutdown()
        return exception_for_return

    def _event_loop(self):
        """
        Main loop for consuming the gRPC stream events.
        Blocks until computation is cancelled
        :return: None on success, exception object on failure
        """
        try:
            for event in self.server_stream_call:
                for (callback, matcher_fn) in self.handlers:
                    if not matcher_fn or matcher_fn(event):
                        callback(event)
            return None
        except RpcError as exp:
            if self.server_stream_call.cancelled():
                logging.debug("Cancelled")
                return None
            else:
                logging.warning("Some RPC error not due to cancellation")
            return exp
