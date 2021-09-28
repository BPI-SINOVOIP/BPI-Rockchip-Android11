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

"""Asuite simple Metrics Functions"""

import json
import logging
import os
import uuid

from urllib.request import Request
from urllib.request import urlopen


_JSON_HEADERS = {'Content-Type': 'application/json'}
_METRICS_RESPONSE = 'done'
_METRICS_TIMEOUT = 2 #seconds
_META_FILE = os.path.join(os.path.expanduser('~'),
                          '.config', 'asuite', '.metadata')
_ANDROID_BUILD_TOP = 'ANDROID_BUILD_TOP'

DUMMY_UUID = '00000000-0000-4000-8000-000000000000'


#pylint: disable=broad-except
def log_event(metrics_url, dummy_key_fallback=True, **kwargs):
    """Base log event function for asuite backend.

    Args:
        metrics_url: String, URL to report metrics to.
        dummy_key_fallback: Boolean, If True and unable to get grouping key,
                            use a dummy key otherwise return out. Sometimes we
                            don't want to return metrics for users we are
                            unable to identify. Default True.
        kwargs: Dict, additional fields we want to return metrics for.
    """
    try:
        try:
            key = str(_get_grouping_key())
        except Exception:
            if not dummy_key_fallback:
                return
            key = DUMMY_UUID
        data = {'grouping_key': key,
                'run_id': str(uuid.uuid4())}
        if kwargs:
            data.update(kwargs)
        data = json.dumps(data)
        request = Request(metrics_url, data=data,
                          headers=_JSON_HEADERS)
        response = urlopen(request, timeout=_METRICS_TIMEOUT)
        content = response.read()
        if content != _METRICS_RESPONSE:
            raise Exception('Unexpected metrics response: %s' % content)
    except Exception as e:
        logging.debug('Exception sending metrics: %s', e)


def _get_grouping_key():
    """Get grouping key. Returns UUID.uuid4."""
    if os.path.isfile(_META_FILE):
        with open(_META_FILE) as f:
            try:
                return uuid.UUID(f.read(), version=4)
            except ValueError:
                logging.debug('malformed group_key in file, rewriting')
    # TODO: Delete get_old_key() on 11/17/2018
    key = _get_old_key() or uuid.uuid4()
    dir_path = os.path.dirname(_META_FILE)
    if os.path.isfile(dir_path):
        os.remove(dir_path)
    try:
        os.makedirs(dir_path)
    except OSError as e:
        if not os.path.isdir(dir_path):
            raise e
    with open(_META_FILE, 'w+') as f:
        f.write(str(key))
    return key


def _get_old_key():
    """Get key from old meta data file if exists, else return None."""
    old_file = os.path.join(os.environ[_ANDROID_BUILD_TOP],
                            'tools/tradefederation/core/atest', '.metadata')
    key = None
    if os.path.isfile(old_file):
        with open(old_file) as f:
            try:
                key = uuid.UUID(f.read(), version=4)
            except ValueError:
                logging.debug('error reading old key')
        os.remove(old_file)
    return key
