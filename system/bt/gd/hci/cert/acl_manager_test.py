#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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
from hci.facade import acl_manager_facade_pb2 as acl_manager_facade
from neighbor.facade import facade_pb2 as neighbor_facade
from hci.facade import controller_facade_pb2 as controller_facade
from hci.facade import facade_pb2 as hci_facade
import bluetooth_packets_python3 as bt_packets
from bluetooth_packets_python3 import hci_packets


class AclManagerTest(GdFacadeOnlyBaseTestClass):

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

    def register_for_event(self, event_code):
        msg = hci_facade.EventCodeMsg(code=int(event_code))
        self.cert_device.hci.RegisterEventHandler(msg)

    def enqueue_hci_command(self, command, expect_complete):
        cmd_bytes = bytes(command.Serialize())
        cmd = hci_facade.CommandMsg(command=cmd_bytes)
        if (expect_complete):
            self.cert_device.hci.EnqueueCommandWithComplete(cmd)
        else:
            self.cert_device.hci.EnqueueCommandWithStatus(cmd)

    def enqueue_acl_data(self, handle, pb_flag, b_flag, acl):
        acl_msg = hci_facade.AclMsg(
            handle=int(handle),
            packet_boundary_flag=int(pb_flag),
            broadcast_flag=int(b_flag),
            data=acl)
        self.cert_device.hci.SendAclData(acl_msg)

    def test_dut_connects(self):
        self.register_for_event(hci_packets.EventCode.CONNECTION_REQUEST)
        self.register_for_event(hci_packets.EventCode.CONNECTION_COMPLETE)
        self.register_for_event(
            hci_packets.EventCode.CONNECTION_PACKET_TYPE_CHANGED)
        with EventCallbackStream(self.cert_device.hci.FetchEvents(empty_proto.Empty())) as cert_hci_event_stream, \
            EventCallbackStream(self.cert_device.hci.FetchAclPackets(empty_proto.Empty())) as cert_acl_data_stream, \
            EventCallbackStream(self.device_under_test.hci_acl_manager.FetchAclData(empty_proto.Empty())) as acl_data_stream:

            cert_hci_event_asserts = EventAsserts(cert_hci_event_stream)
            acl_data_asserts = EventAsserts(acl_data_stream)
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)

            # CERT Enables scans and gets its address
            self.enqueue_hci_command(
                hci_packets.WriteScanEnableBuilder(
                    hci_packets.ScanEnable.INQUIRY_AND_PAGE_SCAN), True)

            cert_address = None

            def get_address_from_complete(packet):
                packet_bytes = packet.event
                if b'\x0e\x0a\x01\x09\x10' in packet_bytes:
                    nonlocal cert_address
                    addr_view = hci_packets.ReadBdAddrCompleteView(
                        hci_packets.CommandCompleteView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes)))))
                    cert_address = addr_view.GetBdAddr()
                    return True
                return False

            self.enqueue_hci_command(hci_packets.ReadBdAddrBuilder(), True)

            cert_hci_event_asserts.assert_event_occurs(
                get_address_from_complete)

            with EventCallbackStream(
                    self.device_under_test.hci_acl_manager.CreateConnection(
                        acl_manager_facade.ConnectionMsg(
                            address_type=int(
                                hci_packets.AddressType.PUBLIC_DEVICE_ADDRESS),
                            address=bytes(cert_address,
                                          'utf8')))) as connection_event_stream:

                connection_event_asserts = EventAsserts(connection_event_stream)
                connection_request = None

                def get_connect_request(packet):
                    if b'\x04\x0a' in packet.event:
                        nonlocal connection_request
                        connection_request = hci_packets.ConnectionRequestView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet.event))))
                        return True
                    return False

                # Cert Accepts
                cert_hci_event_asserts.assert_event_occurs(get_connect_request)
                self.enqueue_hci_command(
                    hci_packets.AcceptConnectionRequestBuilder(
                        connection_request.GetBdAddr(),
                        hci_packets.AcceptConnectionRequestRole.REMAIN_SLAVE),
                    False)

                # Cert gets ConnectionComplete with a handle and sends ACL data
                handle = 0xfff

                def get_handle(packet):
                    packet_bytes = packet.event
                    if b'\x03\x0b\x00' in packet_bytes:
                        nonlocal handle
                        cc_view = hci_packets.ConnectionCompleteView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes))))
                        handle = cc_view.GetConnectionHandle()
                        return True
                    return False

                cert_hci_event_asserts.assert_event_occurs(get_handle)
                cert_handle = handle

                self.enqueue_acl_data(
                    cert_handle, hci_packets.PacketBoundaryFlag.
                    FIRST_AUTOMATICALLY_FLUSHABLE,
                    hci_packets.BroadcastFlag.POINT_TO_POINT,
                    bytes(
                        b'\x26\x00\x07\x00This is just SomeAclData from the Cert'
                    ))

                # DUT gets a connection complete event and sends and receives
                handle = 0xfff
                connection_event_asserts.assert_event_occurs(get_handle)

                self.device_under_test.hci_acl_manager.SendAclData(
                    acl_manager_facade.AclData(
                        handle=handle,
                        payload=bytes(
                            b'\x29\x00\x07\x00This is just SomeMoreAclData from the DUT'
                        )))

                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: b'SomeMoreAclData' in packet.data)
                acl_data_asserts.assert_event_occurs(
                    lambda packet: b'SomeAclData' in packet.payload)

    def test_cert_connects(self):
        self.register_for_event(hci_packets.EventCode.CONNECTION_COMPLETE)
        self.register_for_event(hci_packets.EventCode.ROLE_CHANGE)
        self.register_for_event(
            hci_packets.EventCode.CONNECTION_PACKET_TYPE_CHANGED)
        with EventCallbackStream(self.cert_device.hci.FetchEvents(empty_proto.Empty())) as cert_hci_event_stream, \
            EventCallbackStream(self.cert_device.hci.FetchAclPackets(empty_proto.Empty())) as cert_acl_data_stream, \
            EventCallbackStream(self.device_under_test.hci_acl_manager.FetchIncomingConnection(empty_proto.Empty())) as incoming_connection_stream, \
            EventCallbackStream(self.device_under_test.hci_acl_manager.FetchAclData(empty_proto.Empty())) as acl_data_stream:

            cert_hci_event_asserts = EventAsserts(cert_hci_event_stream)
            incoming_connection_asserts = EventAsserts(
                incoming_connection_stream)
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            acl_data_asserts = EventAsserts(acl_data_stream)

            # DUT Enables scans and gets its address
            dut_address = self.device_under_test.hci_controller.GetMacAddress(
                empty_proto.Empty()).address

            self.device_under_test.neighbor.EnablePageScan(
                neighbor_facade.EnableMsg(enabled=True))

            # Cert connects
            self.enqueue_hci_command(
                hci_packets.CreateConnectionBuilder(
                    dut_address.decode('utf-8'),
                    0xcc18,  # Packet Type
                    hci_packets.PageScanRepetitionMode.R1,
                    0x0,
                    hci_packets.ClockOffsetValid.INVALID,
                    hci_packets.CreateConnectionRoleSwitch.ALLOW_ROLE_SWITCH),
                False)

            conn_handle = 0xfff

            def get_handle(packet):
                packet_bytes = packet.event
                if b'\x03\x0b\x00' in packet_bytes:
                    nonlocal conn_handle
                    cc_view = hci_packets.ConnectionCompleteView(
                        hci_packets.EventPacketView(
                            bt_packets.PacketViewLittleEndian(
                                list(packet_bytes))))
                    conn_handle = cc_view.GetConnectionHandle()
                    return True
                return False

            # DUT gets a connection request
            incoming_connection_asserts.assert_event_occurs(get_handle)

            self.device_under_test.hci_acl_manager.SendAclData(
                acl_manager_facade.AclData(
                    handle=conn_handle,
                    payload=bytes(
                        b'\x29\x00\x07\x00This is just SomeMoreAclData from the DUT'
                    )))

            conn_handle = 0xfff

            cert_hci_event_asserts.assert_event_occurs(get_handle)
            cert_handle = conn_handle

            self.enqueue_acl_data(
                cert_handle,
                hci_packets.PacketBoundaryFlag.FIRST_AUTOMATICALLY_FLUSHABLE,
                hci_packets.BroadcastFlag.POINT_TO_POINT,
                bytes(
                    b'\x26\x00\x07\x00This is just SomeAclData from the Cert'))

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b'SomeMoreAclData' in packet.data)
            acl_data_asserts.assert_event_occurs(
                lambda packet: b'SomeAclData' in packet.payload)

    def test_recombination_l2cap_packet(self):
        self.register_for_event(hci_packets.EventCode.CONNECTION_REQUEST)
        self.register_for_event(hci_packets.EventCode.CONNECTION_COMPLETE)
        self.register_for_event(
            hci_packets.EventCode.CONNECTION_PACKET_TYPE_CHANGED)
        with EventCallbackStream(self.cert_device.hci.FetchEvents(empty_proto.Empty())) as cert_hci_event_stream, \
            EventCallbackStream(self.cert_device.hci.FetchAclPackets(empty_proto.Empty())) as cert_acl_data_stream, \
            EventCallbackStream(self.device_under_test.hci_acl_manager.FetchAclData(empty_proto.Empty())) as acl_data_stream:

            cert_hci_event_asserts = EventAsserts(cert_hci_event_stream)
            acl_data_asserts = EventAsserts(acl_data_stream)

            # CERT Enables scans and gets its address
            self.enqueue_hci_command(
                hci_packets.WriteScanEnableBuilder(
                    hci_packets.ScanEnable.INQUIRY_AND_PAGE_SCAN), True)

            cert_address = None

            def get_address_from_complete(packet):
                packet_bytes = packet.event
                if b'\x0e\x0a\x01\x09\x10' in packet_bytes:
                    nonlocal cert_address
                    addr_view = hci_packets.ReadBdAddrCompleteView(
                        hci_packets.CommandCompleteView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes)))))
                    cert_address = addr_view.GetBdAddr()
                    return True
                return False

            self.enqueue_hci_command(hci_packets.ReadBdAddrBuilder(), True)

            cert_hci_event_asserts.assert_event_occurs(
                get_address_from_complete)

            with EventCallbackStream(
                    self.device_under_test.hci_acl_manager.CreateConnection(
                        acl_manager_facade.ConnectionMsg(
                            address_type=int(
                                hci_packets.AddressType.PUBLIC_DEVICE_ADDRESS),
                            address=bytes(cert_address,
                                          'utf8')))) as connection_event_stream:

                connection_event_asserts = EventAsserts(connection_event_stream)
                connection_request = None

                def get_connect_request(packet):
                    if b'\x04\x0a' in packet.event:
                        nonlocal connection_request
                        connection_request = hci_packets.ConnectionRequestView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet.event))))
                        return True
                    return False

                # Cert Accepts
                cert_hci_event_asserts.assert_event_occurs(get_connect_request)
                self.enqueue_hci_command(
                    hci_packets.AcceptConnectionRequestBuilder(
                        connection_request.GetBdAddr(),
                        hci_packets.AcceptConnectionRequestRole.REMAIN_SLAVE),
                    False)

                # Cert gets ConnectionComplete with a handle and sends ACL data
                handle = 0xfff

                def get_handle(packet):
                    packet_bytes = packet.event
                    if b'\x03\x0b\x00' in packet_bytes:
                        nonlocal handle
                        cc_view = hci_packets.ConnectionCompleteView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes))))
                        handle = cc_view.GetConnectionHandle()
                        return True
                    return False

                cert_hci_event_asserts.assert_event_occurs(get_handle)
                cert_handle = handle

                self.enqueue_acl_data(
                    cert_handle, hci_packets.PacketBoundaryFlag.
                    FIRST_AUTOMATICALLY_FLUSHABLE,
                    hci_packets.BroadcastFlag.POINT_TO_POINT,
                    bytes(b'\x06\x00\x07\x00Hello'))
                self.enqueue_acl_data(
                    cert_handle,
                    hci_packets.PacketBoundaryFlag.CONTINUING_FRAGMENT,
                    hci_packets.BroadcastFlag.POINT_TO_POINT, bytes(b'!'))
                self.enqueue_acl_data(
                    cert_handle, hci_packets.PacketBoundaryFlag.
                    FIRST_AUTOMATICALLY_FLUSHABLE,
                    hci_packets.BroadcastFlag.POINT_TO_POINT,
                    bytes(b'\xe8\x03\x07\x00' + b'Hello' * 200))

                # DUT gets a connection complete event and sends and receives
                connection_event_asserts.assert_event_occurs(get_handle)

                acl_data_asserts.assert_event_occurs(
                    lambda packet: b'Hello!' in packet.payload)
                acl_data_asserts.assert_event_occurs(
                    lambda packet: b'Hello' * 200 in packet.payload)
