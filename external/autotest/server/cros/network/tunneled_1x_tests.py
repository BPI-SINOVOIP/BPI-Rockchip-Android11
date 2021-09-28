# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from autotest_lib.client.common_lib.cros import site_eap_certs
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.client.common_lib.cros.network import xmlrpc_security_types
from autotest_lib.server.cros.network import hostap_config


def __get_altsubject_match_positive_test_cases(outer_auth_type,
                                               inner_auth_type):
    configurations = []
    # Pass every subject alternative name included in the alternative subject
    # match of the server certificate.
    for subject_alternative_name in (
        site_eap_certs.server_cert_3_altsubject_match):
        eap_config = xmlrpc_security_types.Tunneled1xConfig(
            site_eap_certs.ca_cert_3,
            site_eap_certs.server_cert_3,
            site_eap_certs.server_private_key_3,
            site_eap_certs.ca_cert_3,
            'testuser',
            'password',
            inner_protocol=inner_auth_type,
            outer_protocol=outer_auth_type,
            altsubject_match=[json.dumps(subject_alternative_name)])
        ap_config = hostap_config.HostapConfig(
            frequency=2412,
            mode=hostap_config.HostapConfig.MODE_11G,
            security_config=eap_config)
        assoc_params = xmlrpc_datatypes.AssociationParameters(
            security_config=eap_config)
        configurations.append((ap_config, assoc_params))
    # Pass multiple DNS subject alternative names (SANs) as altsubject_match.
    # - One DNS SAN which does not match any of the DNS SANs of the server
    #   certificate.
    # - Another one which matches one of the DNS SANs of the server certificate.
    # The connection should be established, i.e. having multiple entries in
    # 'altsubject_match' is treated as OR, not AND.
    # For more information about how wpa_supplicant uses altsubject_match field
    # please refer to:
    # https://w1.fi/cgit/hostap/plain/wpa_supplicant/wpa_supplicant.conf
    eap_config = xmlrpc_security_types.Tunneled1xConfig(
        site_eap_certs.ca_cert_3,
        site_eap_certs.server_cert_3,
        site_eap_certs.server_private_key_3,
        site_eap_certs.ca_cert_3,
        'testuser',
        'password',
        inner_protocol=inner_auth_type,
        outer_protocol=outer_auth_type,
        altsubject_match=[
            '{"Type":"DNS","Value":"wrong_dns.com"}',
            '{"Type":"DNS","Value":"www.example.com"}'
        ])
    ap_config = hostap_config.HostapConfig(
        frequency=2412,
        mode=hostap_config.HostapConfig.MODE_11G,
        security_config=eap_config)
    assoc_params = xmlrpc_datatypes.AssociationParameters(
        security_config=eap_config)
    configurations.append((ap_config, assoc_params))
    return configurations


def get_positive_8021x_test_cases(outer_auth_type, inner_auth_type):
    """Return a test case asserting that outer/inner auth works.

    @param inner_auth_type one of
            xmlrpc_security_types.Tunneled1xConfig.LAYER1_TYPE*
    @param inner_auth_type one of
            xmlrpc_security_types.Tunneled1xConfig.LAYER2_TYPE*
    @return list of ap_config, association_params tuples for
            network_WiFi_SimpleConnect.

    """
    configurations = []
    eap_config = xmlrpc_security_types.Tunneled1xConfig(
            site_eap_certs.ca_cert_1,
            site_eap_certs.server_cert_1,
            site_eap_certs.server_private_key_1,
            site_eap_certs.ca_cert_1,
            'testuser',
            'password',
            inner_protocol=inner_auth_type,
            outer_protocol=outer_auth_type)
    ap_config = hostap_config.HostapConfig(
            frequency=2412,
            mode=hostap_config.HostapConfig.MODE_11G,
            security_config=eap_config)
    assoc_params = xmlrpc_datatypes.AssociationParameters(
            security_config=eap_config)
    configurations.append((ap_config, assoc_params))
    configurations += __get_altsubject_match_positive_test_cases(
            outer_auth_type, inner_auth_type)
    return configurations


