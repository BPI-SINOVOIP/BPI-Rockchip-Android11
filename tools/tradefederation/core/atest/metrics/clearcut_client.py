# Copyright 2018, The Android Open Source Project
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

"""Python client library to write logs to Clearcut.

This class is intended to be general-purpose, usable for any Clearcut LogSource.

    Typical usage example:

    client = clearcut.Clearcut(clientanalytics_pb2.LogRequest.MY_LOGSOURCE)
    client.log(my_event)
    client.flush_events()
"""

import logging
import threading
import time
try:
    # PYTHON2
    from urllib2 import urlopen
    from urllib2 import Request
    from urllib2 import HTTPError
    from urllib2 import URLError
except ImportError:
    # PYTHON3
    from urllib.request import urlopen
    from urllib.request import Request
    from urllib.request import HTTPError
    from urllib.request import URLError

from proto import clientanalytics_pb2

_CLEARCUT_PROD_URL = 'https://play.googleapis.com/log'
_DEFAULT_BUFFER_SIZE = 100  # Maximum number of events to be buffered.
_DEFAULT_FLUSH_INTERVAL_SEC = 60  # 1 Minute.
_BUFFER_FLUSH_RATIO = 0.5  # Flush buffer when we exceed this ratio.
_CLIENT_TYPE = 6

class Clearcut(object):
    """Handles logging to Clearcut."""

    def __init__(self, log_source, url=None, buffer_size=None,
                 flush_interval_sec=None):
        """Initializes a Clearcut client.

        Args:
            log_source: The log source.
            url: The Clearcut url to connect to.
            buffer_size: The size of the client buffer in number of events.
            flush_interval_sec: The flush interval in seconds.
        """
        self._clearcut_url = url if url else _CLEARCUT_PROD_URL
        self._log_source = log_source
        self._buffer_size = buffer_size if buffer_size else _DEFAULT_BUFFER_SIZE
        self._pending_events = []
        if flush_interval_sec:
            self._flush_interval_sec = flush_interval_sec
        else:
            self._flush_interval_sec = _DEFAULT_FLUSH_INTERVAL_SEC
        self._pending_events_lock = threading.Lock()
        self._scheduled_flush_thread = None
        self._scheduled_flush_time = float('inf')
        self._min_next_request_time = 0

    def log(self, event):
        """Logs events to Clearcut.

        Logging an event can potentially trigger a flush of queued events. Flushing
        is triggered when the buffer is more than half full or after the flush
        interval has passed.

        Args:
          event: A LogEvent to send to Clearcut.
        """
        self._append_events_to_buffer([event])

    def flush_events(self):
        """ Cancel whatever is scheduled and schedule an immediate flush."""
        if self._scheduled_flush_thread:
            self._scheduled_flush_thread.cancel()
        self._min_next_request_time = 0
        self._schedule_flush_thread(0)

    def _serialize_events_to_proto(self, events):
        log_request = clientanalytics_pb2.LogRequest()
        log_request.request_time_ms = int(time.time() * 1000)
        # pylint: disable=no-member
        log_request.client_info.client_type = _CLIENT_TYPE
        log_request.log_source = self._log_source
        log_request.log_event.extend(events)
        return log_request

    def _append_events_to_buffer(self, events, retry=False):
        with self._pending_events_lock:
            self._pending_events.extend(events)
            if len(self._pending_events) > self._buffer_size:
                index = len(self._pending_events) - self._buffer_size
                del self._pending_events[:index]
            self._schedule_flush(retry)

    def _schedule_flush(self, retry):
        if (not retry
                and len(self._pending_events) >= int(self._buffer_size *
                                                     _BUFFER_FLUSH_RATIO)
                and self._scheduled_flush_time > time.time()):
            # Cancel whatever is scheduled and schedule an immediate flush.
            if self._scheduled_flush_thread:
                self._scheduled_flush_thread.cancel()
            self._schedule_flush_thread(0)
        elif self._pending_events and not self._scheduled_flush_thread:
            # Schedule a flush to run later.
            self._schedule_flush_thread(self._flush_interval_sec)

    def _schedule_flush_thread(self, time_from_now):
        min_wait_sec = self._min_next_request_time - time.time()
        if min_wait_sec > time_from_now:
            time_from_now = min_wait_sec
        logging.debug('Scheduling thread to run in %f seconds', time_from_now)
        self._scheduled_flush_thread = threading.Timer(time_from_now, self._flush)
        self._scheduled_flush_time = time.time() + time_from_now
        self._scheduled_flush_thread.start()

    def _flush(self):
        """Flush buffered events to Clearcut.

        If the sent request is unsuccessful, the events will be appended to
        buffer and rescheduled for next flush.
        """
        with self._pending_events_lock:
            self._scheduled_flush_time = float('inf')
            self._scheduled_flush_thread = None
            events = self._pending_events
            self._pending_events = []
        if self._min_next_request_time > time.time():
            self._append_events_to_buffer(events, retry=True)
            return
        log_request = self._serialize_events_to_proto(events)
        self._send_to_clearcut(log_request.SerializeToString())

    #pylint: disable=broad-except
    def _send_to_clearcut(self, data):
        """Sends a POST request with data as the body.

        Args:
            data: The serialized proto to send to Clearcut.
        """
        request = Request(self._clearcut_url, data=data)
        try:
            response = urlopen(request)
            msg = response.read()
            logging.debug('LogRequest successfully sent to Clearcut.')
            log_response = clientanalytics_pb2.LogResponse()
            log_response.ParseFromString(msg)
            # pylint: disable=no-member
            # Throttle based on next_request_wait_millis value.
            self._min_next_request_time = (log_response.next_request_wait_millis
                                           / 1000 + time.time())
            logging.debug('LogResponse: %s', log_response)
        except HTTPError as e:
            logging.debug('Failed to push events to Clearcut. Error code: %d',
                          e.code)
        except URLError:
            logging.debug('Failed to push events to Clearcut.')
        except Exception as e:
            logging.debug(e)
