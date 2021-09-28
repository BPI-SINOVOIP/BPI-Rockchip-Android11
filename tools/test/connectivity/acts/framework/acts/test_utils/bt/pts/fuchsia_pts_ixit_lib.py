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
"""This is a placeholder for all IXIT values in PTS
    that matter to Fuchsia devices.
"""

A2DP_IXIT = {
    b'TSPX_security_enabled': (b'BOOLEAN', b'FALSE'),
    b'TSPX_bd_addr_iut': (b'OCTETSTRING', b'000000000000'),
    b'TSPX_SRC_class_of_device': (b'OCTETSTRING', b'080418'),
    b'TSPX_SNK_class_of_device': (b'OCTETSTRING', b'04041C'),
    b'TSPX_pin_code': (b'IA5STRING', b'0000'),
    b'TSPX_delete_link_key': (b'BOOLEAN', b'FALSE'),
    b'TSPX_time_guard': (b'INTEGER', b'300000'),
    b'TSPX_use_implicit_send': (b'BOOLEAN', b'TRUE'),
    b'TSPX_media_directory':
    (b'IA5STRING', b'C:\Program Files\Bluetooth SIG\Bluetooth PTS\\bin\\audio'),
    b'TSPX_auth_password': (b'IA5STRING', b'0000'),
    b'TSPX_auth_user_id': (b'IA5STRING', b'PTS'),
    b'TSPX_rfcomm_channel': (b'INTEGER', b'8'),
    b'TSPX_l2cap_psm': (b'OCTETSTRING', b'1011'),
    b'TSPX_no_confirmations': (b'BOOLEAN', b'FALSE'),
    b'TSPX_cover_art_uuid': (b'OCTETSTRING', b'3EEE'),
}

GATT_IXIT = {
    b'TSPX_bd_addr_iut': (b'OCTETSTRING', b'000000000000'),
    b'TSPX_iut_device_name_in_adv_packet_for_random_address': (b'IA5STRING', b'tbd'),
    b'TSPX_security_enabled': (b'BOOLEAN', b'FALSE'),
    b'TSPX_delete_link_key': (b'BOOLEAN', b'TRUE'),
    b'TSPX_time_guard': (b'INTEGER', b'180000'),
    b'TSPX_selected_handle': (b'OCTETSTRING', b'0012'),
    b'TSPX_use_implicit_send': (b'BOOLEAN', b'TRUE'),
    b'TSPX_secure_simple_pairing_pass_key_confirmation': (b'BOOLEAN', b'FALSE'),
    b'TSPX_iut_use_dynamic_bd_addr': (b'BOOLEAN', b'FALSE'),
    b'TSPX_iut_setup_att_over_br_edr': (b'BOOLEAN', b'FALSE'),
    b'TSPX_tester_database_file': (b'IA5STRING', b'C:\Program Files\Bluetooth SIG\Bluetooth PTS\Data\SIGDatabase\GATT_Qualification_Test_Databases.xml'),
    b'TSPX_iut_is_client_periphral': (b'BOOLEAN', b'FALSE'),
    b'TSPX_iut_is_server_central': (b'BOOLEAN', b'FALSE'),
    b'TSPX_mtu_size': (b'INTEGER', b'23'),
    b'TSPX_pin_code':  (b'IA5STRING', b'0000'),
    b'TSPX_use_dynamic_pin': (b'BOOLEAN', b'FALSE'),
    b'TSPX_delete_ltk': (b'BOOLEAN', b'FALSE'),
    b'TSPX_tester_appearance': (b'OCTETSTRING', b'0000'),
}

SDP_IXIT = {
    b'TSPX_sdp_service_search_pattern': (b'IA5STRING', b'0100'),
    b'TSPX_sdp_service_search_pattern_no_results': (b'IA5STRING', b'EEEE'),
    b'TSPX_sdp_service_search_pattern_additional_protocol_descriptor_list': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_bluetooth_profile_descriptor_list': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_browse_group_list': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_client_exe_url': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_documentation_url': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_icon_url': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_language_base_attribute_id_list': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_protocol_descriptor_list': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_provider_name': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_service_availability': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_service_data_base_state': (b'IA5STRING', b'1000'),
    b'TSPX_sdp_service_search_pattern_service_description': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_service_id': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_service_info_time_to_live': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_version_number_list': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_service_name': (b'IA5STRING', b''),
    b'TSPX_sdp_service_search_pattern_service_record_state': (b'IA5STRING', b''),
    b'TSPX_sdp_unsupported_attribute_id': (b'OCTETSTRING', b'EEEE'),
    b'TSPX_security_enabled': (b'BOOLEAN', b'FALSE'),
    b'TSPX_delete_link_key': (b'BOOLEAN', b'FALSE'),
    b'TSPX_bd_addr_iut': (b'OCTETSTRING', b''),
    b'TSPX_class_of_device_pts': (b'OCTETSTRING', b'200404'),
    b'TSPX_class_of_device_test_pts_initiator': (b'BOOLEAN', b'TRUE'),
    b'TSPX_limited_inquiry_used': (b'BOOLEAN', b'FALSE'),
    b'TSPX_pin_code': (b'IA5STRING', b'0000'),
    b'TSPX_time_guard': (b'INTEGER', b'200000'),
    b'TSPX_device_search_time': (b'INTEGER', b'20'),
    b'TSPX_use_implicit_send': (b'BOOLEAN', b'TRUE'),
    b'TSPX_secure_simple_pairing_pass_key_confirmation': (b'BOOLEAN', b'FALSE'),
}
