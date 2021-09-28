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
from datetime import timedelta
import time

from cert.pts_base_test import PTSBaseTestClass
from cert.event_asserts import EventAsserts
from cert.event_callback_stream import EventCallbackStream
from facade import common_pb2
from facade import rootservice_pb2 as facade_rootservice_pb2
from l2cap.classic import facade_pb2 as l2cap_facade_pb2
from google.protobuf import empty_pb2
from neighbor.facade import facade_pb2 as neighbor_facade


class PTSL2capTest(PTSBaseTestClass):

    def setup_test(self):
        self.device_under_test = self.gd_devices[0]

        self.device_under_test.rootservice.StartStack(
            facade_rootservice_pb2.StartStackRequest(
                module_under_test=facade_rootservice_pb2.BluetoothModule.Value(
                    'L2CAP'),))

        self.device_under_test.wait_channel_ready()

        dut_address = self.device_under_test.controller_read_only_property.ReadLocalAddress(
            empty_pb2.Empty()).address
        pts_address = self.controller_configs.get('pts_address').lower()
        self.device_under_test.address = dut_address

        self.dut_address = common_pb2.BluetoothAddress(
            address=self.device_under_test.address)
        self.pts_address = common_pb2.BluetoothAddress(
            address=str.encode(pts_address))

        self.device_under_test.neighbor.EnablePageScan(
            neighbor_facade.EnableMsg(enabled=True))

    def teardown_test(self):
        self.device_under_test.rootservice.StopStack(
            facade_rootservice_pb2.StopStackRequest())

    def _dut_connection_stream(self):
        return EventCallbackStream(
            self.device_under_test.l2cap.FetchConnectionComplete(
                empty_pb2.Empty()))

    def _dut_connection_close_stream(self):
        return EventCallbackStream(
            self.device_under_test.l2cap.FetchConnectionClose(
                empty_pb2.Empty()))

    def _assert_connection_complete(self, due_connection_asserts, timeout=30):
        due_connection_asserts.assert_event_occurs(
            lambda device: device.remote.address == self.pts_address.address,
            timeout=timedelta(seconds=timeout))

    def _assert_connection_close(self, due_connection_close_asserts,
                                 timeout=30):
        due_connection_close_asserts.assert_event_occurs(
            lambda device: device.remote.address == self.pts_address.address,
            timeout=timedelta(seconds=timeout))

    def test_L2CAP_IEX_BV_01_C(self):
        """
        L2CAP/COS/IEX/BV-01-C [Query for 1.2 Features]
        Verify that the IUT transmits an information request command to solicit if the remote device supports
        Specification 1.2 features.
        """
        psm = 1
        self.device_under_test.l2cap.OpenChannel(
            l2cap_facade_pb2.OpenChannelRequest(
                remote=self.pts_address, psm=psm))
        time.sleep(5)

    def test_L2CAP_IEX_BV_02_C(self):
        """
        L2CAP/COS/IEX/BV-02-C [Respond with 1.2 Features]
        Verify that the IUT responds to an information request command soliciting for Specification 1.2
        features.
        """
        psm = 1
        retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
        self.device_under_test.l2cap.SetDynamicChannel(
            l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                psm=psm, retransmission_mode=retransmission_mode))
        time.sleep(20)

    def test_L2CAP_EXF_BV_01_C(self):
        """
        L2CAP/EXF/BV-01-C [Extended Features Information Response for Enhanced Retransmission Mode]
        Verify the IUT can format an Information Response for the information type of Extended Features that
        correctly identifies that Enhanced Retransmission Mode is locally supported.

        """
        psm = 1
        retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
        self.device_under_test.l2cap.SetDynamicChannel(
            l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                psm=psm, retransmission_mode=retransmission_mode))
        time.sleep(5)

    def test_L2CAP_EXF_BV_03_C(self):
        """
        L2CAP/EXF/BV-03-C [Extended Features Information Response for FCS Option]
        Verify the IUT can format an Information Response for the information type of Extended Features that
        correctly identifies that the FCS Option is locally supported.
        """
        psm = 1
        retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
        self.device_under_test.l2cap.SetDynamicChannel(
            l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                psm=psm, retransmission_mode=retransmission_mode))
        time.sleep(5)

    def test_L2CAP_CMC_BV_01_C(self):
        """
        L2CAP/CMC/BV-01-C [IUT Initiated Configuration of Enhanced Retransmission Mode]
        Verify the IUT can send a Configuration Request command containing the F&EC option that specifies
        Enhanced Retransmission Mode.
        """
        with self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_CMC_BV_02_C(self):
        """
        L2CAP/CMC/BV-02-C [Lower Tester Initiated Configuration of Enhanced Retransmission Mode]
        Verify the IUT can accept a Configuration Request from the Lower Tester containing an F&EC option
        that specifies Enhanced Retransmission Mode.
        """
        with self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_01_C(self):
        """
        L2CAP/ERM/BV-01-C [Transmit I-frames]
        Verify the IUT can send correctly formatted sequential I-frames with valid values for the enhanced
        control fields (SAR, F-bit, ReqSeq, TxSeq).
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_02_C(self):
        """
        L2CAP/ERM/BV-02-C [Receive I-Frames]
        Verify the IUT can receive in-sequence valid I-frames and deliver L2CAP SDUs to the Upper Tester
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_03_C(self):
        """
        L2CAP/ERM/BV-03-C [Acknowledging Received I-Frames]
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_05_C(self):
        """
        L2CAP/ERM/BV-05-C [Resume Transmitting I-Frames when an S-Frame [RR] is Received]
        Verify the IUT will cease transmission of I-frames when the negotiated TxWindow is full. Verify the
        IUT will resume transmission of I-frames when an S-frame [RR] is received that acknowledges
        previously sent I-frames.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_06_C(self):
        """
        L2CAP/ERM/BV-06-C [Resume Transmitting I-Frames when an I-Frame is Received]
        Verify the IUT will cease transmission of I-frames when the negotiated TxWindow is full. Verify the
        IUT will resume transmission of I-frames when an I-frame is received that acknowledges previously
        sent I-frames.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_08_C(self):
        """
        L2CAP/ERM/BV-08-C [Send S-Frame [RR] with Poll Bit Set]
        Verify the IUT sends an S-frame [RR] with the Poll bit set when its retransmission timer expires.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_09_C(self):
        """
        L2CAP/ERM/BV-09-C [Send S-frame [RR] with Final Bit Set]
        Verify the IUT responds with an S-frame [RR] with the Final bit set after receiving an S-frame [RR]
        with the Poll bit set.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_10_C(self):
        """
      L2CAP/ERM/BV-10-C [Retransmit S-Frame [RR] with Poll Bit Set]
      Verify the IUT will retransmit the S-frame [RR] with the Poll bit set when the Monitor Timer expires.
      """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_13_C(self):
        """
        L2CAP/ERM/BV-13-C [Respond to S-Frame [REJ]]
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_16_C(self):
        """
         L2CAP/ERM/BV-16-C [Send S-Frame [REJ]]
        Verify the IUT can send an S-frame [REJ] after receiving out of sequence I-frames.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(
                due_connection_close_asserts, timeout=60)

    def test_L2CAP_ERM_BV_18_C(self):
        """
        L2CAP/ERM/BV-18-C [Receive S-Frame [RR] Final Bit = 1]
        Verify the IUT will retransmit any previously sent I-frames unacknowledged by receipt of an S-Frame
        [RR] with the Final Bit set.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_19_C(self):
        """
        L2CAP/ERM/BV-19-C [Receive I-Frame Final Bit = 1]
        Verify the IUT will retransmit any previously sent I-frames unacknowledged by receipt of an I-frame
        with the final bit set.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BV_20_C(self):
        """
        L2CAP/ERM/BV-20-C [Enter Remote Busy Condition]
        Verify the IUT will not retransmit any I-frames when it receives a remote busy indication from the
        Lower Tester (S-frame [RNR]).
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_01_C(self):
        """
        L2CAP/COS/CED/BV-01-C [Request Connection]
        Verify that the IUT is able to request the connection establishment for an L2CAP data channel and
        initiate the configuration procedure.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            self.device_under_test.l2cap.OpenChannel(
                l2cap_facade_pb2.OpenChannelRequest(
                    remote=self.pts_address, psm=psm))
            self._assert_connection_complete(due_connection_asserts)

            self.device_under_test.l2cap.CloseChannel(
                l2cap_facade_pb2.CloseChannelRequest(psm=psm))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_03_C(self):
        """
        L2CAP/COS/CED/BV-03-C [Send Data]
        Verify that the IUT is able to send DATA.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)

            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(
                    psm=psm, payload=b'abc' * 34))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_04_C(self):
        """
        L2CAP/COS/CED/BV-04-C [Disconnect]
        Verify that the IUT is able to disconnect the data channel.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            time.sleep(2)
            self.device_under_test.l2cap.CloseChannel(
                l2cap_facade_pb2.CloseChannelRequest(psm=psm))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_05_C(self):
        """
        L2CAP/COS/CED/BV-05-C [Accept Connection]
        Verify that the IUT is able to disconnect the data channel.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_07_C(self):
        """
        L2CAP/COS/CED/BV-07-C [Accept Disconnect]
        Verify that the IUT is able to respond to the request to disconnect the data channel.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_08_C(self):
        """
        L2CAP/COS/CED/BV-08-C [Disconnect on Timeout]
        Verify that the IUT disconnects the data channel and shuts down this channel if no response occurs
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            time.sleep(120)

    def test_L2CAP_COS_CED_BV_09_C(self):
        """
        L2CAP/COS/CED/BV-09-C [Receive Multi-Command Packet]
        Verify that the IUT is able to receive more than one signaling command in one L2CAP packet.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BV_11_C(self):
        """
        L2CAP/COS/CED/BV-11-C [Configure MTU Size]
        Verify that the IUT is able to configure the supported MTU size
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CED_BI_01_C(self):
        """
        L2CAP/COS/CED/BI-01-C [Reject Unknown Command]
        Verify that the IUT rejects an unknown signaling command.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            time.sleep(5)

    def test_L2CAP_COS_CFD_BV_03_C(self):
        """
        L2CAP/COS/CFD/BV-03-C [Send Requested Options]
        Verify that the IUT can receive a configuration request with no options and send the requested
        options to the Lower Tester
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.BASIC
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_COS_CFD_BV_08_C(self):
        """
        L2CAP/COS/CFD/BV-08-C [Non-blocking Config Response]
        Verify that the IUT does not block transmitting L2CAP_ConfigRsp while waiting for L2CAP_ConfigRsp
        from the Lower Tester.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.OpenChannel(
                l2cap_facade_pb2.OpenChannelRequest(
                    remote=self.pts_address, psm=psm))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.CloseChannel(
                l2cap_facade_pb2.CloseChannelRequest(psm=psm))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BI_01_C(self):
        """
        L2CAP/ERM/BI-01-C [S-Frame [REJ] Lost or Corrupted]
        Verify the IUT can handle receipt of an S-=frame [RR] Poll = 1 if the S-frame [REJ] sent from the IUT
        is lost.
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1
            retransmission_mode = l2cap_facade_pb2.RetransmissionFlowControlMode.ERTM
            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm, retransmission_mode=retransmission_mode))
            self._assert_connection_complete(due_connection_asserts)
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BI_03_C(self):
        """
        L2CAP/ERM/BI-03-C [Handle Duplicate S-Frame [SREJ]]
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BI_04_C(self):
        """
        L2CAP/ERM/BI-04-C [Handle Receipt of S-Frame [REJ] and S-Frame [RR, F=1]
        that Both Require Retransmission of the Same I-Frames]
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)

    def test_L2CAP_ERM_BI_05_C(self):
        """
        L2CAP/ERM/BI-05-C [Handle receipt of S-Frame [REJ] and I-Frame [F=1] that
        Both Require Retransmission of the Same I-Frames]
        """
        with self._dut_connection_stream() as dut_connection_stream, \
            self._dut_connection_close_stream() as dut_connection_close_stream:
            due_connection_asserts = EventAsserts(dut_connection_stream)
            due_connection_close_asserts = EventAsserts(
                dut_connection_close_stream)
            psm = 1

            self.device_under_test.l2cap.SetDynamicChannel(
                l2cap_facade_pb2.SetEnableDynamicChannelRequest(
                    psm=psm,
                    retransmission_mode=l2cap_facade_pb2.
                    RetransmissionFlowControlMode.ERTM))
            self._assert_connection_complete(due_connection_asserts)
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self.device_under_test.l2cap.SendDynamicChannelPacket(
                l2cap_facade_pb2.DynamicChannelPacket(psm=psm, payload=b'abc'))
            self._assert_connection_close(due_connection_close_asserts)
