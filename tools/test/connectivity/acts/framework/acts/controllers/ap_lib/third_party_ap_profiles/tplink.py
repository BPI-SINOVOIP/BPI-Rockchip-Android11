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


def tplink_archerc5(iface_wlan_2g=None,
                    iface_wlan_5g=None,
                    channel=None,
                    security=None,
                    ssid=None):
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of an TPLink ArcherC5 AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real ArcherC5:
        2.4GHz:
            Rates:
                ArcherC5:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
            HT Capab:
                Info:
                    ArcherC5: Green Field supported
                    Simulated: Green Field not supported on Whirlwind.
        5GHz:
            VHT Capab:
                ArcherC5:
                    SU Beamformer Supported,
                    SU Beamformee Supported,
                    Beamformee STS Capability: 3,
                    Number of Sounding Dimensions: 3,
                    VHT Link Adaptation: Both
                Simulated:
                    Above are not supported on Whirlwind.
            VHT Operation Info:
                ArcherC5: Basic MCS Map (0x0000)
                Simulated: Basic MCS Map (0xfffc)
            VHT Tx Power Envelope:
                ArcherC5: Local Max Tx Pwr Constraint: 1.0 dBm
                Simulated: Local Max Tx Pwr Constraint: 23.0 dBm
        Both:
            HT Capab:
                A-MPDU
                    ArcherC5: MPDU Density 4
                    Simulated: MPDU Density 8
            HT Info:
                ArcherC5: RIFS Permitted
                Simulated: RIFS Prohibited
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
    vht_channel_width = 20
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935
    ]
    # WPS IE
    # Broadcom IE
    vendor_elements = {
        'vendor_elements':
        'dd310050f204104a000110104400010210470010d96c7efc2f8938f1efbd6e5148bfa8'
        '12103c0001031049000600372a000120'
        'dd090010180200001c0000'
    }
    qbss = {'bss_load_update_period': 50, 'chan_util_avg_period': 600}

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        short_preamble = True
        mode = hostapd_constants.MODE_11N_MIXED
        n_capabilities.append(hostapd_constants.N_CAPABILITY_DSSS_CCK_40)
        ac_capabilities = None

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        short_preamble = False
        mode = hostapd_constants.MODE_11AC_MIXED
        n_capabilities.append(hostapd_constants.N_CAPABILITY_LDPC)
        ac_capabilities = [
            hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
            hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
            hostapd_constants.AC_CAPABILITY_RXLDPC,
            hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
            hostapd_constants.AC_CAPABILITY_RX_STBC_1,
            hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7,
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
        dtim_period=1,
        short_preamble=short_preamble,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        additional_parameters=additional_params)
    return config


