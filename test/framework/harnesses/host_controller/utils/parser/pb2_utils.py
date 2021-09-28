#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import json
import logging
import requests

# timeout seconds for requests
REQUESTS_TIMEOUT_SECONDS = 60


def FillDictAndPost(msg,
                    dict_to_fill,
                    url,
                    headers,
                    filters={},
                    caller_name=""):
    """Fills up a dict using contents of the pb2 message and uploads it.

    Args:
        msg: pb2 msg, containing info about all task schedules.
        dict_to_fill: dict, to be converted into a json object before
                      upload.
        url: string, URL for the endpoit API to be called when
                     the dict_to_fill fills up.

    Returns:
        True if successful, False otherwise.
    """
    terminal = True
    ret = True
    sub_msg_list = []
    for field in msg.DESCRIPTOR.fields:
        if field.type == field.TYPE_MESSAGE:
            terminal = False
            for sub_msg in msg.__getattribute__(field.name):
                # make all the messages to be processed after other attrs,
                # otherwise dict_to_fill would be incomplete.
                sub_msg_list.append(sub_msg)
        else:
            if filters and field.name in filters:
                filtered_key = filters[field.name]
            else:
                filtered_key = field.name

            if field.label == field.LABEL_REPEATED:
                dict_to_fill[filtered_key] = list(
                    msg.__getattribute__(field.name))
            else:
                dict_to_fill[filtered_key] = msg.__getattribute__(field.name)

    for sub_msg in sub_msg_list:
        ret = ret and FillDictAndPost(sub_msg, dict_to_fill, url, headers,
                                      filters, caller_name)

    if terminal:
        try:
            response = requests.post(url, data=json.dumps(dict_to_fill),
                                     headers=headers,
                                     timeout=REQUESTS_TIMEOUT_SECONDS)
        except requests.exceptions.Timeout as e:
            logging.exception(e)
            return False
        if response.status_code != requests.codes.ok:
            logging.error("%s error: %s", caller_name, response)
            ret = ret and False

    return ret
