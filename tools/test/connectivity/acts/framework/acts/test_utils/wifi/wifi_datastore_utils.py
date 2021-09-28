#!/usr/bin/env python3
#
#   Copyright 2018 Google, Inc.
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

import json
import logging
import pprint
import requests
import time

from acts import asserts
from acts import signals
from acts import utils
from acts.test_utils.wifi import wifi_constants

"""This file consists of all the helper methods needed to interact with the
   Datastore @ https://chaos-188802.appspot.com/ used for Android Interop
   testing.
"""

DATASTORE_HOST = "https://chaos-188802.appspot.com"

# The Datastore defines the following paths for operating methods.
ADD_DEVICE = "devices/new"
REMOVE_DEVICE = "devices/delete"
LOCK_DEVICE = "devices/lock"
UNLOCK_DEVICE = "devices/unlock"
SHOW_DEVICE = "devices/"
GET_DEVICES = "devices/"

# HTTP content type. JSON encoded with UTF-8 character encoding.
HTTP_HEADER = {'content-type': 'application/json'}

def add_device(name, ap_label, lab_label):
    """Add a device(AP or Packet Capturer) in datastore.

       Args:
           name: string, hostname of the device.
           ap_label: string, AP brand name.
           lab_label: string, lab label for AP.
       Returns:
          True if device was added successfully; 0 otherwise.
    """
    request = DATASTORE_HOST + '/' + ADD_DEVICE
    logging.debug("Request = %s" % request)
    response = requests.post(request,
                             headers=HTTP_HEADER,
                             data=json.dumps({"hostname":name,
                                              "ap_label":ap_label,
                                              "lab_label":lab_label}))
    if response.json()['result'] == 'success':
        logging.info("Added device %s to datastore" % name)
        return True
    return False

def remove_device(name):
    """Delete a device(AP or Packet Capturer) in datastore.

       Args:
           name: string, hostname of the device to delete.
       Returns:
           True if device was deleted successfully; 0 otherwise.
    """
    request = DATASTORE_HOST + '/' + REMOVE_DEVICE
    logging.debug("Request = %s" % request)
    response = requests.put(request,
                            headers=HTTP_HEADER,
                            data=json.dumps({"hostname":name}))
    result_str = "%s deleted." % name
    if result_str in response.text:
        logging.info("Removed device %s from datastore" % name)
        return True
    return False

def lock_device(name, admin):
    """Lock a device(AP or Packet Capturer) in datastore.

       Args:
           name: string, hostname of the device in datastore.
           admin: string, unique admin name for locking.
      Returns:
          True if operation was successful; 0 otherwise.
    """
    request = DATASTORE_HOST + '/' + LOCK_DEVICE
    logging.debug("Request = %s" % request)
    response = requests.put(request,
                            headers=HTTP_HEADER,
                            data=json.dumps({"hostname":name, "locked_by":admin}))
    if response.json()['result']:
        logging.info("Locked device %s in datastore" % name)
        return True
    return False

def unlock_device(name):
    """Un-lock a device(AP or Packet Capturer) in datastore.

       Args:
           name: string, hostname of the device in datastore.
      Returns:
          True if operation was successful; 0 otherwise.
    """
    request = DATASTORE_HOST + '/' + UNLOCK_DEVICE
    logging.debug("Request = %s" % request)
    response = requests.put(request,
                            headers=HTTP_HEADER,
                            data=json.dumps({"hostname":name}))
    if response.json()['result']:
        logging.info("Finished un-locking AP %s in datastore" % name)
        return True
    return False

def show_device(name):
    """Show device properties for a given device(AP or Packet Capturer).

       Args:
           name: string, hostname of the device in datastore to fetch info.
           Returns: dict of device name:value properties if successful;
                    None otherwise.
    """
    request = DATASTORE_HOST + '/' + SHOW_DEVICE + name
    logging.debug("Request = %s" % request)
    response = requests.get(request)
    if 'error' in response.text:
        return None
    return response.json()

def get_devices():
    """Get a list of all devices in the datastore.

    Returns: dict of all devices' name:value properties if successful;
             None otherwise.
    """
    request = DATASTORE_HOST + '/' + GET_DEVICES
    logging.debug("Request = %s" % request)
    response = requests.get(request)
    if 'error' in response.text:
        return None
    return response.json()
