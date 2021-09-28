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

import os
import sys
import logging

from cert.gd_base_test_facade_only import GdFacadeOnlyBaseTestClass
from cert.event_callback_stream import EventCallbackStream
from cert.event_asserts import EventAsserts
from google.protobuf import empty_pb2 as empty_proto
from facade import rootservice_pb2 as facade_rootservice
from hci.facade import facade_pb2 as hci_facade
from hci.facade import le_scanning_manager_facade_pb2 as le_scanning_facade
from hci.facade import le_advertising_manager_facade_pb2 as le_advertising_facade
from bluetooth_packets_python3 import hci_packets
from facade import common_pb2 as common


class LeScanningManagerTest(GdFacadeOnlyBaseTestClass):

    def setup_test(self):
        self.device_under_test.rootservice.StartStack(
            facade_rootservice.StartStackRequest(
                module_under_test=facade_rootservice.BluetoothModule.Value(
                    'HCI_INTERFACES'),))
        self.cert_device.rootservice.StartStack(
            facade_rootservice.StartStackRequest(
                module_under_test=facade_rootservice.BluetoothModule.Value(
                    'HCI_INTERFACES'),))

        self.device_under_test.wait_channel_ready()
        self.cert_device.wait_channel_ready()

    def teardown_test(self):
        self.device_under_test.rootservice.StopStack(
            facade_rootservice.StopStackRequest())
        self.cert_device.rootservice.StopStack(
            facade_rootservice.StopStackRequest())

    def register_for_event(self, event_code):
        msg = hci_facade.EventCodeMsg(code=int(event_code))
        self.cert_device.hci.RegisterEventHandler(msg)

    def register_for_le_event(self, event_code):
        msg = hci_facade.LeSubeventCodeMsg(code=int(event_code))
        self.cert_device.hci.RegisterLeEventHandler(msg)

    def enqueue_hci_command(self, command, expect_complete):
        cmd_bytes = bytes(command.Serialize())
        cmd = hci_facade.CommandMsg(command=cmd_bytes)
        if (expect_complete):
            self.cert_device.hci.EnqueueCommandWithComplete(cmd)
        else:
            self.cert_device.hci.EnqueueCommandWithStatus(cmd)

    def test_le_ad_scan_dut_scans(self):
        with EventCallbackStream(
                # DUT Scans
                self.device_under_test.hci_le_scanning_manager.StartScan(
                    empty_proto.Empty())) as advertising_event_stream:

            hci_event_asserts = EventAsserts(advertising_event_stream)

            # CERT Advertises
            gap_name = hci_packets.GapData()
            gap_name.data_type = hci_packets.GapDataType.COMPLETE_LOCAL_NAME
            gap_name.data = list(bytes(b'Im_The_CERT!'))
            gap_data = le_advertising_facade.GapDataMsg(
                data=bytes(gap_name.Serialize()))
            config = le_advertising_facade.AdvertisingConfig(
                advertisement=[gap_data],
                random_address=common.BluetoothAddress(
                    address=bytes(b'A6:A5:A4:A3:A2:A1')),
                interval_min=512,
                interval_max=768,
                event_type=le_advertising_facade.AdvertisingEventType.ADV_IND,
                address_type=common.RANDOM_DEVICE_ADDRESS,
                peer_address_type=common.PUBLIC_DEVICE_OR_IDENTITY_ADDRESS,
                peer_address=common.BluetoothAddress(
                    address=bytes(b'0C:05:04:03:02:01')),
                channel_map=7,
                filter_policy=le_advertising_facade.AdvertisingFilterPolicy.
                ALL_DEVICES)
            request = le_advertising_facade.CreateAdvertiserRequest(
                config=config)

            create_response = self.cert_device.hci_le_advertising_manager.CreateAdvertiser(
                request)

            hci_event_asserts.assert_event_occurs(
                lambda packet: b'Im_The_CERT' in packet.event)

            remove_request = le_advertising_facade.RemoveAdvertiserRequest(
                advertiser_id=create_response.advertiser_id)
            self.cert_device.hci_le_advertising_manager.RemoveAdvertiser(
                remove_request)
