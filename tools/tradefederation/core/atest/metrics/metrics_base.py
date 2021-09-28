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

"""
Metrics base class.
"""

from __future__ import print_function

import logging
import random
import socket
import subprocess
import time
import uuid

import asuite_metrics
import constants

from proto import clientanalytics_pb2
from proto import external_user_log_pb2
from proto import internal_user_log_pb2

from . import clearcut_client

INTERNAL_USER = 0
EXTERNAL_USER = 1

ATEST_EVENTS = {
    INTERNAL_USER: internal_user_log_pb2.AtestLogEventInternal,
    EXTERNAL_USER: external_user_log_pb2.AtestLogEventExternal
}
# log source
ATEST_LOG_SOURCE = {
    INTERNAL_USER: 971,
    EXTERNAL_USER: 934
}


def get_user_type():
    """Get user type.

    Determine the internal user by passing at least one check:
      - whose git mail domain is from google
      - whose hostname is from google
    Otherwise is external user.

    Returns:
        INTERNAL_USER if user is internal, EXTERNAL_USER otherwise.
    """
    try:
        output = subprocess.check_output(['git', 'config', '--get', 'user.email'],
                                         universal_newlines=True)
        if output and output.strip().endswith(constants.INTERNAL_EMAIL):
            return INTERNAL_USER
    except OSError:
        # OSError can be raised when running atest_unittests on a host
        # without git being set up.
        logging.debug('Unable to determine if this is an external run, git is '
                      'not found.')
    except subprocess.CalledProcessError:
        logging.debug('Unable to determine if this is an external run, email '
                      'is not found in git config.')
    try:
        hostname = socket.getfqdn()
        if (hostname and
                any([(x in hostname) for x in constants.INTERNAL_HOSTNAME])):
            return INTERNAL_USER
    except IOError:
        logging.debug('Unable to determine if this is an external run, '
                      'hostname is not found.')
    return EXTERNAL_USER


class MetricsBase(object):
    """Class for separating allowed fields and sending metric."""

    _run_id = str(uuid.uuid4())
    try:
        #pylint: disable=protected-access
        _user_key = str(asuite_metrics._get_grouping_key())
    #pylint: disable=broad-except
    except Exception:
        _user_key = asuite_metrics.DUMMY_UUID
    _user_type = get_user_type()
    _log_source = ATEST_LOG_SOURCE[_user_type]
    cc = clearcut_client.Clearcut(_log_source)
    tool_name = None

    def __new__(cls, **kwargs):
        """Send metric event to clearcut.

        Args:
            cls: this class object.
            **kwargs: A dict of named arguments.

        Returns:
            A Clearcut instance.
        """
        # pylint: disable=no-member
        if not cls.tool_name:
            logging.debug('There is no tool_name, and metrics stops sending.')
            return None
        allowed = ({constants.EXTERNAL} if cls._user_type == EXTERNAL_USER
                   else {constants.EXTERNAL, constants.INTERNAL})
        fields = [k for k, v in vars(cls).items()
                  if not k.startswith('_') and v in allowed]
        fields_and_values = {}
        for field in fields:
            if field in kwargs:
                fields_and_values[field] = kwargs.pop(field)
        params = {'user_key': cls._user_key,
                  'run_id': cls._run_id,
                  'user_type': cls._user_type,
                  'tool_name': cls.tool_name,
                  cls._EVENT_NAME: fields_and_values}
        log_event = cls._build_full_event(ATEST_EVENTS[cls._user_type](**params))
        cls.cc.log(log_event)
        return cls.cc

    @classmethod
    def _build_full_event(cls, atest_event):
        """This is all protobuf building you can ignore.

        Args:
            cls: this class object.
            atest_event: A client_pb2.AtestLogEvent instance.

        Returns:
            A clientanalytics_pb2.LogEvent instance.
        """
        log_event = clientanalytics_pb2.LogEvent()
        log_event.event_time_ms = int((time.time() - random.randint(1, 600)) * 1000)
        log_event.source_extension = atest_event.SerializeToString()
        return log_event
