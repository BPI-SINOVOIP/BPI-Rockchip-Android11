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

from cert.gd_base_test_facade_only import GdFacadeOnlyBaseTestClass
from cert.event_callback_stream import EventCallbackStream
from cert.event_asserts import EventAsserts
from google.protobuf import empty_pb2 as empty_proto
from facade import rootservice_pb2 as facade_rootservice
from facade import common_pb2 as common
from hci.facade import le_acl_manager_facade_pb2 as le_acl_manager_facade
from hci.facade import le_advertising_manager_facade_pb2 as le_advertising_facade
from hci.facade import facade_pb2 as hci_facade
import bluetooth_packets_python3 as bt_packets
from bluetooth_packets_python3 import hci_packets


class LeAclManagerTest(GdFacadeOnlyBaseTestClass):

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

    def enqueue_acl_data(self, handle, pb_flag, b_flag, acl):
        acl_msg = hci_facade.AclMsg(
            handle=int(handle),
            packet_boundary_flag=int(pb_flag),
            broadcast_flag=int(b_flag),
            data=acl)
        self.cert_device.hci.SendAclData(acl_msg)

    def test_dut_connects(self):
        self.register_for_le_event(hci_packets.SubeventCode.CONNECTION_COMPLETE)
        with EventCallbackStream(self.cert_device.hci.FetchLeSubevents(empty_proto.Empty())) as cert_hci_le_event_stream, \
            EventCallbackStream(self.cert_device.hci.FetchAclPackets(empty_proto.Empty())) as cert_acl_data_stream, \
            EventCallbackStream(self.device_under_test.hci_le_acl_manager.FetchAclData(empty_proto.Empty())) as acl_data_stream:

            cert_hci_le_event_asserts = EventAsserts(cert_hci_le_event_stream)
            acl_data_asserts = EventAsserts(acl_data_stream)
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)

            # Cert Advertises
            advertising_handle = 0
            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingLegacyParametersBuilder(
                    advertising_handle,
                    hci_packets.LegacyAdvertisingProperties.ADV_IND,
                    400,
                    450,
                    7,
                    hci_packets.OwnAddressType.RANDOM_DEVICE_ADDRESS,
                    hci_packets.PeerAddressType.
                    PUBLIC_DEVICE_OR_IDENTITY_ADDRESS,
                    '00:00:00:00:00:00',
                    hci_packets.AdvertisingFilterPolicy.ALL_DEVICES,
                    0xF8,
                    1,  #SID
                    hci_packets.Enable.DISABLED  # Scan request notification
                ),
                True)

            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingRandomAddressBuilder(
                    advertising_handle, '0C:05:04:03:02:01'), True)

            gap_name = hci_packets.GapData()
            gap_name.data_type = hci_packets.GapDataType.COMPLETE_LOCAL_NAME
            gap_name.data = list(bytes(b'Im_A_Cert'))

            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingDataBuilder(
                    advertising_handle,
                    hci_packets.Operation.COMPLETE_ADVERTISEMENT,
                    hci_packets.FragmentPreference.CONTROLLER_SHOULD_NOT,
                    [gap_name]), True)

            gap_short_name = hci_packets.GapData()
            gap_short_name.data_type = hci_packets.GapDataType.SHORTENED_LOCAL_NAME
            gap_short_name.data = list(bytes(b'Im_A_C'))

            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingScanResponseBuilder(
                    advertising_handle,
                    hci_packets.Operation.COMPLETE_ADVERTISEMENT,
                    hci_packets.FragmentPreference.CONTROLLER_SHOULD_NOT,
                    [gap_short_name]), True)

            enabled_set = hci_packets.EnabledSet()
            enabled_set.advertising_handle = advertising_handle
            enabled_set.duration = 0
            enabled_set.max_extended_advertising_events = 0
            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingEnableBuilder(
                    hci_packets.Enable.ENABLED, [enabled_set]), True)

            with EventCallbackStream(
                    self.device_under_test.hci_le_acl_manager.CreateConnection(
                        le_acl_manager_facade.LeConnectionMsg(
                            address_type=int(
                                hci_packets.AddressType.RANDOM_DEVICE_ADDRESS),
                            address=bytes('0C:05:04:03:02:01',
                                          'utf8')))) as connection_event_stream:

                connection_event_asserts = EventAsserts(connection_event_stream)

                # Cert gets ConnectionComplete with a handle and sends ACL data
                handle = 0xfff

                def get_handle(packet):
                    packet_bytes = packet.event
                    nonlocal handle
                    if b'\x3e\x13\x01\x00' in packet_bytes:
                        cc_view = hci_packets.LeConnectionCompleteView(
                            hci_packets.LeMetaEventView(
                                hci_packets.EventPacketView(
                                    bt_packets.PacketViewLittleEndian(
                                        list(packet_bytes)))))
                        handle = cc_view.GetConnectionHandle()
                        return True
                    if b'\x3e\x13\x0A\x00' in packet_bytes:
                        cc_view = hci_packets.LeEnhancedConnectionCompleteView(
                            hci_packets.LeMetaEventView(
                                hci_packets.EventPacketView(
                                    bt_packets.PacketViewLittleEndian(
                                        list(packet_bytes)))))
                        handle = cc_view.GetConnectionHandle()
                        return True
                    return False

                cert_hci_le_event_asserts.assert_event_occurs(get_handle)
                cert_handle = handle

                self.enqueue_acl_data(
                    cert_handle, hci_packets.PacketBoundaryFlag.
                    FIRST_AUTOMATICALLY_FLUSHABLE,
                    hci_packets.BroadcastFlag.POINT_TO_POINT,
                    bytes(b'\x19\x00\x07\x00SomeAclData from the Cert'))

                # DUT gets a connection complete event and sends and receives
                handle = 0xfff
                connection_event_asserts.assert_event_occurs(get_handle)

                self.device_under_test.hci_le_acl_manager.SendAclData(
                    le_acl_manager_facade.LeAclData(
                        handle=handle,
                        payload=bytes(
                            b'\x1C\x00\x07\x00SomeMoreAclData from the DUT')))

                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: b'SomeMoreAclData' in packet.data)
                acl_data_asserts.assert_event_occurs(
                    lambda packet: b'SomeAclData' in packet.payload)

    def test_cert_connects(self):
        self.register_for_le_event(hci_packets.SubeventCode.CONNECTION_COMPLETE)
        with EventCallbackStream(self.cert_device.hci.FetchLeSubevents(empty_proto.Empty())) as cert_hci_le_event_stream, \
                EventCallbackStream(self.cert_device.hci.FetchAclPackets(empty_proto.Empty())) as cert_acl_data_stream, \
                EventCallbackStream(self.device_under_test.hci_le_acl_manager.FetchIncomingConnection(empty_proto.Empty())) as incoming_connection_stream, \
                EventCallbackStream(self.device_under_test.hci_le_acl_manager.FetchAclData(empty_proto.Empty())) as acl_data_stream:

            cert_hci_le_event_asserts = EventAsserts(cert_hci_le_event_stream)
            incoming_connection_asserts = EventAsserts(
                incoming_connection_stream)
            acl_data_asserts = EventAsserts(acl_data_stream)
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)

            # DUT Advertises
            gap_name = hci_packets.GapData()
            gap_name.data_type = hci_packets.GapDataType.COMPLETE_LOCAL_NAME
            gap_name.data = list(bytes(b'Im_The_DUT'))
            gap_data = le_advertising_facade.GapDataMsg(
                data=bytes(gap_name.Serialize()))
            config = le_advertising_facade.AdvertisingConfig(
                advertisement=[gap_data],
                random_address=common.BluetoothAddress(
                    address=bytes(b'0D:05:04:03:02:01')),
                interval_min=512,
                interval_max=768,
                event_type=le_advertising_facade.AdvertisingEventType.ADV_IND,
                address_type=common.RANDOM_DEVICE_ADDRESS,
                peer_address_type=common.PUBLIC_DEVICE_OR_IDENTITY_ADDRESS,
                peer_address=common.BluetoothAddress(
                    address=bytes(b'A6:A5:A4:A3:A2:A1')),
                channel_map=7,
                filter_policy=le_advertising_facade.AdvertisingFilterPolicy.
                ALL_DEVICES)
            request = le_advertising_facade.CreateAdvertiserRequest(
                config=config)

            create_response = self.device_under_test.hci_le_advertising_manager.CreateAdvertiser(
                request)

            # Cert Connects
            self.enqueue_hci_command(
                hci_packets.LeSetRandomAddressBuilder('0C:05:04:03:02:01'),
                True)
            phy_scan_params = hci_packets.LeCreateConnPhyScanParameters()
            phy_scan_params.scan_interval = 0x60
            phy_scan_params.scan_window = 0x30
            phy_scan_params.conn_interval_min = 0x18
            phy_scan_params.conn_interval_max = 0x28
            phy_scan_params.conn_latency = 0
            phy_scan_params.supervision_timeout = 0x1f4
            phy_scan_params.min_ce_length = 0
            phy_scan_params.max_ce_length = 0
            self.enqueue_hci_command(
                hci_packets.LeExtendedCreateConnectionBuilder(
                    hci_packets.InitiatorFilterPolicy.USE_PEER_ADDRESS,
                    hci_packets.OwnAddressType.RANDOM_DEVICE_ADDRESS,
                    hci_packets.AddressType.RANDOM_DEVICE_ADDRESS,
                    '0D:05:04:03:02:01', 1, [phy_scan_params]), False)

            # Cert gets ConnectionComplete with a handle and sends ACL data
            handle = 0xfff

            def get_handle(packet):
                packet_bytes = packet.event
                nonlocal handle
                if b'\x3e\x13\x01\x00' in packet_bytes:
                    cc_view = hci_packets.LeConnectionCompleteView(
                        hci_packets.LeMetaEventView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes)))))
                    handle = cc_view.GetConnectionHandle()
                    return True
                if b'\x3e\x13\x0A\x00' in packet_bytes:
                    cc_view = hci_packets.LeEnhancedConnectionCompleteView(
                        hci_packets.LeMetaEventView(
                            hci_packets.EventPacketView(
                                bt_packets.PacketViewLittleEndian(
                                    list(packet_bytes)))))
                    handle = cc_view.GetConnectionHandle()
                    return True
                return False

            cert_hci_le_event_asserts.assert_event_occurs(get_handle)
            cert_handle = handle

            self.enqueue_acl_data(
                cert_handle,
                hci_packets.PacketBoundaryFlag.FIRST_AUTOMATICALLY_FLUSHABLE,
                hci_packets.BroadcastFlag.POINT_TO_POINT,
                bytes(b'\x19\x00\x07\x00SomeAclData from the Cert'))

            # DUT gets a connection complete event and sends and receives
            handle = 0xfff
            incoming_connection_asserts.assert_event_occurs(get_handle)

            self.device_under_test.hci_le_acl_manager.SendAclData(
                le_acl_manager_facade.LeAclData(
                    handle=handle,
                    payload=bytes(
                        b'\x1C\x00\x07\x00SomeMoreAclData from the DUT')))

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b'SomeMoreAclData' in packet.data)
            acl_data_asserts.assert_event_occurs(
                lambda packet: b'SomeAclData' in packet.payload)

    def test_recombination_l2cap_packet(self):
        self.register_for_le_event(hci_packets.SubeventCode.CONNECTION_COMPLETE)
        with EventCallbackStream(self.cert_device.hci.FetchLeSubevents(empty_proto.Empty())) as cert_hci_le_event_stream, \
                EventCallbackStream(self.cert_device.hci.FetchAclPackets(empty_proto.Empty())) as cert_acl_data_stream, \
                EventCallbackStream(self.device_under_test.hci_le_acl_manager.FetchAclData(empty_proto.Empty())) as acl_data_stream:

            cert_hci_le_event_asserts = EventAsserts(cert_hci_le_event_stream)
            acl_data_asserts = EventAsserts(acl_data_stream)
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)

            # Cert Advertises
            advertising_handle = 0
            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingLegacyParametersBuilder(
                    advertising_handle,
                    hci_packets.LegacyAdvertisingProperties.ADV_IND,
                    400,
                    450,
                    7,
                    hci_packets.OwnAddressType.RANDOM_DEVICE_ADDRESS,
                    hci_packets.PeerAddressType.
                    PUBLIC_DEVICE_OR_IDENTITY_ADDRESS,
                    '00:00:00:00:00:00',
                    hci_packets.AdvertisingFilterPolicy.ALL_DEVICES,
                    0xF8,
                    1,  #SID
                    hci_packets.Enable.DISABLED  # Scan request notification
                ),
                True)

            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingRandomAddressBuilder(
                    advertising_handle, '0C:05:04:03:02:01'), True)

            gap_name = hci_packets.GapData()
            gap_name.data_type = hci_packets.GapDataType.COMPLETE_LOCAL_NAME
            gap_name.data = list(bytes(b'Im_A_Cert'))

            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingDataBuilder(
                    advertising_handle,
                    hci_packets.Operation.COMPLETE_ADVERTISEMENT,
                    hci_packets.FragmentPreference.CONTROLLER_SHOULD_NOT,
                    [gap_name]), True)

            gap_short_name = hci_packets.GapData()
            gap_short_name.data_type = hci_packets.GapDataType.SHORTENED_LOCAL_NAME
            gap_short_name.data = list(bytes(b'Im_A_C'))

            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingScanResponseBuilder(
                    advertising_handle,
                    hci_packets.Operation.COMPLETE_ADVERTISEMENT,
                    hci_packets.FragmentPreference.CONTROLLER_SHOULD_NOT,
                    [gap_short_name]), True)

            enabled_set = hci_packets.EnabledSet()
            enabled_set.advertising_handle = advertising_handle
            enabled_set.duration = 0
            enabled_set.max_extended_advertising_events = 0
            self.enqueue_hci_command(
                hci_packets.LeSetExtendedAdvertisingEnableBuilder(
                    hci_packets.Enable.ENABLED, [enabled_set]), True)

            with EventCallbackStream(
                    self.device_under_test.hci_le_acl_manager.CreateConnection(
                        le_acl_manager_facade.LeConnectionMsg(
                            address_type=int(
                                hci_packets.AddressType.RANDOM_DEVICE_ADDRESS),
                            address=bytes('0C:05:04:03:02:01',
                                          'utf8')))) as connection_event_stream:

                connection_event_asserts = EventAsserts(connection_event_stream)

                # Cert gets ConnectionComplete with a handle and sends ACL data
                handle = 0xfff

                def get_handle(packet):
                    packet_bytes = packet.event
                    nonlocal handle
                    if b'\x3e\x13\x01\x00' in packet_bytes:
                        cc_view = hci_packets.LeConnectionCompleteView(
                            hci_packets.LeMetaEventView(
                                hci_packets.EventPacketView(
                                    bt_packets.PacketViewLittleEndian(
                                        list(packet_bytes)))))
                        handle = cc_view.GetConnectionHandle()
                        return True
                    if b'\x3e\x13\x0A\x00' in packet_bytes:
                        cc_view = hci_packets.LeEnhancedConnectionCompleteView(
                            hci_packets.LeMetaEventView(
                                hci_packets.EventPacketView(
                                    bt_packets.PacketViewLittleEndian(
                                        list(packet_bytes)))))
                        handle = cc_view.GetConnectionHandle()
                        return True
                    return False

                cert_hci_le_event_asserts.assert_event_occurs(get_handle)
                cert_handle = handle

                # DUT gets a connection complete event
                connection_event_asserts.assert_event_occurs(get_handle)

                self.enqueue_acl_data(
                    cert_handle, hci_packets.PacketBoundaryFlag.
                    FIRST_AUTOMATICALLY_FLUSHABLE,
                    hci_packets.BroadcastFlag.POINT_TO_POINT,
                    bytes(b'\x06\x00\x07\x00Hello'))
                self.enqueue_acl_data(
                    cert_handle,
                    hci_packets.PacketBoundaryFlag.CONTINUING_FRAGMENT,
                    hci_packets.BroadcastFlag.POINT_TO_POINT, bytes(b'!'))

                acl_data_asserts.assert_event_occurs(
                    lambda packet: b'Hello!' in packet.payload)
