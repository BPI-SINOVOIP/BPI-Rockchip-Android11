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
GATT PTS Automation
"""

from acts import signals
from acts.test_utils.abstract_devices.bluetooth_device import AndroidBluetoothDevice
from acts.test_utils.abstract_devices.bluetooth_device import FuchsiaBluetoothDevice
from acts.controllers.bluetooth_pts_device import VERDICT_STRINGS
from acts.test_utils.bt.pts.pts_base_class import PtsBaseClass

import acts.test_utils.bt.gatt_test_database as gatt_test_database
import acts.test_utils.bt.pts.fuchsia_pts_ics_lib as f_ics_lib
import acts.test_utils.bt.pts.fuchsia_pts_ixit_lib as f_ixit_lib


class GattPtsTest(PtsBaseClass):
    ble_advertise_interval = 100
    pts_action_mapping = None

    def setup_class(self):
        super().setup_class()
        self.dut_bluetooth_local_name = "fs_test"
        self.dut.initialize_bluetooth_controller()
        self.dut.set_bluetooth_local_name(self.dut_bluetooth_local_name)
        local_dut_mac_address = self.dut.get_local_bluetooth_address()

        ics = None
        ixit = None
        if isinstance(self.dut, FuchsiaBluetoothDevice):
            fuchsia_ixit = f_ixit_lib.GATT_IXIT
            fuchsia_ixit[b'TSPX_bd_addr_iut'] = (b'OCTETSTRING',
                                                 local_dut_mac_address.replace(
                                                     ':', '').encode())
            fuchsia_ixit[
                b'TSPX_iut_device_name_in_adv_packet_for_random_address'] = (
                    b'IA5STRING', self.dut_bluetooth_local_name.encode())
            ics = f_ics_lib.GATT_ICS
            ixit = fuchsia_ixit
        elif isinstance(self.dut, AndroidBluetoothDevice):
            # TODO: Add ICS and IXIT values for Android. For now just default
            # To Fuchsia as it's a subset of Android.
            self.log.warn(
                "ICS/IXIT values not set for Android, using Fuchsia as default."
            )
            fuchsia_ixit = f_ixit_lib.GATT_IXIT
            fuchsia_ixit[b'TSPX_bd_addr_iut'] = (b'OCTETSTRING',
                                                 local_dut_mac_address.replace(
                                                     ':', '').encode())
            fuchsia_ixit[
                b'TSPX_iut_device_name_in_adv_packet_for_random_address'] = (
                    b'IA5STRING', self.dut_bluetooth_local_name.encode())
            ics = f_ics_lib.GATT_ICS
            ixit = fuchsia_ixit
        else:
            raise ValueError(
                "Unable to run PTS tests on unsupported hardare {}.".format(
                    type(self.dut)))

        ### PTS SETUP: Required after ICS, IXIT, and profile is setup ###
        self.pts.set_profile_under_test("GATT")
        self.pts.set_ics_and_ixit(ics, ixit)
        self.pts.setup_pts()
        ### End PTS Setup ###

        self.dut.unbond_all_known_devices()
        self.dut.start_pairing_helper()

    def setup_test(self):
        super(GattPtsTest, self).setup_test()
        # Make sure there were no lingering answers due to a failed test.
        self.pts.extra_answers = []

    def teardown_test(self):
        super(GattPtsTest, self).teardown_test()
        self.dut.stop_le_advertisement()
        self.dut.close_gatt_server()

    def teardown_class(self):
        super(GattPtsTest, self).teardown_class()
        self.dut.unbond_device(self.peer_identifier)

    # BEGIN GATT CLIENT TESTCASES #
    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_01_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_03_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_04_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_05_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_06_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_07_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gad_bv_08_c(self):
        return self.pts.execute_test("GATT/CL/GAD/BV-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gar_bv_01_c(self):
        return self.pts.execute_test("GATT/CL/GAR/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bv_01_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bv_03_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bv_05_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BV-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bv_08_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BV-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bv_09_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BV-09-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_02_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-02-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_03_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_05_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_06_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_07_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_08_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_09_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-09-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gaw_bi_33_c(self):
        return self.pts.execute_test("GATT/CL/GAW/BI-33-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gar_bv_01_c(self):
        return self.pts.execute_test("GATT/CL/GAR/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gar_bv_03_c(self):
        return self.pts.execute_test("GATT/CL/GAR/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gar_bv_04_c(self):
        return self.pts.execute_test("GATT/CL/GAR/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gar_bv_06_c(self):
        return self.pts.execute_test("GATT/CL/GAR/BV-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_cl_gar_bv_07_c(self):
        return self.pts.execute_test("GATT/CL/GAR/BV-07-C")

    # END GATT CLIENT TESTCASES #
    # BEGIN GATT SERVER TESTCASES #

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_01_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_02_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-02-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_03_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_04_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_05_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_06_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_07_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gad_bv_08_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAD/BV-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bv_01_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_01_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_02_c(self):
        if self.characteristic_read_invalid_handle is None:
            raise signals.TestSkip(
                "Required user params missing:\n{}\n{}".format(
                    "characteristic_read_invalid_handle"))
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-02-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_05_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('TEST_DB_2'))
        return self.pts.execute_test("GATT/SR/GAR/BI-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bv_03_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_06_c(self):
        if (self.characteristic_read_not_permitted_uuid is None
                or self.characteristic_read_not_permitted_handle is None):
            raise signals.TestSkip(
                "Required user params missing:\n{}\n{}".format(
                    "characteristic_read_not_permitted_uuid",
                    "characteristic_read_not_permitted_handle"))
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_07_c(self):
        if self.characteristic_attribute_not_found_uuid is None:
            raise signals.TestSkip("Required user params missing:\n{}".format(
                "characteristic_attribute_not_found_uuid"))
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_08_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAR/BI-08-C")

    def test_gatt_sr_gar_bi_11_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-11-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bv_04_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAR/BV-04-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_12_c(self):
        if self.characteristic_read_not_permitted_handle is None:
            raise signals.TestSkip("Required user params missing:\n{}".format(
                "characteristic_read_not_permitted_handle"))
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-12-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_13_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-13-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_14_c(self):
        if self.characteristic_read_invalid_handle is None:
            raise signals.TestSkip("Required user params missing:\n{}".format(
                "characteristic_read_invalid_handle"))
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BI-14-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bi_16_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('TEST_DB_2'))
        return self.pts.execute_test("GATT/SR/GAR/BI-16-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bv_06_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BV-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bv_07_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BV-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gar_bv_08_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAR/BV-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_01_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BV-01-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_03_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BV-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_03_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BI-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_07_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BI-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_08_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BI-08-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_12_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BI-12-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_13_c(self):

        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BI-03-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_05_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BV-05-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_09_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BI-09-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_06_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BV-06-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_07_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BV-07-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_09_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BV-09-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bv_10_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_1'))
        return self.pts.execute_test("GATT/SR/GAW/BV-10-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_32_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('DB_TEST'))
        return self.pts.execute_test("GATT/SR/GAW/BI-32-C")

    @PtsBaseClass.pts_test_wrap
    def test_gatt_sr_gaw_bi_33_c(self):
        self.dut.setup_gatt_server(
            gatt_test_database.GATT_SERVER_DB_MAPPING.get('LARGE_DB_3'))
        return self.pts.execute_test("GATT/SR/GAW/BI-33-C")

    # END GATT SERVER TESTCASES #
