#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""
Basic SDP Tests.

This only requires a single bluetooth_device
and exercises adding/removing services, initialization, and
adding search records.
"""
from acts import signals
from acts.base_test import BaseTestClass
from acts.test_utils.abstract_devices.bluetooth_device import AndroidBluetoothDevice
from acts.test_utils.abstract_devices.bluetooth_device import FuchsiaBluetoothDevice
from acts.test_utils.abstract_devices.bluetooth_device import create_bluetooth_device
from acts.test_utils.bt.bt_constants import bt_attribute_values
from acts.test_utils.bt.bt_constants import sig_uuid_constants

TEST_SDP_RECORD = {
    'service_class_uuids': ["0001"],
    'protocol_descriptors': [
        {
            'protocol':
            int(sig_uuid_constants['AVDTP'], 16),
            'params': [
                {
                    'data': 0x0103  # to indicate 1.3
                },
                {
                    'data': 0x0105  # to indicate 1.5
                }
            ]
        },
        {
            'protocol': int(sig_uuid_constants['SDP'], 16),
            'params': [{
                'data': int(sig_uuid_constants['AVDTP'], 16),
            }]
        }
    ],
    'profile_descriptors': [{
        'profile_id':
        int(sig_uuid_constants['AdvancedAudioDistribution'], 16),
        'major_version':
        1,
        'minor_version':
        3,
    }],
    'additional_protocol_descriptors': [{
        'protocol':
        int(sig_uuid_constants['L2CAP'], 16),
        'params': [{
            'data': int(sig_uuid_constants['AVDTP'], 16),
        }]
    }],
    'information': [{
        'language': "en",
        'name': "A2DP",
        'description': "Advanced Audio Distribution Profile",
        'provider': "Fuchsia"
    }],
    'additional_attributes':
    None
}


class SdpSetupTest(BaseTestClass):
    def setup_class(self):
        super(SdpSetupTest, self).setup_class()
        if 'dut' in self.user_params:
            if self.user_params['dut'] == 'fuchsia_devices':
                self.dut = create_bluetooth_device(self.fuchsia_devices[0])
            elif self.user_params['dut'] == 'android_devices':
                self.dut = create_bluetooth_device(self.android_devices[0])
            else:
                raise ValueError('Invalid DUT specified in config. (%s)' %
                                 self.user_params['dut'])
        else:
            # Default is an fuchsia device
            self.dut = create_bluetooth_device(self.fuchsia_devices[0])
        self.dut.initialize_bluetooth_controller()


    def setup_test(self):
        self.dut.sdp_clean_up()

    def cleanup_class(self):
        self.dut.sdp_clean_up()

    def test_init(self):
        result = self.dut.sdp_init()
        if result.get("error") is None:
            raise signals.TestPass("Success")
        else:
            raise signals.TestFailure(
                "Failed to initialize SDP with {}".format(result.get("error")))

    def test_add_service(self):
        self.dut.sdp_init()
        result = self.dut.sdp_add_service(TEST_SDP_RECORD)
        if result.get("error") is not None:
            raise signals.TestFailure(
                "Failed to add SDP service record: {}".format(
                    result.get("error")))
        else:
            raise signals.TestPass("Success")

    def test_malformed_service(self):
        self.dut.sdp_init()
        malformed_record = {'malformed_sdp_record_input': ["1101"]}
        result = self.dut.sdp_add_service(malformed_record)
        if result.get("error") is not None:
            raise signals.TestPass("Successfully failed with: {}".format(
                result.get("error")))
        else:
            raise signals.TestFailure(
                "Expected failure of adding SDP record: {}".format(
                    malformed_record))

    def test_add_search(self):
        attributes = [
            bt_attribute_values['ATTR_PROTOCOL_DESCRIPTOR_LIST'],
            bt_attribute_values['ATTR_SERVICE_CLASS_ID_LIST'],
            bt_attribute_values['ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST'],
            bt_attribute_values['ATTR_A2DP_SUPPORTED_FEATURES'],
        ]

        self.dut.sdp_init()
        profile_id = int(sig_uuid_constants['AudioSource'], 16)
        result = self.dut.sdp_add_search(attributes, profile_id)
        if result.get("error") is not None:
            raise signals.TestFailure("Failed to add SDP search: {}".format(
                result.get("error")))
        else:
            raise signals.TestPass("Success")

    def test_include_additional_attributes(self):
        self.dut.sdp_init()
        additional_attributes = [{
            'id': 0x0201,
            'element': {
                'data': int(sig_uuid_constants['AVDTP'], 16)
            }
        }]

        TEST_SDP_RECORD['additional_attributes'] = additional_attributes
        result = self.dut.sdp_add_service(TEST_SDP_RECORD)
        if result.get("error") is not None:
            raise signals.TestFailure(
                "Failed to add SDP service record: {}".format(
                    result.get("error")))
        else:
            raise signals.TestPass("Success")

    def test_add_multiple_services(self):
        self.dut.sdp_init()
        number_of_records = 10
        for _ in range(number_of_records):
            result = self.dut.sdp_add_service(TEST_SDP_RECORD)
            if result.get("error") is not None:
                raise signals.TestFailure(
                    "Failed to add SDP service record: {}".format(
                        result.get("error")))
        raise signals.TestPass("Success")
