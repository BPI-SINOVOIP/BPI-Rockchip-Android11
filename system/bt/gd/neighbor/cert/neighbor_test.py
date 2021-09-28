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
from hci.facade import controller_facade_pb2 as controller_facade
from neighbor.facade import facade_pb2 as neighbor_facade
from bluetooth_packets_python3 import hci_packets
import bluetooth_packets_python3 as bt_packets


class NeighborTest(GdFacadeOnlyBaseTestClass):

    def setup_test(self):
        self.device_under_test.rootservice.StartStack(
            facade_rootservice.StartStackRequest(
                module_under_test=facade_rootservice.BluetoothModule.Value(
                    'HCI_INTERFACES'),))
        self.cert_device.rootservice.StartStack(
            facade_rootservice.StartStackRequest(
                module_under_test=facade_rootservice.BluetoothModule.Value(
                    'HCI'),))

        self.device_under_test.wait_channel_ready()
        self.cert_device.wait_channel_ready()

    def teardown_test(self):
        self.device_under_test.rootservice.StopStack(
            facade_rootservice.StopStackRequest())
        self.cert_device.rootservice.StopStack(
            facade_rootservice.StopStackRequest())

    def register_for_dut_event(self, event_code):
        msg = hci_facade.EventCodeMsg(code=int(event_code))
        self.device_under_test.hci.RegisterEventHandler(msg)

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

    def test_inquiry_from_dut(self):
        inquiry_msg = neighbor_facade.InquiryMsg(
            inquiry_mode=neighbor_facade.DiscoverabilityMode.GENERAL,
            result_mode=neighbor_facade.ResultMode.STANDARD,
            length_1_28s=3,
            max_results=0)
        with EventCallbackStream(
                self.device_under_test.neighbor.SetInquiryMode(
                    inquiry_msg)) as inquiry_event_stream:
            hci_event_asserts = EventAsserts(inquiry_event_stream)
            self.enqueue_hci_command(
                hci_packets.WriteScanEnableBuilder(
                    hci_packets.ScanEnable.INQUIRY_AND_PAGE_SCAN), True)
            hci_event_asserts.assert_event_occurs(
                lambda msg: b'\x02\x0f' in msg.packet
                # Expecting an HCI Event (code 0x02, length 0x0f)
            )

    def test_inquiry_rssi_from_dut(self):
        inquiry_msg = neighbor_facade.InquiryMsg(
            inquiry_mode=neighbor_facade.DiscoverabilityMode.GENERAL,
            result_mode=neighbor_facade.ResultMode.RSSI,
            length_1_28s=3,
            max_results=0)
        with EventCallbackStream(
                self.device_under_test.neighbor.SetInquiryMode(
                    inquiry_msg)) as inquiry_event_stream:
            hci_event_asserts = EventAsserts(inquiry_event_stream)
            self.enqueue_hci_command(
                hci_packets.WriteScanEnableBuilder(
                    hci_packets.ScanEnable.INQUIRY_AND_PAGE_SCAN), True)
            hci_event_asserts.assert_event_occurs(
                lambda msg: b'\x22\x0f' in msg.packet
                # Expecting an HCI Event (code 0x22, length 0x0f)
            )

    def test_inquiry_extended_from_dut(self):
        name_string = b'Im_A_Cert'
        gap_name = hci_packets.GapData()
        gap_name.data_type = hci_packets.GapDataType.COMPLETE_LOCAL_NAME
        gap_name.data = list(bytes(name_string))
        gap_data = list([gap_name])

        self.enqueue_hci_command(
            hci_packets.WriteExtendedInquiryResponseBuilder(
                hci_packets.FecRequired.NOT_REQUIRED, gap_data), True)
        inquiry_msg = neighbor_facade.InquiryMsg(
            inquiry_mode=neighbor_facade.DiscoverabilityMode.GENERAL,
            result_mode=neighbor_facade.ResultMode.EXTENDED,
            length_1_28s=3,
            max_results=0)
        with EventCallbackStream(
                self.device_under_test.neighbor.SetInquiryMode(
                    inquiry_msg)) as inquiry_event_stream:
            hci_event_asserts = EventAsserts(inquiry_event_stream)
            self.enqueue_hci_command(
                hci_packets.WriteScanEnableBuilder(
                    hci_packets.ScanEnable.INQUIRY_AND_PAGE_SCAN), True)
            hci_event_asserts.assert_event_occurs(
                lambda msg: name_string in msg.packet)

    def test_remote_name(self):
        self.register_for_dut_event(
            hci_packets.EventCode.REMOTE_HOST_SUPPORTED_FEATURES_NOTIFICATION)

        with EventCallbackStream(self.cert_device.hci.FetchEvents(empty_proto.Empty())) as hci_event_stream, \
            EventCallbackStream(self.device_under_test.neighbor.GetRemoteNameEvents(empty_proto.Empty())) as name_event_stream:
            name_event_asserts = EventAsserts(name_event_stream)
            hci_event_asserts = EventAsserts(hci_event_stream)

            cert_name = b'Im_A_Cert'
            padded_name = cert_name
            while len(padded_name) < 248:
                padded_name = padded_name + b'\0'
            self.enqueue_hci_command(
                hci_packets.WriteLocalNameBuilder(padded_name), True)

            hci_event_asserts.assert_event_occurs(
                lambda msg: b'\x0e\x04\x01\x13\x0c' in msg.event)

            address = hci_packets.Address()

            def get_address_from_complete(packet):
                packet_bytes = packet.event
                if b'\x0e\x0a\x01\x09\x10' in packet_bytes:
                    nonlocal address
                    addr_view = hci_packets.ReadBdAddrCompleteView(
                        hci_packets.CommandCompleteView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes)))))
                    address = addr_view.GetBdAddr()
                    return True
                return False

            # DUT Enables scans and gets its address
            self.enqueue_hci_command(
                hci_packets.WriteScanEnableBuilder(
                    hci_packets.ScanEnable.INQUIRY_AND_PAGE_SCAN), True)
            self.enqueue_hci_command(hci_packets.ReadBdAddrBuilder(), True)

            hci_event_asserts.assert_event_occurs(get_address_from_complete)

            cert_address = address.encode('utf8')

            self.device_under_test.neighbor.ReadRemoteName(
                neighbor_facade.RemoteNameRequestMsg(
                    address=cert_address,
                    page_scan_repetition_mode=1,
                    clock_offset=0x6855))
            name_event_asserts.assert_event_occurs(
                lambda msg: cert_name in msg.name)
