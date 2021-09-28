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

import time
from datetime import timedelta
from mobly import asserts

from cert.gd_base_test_facade_only import GdFacadeOnlyBaseTestClass
from cert.event_asserts import EventAsserts
from cert.event_callback_stream import EventCallbackStream
from facade import common_pb2
from facade import rootservice_pb2 as facade_rootservice
from google.protobuf import empty_pb2 as empty_proto
from l2cap.classic import facade_pb2 as l2cap_facade_pb2
from neighbor.facade import facade_pb2 as neighbor_facade
from hci.facade import acl_manager_facade_pb2 as acl_manager_facade
import bluetooth_packets_python3 as bt_packets
from bluetooth_packets_python3 import hci_packets, l2cap_packets

# Assemble a sample packet. TODO: Use RawBuilder
SAMPLE_PACKET = l2cap_packets.CommandRejectNotUnderstoodBuilder(1)


class L2capTest(GdFacadeOnlyBaseTestClass):

    def setup_test(self):
        self.device_under_test.rootservice.StartStack(
            facade_rootservice.StartStackRequest(
                module_under_test=facade_rootservice.BluetoothModule.Value(
                    'L2CAP'),))
        self.cert_device.rootservice.StartStack(
            facade_rootservice.StartStackRequest(
                module_under_test=facade_rootservice.BluetoothModule.Value(
                    'HCI_INTERFACES'),))

        self.device_under_test.wait_channel_ready()
        self.cert_device.wait_channel_ready()

        self.device_under_test.address = self.device_under_test.hci_controller.GetMacAddress(
            empty_proto.Empty()).address
        cert_address = self.cert_device.controller_read_only_property.ReadLocalAddress(
            empty_proto.Empty()).address
        self.cert_device.address = cert_address
        self.dut_address = common_pb2.BluetoothAddress(
            address=self.device_under_test.address)
        self.cert_address = common_pb2.BluetoothAddress(
            address=self.cert_device.address)

        self.device_under_test.neighbor.EnablePageScan(
            neighbor_facade.EnableMsg(enabled=True))

        self.cert_acl_handle = 0

        self.on_connection_request = None
        self.on_connection_response = None
        self.on_configuration_request = None
        self.on_configuration_response = None
        self.on_disconnection_request = None
        self.on_disconnection_response = None
        self.on_information_request = None
        self.on_information_response = None

        self.scid_to_dcid = {}
        self.ertm_tx_window_size = 10
        self.ertm_max_transmit = 20

    def _on_connection_request_default(self, l2cap_control_view):
        connection_request_view = l2cap_packets.ConnectionRequestView(
            l2cap_control_view)
        sid = connection_request_view.GetIdentifier()
        cid = connection_request_view.GetSourceCid()

        self.scid_to_dcid[cid] = cid

        connection_response = l2cap_packets.ConnectionResponseBuilder(
            sid, cid, cid, l2cap_packets.ConnectionResponseResult.SUCCESS,
            l2cap_packets.ConnectionResponseStatus.
            NO_FURTHER_INFORMATION_AVAILABLE)
        connection_response_l2cap = l2cap_packets.BasicFrameBuilder(
            1, connection_response)
        self.cert_device.hci_acl_manager.SendAclData(
            acl_manager_facade.AclData(
                handle=self.cert_acl_handle,
                payload=bytes(connection_response_l2cap.Serialize())))
        return True

    def _on_connection_response_default(self, l2cap_control_view):
        connection_response_view = l2cap_packets.ConnectionResponseView(
            l2cap_control_view)
        sid = connection_response_view.GetIdentifier()
        scid = connection_response_view.GetSourceCid()
        dcid = connection_response_view.GetDestinationCid()
        self.scid_to_dcid[scid] = dcid

        config_request = l2cap_packets.ConfigurationRequestBuilder(
            sid + 1, dcid, l2cap_packets.Continuation.END, [])
        config_request_l2cap = l2cap_packets.BasicFrameBuilder(
            1, config_request)
        self.cert_device.hci_acl_manager.SendAclData(
            acl_manager_facade.AclData(
                handle=self.cert_acl_handle,
                payload=bytes(config_request_l2cap.Serialize())))
        return True

    def _on_connection_response_use_ertm(self, l2cap_control_view):
        connection_response_view = l2cap_packets.ConnectionResponseView(
            l2cap_control_view)
        sid = connection_response_view.GetIdentifier()
        scid = connection_response_view.GetSourceCid()
        dcid = connection_response_view.GetDestinationCid()
        self.scid_to_dcid[scid] = dcid

        # FIXME: This doesn't work!
        ertm_option = l2cap_packets.RetransmissionAndFlowControlConfigurationOption(
        )
        ertm_option.mode = l2cap_packets.RetransmissionAndFlowControlModeOption.L2CAP_BASIC
        ertm_option.tx_window_size = self.ertm_tx_window_size
        ertm_option.max_transmit = self.ertm_max_transmit
        ertm_option.retransmission_time_out = 2000
        ertm_option.monitor_time_out = 12000
        ertm_option.maximum_pdu_size = 1010

        options = [ertm_option]

        config_request = l2cap_packets.ConfigurationRequestBuilder(
            sid + 1, dcid, l2cap_packets.Continuation.END, options)

        config_request_l2cap = l2cap_packets.BasicFrameBuilder(
            1, config_request)

        config_packet = bytearray([
            0x1a,
            0x00,
            0x01,
            0x00,
            0x04,
            sid + 1,
            0x16,
            0x00,
            dcid & 0xff,
            dcid >> 8,
            0x00,
            0x00,
            0x01,
            0x02,
            0xa0,
            0x02,  # MTU
            0x04,
            0x09,
            0x03,
            self.ertm_tx_window_size,
            self.ertm_max_transmit,
            0xd0,
            0x07,
            0xe0,
            0x2e,
            0xf2,
            0x03,  # ERTM
            0x05,
            0x01,
            0x00  # FCS
        ])

        self.cert_device.hci_acl_manager.SendAclData(
            acl_manager_facade.AclData(
                handle=self.cert_acl_handle, payload=bytes(config_packet)))
        return True

    def _on_connection_response_use_ertm_and_fcs(self, l2cap_control_view):
        connection_response_view = l2cap_packets.ConnectionResponseView(
            l2cap_control_view)
        sid = connection_response_view.GetIdentifier()
        scid = connection_response_view.GetSourceCid()
        dcid = connection_response_view.GetDestinationCid()
        self.scid_to_dcid[scid] = dcid

        # FIXME: This doesn't work!
        ertm_option = l2cap_packets.RetransmissionAndFlowControlConfigurationOption(
        )
        ertm_option.mode = l2cap_packets.RetransmissionAndFlowControlModeOption.L2CAP_BASIC
        ertm_option.tx_window_size = self.ertm_tx_window_size
        ertm_option.max_transmit = self.ertm_max_transmit
        ertm_option.retransmission_time_out = 2000
        ertm_option.monitor_time_out = 12000
        ertm_option.maximum_pdu_size = 1010

        options = [ertm_option]

        config_request = l2cap_packets.ConfigurationRequestBuilder(
            sid + 1, dcid, l2cap_packets.Continuation.END, options)

        config_request_l2cap = l2cap_packets.BasicFrameBuilder(
            1, config_request)

        config_packet = bytearray([
            0x1a,
            0x00,
            0x01,
            0x00,
            0x04,
            sid + 1,
            0x16,
            0x00,
            dcid & 0xff,
            dcid >> 8,
            0x00,
            0x00,
            0x01,
            0x02,
            0xa0,
            0x02,  # MTU
            0x04,
            0x09,
            0x03,
            self.ertm_tx_window_size,
            self.ertm_max_transmit,
            0xd0,
            0x07,
            0xe0,
            0x2e,
            0xf2,
            0x03,  # ERTM
            0x05,
            0x01,
            0x01  # FCS
        ])

        self.cert_device.hci_acl_manager.SendAclData(
            acl_manager_facade.AclData(
                handle=self.cert_acl_handle, payload=bytes(config_packet)))
        return True

    def _on_configuration_request_default(self, l2cap_control_view):
        configuration_request = l2cap_packets.ConfigurationRequestView(
            l2cap_control_view)
        sid = configuration_request.GetIdentifier()
        dcid = configuration_request.GetDestinationCid()
        config_response = l2cap_packets.ConfigurationResponseBuilder(
            sid, self.scid_to_dcid.get(dcid, 0), l2cap_packets.Continuation.END,
            l2cap_packets.ConfigurationResponseResult.SUCCESS, [])
        config_response_l2cap = l2cap_packets.BasicFrameBuilder(
            1, config_response)
        self.cert_send_b_frame(config_response_l2cap)

    def _on_configuration_response_default(self, l2cap_control_view):
        configuration_response = l2cap_packets.ConfigurationResponseView(
            l2cap_control_view)
        sid = configuration_response.GetIdentifier()

    def _on_disconnection_request_default(self, l2cap_control_view):
        disconnection_request = l2cap_packets.DisconnectionRequestView(
            l2cap_control_view)
        sid = disconnection_request.GetIdentifier()
        scid = disconnection_request.GetSourceCid()
        dcid = disconnection_request.GetDestinationCid()
        disconnection_response = l2cap_packets.DisconnectionResponseBuilder(
            sid, dcid, scid)
        disconnection_response_l2cap = l2cap_packets.BasicFrameBuilder(
            1, disconnection_response)
        self.cert_device.hci_acl_manager.SendAclData(
            acl_manager_facade.AclData(
                handle=self.cert_acl_handle,
                payload=bytes(disconnection_response_l2cap.Serialize())))

    def _on_disconnection_response_default(self, l2cap_control_view):
        disconnection_response = l2cap_packets.DisconnectionResponseView(
            l2cap_control_view)

    def _on_information_request_default(self, l2cap_control_view):
        information_request = l2cap_packets.InformationRequestView(
            l2cap_control_view)
        sid = information_request.GetIdentifier()
        information_type = information_request.GetInfoType()
        if information_type == l2cap_packets.InformationRequestInfoType.CONNECTIONLESS_MTU:
            response = l2cap_packets.InformationResponseConnectionlessMtuBuilder(
                sid, l2cap_packets.InformationRequestResult.SUCCESS, 100)
            response_l2cap = l2cap_packets.BasicFrameBuilder(1, response)
            self.cert_device.hci_acl_manager.SendAclData(
                acl_manager_facade.AclData(
                    handle=self.cert_acl_handle,
                    payload=bytes(response_l2cap.Serialize())))
            return
        if information_type == l2cap_packets.InformationRequestInfoType.EXTENDED_FEATURES_SUPPORTED:
            response = l2cap_packets.InformationResponseExtendedFeaturesBuilder(
                sid, l2cap_packets.InformationRequestResult.SUCCESS, 0, 0, 0, 1,
                0, 1, 0, 0, 0, 0)
            response_l2cap = l2cap_packets.BasicFrameBuilder(1, response)
            self.cert_device.hci_acl_manager.SendAclData(
                acl_manager_facade.AclData(
                    handle=self.cert_acl_handle,
                    payload=bytes(response_l2cap.Serialize())))
            return
        if information_type == l2cap_packets.InformationRequestInfoType.FIXED_CHANNELS_SUPPORTED:
            response = l2cap_packets.InformationResponseFixedChannelsBuilder(
                sid, l2cap_packets.InformationRequestResult.SUCCESS, 2)
            response_l2cap = l2cap_packets.BasicFrameBuilder(1, response)
            self.cert_device.hci_acl_manager.SendAclData(
                acl_manager_facade.AclData(
                    handle=self.cert_acl_handle,
                    payload=bytes(response_l2cap.Serialize())))
            return

    def _on_information_response_default(self, l2cap_control_view):
        information_response = l2cap_packets.InformationResponseView(
            l2cap_control_view)

    def teardown_test(self):
        self.device_under_test.rootservice.StopStack(
            facade_rootservice.StopStackRequest())
        self.cert_device.rootservice.StopStack(
            facade_rootservice.StopStackRequest())

    def _handle_control_packet(self, l2cap_packet):
        packet_bytes = l2cap_packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != 1:
            return
        l2cap_control_view = l2cap_packets.ControlView(l2cap_view.GetPayload())
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.CONNECTION_REQUEST:
            return self.on_connection_request(
                l2cap_control_view
            ) if self.on_connection_request else self._on_connection_request_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.CONNECTION_RESPONSE:
            return self.on_connection_response(
                l2cap_control_view
            ) if self.on_connection_response else self._on_connection_response_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.CONFIGURATION_REQUEST:
            return self.on_configuration_request(
                l2cap_control_view
            ) if self.on_configuration_request else self._on_configuration_request_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.CONFIGURATION_RESPONSE:
            return self.on_configuration_response(
                l2cap_control_view
            ) if self.on_configuration_response else self._on_configuration_response_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.DISCONNECTION_REQUEST:
            return self.on_disconnection_request(
                l2cap_control_view
            ) if self.on_disconnection_request else self._on_disconnection_request_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.DISCONNECTION_RESPONSE:
            return self.on_disconnection_response(
                l2cap_control_view
            ) if self.on_disconnection_response else self._on_disconnection_response_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.INFORMATION_REQUEST:
            return self.on_information_request(
                l2cap_control_view
            ) if self.on_information_request else self._on_information_request_default(
                l2cap_control_view)
        if l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.INFORMATION_RESPONSE:
            return self.on_information_response(
                l2cap_control_view
            ) if self.on_information_response else self._on_information_response_default(
                l2cap_control_view)
        return

    def is_correct_connection_request(self, l2cap_packet):
        packet_bytes = l2cap_packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != 1:
            return False
        l2cap_control_view = l2cap_packets.ControlView(l2cap_view.GetPayload())
        return l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.CONNECTION_REQUEST

    def is_correct_configuration_response(self, l2cap_packet):
        packet_bytes = l2cap_packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != 1:
            return False
        l2cap_control_view = l2cap_packets.ControlView(l2cap_view.GetPayload())
        if l2cap_control_view.GetCode(
        ) != l2cap_packets.CommandCode.CONFIGURATION_RESPONSE:
            return False
        configuration_response_view = l2cap_packets.ConfigurationResponseView(
            l2cap_control_view)
        return configuration_response_view.GetResult(
        ) == l2cap_packets.ConfigurationResponseResult.SUCCESS

    def is_correct_configuration_request(self, l2cap_packet):
        packet_bytes = l2cap_packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != 1:
            return False
        l2cap_control_view = l2cap_packets.ControlView(l2cap_view.GetPayload())
        return l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.CONFIGURATION_REQUEST

    def is_correct_disconnection_request(self, l2cap_packet):
        packet_bytes = l2cap_packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != 1:
            return False
        l2cap_control_view = l2cap_packets.ControlView(l2cap_view.GetPayload())
        return l2cap_control_view.GetCode(
        ) == l2cap_packets.CommandCode.DISCONNECTION_REQUEST

    def cert_send_b_frame(self, b_frame):
        self.cert_device.hci_acl_manager.SendAclData(
            acl_manager_facade.AclData(
                handle=self.cert_acl_handle,
                payload=bytes(b_frame.Serialize())))

    def get_req_seq_from_ertm_s_frame(self, scid, packet):
        packet_bytes = packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != scid:
            return False
        standard_view = l2cap_packets.StandardFrameView(l2cap_view)
        if standard_view.GetFrameType() == l2cap_packets.FrameType.I_FRAME:
            return False
        s_frame = l2cap_packets.EnhancedSupervisoryFrameView(standard_view)
        return s_frame.GetReqSeq()

    def get_s_from_ertm_s_frame(self, scid, packet):
        packet_bytes = packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != scid:
            return False
        standard_view = l2cap_packets.StandardFrameView(l2cap_view)
        if standard_view.GetFrameType() == l2cap_packets.FrameType.I_FRAME:
            return False
        s_frame = l2cap_packets.EnhancedSupervisoryFrameView(standard_view)
        return s_frame.GetS()

    def get_p_from_ertm_s_frame(self, scid, packet):
        packet_bytes = packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != scid:
            return False
        standard_view = l2cap_packets.StandardFrameView(l2cap_view)
        if standard_view.GetFrameType() == l2cap_packets.FrameType.I_FRAME:
            return False
        s_frame = l2cap_packets.EnhancedSupervisoryFrameView(standard_view)
        return s_frame.GetP()

    def get_f_from_ertm_s_frame(self, scid, packet):
        packet_bytes = packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != scid:
            return False
        standard_view = l2cap_packets.StandardFrameView(l2cap_view)
        if standard_view.GetFrameType() == l2cap_packets.FrameType.I_FRAME:
            return False
        s_frame = l2cap_packets.EnhancedSupervisoryFrameView(standard_view)
        return s_frame.GetF()

    def get_tx_seq_from_ertm_i_frame(self, scid, packet):
        packet_bytes = packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != scid:
            return False
        standard_view = l2cap_packets.StandardFrameView(l2cap_view)
        if standard_view.GetFrameType() == l2cap_packets.FrameType.S_FRAME:
            return False
        i_frame = l2cap_packets.EnhancedInformationFrameView(standard_view)
        return i_frame.GetTxSeq()

    def get_payload_from_ertm_i_frame(self, scid, packet):
        packet_bytes = packet.payload
        l2cap_view = l2cap_packets.BasicFrameView(
            bt_packets.PacketViewLittleEndian(list(packet_bytes)))
        if l2cap_view.GetChannelId() != scid:
            return False
        standard_view = l2cap_packets.StandardFrameView(l2cap_view)
        if standard_view.GetFrameType() == l2cap_packets.FrameType.S_FRAME:
            return False
        i_frame = l2cap_packets.EnhancedInformationFrameView(standard_view)
        return i_frame.GetPayload()  # TODO(mylesgw): This doesn't work!

    def _setup_link_from_cert(self):

        self.device_under_test.neighbor.EnablePageScan(
            neighbor_facade.EnableMsg(enabled=True))

        with EventCallbackStream(
                self.cert_device.hci_acl_manager.CreateConnection(
                    acl_manager_facade.ConnectionMsg(
                        address_type=int(
                            hci_packets.AddressType.PUBLIC_DEVICE_ADDRESS),
                        address=bytes(self.dut_address.address)))
        ) as connection_event_stream:

            connection_event_asserts = EventAsserts(connection_event_stream)

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

            connection_event_asserts.assert_event_occurs(get_handle)

        self.cert_acl_handle = handle
        return handle

    def _open_channel(
            self,
            cert_acl_data_stream,
            signal_id=1,
            cert_acl_handle=0x1,
            scid=0x0101,
            psm=0x33,
            mode=l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC):
        cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)

        self.device_under_test.l2cap.SetDynamicChannel(
            l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                psm=psm, retransmission_mode=mode))
        open_channel = l2cap_packets.ConnectionRequestBuilder(
            signal_id, psm, scid)
        open_channel_l2cap = l2cap_packets.BasicFrameBuilder(1, open_channel)
        self.cert_send_b_frame(open_channel_l2cap)

        def verify_connection_response(packet):
            packet_bytes = packet.payload
            l2cap_view = l2cap_packets.BasicFrameView(
                bt_packets.PacketViewLittleEndian(list(packet_bytes)))
            l2cap_control_view = l2cap_packets.ControlView(
                l2cap_view.GetPayload())
            if l2cap_control_view.GetCode(
            ) != l2cap_packets.CommandCode.CONNECTION_RESPONSE:
                return False
            connection_response_view = l2cap_packets.ConnectionResponseView(
                l2cap_control_view)
            return connection_response_view.GetSourceCid(
            ) == scid and connection_response_view.GetResult(
            ) == l2cap_packets.ConnectionResponseResult.SUCCESS and connection_response_view.GetDestinationCid(
            ) != 0

        cert_acl_data_asserts.assert_event_occurs(verify_connection_response)

    def test_connect_dynamic_channel_and_send_data(self):
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            psm = 0x33
            scid = 0x41
            self._open_channel(cert_acl_data_stream, 1, cert_acl_handle, scid,
                               psm)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=0x33, payload=b'abc'))
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b'abc' in packet.payload)

    def test_fixed_channel(self):
        cert_acl_handle = self._setup_link_from_cert()
        self.device_under_test.l2cap.RegisterChannel(
            l2cap_facade_pb2.RegisterChannelRequest(channel=2))
        asserts.skip("FIXME: Not working")
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            self.device_under_test.l2cap.SendL2capPacket(
                l2cap_facade_pb2.L2capPacket(channel=2, payload=b"123"))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b'123' in packet.payload)

    def test_open_two_channels(self):
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self._open_channel(cert_acl_data_stream, 1, cert_acl_handle, 0x41,
                               0x41)
            self._open_channel(cert_acl_data_stream, 2, cert_acl_handle, 0x43,
                               0x43)

    def test_connect_and_send_data_ertm_no_segmentation(self):
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(
                    psm=psm, payload=b'abc' * 34))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b'abc' * 34 in packet.payload)

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 0, l2cap_packets.Final.NOT_SET, 1,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

    def test_basic_operation_request_connection(self):
        """
        L2CAP/COS/CED/BV-01-C [Request Connection]
        Verify that the IUT is able to request the connection establishment for an L2CAP data channel and
        initiate the configuration procedure.
        """
        cert_acl_handle = self._setup_link_from_cert()

        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            psm = 0x33
            # TODO: Use another test case
            self.device_under_test.l2cap.OpenChannel(
                l2cap_facade_pb2.OpenChannelRequest(
                    remote=self.cert_address, psm=psm))
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_connection_request)

    def test_accept_disconnect(self):
        """
        L2CAP/COS/CED/BV-07-C
        """
        cert_acl_handle = self._setup_link_from_cert()

        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            scid = 0x41
            psm = 0x33
            self._open_channel(cert_acl_data_stream, 1, cert_acl_handle, scid,
                               psm)

            dcid = self.scid_to_dcid[scid]

            close_channel = l2cap_packets.DisconnectionRequestBuilder(
                1, dcid, scid)
            close_channel_l2cap = l2cap_packets.BasicFrameBuilder(
                1, close_channel)
            self.cert_send_b_frame(close_channel_l2cap)

            def verify_disconnection_response(packet):
                packet_bytes = packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                if l2cap_control_view.GetCode(
                ) != l2cap_packets.CommandCode.DISCONNECTION_RESPONSE:
                    return False
                disconnection_response_view = l2cap_packets.DisconnectionResponseView(
                    l2cap_control_view)
                return disconnection_response_view.GetSourceCid(
                ) == scid and disconnection_response_view.GetDestinationCid(
                ) == dcid

            cert_acl_data_asserts.assert_event_occurs(
                verify_disconnection_response)

    def test_disconnect_on_timeout(self):
        """
        L2CAP/COS/CED/BV-08-C
        """
        cert_acl_handle = self._setup_link_from_cert()

        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            scid = 0x41
            psm = 0x33
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            # Don't send configuration request or response back
            self.on_configuration_request = lambda _: True
            self.on_connection_response = lambda _: True

            self._open_channel(cert_acl_data_stream, 1, cert_acl_handle, scid,
                               psm)

            def is_configuration_response(l2cap_packet):
                packet_bytes = l2cap_packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                if l2cap_view.GetChannelId() != 1:
                    return False
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                return l2cap_control_view.GetCode(
                ) == l2cap_packets.CommandCode.CONFIGURATION_RESPONSE

            cert_acl_data_asserts.assert_none_matching(
                is_configuration_response)

    def test_respond_to_echo_request(self):
        """
        L2CAP/COS/ECH/BV-01-C [Respond to Echo Request]
        Verify that the IUT responds to an echo request.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            echo_request = l2cap_packets.EchoRequestBuilder(
                100, l2cap_packets.DisconnectionRequestBuilder(1, 2, 3))
            echo_request_l2cap = l2cap_packets.BasicFrameBuilder(
                1, echo_request)
            self.cert_send_b_frame(echo_request_l2cap)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"\x06\x01\x04\x00\x02\x00\x03\x00" in packet.payload
            )

    def test_reject_unknown_command(self):
        """
        L2CAP/COS/CED/BI-01-C
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            invalid_command_packet = b"\x04\x00\x01\x00\xff\x01\x00\x00"
            self.cert_device.hci_acl_manager.SendAclData(
                acl_manager_facade.AclData(
                    handle=cert_acl_handle,
                    payload=bytes(invalid_command_packet)))

            def is_command_reject(l2cap_packet):
                packet_bytes = l2cap_packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                if l2cap_view.GetChannelId() != 1:
                    return False
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                return l2cap_control_view.GetCode(
                ) == l2cap_packets.CommandCode.COMMAND_REJECT

            cert_acl_data_asserts.assert_event_occurs(is_command_reject)

    def test_query_for_1_2_features(self):
        """
        L2CAP/COS/IEX/BV-01-C [Query for 1.2 Features]
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_asserts_alt = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            signal_id = 3
            information_request = l2cap_packets.InformationRequestBuilder(
                signal_id, l2cap_packets.InformationRequestInfoType.
                EXTENDED_FEATURES_SUPPORTED)
            information_request_l2cap = l2cap_packets.BasicFrameBuilder(
                1, information_request)
            self.cert_send_b_frame(information_request_l2cap)

            def is_correct_information_response(l2cap_packet):
                packet_bytes = l2cap_packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                if l2cap_view.GetChannelId() != 1:
                    return False
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                if l2cap_control_view.GetCode(
                ) != l2cap_packets.CommandCode.INFORMATION_RESPONSE:
                    return False
                information_response_view = l2cap_packets.InformationResponseView(
                    l2cap_control_view)
                return information_response_view.GetInfoType(
                ) == l2cap_packets.InformationRequestInfoType.EXTENDED_FEATURES_SUPPORTED

            cert_acl_data_asserts.assert_event_occurs(
                is_correct_information_response)

            def is_correct_information_request(l2cap_packet):
                packet_bytes = l2cap_packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                if l2cap_view.GetChannelId() != 1:
                    return False
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                if l2cap_control_view.GetCode(
                ) != l2cap_packets.CommandCode.INFORMATION_REQUEST:
                    return False
                information_request_view = l2cap_packets.InformationRequestView(
                    l2cap_control_view)
                return information_request_view.GetInfoType(
                ) == l2cap_packets.InformationRequestInfoType.EXTENDED_FEATURES_SUPPORTED

            cert_acl_data_asserts_alt.assert_event_occurs(
                is_correct_information_request)

    def test_extended_feature_info_response_ertm(self):
        """
        L2CAP/EXF/BV-01-C [Extended Features Information Response for Enhanced
        Retransmission Mode]
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            signal_id = 3
            information_request = l2cap_packets.InformationRequestBuilder(
                signal_id, l2cap_packets.InformationRequestInfoType.
                EXTENDED_FEATURES_SUPPORTED)
            information_request_l2cap = l2cap_packets.BasicFrameBuilder(
                1, information_request)
            self.cert_send_b_frame(information_request_l2cap)

            def is_correct_information_response(l2cap_packet):
                packet_bytes = l2cap_packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                if l2cap_view.GetChannelId() != 1:
                    return False
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                if l2cap_control_view.GetCode(
                ) != l2cap_packets.CommandCode.INFORMATION_RESPONSE:
                    return False
                information_response_view = l2cap_packets.InformationResponseView(
                    l2cap_control_view)
                if information_response_view.GetInfoType(
                ) != l2cap_packets.InformationRequestInfoType.EXTENDED_FEATURES_SUPPORTED:
                    return False
                extended_features_view = l2cap_packets.InformationResponseExtendedFeaturesView(
                    information_response_view)
                return extended_features_view.GetEnhancedRetransmissionMode()

            cert_acl_data_asserts.assert_event_occurs(
                is_correct_information_response)

    def test_extended_feature_info_response_fcs(self):
        """
        L2CAP/EXF/BV-03-C [Extended Features Information Response for FCS Option]
        Note: This is not mandated by L2CAP Spec
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            signal_id = 3
            information_request = l2cap_packets.InformationRequestBuilder(
                signal_id, l2cap_packets.InformationRequestInfoType.
                EXTENDED_FEATURES_SUPPORTED)
            information_request_l2cap = l2cap_packets.BasicFrameBuilder(
                1, information_request)
            self.cert_send_b_frame(information_request_l2cap)

            def is_correct_information_response(l2cap_packet):
                packet_bytes = l2cap_packet.payload
                l2cap_view = l2cap_packets.BasicFrameView(
                    bt_packets.PacketViewLittleEndian(list(packet_bytes)))
                if l2cap_view.GetChannelId() != 1:
                    return False
                l2cap_control_view = l2cap_packets.ControlView(
                    l2cap_view.GetPayload())
                if l2cap_control_view.GetCode(
                ) != l2cap_packets.CommandCode.INFORMATION_RESPONSE:
                    return False
                information_response_view = l2cap_packets.InformationResponseView(
                    l2cap_control_view)
                if information_response_view.GetInfoType(
                ) != l2cap_packets.InformationRequestInfoType.EXTENDED_FEATURES_SUPPORTED:
                    return False
                extended_features_view = l2cap_packets.InformationResponseExtendedFeaturesView(
                    information_response_view)
                return extended_features_view.GetFcsOption()

            cert_acl_data_asserts.assert_event_occurs(
                is_correct_information_response)

    def test_config_channel_not_use_FCS(self):
        """
        L2CAP/FOC/BV-01-C [IUT Initiated Configuration of the FCS Option]
        Verify the IUT can configure a channel to not use FCS in I/S-frames.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)
            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"abc" in packet.payload)

    def test_explicitly_request_use_FCS(self):
        """
        L2CAP/FOC/BV-02-C [Lower Tester Explicitly Requests FCS should be Used]
        Verify the IUT will include the FCS in I/S-frames if the Lower Tester explicitly requests that FCS
        should be used.
        """

        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            self.on_connection_response = self._on_connection_response_use_ertm_and_fcs
            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)
            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"abc\x4f\xa3" in packet.payload
            )  # TODO: Use packet parser

    def test_implicitly_request_use_FCS(self):
        """
        L2CAP/FOC/BV-03-C [Lower Tester Implicitly Requests FCS should be Used]
        TODO: Update this test case. What's the difference between this one and test_explicitly_request_use_FCS?
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            self.on_connection_response = self._on_connection_response_use_ertm_and_fcs
            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)
            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"abc\x4f\xa3" in packet.payload
            )  # TODO: Use packet parser

    def test_transmit_i_frames(self):
        """
        L2CAP/ERM/BV-01-C [Transmit I-frames]
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)

            self.on_connection_response = self._on_connection_response_use_ertm
            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            dcid = self.scid_to_dcid[scid]

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"abc" in packet.payload)

            # Assemble a sample packet. TODO: Use RawBuilder
            SAMPLE_PACKET = l2cap_packets.CommandRejectNotUnderstoodBuilder(1)

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 0, l2cap_packets.Final.NOT_SET, 1,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"abc" in packet.payload)

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 1, l2cap_packets.Final.NOT_SET, 2,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: b"abc" in packet.payload)

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 2, l2cap_packets.Final.NOT_SET, 3,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

    def test_receive_i_frames(self):
        """
        L2CAP/ERM/BV-02-C [Receive I-Frames]
        Verify the IUT can receive in-sequence valid I-frames and deliver L2CAP SDUs to the Upper Tester
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            dcid = self.scid_to_dcid[scid]

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            for i in range(3):
                i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                    dcid, i, l2cap_packets.Final.NOT_SET, 0,
                    l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                    SAMPLE_PACKET)
                self.cert_send_b_frame(i_frame)
                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == i + 1
                )

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 3, l2cap_packets.Final.NOT_SET, 0,
                l2cap_packets.SegmentationAndReassembly.START, SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == 4
            )

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 4, l2cap_packets.Final.NOT_SET, 0,
                l2cap_packets.SegmentationAndReassembly.CONTINUATION,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == 5
            )

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 5, l2cap_packets.Final.NOT_SET, 0,
                l2cap_packets.SegmentationAndReassembly.END, SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == 6
            )

    def test_acknowledging_received_i_frames(self):
        """
        L2CAP/ERM/BV-03-C [Acknowledging Received I-Frames]
        Verify the IUT sends S-frame [RR] with the Poll bit not set to acknowledge data received from the
        Lower Tester
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            dcid = self.scid_to_dcid[scid]

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            for i in range(3):
                i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                    dcid, i, l2cap_packets.Final.NOT_SET, 0,
                    l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                    SAMPLE_PACKET)
                self.cert_send_b_frame(i_frame)
                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == i + 1
                )

            cert_acl_data_asserts.assert_none_matching(
                lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == 4,
                timedelta(seconds=1))

    def test_resume_transmitting_when_received_rr(self):
        """
        L2CAP/ERM/BV-05-C [Resume Transmitting I-Frames when an S-Frame [RR] is Received]
        Verify the IUT will cease transmission of I-frames when the negotiated TxWindow is full. Verify the
        IUT will resume transmission of I-frames when an S-frame [RR] is received that acknowledges
        previously sent I-frames.
        """
        self.ertm_tx_window_size = 1
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            dcid = self.scid_to_dcid[scid]

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'def'))

            # TODO: Besides checking TxSeq, we also want to check payload, once we can get it from packet view
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )
            cert_acl_data_asserts.assert_none_matching(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1,
            )
            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.RECEIVER_READY,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.POLL_RESPONSE,
                1)
            self.cert_send_b_frame(s_frame)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1
            )

    def test_resume_transmitting_when_acknowledge_previously_sent(self):
        """
        L2CAP/ERM/BV-06-C [Resume Transmitting I-Frames when an I-Frame is Received]
        Verify the IUT will cease transmission of I-frames when the negotiated TxWindow is full. Verify the
        IUT will resume transmission of I-frames when an I-frame is received that acknowledges previously
        sent I-frames.
        """
        self.ertm_tx_window_size = 1
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            dcid = self.scid_to_dcid[scid]

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'def'))

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )
            # TODO: If 1 second is greater than their retransmit timeout, use a smaller timeout
            cert_acl_data_asserts.assert_none_matching(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1,
                timedelta(seconds=1))

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 0, l2cap_packets.Final.NOT_SET, 1,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1
            )

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 1, l2cap_packets.Final.NOT_SET, 2,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

    def test_transmit_s_frame_rr_with_poll_bit_set(self):
        """
        L2CAP/ERM/BV-08-C [Send S-Frame [RR] with Poll Bit Set]
        Verify the IUT sends an S-frame [RR] with the Poll bit set when its retransmission timer expires.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            # TODO: Always use their retransmission timeout value
            time.sleep(2)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL
            )

    def test_transmit_s_frame_rr_with_final_bit_set(self):
        """
        L2CAP/ERM/BV-09-C [Send S-Frame [RR] with Final Bit Set]
        Verify the IUT responds with an S-frame [RR] with the Final bit set after receiving an S-frame [RR]
        with the Poll bit set.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.RECEIVER_READY,
                l2cap_packets.Poll.POLL, l2cap_packets.Final.NOT_SET, 0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_f_from_ertm_s_frame(scid, packet) == l2cap_packets.Final.POLL_RESPONSE
            )

    def test_s_frame_transmissions_exceed_max_transmit(self):
        """
        L2CAP/ERM/BV-11-C [S-Frame Transmissions Exceed MaxTransmit]
        Verify the IUT will close the channel when the Monitor Timer expires.
        """
        asserts.skip("Need to configure DUT to have a shorter timer")
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))

            # Retransmission timer = 2, 20 * monitor timer = 360, so total timeout is 362
            time.sleep(362)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_disconnection_request)

    def test_i_frame_transmissions_exceed_max_transmit(self):
        """
        L2CAP/ERM/BV-12-C [I-Frame Transmissions Exceed MaxTransmit]
        Verify the IUT will close the channel when it receives an S-frame [RR] with the final bit set that does
        not acknowledge the previous I-frame sent by the IUT.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )

            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.RECEIVER_READY,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.POLL_RESPONSE,
                0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_disconnection_request)

    def test_respond_to_rej(self):
        """
        L2CAP/ERM/BV-13-C [Respond to S-Frame [REJ]]
        Verify the IUT retransmits I-frames starting from the sequence number specified in the S-frame [REJ].
        """
        self.ertm_tx_window_size = 2
        self.ertm_max_transmit = 2
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            for i in range(2):
                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == i,
                    timeout=timedelta(seconds=0.5))

            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.REJECT,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.NOT_SET, 0)
            self.cert_send_b_frame(s_frame)

            for i in range(2):
                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == i,
                    timeout=timedelta(seconds=0.5))

    def test_receive_s_frame_rr_final_bit_set(self):
        """
        L2CAP/ERM/BV-18-C [Receive S-Frame [RR] Final Bit = 1]
        Verify the IUT will retransmit any previously sent I-frames unacknowledged by receipt of an S-Frame
        [RR] with the Final Bit set.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))

            # TODO: Always use their retransmission timeout value
            time.sleep(2)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL
            )

            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.RECEIVER_READY,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.POLL_RESPONSE,
                0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )

    def test_receive_i_frame_final_bit_set(self):
        """
        L2CAP/ERM/BV-19-C [Receive I-Frame Final Bit = 1]
        Verify the IUT will retransmit any previously sent I-frames unacknowledged by receipt of an I-frame
        with the final bit set.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))

            # TODO: Always use their retransmission timeout value
            time.sleep(2)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL
            )

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 0, l2cap_packets.Final.POLL_RESPONSE, 0,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )

    def test_recieve_rnr(self):
        """
        L2CAP/ERM/BV-20-C [Enter Remote Busy Condition]
        Verify the IUT will not retransmit any I-frames when it receives a remote busy indication from the
        Lower Tester (S-frame [RNR]).
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=0x33, payload=b'abc'))

            # TODO: Always use their retransmission timeout value
            time.sleep(2)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL
            )

            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.RECEIVER_NOT_READY,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.POLL_RESPONSE,
                0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_none_matching(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )

    def test_sent_rej_lost(self):
        """
        L2CAP/ERM/BI-01-C [S-Frame [REJ] Lost or Corrupted]
        Verify the IUT can handle receipt of an S-=frame [RR] Poll = 1 if the S-frame [REJ] sent from the IUT
        is lost.
        """
        self.ertm_tx_window_size = 5
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 0, l2cap_packets.Final.NOT_SET, 0,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == 1
            )

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, self.ertm_tx_window_size - 1, l2cap_packets.Final.NOT_SET,
                0, l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_s_from_ertm_s_frame(scid, packet) == l2cap_packets.SupervisoryFunction.REJECT
            )

            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.RECEIVER_READY,
                l2cap_packets.Poll.POLL, l2cap_packets.Final.NOT_SET, 0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == 1 and self.get_f_from_ertm_s_frame(scid, packet) == l2cap_packets.Final.POLL_RESPONSE)
            for i in range(1, self.ertm_tx_window_size):
                i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                    dcid, i, l2cap_packets.Final.NOT_SET, 0,
                    l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                    SAMPLE_PACKET)
                self.cert_send_b_frame(i_frame)
                cert_acl_data_asserts.assert_event_occurs(
                    lambda packet: self.get_req_seq_from_ertm_s_frame(scid, packet) == i + 1
                )

    def test_handle_duplicate_srej(self):
        """
        L2CAP/ERM/BI-03-C [Handle Duplicate S-Frame [SREJ]]
        Verify the IUT will only retransmit the requested I-frame once after receiving a duplicate SREJ.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0,
                timeout=timedelta(0.5))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1,
                timeout=timedelta(0.5))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL
            )

            # Send SREJ with F not set
            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.SELECT_REJECT,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.NOT_SET, 0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_none(timeout=timedelta(seconds=0.5))
            # Send SREJ with F set
            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.SELECT_REJECT,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.POLL_RESPONSE,
                0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )

    def test_handle_receipt_rej_and_rr_with_f_set(self):
        """
        L2CAP/ERM/BI-04-C [Handle Receipt of S-Frame [REJ] and S-Frame [RR, F=1] that Both Require Retransmission of the Same I-Frames]
        Verify the IUT will only retransmit the requested I-frames once after receiving an S-frame [REJ]
        followed by an S-frame [RR] with the Final bit set that indicates the same I-frames should be
        retransmitted.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0,
                timeout=timedelta(0.5))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1,
                timeout=timedelta(0.5))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL,
                timeout=timedelta(2))

            # Send REJ with F not set
            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.REJECT,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.NOT_SET, 0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_none(timeout=timedelta(seconds=0.5))

            # Send RR with F set
            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.REJECT,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.POLL_RESPONSE,
                0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1
            )

    def test_handle_rej_and_i_frame_with_f_set(self):
        """
        L2CAP/ERM/BI-05-C [Handle receipt of S-Frame [REJ] and I-Frame [F=1] that Both Require Retransmission of the Same I-Frames]
        Verify the IUT will only retransmit the requested I-frames once after receiving an S-frame [REJ]
        followed by an I-frame with the Final bit set that indicates the same I-frames should be retransmitted.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # FIXME: Order shouldn't matter here
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)

            dcid = self.scid_to_dcid[scid]

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0,
                timeout=timedelta(0.5))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1,
                timeout=timedelta(0.5))
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_p_from_ertm_s_frame(scid, packet) == l2cap_packets.Poll.POLL,
                timeout=timedelta(2))

            # Send SREJ with F not set
            s_frame = l2cap_packets.EnhancedSupervisoryFrameBuilder(
                dcid, l2cap_packets.SupervisoryFunction.SELECT_REJECT,
                l2cap_packets.Poll.NOT_SET, l2cap_packets.Final.NOT_SET, 0)
            self.cert_send_b_frame(s_frame)

            cert_acl_data_asserts.assert_none(timeout=timedelta(seconds=0.5))

            i_frame = l2cap_packets.EnhancedInformationFrameBuilder(
                dcid, 0, l2cap_packets.Final.POLL_RESPONSE, 0,
                l2cap_packets.SegmentationAndReassembly.UNSEGMENTED,
                SAMPLE_PACKET)
            self.cert_send_b_frame(i_frame)

            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 0
            )
            cert_acl_data_asserts.assert_event_occurs(
                lambda packet: self.get_tx_seq_from_ertm_i_frame(scid, packet) == 1
            )

    def test_initiated_configurtion_request_ertm(self):
        """
        L2CAP/CMC/BV-01-C [IUT Initiated Configuration of Enhanced Retransmission Mode]
        Verify the IUT can send a Configuration Request command containing the F&EC option that specifies
        Enhanced Retransmission Mode.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            self.on_connection_response = self._on_connection_response_use_ertm

            psm = 0x33
            scid = 0x41
            self._open_channel(
                cert_acl_data_stream,
                1,
                cert_acl_handle,
                scid,
                psm,
                mode=l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM)

            # TODO: Fix this test. It doesn't work so far with PDL struct

            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_request)
            asserts.skip("Struct not working")

    def test_respond_configuration_request_ertm(self):
        """
        L2CAP/CMC/BV-02-C [Lower Tester Initiated Configuration of Enhanced Retransmission Mode]
        Verify the IUT can accept a Configuration Request from the Lower Tester containing an F&EC option
        that specifies Enhanced Retransmission Mode.
        """
        cert_acl_handle = self._setup_link_from_cert()
        with EventCallbackStream(
                self.cert_device.hci_acl_manager.FetchAclData(
                    empty_proto.Empty())) as cert_acl_data_stream:
            cert_acl_data_asserts = EventAsserts(cert_acl_data_stream)
            cert_acl_data_stream.register_callback(self._handle_control_packet)
            psm = 1
            scid = 0x0101
            self.retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=self.retransmission_mode))

            open_channel = l2cap_packets.ConnectionRequestBuilder(1, psm, scid)
            open_channel_l2cap = l2cap_packets.BasicFrameBuilder(
                1, open_channel)
            self.cert_send_b_frame(open_channel_l2cap)

            # TODO: Verify that the type should be ERTM
            cert_acl_data_asserts.assert_event_occurs(
                self.is_correct_configuration_response)
