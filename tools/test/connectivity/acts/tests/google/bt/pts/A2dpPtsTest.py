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
A2DP PTS Tests.
"""
from acts.test_utils.abstract_devices.bluetooth_device import AndroidBluetoothDevice
from acts.test_utils.abstract_devices.bluetooth_device import FuchsiaBluetoothDevice
from acts.test_utils.bt.pts.pts_base_class import PtsBaseClass

import acts.test_utils.bt.pts.fuchsia_pts_ics_lib as f_ics_lib
import acts.test_utils.bt.pts.fuchsia_pts_ixit_lib as f_ixit_lib


class A2dpPtsTest(PtsBaseClass):
    ble_advertise_interval = 100
    pts_action_mapping = None

    def setup_class(self):
        super().setup_class()
        self.dut.initialize_bluetooth_controller()
        # self.dut.set_bluetooth_local_name(self.dut_bluetooth_local_name)
        local_dut_mac_address = self.dut.get_local_bluetooth_address()

        ics = None
        ixit = None
        if isinstance(self.dut, FuchsiaBluetoothDevice):
            fuchsia_ixit = f_ixit_lib.A2DP_IXIT
            fuchsia_ixit[b'TSPX_bd_addr_iut'] = (b'OCTETSTRING',
                                                 local_dut_mac_address.replace(
                                                     ':', '').encode())
            ics = f_ics_lib.A2DP_ICS
            ixit = fuchsia_ixit
        elif isinstance(self.dut, AndroidBluetoothDevice):
            # TODO: Add ICS and IXIT values for Android
            self.log.warn(
                "ICS/IXIT values not set for Android, using Fuchsia as default."
            )
            fuchsia_ixit = f_ixit_lib.A2DP_IXIT
            fuchsia_ixit[b'TSPX_bd_addr_iut'] = (b'OCTETSTRING',
                                                 local_dut_mac_address.replace(
                                                     ':', '').encode())
            ics = f_ics_lib.A2DP_ICS
            ixit = fuchsia_ixit

        ### PTS SETUP: Required after ICS, IXIT, and profile is setup ###
        self.pts.set_profile_under_test("A2DP")
        self.pts.set_ics_and_ixit(ics, ixit)
        self.pts.setup_pts()
        ### End PTS Setup ###

        self.dut.unbond_all_known_devices()
        self.dut.start_pairing_helper()

    def setup_test(self):
        super(A2dpPtsTest, self).setup_test()
        # Make sure there were no lingering answers due to a failed test.
        self.pts.extra_answers = []

    def teardown_test(self):
        super(A2dpPtsTest, self).teardown_test()

    def teardown_class(self):
        super(A2dpPtsTest, self).teardown_class()
        self.dut.stop_profile_a2dp_sink()

    # BEGIN A2DP SINK TESTCASES #
    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_as_bv_01_i(self):
        return self.pts.execute_test("A2DP/SNK/AS/BV-01-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_as_bv_02_i(self):
        return self.pts.execute_test("A2DP/SNK/AS/BV-02-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_cc_bv_01_i(self):
        return self.pts.execute_test("A2DP/SNK/CC/BV-01-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_cc_bv_02_i(self):
        return self.pts.execute_test("A2DP/SNK/CC/BV-02-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_cc_bv_05_i(self):
        return self.pts.execute_test("A2DP/SNK/CC/BV-05-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_cc_bv_06_i(self):
        return self.pts.execute_test("A2DP/SNK/CC/BV-06-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_cc_bv_07_i(self):
        return self.pts.execute_test("A2DP/SNK/CC/BV-07-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_cc_bv_08_i(self):
        return self.pts.execute_test("A2DP/SNK/CC/BV-08-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_rel_bv_01_i(self):
        return self.pts.execute_test("A2DP/SNK/REL/BV-01-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_set_bv_01_i(self):
        return self.pts.execute_test("A2DP/SNK/SET/BV-01-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_set_bv_02_i(self):
        return self.pts.execute_test("A2DP/SNK/SET/BV-02-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_set_bv_03_i(self):
        return self.pts.execute_test("A2DP/SNK/SET/BV-03-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_set_bv_03_i_bv_05_i(self):
        return self.pts.execute_test("A2DP/SNK/SET/BV-05-I")

    @PtsBaseClass.pts_test_wrap
    def test_a2dp_snk_sus_bv_01_i(self):
        return self.pts.execute_test("A2DP/SNK/SUS/BV-01-I")

    # END A2DP SINK TESTCASES #
