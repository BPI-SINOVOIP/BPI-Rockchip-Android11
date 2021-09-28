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
SDP PTS Tests.
"""
from acts.test_utils.abstract_devices.bluetooth_device import AndroidBluetoothDevice
from acts.test_utils.abstract_devices.bluetooth_device import FuchsiaBluetoothDevice
from acts.test_utils.bt.bt_constants import bt_attribute_values
from acts.test_utils.bt.bt_constants import sig_uuid_constants
from acts.test_utils.bt.pts.pts_base_class import PtsBaseClass

import acts.test_utils.bt.pts.fuchsia_pts_ics_lib as f_ics_lib
import acts.test_utils.bt.pts.fuchsia_pts_ixit_lib as f_ixit_lib

# SDP_RECORD Definition is WIP
SDP_RECORD = {
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
        'params': [
            {
                'data': int(sig_uuid_constants['AVDTP'], 16),
            },
            {
                'data': int(sig_uuid_constants['AVCTP'], 16),
            },
            {
                'data': int(sig_uuid_constants['GenericAudio'], 16),
            },
        ]
    }],
    'information': [{
        'language': "en",
        'name': "A2DP",
        'description': "Advanced Audio Distribution Profile",
        'provider': "Fuchsia"
    }],
    'additional_attributes': [{
        'id': 0x0201,
        'element': {
            'data': int(sig_uuid_constants['AVDTP'], 16)
        }
    }]
}

ATTRIBUTES = [
    bt_attribute_values['ATTR_PROTOCOL_DESCRIPTOR_LIST'],
    bt_attribute_values['ATTR_SERVICE_CLASS_ID_LIST'],
    bt_attribute_values['ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST'],
    bt_attribute_values['ATTR_SERVICE_AVAILABILITY'],
    bt_attribute_values['ATTR_A2DP_SUPPORTED_FEATURES'],
]

PROFILE_ID = int(sig_uuid_constants['AudioSource'], 16)


class SdpPtsTest(PtsBaseClass):

    def setup_class(self):
        super().setup_class()
        self.dut.initialize_bluetooth_controller()
        # self.dut.set_bluetooth_local_name(self.dut_bluetooth_local_name)
        local_dut_mac_address = self.dut.get_local_bluetooth_address()

        ics = None
        ixit = None
        if isinstance(self.dut, FuchsiaBluetoothDevice):
            fuchsia_ixit = f_ixit_lib.SDP_IXIT
            fuchsia_ixit[b'TSPX_bd_addr_iut'] = (b'OCTETSTRING',
                                                 local_dut_mac_address.replace(
                                                     ':', '').encode())
            ics = f_ics_lib.SDP_ICS
            ixit = fuchsia_ixit
        elif isinstance(self.dut, AndroidBluetoothDevice):
            # TODO: Add ICS and IXIT values for Android
            self.log.warn(
                "ICS/IXIT values not set for Android, using Fuchsia as default."
            )
            fuchsia_ixit = f_ixit_lib.SDP_IXIT
            fuchsia_ixit[b'TSPX_bd_addr_iut'] = (b'OCTETSTRING',
                                                 local_dut_mac_address.replace(
                                                     ':', '').encode())
            ics = f_ics_lib.SDP_ICS
            ixit = fuchsia_ixit

        ### PTS SETUP: Required after ICS, IXIT, and profile is setup ###
        self.pts.set_profile_under_test("SDP")
        self.pts.set_ics_and_ixit(ics, ixit)
        self.pts.setup_pts()
        ### End PTS Setup ###

        self.dut.unbond_all_known_devices()
        self.dut.set_discoverable(True)

    def setup_test(self):
        super(SdpPtsTest, self).setup_test()
        self.dut.sdp_init()
        self.dut.sdp_add_search(ATTRIBUTES, PROFILE_ID)
        self.dut.sdp_add_service(SDP_RECORD)

        # Make sure there were no lingering answers due to a failed test.
        self.pts.extra_answers = []

    def teardown_test(self):
        super(SdpPtsTest, self).teardown_test()
        self.dut.sdp_clean_up()

    def teardown_class(self):
        super(SdpPtsTest, self).teardown_class()
        self.dut.sdp_clean_up()
        self.dut.set_discoverable(False)

    # BEGIN SDP TESTCASES #

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_01_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_03_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_04_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_05_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_08_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_09_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-09-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_12_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-12-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_13_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-13-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_14_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-14-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_17_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-17-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_20_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-20-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bv_21_c(self):
        return self.pts.execute_test("SDP/SR/SA/BV-21-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bi_01_c(self):
        return self.pts.execute_test("SDP/SR/SA/BI-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bi_02_c(self):
        return self.pts.execute_test("SDP/SR/SA/BI-02-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_sa_bi_03_c(self):
        return self.pts.execute_test("SDP/SR/SA/BI-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ss_bv_01_c(self):
        return self.pts.execute_test("SDP/SR/SS/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ss_bv_03_c(self):
        # Triggers continuation response for supported devices.
        num_of_records = 9
        for _ in range(num_of_records):
            self.dut.sdp_add_service(SDP_RECORD)
        return self.pts.execute_test("SDP/SR/SS/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ss_bv_04_c(self):
        # Triggers continuation response for supported devices.
        num_of_records = 9
        for _ in range(num_of_records):
            self.dut.sdp_add_service(SDP_RECORD)
        return self.pts.execute_test("SDP/SR/SS/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ss_bi_01_c(self):
        return self.pts.execute_test("SDP/SR/SS/BI-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ss_bi_02_c(self):
        return self.pts.execute_test("SDP/SR/SS/BI-02-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_01_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_02_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-02-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_03_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_04_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_06_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_10_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-10-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_11_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-11-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_12_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-12-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_13_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-13-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_16_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-16-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_17_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-17-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_18_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-18-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_20_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-20-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bv_23_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BV-23-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bi_01_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BI-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_sdp_sr_ssa_bi_02_c(self):
        return self.pts.execute_test("SDP/SR/SSA/BI-02-C")

    # END SDP TESTCASES #
