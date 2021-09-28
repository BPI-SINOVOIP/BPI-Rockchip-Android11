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


def netgear_r7000(iface_wlan_2g=None,
                  iface_wlan_5g=None,
                  channel=None,
                  security=None,
                  ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of what a Netgear R7000 AP
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real R7000:
        2.4GHz:
            Rates:
                R7000:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48,
        5GHz:
            VHT Capab:
                R7000:
                    SU Beamformer Supported,
                    SU Beamformee Supported,
                    Beamformee STS Capability: 3,
                    Number of Sounding Dimensions: 3,
                    VHT Link Adaptation: Both
                Simulated:
                    Above are not supported on Whirlwind.
            VHT Operation Info:
                R7000: Basic MCS Map (0x0000)
                Simulated: Basic MCS Map (0xfffc)
            VHT Tx Power Envelope:
                R7000: Local Max Tx Pwr Constraint: 1.0 dBm
                Simulated: Local Max Tx Pwr Constraint: 23.0 dBm
        Both:
            HT Capab:
                A-MPDU
                    R7000: MPDU Density 4
                    Simulated: MPDU Density 8
            HT Info:
                R7000: RIFS Permitted
                Simulated: RIFS Prohibited
            RM Capabilities:
                R7000:
                    Beacon Table Measurement: Not Supported
                    Statistic Measurement: Enabled
                    AP Channel Report Capability: Enabled
                Simulated:
                    Beacon Table Measurement: Supported
                    Statistic Measurement: Disabled
                    AP Channel Report Capability: Disabled
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
    vht_channel_width = 80
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_LDPC,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935,
        hostapd_constants.N_CAPABILITY_SGI20,
    ]
    # Netgear IE
    # WPS IE
    # Epigram, Inc. IE
    # Broadcom IE
    vendor_elements = {
        'vendor_elements':
        'dd0600146c000000'
        'dd310050f204104a00011010440001021047001066189606f1e967f9c0102048817a7'
        '69e103c0001031049000600372a000120'
        'dd1e00904c0408bf0cb259820feaff0000eaff0000c0050001000000c3020002'
        'dd090010180200001c0000'
    }
    qbss = {'bss_load_update_period': 50, 'chan_util_avg_period': 600}

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        mode = hostapd_constants.MODE_11N_MIXED
        obss_interval = 300
        ac_capabilities = None

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        mode = hostapd_constants.MODE_11AC_MIXED
        n_capabilities += [
            hostapd_constants.N_CAPABILITY_SGI40,
        ]

        if hostapd_config.ht40_plus_allowed(channel):
            n_capabilities.append(hostapd_constants.N_CAPABILITY_HT40_PLUS)
        elif hostapd_config.ht40_minus_allowed(channel):
            n_capabilities.append(hostapd_constants.N_CAPABILITY_HT40_MINUS)

        obss_interval = None
        ac_capabilities = [
            hostapd_constants.AC_CAPABILITY_RXLDPC,
            hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
            hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
            hostapd_constants.AC_CAPABILITY_RX_STBC_1,
            hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
            hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7
        ]

    additional_params = utils.merge_dicts(
        rates, vendor_elements, qbss,
        hostapd_constants.ENABLE_RRM_BEACON_REPORT,
        hostapd_constants.ENABLE_RRM_NEIGHBOR_REPORT,
        hostapd_constants.UAPSD_ENABLED)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=interface,
        mode=mode,
        force_wmm=True,
        beacon_interval=100,
        dtim_period=2,
        short_preamble=False,
        obss_interval=obss_interval,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        additional_parameters=additional_params)
    return config


def netgear_wndr3400(iface_wlan_2g=None,
                     iface_wlan_5g=None,
                     channel=None,
                     security=None,
                     ssid=None):
    # TODO(b/143104825): Permit RIFS on 5GHz once it is supported
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of what a Netgear WNDR3400 AP
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real WNDR3400:
        2.4GHz:
            Rates:
                WNDR3400:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48,
        5GHz:
            HT Info:
                WNDR3400: RIFS Permitted
                Simulated: RIFS Prohibited
        Both:
            HT Capab:
                A-MPDU
                    WNDR3400: MPDU Density 16
                    Simulated: MPDU Density 8
                Info
                    WNDR3400: Green Field supported
                    Simulated: Green Field not supported on Whirlwind.
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
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935,
        hostapd_constants.N_CAPABILITY_DSSS_CCK_40
    ]
    # WPS IE
    # Broadcom IE
    vendor_elements = {
        'vendor_elements':
        'dd310050f204104a0001101044000102104700108c403eb883e7e225ab139828703ade'
        'dc103c0001031049000600372a000120'
        'dd090010180200f0040000'
    }

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        obss_interval = 300
        n_capabilities.append(hostapd_constants.N_CAPABILITY_DSSS_CCK_40)

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        obss_interval = None
        n_capabilities.append(hostapd_constants.N_CAPABILITY_HT40_PLUS)

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
        dtim_period=2,
        short_preamble=False,
        obss_interval=obss_interval,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config