def tplink_archerc7(iface_wlan_2g=None,
                    iface_wlan_5g=None,
                    channel=None,
                    security=None,
                    ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    """A simulated implementation of an TPLink ArcherC7 AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real ArcherC7:
        5GHz:
            Country Code:
                Simulated: Has two country code IEs, one that matches
                the actual, and another explicit IE that was required for
                hostapd's 802.11d to work.
        Both:
            HT Info:
                ArcherC7: RIFS Permitted
                Simulated: RIFS Prohibited
            RSN Capabilities (w/ WPA2):
                ArcherC7:
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
    vht_channel_width = 80
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_LDPC,
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1
    ]
    # Atheros IE
    # WPS IE
    vendor_elements = {
        'vendor_elements':
        'dd0900037f01010000ff7f'
        'dd180050f204104a00011010440001021049000600372a000120'
    }

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        short_preamble = True
        mode = hostapd_constants.MODE_11N_MIXED
        spectrum_mgmt = False
        pwr_constraint = {}
        ac_capabilities = None
        vht_channel_width = None

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        short_preamble = False
        mode = hostapd_constants.MODE_11AC_MIXED
        spectrum_mgmt = True
        # Country Information IE (w/ individual channel info)
        vendor_elements['vendor_elements'] += (
            '074255532024011e28011e2c011e30'
            '011e3401173801173c01174001176401176801176c0117700117740117840117'
            '8801178c011795011e99011e9d011ea1011ea5011e')
        pwr_constraint = {'local_pwr_constraint': 3}
        n_capabilities += [
            hostapd_constants.N_CAPABILITY_SGI40,
            hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935
        ]

        if hostapd_config.ht40_plus_allowed(channel):
            n_capabilities.append(hostapd_constants.N_CAPABILITY_HT40_PLUS)
        elif hostapd_config.ht40_minus_allowed(channel):
            n_capabilities.append(hostapd_constants.N_CAPABILITY_HT40_MINUS)

        ac_capabilities = [
            hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
            hostapd_constants.AC_CAPABILITY_RXLDPC,
            hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
            hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
            hostapd_constants.AC_CAPABILITY_RX_STBC_1,
            hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7,
            hostapd_constants.AC_CAPABILITY_RX_ANTENNA_PATTERN,
            hostapd_constants.AC_CAPABILITY_TX_ANTENNA_PATTERN
        ]

    additional_params = utils.merge_dicts(rates, vendor_elements,
                                          hostapd_constants.UAPSD_ENABLED,
                                          pwr_constraint)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=interface,
        mode=mode,
        force_wmm=True,
        beacon_interval=100,
        dtim_period=1,
        short_preamble=short_preamble,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        spectrum_mgmt_required=spectrum_mgmt,
        additional_parameters=additional_params)
    return config


def tplink_c1200(iface_wlan_2g=None,
                 iface_wlan_5g=None,
                 channel=None,
                 security=None,
                 ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of an TPLink C1200 AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5GHz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real C1200:
        2.4GHz:
            Rates:
                C1200:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
            HT Capab:
                Info:
                    C1200: Green Field supported
                    Simulated: Green Field not supported on Whirlwind.
        5GHz:
            VHT Operation Info:
                C1200: Basic MCS Map (0x0000)
                Simulated: Basic MCS Map (0xfffc)
            VHT Tx Power Envelope:
                C1200: Local Max Tx Pwr Constraint: 7.0 dBm
                Simulated: Local Max Tx Pwr Constraint: 23.0 dBm
        Both:
            HT Info:
                C1200: RIFS Permitted
                Simulated: RIFS Prohibited
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
    vht_channel_width = 20
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935
    ]
    # WPS IE
    # Broadcom IE
    vendor_elements = {
        'vendor_elements':
        'dd350050f204104a000110104400010210470010000000000000000000000000000000'
        '00103c0001031049000a00372a00012005022688'
        'dd090010180200000c0000'
    }

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        short_preamble = True
        mode = hostapd_constants.MODE_11N_MIXED
        ac_capabilities = None

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        short_preamble = False
        mode = hostapd_constants.MODE_11AC_MIXED
        n_capabilities.append(hostapd_constants.N_CAPABILITY_LDPC)
        ac_capabilities = [
            hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
            hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
            hostapd_constants.AC_CAPABILITY_RXLDPC,
            hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
            hostapd_constants.AC_CAPABILITY_RX_STBC_1,
            hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7,
        ]

    additional_params = utils.merge_dicts(
        rates, vendor_elements, hostapd_constants.ENABLE_RRM_BEACON_REPORT,
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
        dtim_period=1,
        short_preamble=short_preamble,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        additional_parameters=additional_params)
    return config


def tplink_tlwr940n(iface_wlan_2g=None,
                    channel=None,
                    security=None,
                    ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    """A simulated implementation of an TPLink TLWR940N AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real TLWR940N:
        HT Info:
            TLWR940N: RIFS Permitted
            Simulated: RIFS Prohibited
        RSN Capabilities (w/ WPA2):
            TLWR940N:
                RSN PTKSA Replay Counter Capab: 1
            Simulated:
                RSN PTKSA Replay Counter Capab: 16
    """
    if channel > 11:
        raise ValueError('The mock TP-Link TLWR940N does not support 5Ghz. '
                         'Invalid channel (%s)' % channel)
    # Verify interface and security
    hostapd_utils.verify_interface(iface_wlan_2g,
                                   hostapd_constants.INTERFACE_2G_LIST)
    hostapd_utils.verify_security_mode(security,
                                       [None, hostapd_constants.WPA2])
    if security:
        hostapd_utils.verify_cipher(security,
                                    [hostapd_constants.WPA2_DEFAULT_CIPER])

    n_capabilities = [
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1
    ]

    rates = utils.merge_dicts(hostapd_constants.CCK_AND_OFDM_BASIC_RATES,
                              hostapd_constants.CCK_AND_OFDM_DATA_RATES)

    # Atheros Communications, Inc. IE
    # WPS IE
    vendor_elements = {
        'vendor_elements':
        'dd0900037f01010000ff7f'
        'dd260050f204104a0001101044000102104900140024e2600200010160000002000160'
        '0100020001'
    }

    additional_params = utils.merge_dicts(rates, vendor_elements,
                                          hostapd_constants.UAPSD_ENABLED)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=iface_wlan_2g,
        mode=hostapd_constants.MODE_11N_MIXED,
        force_wmm=True,
        beacon_interval=100,
        dtim_period=1,
        short_preamble=True,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config
