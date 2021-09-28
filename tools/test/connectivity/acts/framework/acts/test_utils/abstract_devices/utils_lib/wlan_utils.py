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

import logging

from acts import asserts
from acts.controllers.ap_lib import hostapd_ap_preset


def validate_setup_ap_and_associate(*args, **kwargs):
    """Validates if setup_ap_and_associate was a success or not

       Args: Args match setup_ap_and_associate
    """
    asserts.assert_true(setup_ap_and_associate(*args, **kwargs),
                        'Failed to associate.')
    asserts.explicit_pass('Successfully associated.')


def setup_ap_and_associate(access_point,
                           client,
                           profile_name,
                           channel,
                           ssid,
                           mode=None,
                           preamble=None,
                           beacon_interval=None,
                           dtim_period=None,
                           frag_threshold=None,
                           rts_threshold=None,
                           force_wmm=None,
                           hidden=False,
                           security=None,
                           additional_ap_parameters=None,
                           password=None,
                           check_connectivity=False,
                           n_capabilities=None,
                           ac_capabilities=None,
                           vht_bandwidth=None,
                           setup_bridge=False):
    """Sets up the AP and associates a client.

    Args:
        access_point: An ACTS access_point controller
        client: A WlanDevice.
        profile_name: The profile name of one of the hostapd ap presets.
        channel: What channel to set the AP to.
        preamble: Whether to set short or long preamble (True or False)
        beacon_interval: The beacon interval (int)
        dtim_period: Length of dtim period (int)
        frag_threshold: Fragmentation threshold (int)
        rts_threshold: RTS threshold (int)
        force_wmm: Enable WMM or not (True or False)
        hidden: Advertise the SSID or not (True or False)
        security: What security to enable.
        additional_ap_parameters: Additional parameters to send the AP.
        password: Password to connect to WLAN if necessary.
        check_connectivity: Whether to check for internet connectivity.
    """
    setup_ap(access_point, profile_name, channel, ssid, mode, preamble,
             beacon_interval, dtim_period, frag_threshold, rts_threshold,
             force_wmm, hidden, security, additional_ap_parameters, password,
             check_connectivity, n_capabilities, ac_capabilities,
             vht_bandwidth, setup_bridge)

    if security and security.wpa3:
        return associate(client,
                         ssid,
                         password,
                         key_mgmt='SAE',
                         check_connectivity=check_connectivity,
                         hidden=hidden)
    else:
        return associate(client,
                         ssid,
                         password=password,
                         check_connectivity=check_connectivity,
                         hidden=hidden)


def setup_ap(access_point,
             profile_name,
             channel,
             ssid,
             mode=None,
             preamble=None,
             beacon_interval=None,
             dtim_period=None,
             frag_threshold=None,
             rts_threshold=None,
             force_wmm=None,
             hidden=False,
             security=None,
             additional_ap_parameters=None,
             password=None,
             check_connectivity=False,
             n_capabilities=None,
             ac_capabilities=None,
             vht_bandwidth=None,
             setup_bridge=False):
    """Sets up the AP.

    Args:
        access_point: An ACTS access_point controller
        profile_name: The profile name of one of the hostapd ap presets.
        channel: What channel to set the AP to.
        preamble: Whether to set short or long preamble (True or False)
        beacon_interval: The beacon interval (int)
        dtim_period: Length of dtim period (int)
        frag_threshold: Fragmentation threshold (int)
        rts_threshold: RTS threshold (int)
        force_wmm: Enable WMM or not (True or False)
        hidden: Advertise the SSID or not (True or False)
        security: What security to enable.
        additional_ap_parameters: Additional parameters to send the AP.
        password: Password to connect to WLAN if necessary.
        check_connectivity: Whether to check for internet connectivity.
    """
    ap = hostapd_ap_preset.create_ap_preset(profile_name=profile_name,
                                            iface_wlan_2g=access_point.wlan_2g,
                                            iface_wlan_5g=access_point.wlan_5g,
                                            channel=channel,
                                            ssid=ssid,
                                            mode=mode,
                                            short_preamble=preamble,
                                            beacon_interval=beacon_interval,
                                            dtim_period=dtim_period,
                                            frag_threshold=frag_threshold,
                                            rts_threshold=rts_threshold,
                                            force_wmm=force_wmm,
                                            hidden=hidden,
                                            bss_settings=[],
                                            security=security,
                                            n_capabilities=n_capabilities,
                                            ac_capabilities=ac_capabilities,
                                            vht_bandwidth=vht_bandwidth)
    access_point.start_ap(hostapd_config=ap,
                          setup_bridge=setup_bridge,
                          additional_parameters=additional_ap_parameters)


def associate(client,
              ssid,
              password=None,
              key_mgmt=None,
              check_connectivity=True,
              hidden=False):
    """Associates a client to a WLAN network.

    Args:
        client: A WlanDevice
        ssid: SSID of the ap we are looking for.
        password: The password for the WLAN, if applicable.
        key_mgmt: The hostapd wpa_key_mgmt value.
        check_connectivity: Whether to check internet connectivity.
        hidden: If the WLAN is hidden or not.
    """
    return client.associate(ssid,
                            password,
                            key_mgmt=key_mgmt,
                            check_connectivity=check_connectivity,
                            hidden=hidden)


def status(client):
    """Requests the state of WLAN network.

    Args:
        None
    """
    status = ''
    status_response = client.status()

    if status_response.get('error') is None:
        # No error, so get the result
        status = status_response['result']

    logging.debug('status: %s' % status)
    return status


def is_connected(client):
    """Gets status to determine if WLAN is connected or not.

    Args:
        None
    """
    connected = False
    client_status = status(client)
    if client_status and client_status['state'] == 'ConnectionsEnabled':
        for index, network in enumerate(client_status['networks']):
            if network['state'] == 'Connected':
                connected = True

    return connected


def disconnect(client):
    """Disconnect client from its WLAN network.

    Args:
        client: A WlanDevice
    """
    client.disconnect()
