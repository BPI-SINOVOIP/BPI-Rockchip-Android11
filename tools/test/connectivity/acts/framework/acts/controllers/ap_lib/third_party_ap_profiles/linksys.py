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

from acts import utils

from acts.controllers.ap_lib import hostapd_config
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_utils


def linksys_ea4500(iface_wlan_2g=None,
                   iface_wlan_5g=None,
                   channel=None,
                   security=None,
                   ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of what a Linksys EA4500 AP
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real EA4500:
        CF (Contention-Free) Parameter IE:
            EA4500: has CF Parameter IE
            Simulated: does not have CF Parameter IE
        HT Capab:
            Info:
                EA4500: Green Field supported
                Simulated: Green Field not supported on Whirlwind.
            A-MPDU
                RTAC66U: MPDU Density 4
                Simulated: MPDU Density 8
        RSN Capab (w/ WPA2):
            EA4500:
                RSN PTKSA Replay Counter Capab: 1
            Simulated:
                RSN PTKSA Replay Counter Capab: 16
    """
    # Verify interface and security
    hostapd_utils.verify_interface(iface_wlan_2g,
                                   hostapd_constants.INTERFACE_2G_LIST)
    hostapd_utils.verify_interface(iface_wlan_5g,
                                   hostapd_constants.INTERFACE_5G_LIST)
    hostapd_utils.verify_security_mode(security,
                                       [None, hostapd_constants.WPA2])
    if security:
        hostapd_utils.verify_cipher(security,
                                    [hostapd_constants.WPA2_DEFAULT_CIPER])

    # Common Parameters
    rates = hostapd_constants.CCK_AND_OFDM_DATA_RATES

    n_capabilities = [
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_SGI40,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_DSSS_CCK_40
    ]

    # Epigram HT Capabilities IE
    # Epigram HT Additional Capabilities IE
    # Marvell Semiconductor, Inc. IE
    vendor_elements = {
        'vendor_elements':
        'dd1e00904c33fc0117ffffff0000000000000000000000000000000000000000'
        'dd1a00904c3424000000000000000000000000000000000000000000'
        'dd06005043030000'
    }

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        obss_interval = 180
        n_capabilities.append(hostapd_constants.N_CAPABILITY_HT40_PLUS)

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        obss_interval = None

    additional_params = utils.merge_dicts(rates, vendor_elements,
                                          hostapd_constants.UAPSD_ENABLED)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=interface,
        mode=hostapd_constants.MODE_11N_MIXED,
        force_wmm=True,
        beacon_interval=100,
        dtim_period=1,
        short_preamble=True,
        obss_interval=obss_interval,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config


def linksys_ea9500(iface_wlan_2g=None,
                   iface_wlan_5g=None,
                   channel=None,
                   security=None,
                   ssid=None):
    """A simulated implementation of what a Linksys EA9500 AP
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real EA9500:
        2.4GHz:
            Rates:
                EA9500:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
        RSN Capab (w/ WPA2):
            EA9500:
                RSN PTKSA Replay Counter Capab: 16
            Simulated:
                RSN PTKSA Replay Counter Capab: 1
    """
    # Verify interface and security
    hostapd_utils.verify_interface(iface_wlan_2g,
                                   hostapd_constants.INTERFACE_2G_LIST)
    hostapd_utils.verify_interface(iface_wlan_5g,
                                   hostapd_constants.INTERFACE_5G_LIST)
    hostapd_utils.verify_security_mode(security,
                                       [None, hostapd_constants.WPA2])
    if security:
        hostapd_utils.verify_cipher(security,
                                    [hostapd_constants.WPA2_DEFAULT_CIPER])

    # Common Parameters
    rates = hostapd_constants.CCK_AND_OFDM_DATA_RATES
    qbss = {'bss_load_update_period': 50, 'chan_util_avg_period': 600}
    # Measurement Pilot Transmission IE
    vendor_elements = {'vendor_elements': '42020000'}

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        mode = hostapd_constants.MODE_11G
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)

    # 5GHz
    else:
        interface = iface_wlan_5g
        mode = hostapd_constants.MODE_11A
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)

    additional_params = utils.merge_dicts(rates, qbss, vendor_elements)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=interface,
        mode=mode,
        force_wmm=False,
        beacon_interval=100,
        dtim_period=1,
        short_preamble=False,
        additional_parameters=additional_params)
    return config


def linksys_wrt1900acv2(iface_wlan_2g=None,
                        iface_wlan_5g=None,
                        channel=None,
                        security=None,
                        ssid=None):
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of what a Linksys WRT1900ACV2 AP
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real WRT1900ACV2:
        5 GHz:
            Simulated: Has two country code IEs, one that matches
                the actual, and another explicit IE that was required for
                hostapd's 802.11d to work.
        Both:
            HT Capab:
                A-MPDU
                    WRT1900ACV2: MPDU Density 4
                    Simulated: MPDU Density 8
            VHT Capab:
                WRT1900ACV2:
                    SU Beamformer Supported,
                    SU Beamformee Supported,
                    Beamformee STS Capability: 4,
                    Number of Sounding Dimensions: 4,
                Simulated:
                    Above are not supported on Whirlwind.
            RSN Capabilities (w/ WPA2):
                WRT1900ACV2:
                    RSN PTKSA Replay Counter Capab: 1
                Simulated:
                    RSN PTKSA Replay Counter Capab: 16
    """
    # Verify interface and security
    hostapd_utils.verify_interface(iface_wlan_2g,
                                   hostapd_constants.INTERFACE_2G_LIST)
    hostapd_utils.verify_interface(iface_wlan_5g,
                                   hostapd_constants.INTERFACE_5G_LIST)
    hostapd_utils.verify_security_mode(security,
                                       [None, hostapd_constants.WPA2])
    if security:
        hostapd_utils.verify_cipher(security,
                                    [hostapd_constants.WPA2_DEFAULT_CIPER])

    # Common Parameters
    rates = hostapd_constants.CCK_AND_OFDM_DATA_RATES
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_LDPC,
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_SGI40
    ]
    ac_capabilities = [
        hostapd_constants.AC_CAPABILITY_RXLDPC,
        hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
        hostapd_constants.AC_CAPABILITY_RX_STBC_1,
        hostapd_constants.AC_CAPABILITY_RX_ANTENNA_PATTERN,
        hostapd_constants.AC_CAPABILITY_TX_ANTENNA_PATTERN,
        hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7
    ]
    vht_channel_width = 20
    # Epigram, Inc. HT Capabilities IE
    # Epigram, Inc. HT Additional Capabilities IE
    # Marvell Semiconductor IE
    vendor_elements = {
        'vendor_elements':
        'dd1e00904c336c0017ffffff0001000000000000000000000000001fff071800'
        'dd1a00904c3424000000000000000000000000000000000000000000'
        'dd06005043030000'
    }

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        obss_interval = 180
        spectrum_mgmt = False
        local_pwr_constraint = {}

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        obss_interval = None
        spectrum_mgmt = True,
        local_pwr_constraint = {'local_pwr_constraint': 3}
        # Country Information IE (w/ individual channel info)
        vendor_elements['vendor_elements'] += '071e5553202401112801112c011130' \
            '01119501179901179d0117a10117a50117'

    additional_params = utils.merge_dicts(rates, vendor_elements,
                                          hostapd_constants.UAPSD_ENABLED,
                                          local_pwr_constraint)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=interface,
        mode=hostapd_constants.MODE_11AC_MIXED,
        force_wmm=True,
        beacon_interval=100,
        dtim_period=1,
        short_preamble=True,
        obss_interval=obss_interval,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        spectrum_mgmt_required=spectrum_mgmt,
        additional_parameters=additional_params)
    return config
