# -*- coding: utf-8 -*-
#!/usr/bin/env python2.7
# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import requests

"""This class consists of all the helper methods needed to interact with the
   Datastore @ https://chaos-188802.appspot.com/ used for ChromeOS Interop
   testing.
"""

class ChaosDataStoreUtils(object):

    CHAOS_DATASTORE_URL = 'https://chaos-188802.appspot.com'

    # The Datastore defines the following paths for operating methods.
    ADD_DEVICE = "devices/new"
    REMOVE_DEVICE = "devices/delete"
    LOCK_DEVICE = "devices/lock"
    UNLOCK_DEVICE = 'devices/unlock'
    SHOW_DEVICE = "devices/"
    GET_DEVICES = 'devices/'
    GET_UNLOCKED_DEVICES = "unlocked_devices/"
    GET_DEVICES_BY_AP_LABEL = "devices/location"

    # HTTP content type. JSON encoded with UTF-8 character encoding.
    HTTP_HEADER = {'content-type': 'application/json'}


    def add_device(self, host_name, ap_label):
        """
        Add a device(AP or Packet Capturer) in datastore.

        @param host_name: string, hostname of the device.
        @param ap_label: string, CrOS_AP (for AP), CrOS_PCAP (for PCAP)
        @param lab_label: string, CrOS_Chaos (lab name), used for all APs & PCAPs

        @return: True if device was added successfully; False otherwise.
        @rtype: bool

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.ADD_DEVICE
        logging.debug("Request = %s", request)
        response = requests.post(request,
                                 headers=self.HTTP_HEADER,
                                 data=json.dumps({"hostname":host_name,
                                                  "ap_label":ap_label,
                                                  "lab_label":"CrOS_Chaos",
                                                  "router_name":host_name}))
        if response.json()['result']:
            logging.info("Added device %s to datastore", host_name)
            return True

        return False


    def remove_device(self, host_name):
        """
        Delete a device(AP or Packet Capturer) in datastore.

        @param host_name: string, hostname of the device to delete.

        @return: True if device was deleted successfully; False otherwise.
        @rtype: bool

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.REMOVE_DEVICE
        logging.debug("Request = %s", request)
        response = requests.put(request,
                                headers=self.HTTP_HEADER,
                                data=json.dumps({"hostname":host_name}))
        result_str = "%s deleted." % host_name
        if result_str in response.text:
            logging.info("Removed device %s from datastore", host_name)
            return True

        return False


    def lock_device(self, host_name, lock_reason):
        """
        Lock a device(AP or Packet Capturer) in datastore.

        @param host_name: string, hostname of the device in datastore.

        @return: True if operation was successful; False otherwise.
        @rtype: bool

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.LOCK_DEVICE
        logging.debug("Request = %s", request)
        response = requests.put(request,
                                headers=self.HTTP_HEADER,
                                data=json.dumps({"hostname":host_name,
                                                 "locked_by":lock_reason}))
        if response.json()['result']:
            logging.info("Locked device %s in datastore", host_name)
            return True

        return False


    def unlock_device(self, host_name):
        """
        Un-lock a device(AP or Packet Capturer) in datastore.

        @param host_name: string, hostname of the device in datastore.

        @return: True if operation was successful; False otherwise.
        @rtype: bool

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.UNLOCK_DEVICE
        logging.debug("Request = %s", request)
        response = requests.put(request,
                                headers=self.HTTP_HEADER,
                                data=json.dumps({"hostname":host_name}))
        if response.json()['result']:
            logging.info("Finished un-locking AP %s in datastore", host_name)
            return True

        logging.error("Unable to unlock AP %s", host_name)
        return False


    def show_device(self, host_name):
        """
        Show device properties for a given device(AP or Packet Capturer).

        @param host_name: string, hostname of the device in datastore to fetch info.

        @return: dict of device name:value properties if successful;
                 False otherwise.
        @rtype: dict when True, else bool:False

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.SHOW_DEVICE + host_name
        logging.debug("Request = %s", request)
        response = requests.get(request)
        if 'error' in response.text:
            return False

        return response.json()


    def get_unlocked_devices(self):
        """
        Get a list of all un-locked devices in the datastore.

        @return: dict of all un-locked devices' name:value properties if successful;
                 False otherwise.
        @rtype: dict when True, else bool:False

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.GET_UNLOCKED_DEVICES
        logging.debug("Request = %s", request)
        response = requests.get(request)
        if 'error' in response.text:
            return False

        return response.json()


    def get_devices(self):
        """
        Get a list of all devices in the datastore.

        @return: dict of all devices' name:value properties if successful;
                 False otherwise.
        @rtype: dict when True, else bool:False

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.GET_DEVICES
        logging.debug("Request = %s", request)
        response = requests.get(request)
        if 'error' in response.text:
            return False

        return response.json()


    def get_devices_by_type(self, ap_label, lab_label):
        """
        Get list of all un-locked devices by ap_label & lab_label

        @param ap_label: string, CrOS_AP/CrOS_PCAP, to filter device types.
        @param lab_label: string, "CrOS_Chaos", All devices in ChromeOS Chaos lab

        @return: dict of all devices' name:value properties if successful;
                 False otherwise.
        @rtype: dict when True, else bool:False

        """
        request = self.CHAOS_DATASTORE_URL + '/' +  self.GET_DEVICES_BY_AP_LABEL
        logging.debug("Request = %s", request)
        response = requests.put(request,
                                headers=self.HTTP_HEADER,
                                data=json.dumps({"ap_label":ap_label,
                                                 "lab_label":lab_label}))
        if 'error' in response.text:
            return False

        return response.json()


    def find_device(self, host_name):
        """
        Find if given device(AP or Packet Capturer) in DataStore.

        @param host_name: string, hostname of the device in datastore to fetch info.
        @return: True if found; False otherwise.
        @rtype: bool

        """
        request = self.CHAOS_DATASTORE_URL + '/' + self.SHOW_DEVICE + host_name
        logging.debug("Request = %s", request)
        response = requests.get(request)
        if 'null' in response.text:
            return False

        return True
