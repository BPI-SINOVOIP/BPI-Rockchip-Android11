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
"""This is the PTS base class that is inherited from all PTS
Tests.
"""

import ctypes
import random
import re
import time
import traceback

from ctypes import *
from datetime import datetime

from acts import signals
from acts.base_test import BaseTestClass
from acts.controllers.bluetooth_pts_device import VERDICT_STRINGS
from acts.controllers.fuchsia_device import FuchsiaDevice
from acts.signals import TestSignal
from acts.test_utils.abstract_devices.bluetooth_device import create_bluetooth_device
from acts.test_utils.bt.bt_constants import gatt_transport
from acts.test_utils.fuchsia.bt_test_utils import le_scan_for_device_by_name


class PtsBaseClass(BaseTestClass):
    """ Class for representing common functionality across all PTS tests.

    This includes the ability to rerun tests due to PTS instability,
    common PTS action mappings, and setup/teardown related devices.

    """
    scan_timeout_seconds = 10
    peer_identifier = None

    def setup_class(self):
        super().setup_class()
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

        self.characteristic_read_not_permitted_uuid = self.user_params.get(
            "characteristic_read_not_permitted_uuid")
        self.characteristic_read_not_permitted_handle = self.user_params.get(
            "characteristic_read_not_permitted_handle")
        self.characteristic_read_invalid_handle = self.user_params.get(
            "characteristic_read_invalid_handle")
        self.characteristic_attribute_not_found_uuid = self.user_params.get(
            "characteristic_attribute_not_found_uuid")
        self.write_characteristic_not_permitted_handle = self.user_params.get(
            "write_characteristic_not_permitted_handle")

        self.pts = self.bluetooth_pts_device[0]
        # MMI functions commented out until implemented. Added for tracking
        # purposes.
        self.pts_action_mapping = {
            "A2DP": {
                1: self.a2dp_mmi_iut_connectable,
                1002: self.a2dp_mmi_iut_accept_connect,
                1020: self.a2dp_mmi_initiate_open_stream,
            },
            "GATT": {
                1: self.mmi_make_iut_connectable,
                2: self.mmi_iut_initiate_connection,
                3: self.mmi_iut_initiate_disconnection,
                # 4: self.mmi_iut_no_security,
                # 5: self.mmi_iut_initiate_br_connection,
                10: self.mmi_discover_primary_service,
                # 11: self.mmi_confirm_no_primary_service_small,
                # 12: self.mmi_iut_mtu_exchange,
                # 13: self.mmi_discover_all_service_record,
                # 14: self.mmi_iut_discover_gatt_service_record,
                15: self.mmi_iut_find_included_services,
                # 16: self.mmi_confirm_no_characteristic_uuid_small,
                17: self.mmi_confirm_primary_service,
                # 18: self.mmi_send_primary_service_uuid,
                # 19: self.mmi_confirm_primary_service_uuid,
                # 22: self.confirm_primary_service_1801,
                24: self.mmi_confirm_include_service,
                26: self.mmi_confirm_characteristic_service,
                # 27: self.perform_read_all_characteristics,
                29: self.
                mmi_discover_service_uuid_range,  # AKA: discover service by uuid
                # 31: self.perform_read_all_descriptors,
                48: self.mmi_iut_send_read_characteristic_handle,
                58: self.mmi_iut_send_read_descriptor_handle,
                70: self.mmi_send_write_command,
                74: self.mmi_send_write_request,
                76: self.mmi_send_prepare_write,
                77: self.mmi_iut_send_prepare_write_greater_offset,
                80: self.mmi_iut_send_prepare_write_greater,
                110: self.mmi_iut_enter_handle_read_not_permitted,
                111: self.mmi_iut_enter_uuid_read_not_permitted,
                118: self.mmi_iut_enter_handle_invalid,
                119: self.mmi_iut_enter_uuid_attribute_not_found,
                120: self.mmi_iut_enter_handle_write_not_permitted,
                2000: self.mmi_verify_secure_id,  # Enter pairing pin from DUT.
            },
            "SDP": {
                # TODO: Implement MMIs as necessary
            }
        }
        self.pts.bind_to(self.process_next_action)

    def teardown_class(self):
        self.pts.clean_up()

    def setup_test(self):
        # Always start the test with RESULT_INCOMP
        self.pts.pts_test_result = VERDICT_STRINGS['RESULT_INCOMP']

    def teardown_test(self):
        return True

    @staticmethod
    def pts_test_wrap(fn):
        def _safe_wrap_test_case(self, *args, **kwargs):
            test_id = "{}:{}:{}".format(self.__class__.__name__, fn.__name__,
                                        time.time())
            log_string = "[Test ID] {}".format(test_id)
            self.log.info(log_string)
            try:
                self.dut.log_info("Started " + log_string)
                result = fn(self, *args, **kwargs)
                self.dut.log_info("Finished " + log_string)
                rerun_count = self.user_params.get("pts_auto_rerun_count", 0)
                for i in range(int(rerun_count)):
                    if result is not True:
                        self.teardown_test()
                        log_string = "[Rerun Test ID] {}. Run #{} run failed... Retrying".format(
                            test_id, i + 1)
                        self.log.info(log_string)
                        self.setup_test()
                        self.dut.log_info("Rerun Started " + log_string)
                        result = fn(self, *args, **kwargs)
                    else:
                        return result
                return result
            except TestSignal:
                raise
            except Exception as e:
                self.log.error(traceback.format_exc())
                self.log.error(str(e))
                raise
            return fn(self, *args, **kwargs)

        return _safe_wrap_test_case

    def process_next_action(self, action):
        func = self.pts_action_mapping.get(
            self.pts.pts_profile_mmi_request).get(action, "Nothing")
        if func is not 'Nothing':
            func()

    ### BEGIN A2DP MMI Actions ###

    def a2dp_mmi_iut_connectable(self):
        self.dut.start_profile_a2dp_sink()
        self.dut.set_discoverable(True)

    def a2dp_mmi_iut_accept_connect(self):
        self.dut.start_profile_a2dp_sink()
        self.dut.set_discoverable(True)

    def a2dp_mmi_initiate_open_stream(self):
        self.dut.a2dp_initiate_open_stream()

    ### END A2DP MMI Actions ###

    ### BEGIN GATT MMI Actions ###

    def create_write_value_by_size(self, size):
        write_value = []
        for i in range(size):
            write_value.append(i % 256)
        return write_value

    def mmi_send_write_command(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O with', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        raw_size = re.search('with <= \'(.*)\' byte', description_to_parse)
        size = int(raw_size.group(1))
        self.dut.gatt_client_write_characteristic_without_response_by_handle(
            self.peer_identifier, handle,
            self.create_write_value_by_size(size))

    def mmi_send_write_request(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O with', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        raw_size = re.search('with <= \'(.*)\' byte', description_to_parse)
        size = int(raw_size.group(1))
        offset = 0
        self.dut.gatt_client_write_characteristic_by_handle(
            self.peer_identifier, handle, offset,
            self.create_write_value_by_size(size))

    def mmi_send_prepare_write(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O <=', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        raw_size = re.search('<= \'(.*)\' byte', description_to_parse)
        size = int(math.floor(int(raw_size.group(1)) / 2))
        offset = int(size / 2)
        self.dut.gatt_client_write_characteristic_by_handle(
            self.peer_identifier, handle, offset,
            self.create_write_value_by_size(size))

    def mmi_iut_send_prepare_write_greater_offset(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O and', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        raw_offset = re.search('greater than \'(.*)\' byte',
                               description_to_parse)
        offset = int(raw_offset.group(1))
        size = 1
        self.dut.gatt_client_write_characteristic_by_handle(
            self.peer_identifier, handle, offset,
            self.create_write_value_by_size(size))

    def mmi_iut_send_prepare_write_greater(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O with', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        raw_size = re.search('greater than \'(.*)\' byte',
                             description_to_parse)
        size = int(raw_size.group(1))
        offset = 0
        self.dut.gatt_client_write_characteristic_by_handle(
            self.peer_identifier, handle, offset,
            self.create_write_value_by_size(size))

    def mmi_make_iut_connectable(self):
        adv_data = {"name": self.dut_bluetooth_local_name}
        self.dut.start_le_advertisement(adv_data, self.ble_advertise_interval)

    def mmi_iut_enter_uuid_read_not_permitted(self):
        self.pts.extra_answers.append(
            self.characteristic_read_not_permitted_uuid)

    def mmi_iut_enter_handle_read_not_permitted(self):
        self.pts.extra_answers.append(
            self.characteristic_read_not_permitted_handle)

    def mmi_iut_enter_handle_invalid(self):
        self.pts.extra_answers.append(self.characteristic_read_invalid_handle)

    def mmi_iut_enter_uuid_attribute_not_found(self):
        self.pts.extra_answers.append(
            self.characteristic_attribute_not_found_uuid)

    def mmi_iut_enter_handle_write_not_permitted(self):
        self.pts.extra_answers.append(
            self.write_characteristic_not_permitted_handle)

    def mmi_verify_secure_id(self):
        self.pts.extra_answers.append(self.dut.get_pairing_pin())

    def mmi_discover_service_uuid_range(self, uuid):
        self.dut.gatt_client_mmi_discover_service_uuid_range(
            self.peer_identifier, uuid)

    def mmi_iut_initiate_connection(self):
        autoconnect = False
        transport = gatt_transport['le']
        adv_name = "PTS"
        self.peer_identifier = self.dut.le_scan_with_name_filter(
            "PTS", self.scan_timeout_seconds)
        if self.peer_identifier is None:
            raise signals.TestFailure("Scanner unable to find advertisement.")
        tries = 3
        for _ in range(tries):
            if self.dut.gatt_connect(self.peer_identifier, transport,
                                     autoconnect):
                return

        raise signals.TestFailure("Unable to connect to peripheral.")

    def mmi_iut_initiate_disconnection(self):
        if not self.dut.gatt_disconnect(self.peer_identifier):
            raise signals.TestFailure("Failed to disconnect from peer.")

    def mmi_discover_primary_service(self):
        self.dut.gatt_refresh()

    def mmi_iut_find_included_services(self):
        self.dut.gatt_refresh()

        test_result = self.pts.execute_test(test_name)
        return test_result

    def mmi_confirm_primary_service(self):
        # TODO: Write verifier that 1800 and 1801 exists. For now just pass.
        return True

    def mmi_confirm_characteristic_service(self):
        # TODO: Write verifier that no services exist. For now just pass.
        return True

    def mmi_confirm_include_service(self, uuid_description):
        # TODO: Write verifier that input services exist. For now just pass.
        # Note: List comes in the form of a long string to parse:
        # Attribute Handle = '0002'O Included Service Attribute handle = '0080'O,End Group Handle = '0085'O,Service UUID = 'A00B'O
        # \n
        # Attribute Handle = '0021'O Included Service Attribute handle = '0001'O,End Group Handle = '0006'O,Service UUID = 'A00D'O
        # \n ...
        return True

    def mmi_iut_send_read_characteristic_handle(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O to', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        self.dut.gatt_client_read_characteristic_by_handle(
            self.peer_identifier, handle)

    def mmi_iut_send_read_descriptor_handle(self):
        description_to_parse = self.pts.current_implicit_send_description
        raw_handle = re.search('handle = \'(.*)\'O to', description_to_parse)
        handle = int(raw_handle.group(1), 16)
        self.dut.gatt_client_descriptor_read_by_handle(self.peer_identifier,
                                                       handle)

    ### END GATT MMI Actions ###
