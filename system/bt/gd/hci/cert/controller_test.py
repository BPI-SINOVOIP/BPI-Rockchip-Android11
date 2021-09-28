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

import time

from acts import asserts
from cert.gd_base_test_facade_only import GdFacadeOnlyBaseTestClass
from google.protobuf import empty_pb2 as empty_proto
from facade import rootservice_pb2 as facade_rootservice
from hci.facade import controller_facade_pb2 as controller_facade


class ControllerTest(GdFacadeOnlyBaseTestClass):

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

    def test_get_addresses(self):
        cert_address_response = self.cert_device.hci_controller.GetMacAddress(
            empty_proto.Empty())
        dut_address_response = self.device_under_test.hci_controller.GetMacAddress(
            empty_proto.Empty())
        asserts.assert_true(
            cert_address_response.address != dut_address_response.address,
            msg="Expected cert and dut address to be different %s" %
            cert_address_response.address)
        time.sleep(1)  # This shouldn't be needed b/149120542

    def test_get_local_extended_features(self):
        request = controller_facade.PageNumberMsg()
        request.page_number = 1
        dut_feature_response1 = self.device_under_test.hci_controller.GetLocalExtendedFeatures(
            request)
        request0 = controller_facade.PageNumberMsg()
        request0.page_number = 0
        dut_feature_response0 = self.device_under_test.hci_controller.GetLocalExtendedFeatures(
            request0)
        asserts.assert_true(
            dut_feature_response1.page != dut_feature_response0.page,
            msg="Expected cert dut feature pages to be different %d" %
            dut_feature_response1.page)

    def test_write_local_name(self):
        self.device_under_test.hci_controller.WriteLocalName(
            controller_facade.NameMsg(name=b'ImTheDUT'))
        self.cert_device.hci_controller.WriteLocalName(
            controller_facade.NameMsg(name=b'ImTheCert'))
        cert_name_msg = self.cert_device.hci_controller.GetLocalName(
            empty_proto.Empty()).name
        dut_name_msg = self.device_under_test.hci_controller.GetLocalName(
            empty_proto.Empty()).name
        asserts.assert_true(
            dut_name_msg == b'ImTheDUT',
            msg="unexpected dut name %s" % dut_name_msg)
        asserts.assert_true(
            cert_name_msg == b'ImTheCert',
            msg="unexpected cert name %s" % cert_name_msg)