def get_negative_8021x_test_cases(outer_auth_type, inner_auth_type):
    """Build a set of test cases for TTLS/PEAP authentication.

    @param inner_auth_type one of
            xmlrpc_security_types.Tunneled1xConfig.LAYER1_TYPE*
    @param inner_auth_type one of
            xmlrpc_security_types.Tunneled1xConfig.LAYER2_TYPE*
    @return list of ap_config, association_params tuples for
            network_WiFi_SimpleConnect.

    """
    configurations = []
    # Bad passwords won't work.
    eap_config = xmlrpc_security_types.Tunneled1xConfig(
            site_eap_certs.ca_cert_1,
            site_eap_certs.server_cert_1,
            site_eap_certs.server_private_key_1,
            site_eap_certs.ca_cert_1,
            'testuser',
            'password',
            inner_protocol=inner_auth_type,
            outer_protocol=outer_auth_type,
            client_password='wrongpassword')
    ap_config = hostap_config.HostapConfig(
            frequency=2412,
            mode=hostap_config.HostapConfig.MODE_11G,
            security_config=eap_config)
    assoc_params = xmlrpc_datatypes.AssociationParameters(
            security_config=eap_config,
            expect_failure=True)
    configurations.append((ap_config, assoc_params))
    # If use the wrong CA on the client, it won't trust the server credentials.
    eap_config = xmlrpc_security_types.Tunneled1xConfig(
            site_eap_certs.ca_cert_1,
            site_eap_certs.server_cert_1,
            site_eap_certs.server_private_key_1,
            site_eap_certs.ca_cert_2,
            'testuser',
            'password',
            inner_protocol=inner_auth_type,
            outer_protocol=outer_auth_type)
    ap_config = hostap_config.HostapConfig(
            frequency=2412,
            mode=hostap_config.HostapConfig.MODE_11G,
            security_config=eap_config)
    assoc_params = xmlrpc_datatypes.AssociationParameters(
            security_config=eap_config,
            expect_failure=True)
    configurations.append((ap_config, assoc_params))
    # And if the server's credentials are good but expired, we also reject it.
    eap_config = xmlrpc_security_types.Tunneled1xConfig(
            site_eap_certs.ca_cert_1,
            site_eap_certs.server_expired_cert,
            site_eap_certs.server_expired_key,
            site_eap_certs.ca_cert_1,
            'testuser',
            'password',
            inner_protocol=inner_auth_type,
            outer_protocol=outer_auth_type)
    ap_config = hostap_config.HostapConfig(
            frequency=2412,
            mode=hostap_config.HostapConfig.MODE_11G,
            security_config=eap_config)
    assoc_params = xmlrpc_datatypes.AssociationParameters(
            security_config=eap_config,
            expect_failure=True)
    configurations.append((ap_config, assoc_params))
    # A subject alternative name (SAN) which does not match any of the server
    # certificate SANs is used.
    # The connection should not be established, i.e. if the subject alternative
    # name match field is set, the server certificate is only accepted if it
    # contains one of its entries.
    eap_config = xmlrpc_security_types.Tunneled1xConfig(
            site_eap_certs.ca_cert_3,
            site_eap_certs.server_cert_3,
            site_eap_certs.server_private_key_3,
            site_eap_certs.ca_cert_3,
            'testuser',
            'password',
            inner_protocol=inner_auth_type,
            outer_protocol=outer_auth_type,
            altsubject_match=['{"Type":"DNS","Value":"wrong_dns.com"}'])
    ap_config = hostap_config.HostapConfig(
            frequency=2412,
            mode=hostap_config.HostapConfig.MODE_11G,
            security_config=eap_config)
    assoc_params = xmlrpc_datatypes.AssociationParameters(
            security_config=eap_config,
            expect_failure=True)
    configurations.append((ap_config, assoc_params))
    return configurations
