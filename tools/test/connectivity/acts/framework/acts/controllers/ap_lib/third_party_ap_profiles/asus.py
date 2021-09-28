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


def asus_rtac66u(iface_wlan_2g=None,
                 iface_wlan_5g=None,
                 channel=None,
                 security=None,
                 ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of an Asus RTAC66U AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile.  Must be none or WPA2 as this is what is
            supported by the RTAC66U.
        ssid: Network name
    Returns:
        A hostapd config
    Differences from real RTAC66U:
        2.4 GHz:
            Rates:
                RTAC66U:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
            HT Capab:
                Info
                    RTAC66U: Green Field supported
                    Simulated: Green Field not supported on Whirlwind.
        5GHz:
            VHT Capab:
                RTAC66U:
                    SU Beamformer Supported,
                    SU Beamformee Supported,
                    Beamformee STS Capability: 3,
                    Number of Sounding Dimensions: 3,
                    VHT Link Adaptation: Both
                Simulated:
                    Above are not supported on Whirlwind.
            VHT Operation Info:
                RTAC66U: Basic MCS Map (0x0000)
                Simulated: Basic MCS Map (0xfffc)
            VHT Tx Power Envelope:
                RTAC66U: Local Max Tx Pwr Constraint: 1.0 dBm
                Simulated: Local Max Tx Pwr Constraint: 23.0 dBm
        Both:
            HT Capab:
                A-MPDU
                    RTAC66U: MPDU Density 4
                    Simulated: MPDU Density 8
            HT Info:
                RTAC66U: RIFS Permitted
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
        hostapd_constants.N_CAPABILITY_LDPC,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935,
        hostapd_constants.N_CAPABILITY_DSSS_CCK_40,
        hostapd_constants.N_CAPABILITY_SGI20
    ]
    # WPS IE
    # Broadcom IE
    vendor_elements = {
        'vendor_elements':
        'dd310050f204104a00011010440001021047001093689729d373c26cb1563c6c570f33'
        'd7103c0001031049000600372a000120'
        'dd090010180200001c0000'
    }

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        mode = hostapd_constants.MODE_11N_MIXED
        ac_capabilities = None

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        mode = hostapd_constants.MODE_11AC_MIXED
        ac_capabilities = [
            hostapd_constants.AC_CAPABILITY_RXLDPC,
            hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
            hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
            hostapd_constants.AC_CAPABILITY_RX_STBC_1,
            hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
            hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7
        ]

    additional_params = utils.merge_dicts(rates, vendor_elements,
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
        dtim_period=3,
        short_preamble=False,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        additional_parameters=additional_params)

    return config


def asus_rtac86u(iface_wlan_2g=None,
                 iface_wlan_5g=None,
                 channel=None,
                 security=None,
                 ssid=None):
    """A simulated implementation of an Asus RTAC86U AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile.  Must be none or WPA2 as this is what is
            supported by the RTAC86U.
        ssid: Network name
    Returns:
        A hostapd config
    Differences from real RTAC86U:
        2.4GHz:
            Rates:
                RTAC86U:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
        5GHz:
            Country Code:
                Simulated: Has two country code IEs, one that matches
                the actual, and another explicit IE that was required for
                hostapd's 802.11d to work.
        Both:
            RSN Capabilities (w/ WPA2):
                RTAC86U:
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

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        mode = hostapd_constants.MODE_11G
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        spectrum_mgmt = False
        # Measurement Pilot Transmission IE
        vendor_elements = {'vendor_elements': '42020000'}

    # 5GHz
    else:
        interface = iface_wlan_5g
        mode = hostapd_constants.MODE_11A
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        spectrum_mgmt = True,
        # Country Information IE (w/ individual channel info)
        # TPC Report Transmit Power IE
        # Measurement Pilot Transmission IE
        vendor_elements = {
            'vendor_elements':
            '074255532024011e28011e2c011e30011e34011e38011e3c011e40011e64011e'
            '68011e6c011e70011e74011e84011e88011e8c011e95011e99011e9d011ea1011e'
            'a5011e'
            '23021300'
            '42020000'
        }

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
        dtim_period=3,
        short_preamble=False,
        spectrum_mgmt_required=spectrum_mgmt,
        additional_parameters=additional_params)
    return config


def asus_rtac5300(iface_wlan_2g=None,
                  iface_wlan_5g=None,
                  channel=None,
                  security=None,
                  ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    # TODO(b/144446076): Address non-whirlwind hardware capabilities.
    """A simulated implementation of an Asus RTAC5300 AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile.  Must be none or WPA2 as this is what is
            supported by the RTAC5300.
        ssid: Network name
    Returns:
        A hostapd config
    Differences from real RTAC5300:
        2.4GHz:
            Rates:
                RTAC86U:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
        5GHz:
            VHT Capab:
                RTAC5300:
                    SU Beamformer Supported,
                    SU Beamformee Supported,
                    Beamformee STS Capability: 4,
                    Number of Sounding Dimensions: 4,
                    MU Beamformer Supported,
                    VHT Link Adaptation: Both
                Simulated:
                    Above are not supported on Whirlwind.
            VHT Operation Info:
                RTAC5300: Basic MCS Map (0x0000)
                Simulated: Basic MCS Map (0xfffc)
            VHT Tx Power Envelope:
                RTAC5300: Local Max Tx Pwr Constraint: 1.0 dBm
                Simulated: Local Max Tx Pwr Constraint: 23.0 dBm
        Both:
            HT Capab:
                A-MPDU
                    RTAC5300: MPDU Density 4
                    Simulated: MPDU Density 8
            HT Info:
                RTAC5300: RIFS Permitted
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
    qbss = {'bss_load_update_period': 50, 'chan_util_avg_period': 600}
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_LDPC,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_SGI20
    ]

    # Broadcom IE
    vendor_elements = {'vendor_elements': 'dd090010180200009c0000'}

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        mode = hostapd_constants.MODE_11N_MIXED
        # AsusTek IE
        # Epigram 2.4GHz IE
        vendor_elements['vendor_elements'] += 'dd25f832e4010101020100031411b5' \
        '2fd437509c30b3d7f5cf5754fb125aed3b8507045aed3b85' \
        'dd1e00904c0418bf0cb2798b0faaff0000aaff0000c0050001000000c3020002'
        ac_capabilities = None

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        mode = hostapd_constants.MODE_11AC_MIXED
        # Epigram 5GHz IE
        vendor_elements['vendor_elements'] += 'dd0500904c0410'
        ac_capabilities = [
            hostapd_constants.AC_CAPABILITY_RXLDPC,
            hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
            hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
            hostapd_constants.AC_CAPABILITY_RX_STBC_1,
            hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
            hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7
        ]

    additional_params = utils.merge_dicts(rates, qbss, vendor_elements,
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
        dtim_period=3,
        short_preamble=False,
        n_capabilities=n_capabilities,
        ac_capabilities=ac_capabilities,
        vht_channel_width=vht_channel_width,
        additional_parameters=additional_params)
    return config


def asus_rtn56u(iface_wlan_2g=None,
                iface_wlan_5g=None,
                channel=None,
                security=None,
                ssid=None):
    """A simulated implementation of an Asus RTN56U AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile.  Must be none or WPA2 as this is what is
            supported by the RTN56U.
        ssid: Network name
    Returns:
        A hostapd config
    Differences from real RTN56U:
        2.4GHz:
            Rates:
                RTN56U:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
        Both:
            Fixed Parameters:
                RTN56U: APSD Implemented
                Simulated: APSD Not Implemented
            HT Capab:
                A-MPDU
                    RTN56U: MPDU Density 4
                    Simulated: MPDU Density 8
            RSN Capabilities (w/ WPA2):
                RTN56U:
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
    qbss = {'bss_load_update_period': 50, 'chan_util_avg_period': 600}
    n_capabilities = [
        hostapd_constants.N_CAPABILITY_SGI20,
        hostapd_constants.N_CAPABILITY_SGI40,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1
    ]

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        # Ralink Technology IE
        # US Country Code IE
        # AP Channel Report IEs (2)
        # WPS IE
        vendor_elements = {
            'vendor_elements':
            'dd07000c4307000000'
            '0706555320010b14'
            '33082001020304050607'
            '33082105060708090a0b'
            'dd270050f204104a000110104400010210470010bc329e001dd811b286011c872c'
            'd33448103c000101'
        }

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)
        # Ralink Technology IE
        # US Country Code IE
        vendor_elements = {
            'vendor_elements': 'dd07000c4307000000'
            '0706555320010b14'
        }

    additional_params = utils.merge_dicts(rates, vendor_elements, qbss,
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
        short_preamble=False,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config


def asus_rtn66u(iface_wlan_2g=None,
                iface_wlan_5g=None,
                channel=None,
                security=None,
                ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    """A simulated implementation of an Asus RTN66U AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        iface_wlan_5g: The 5Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile.  Must be none or WPA2 as this is what is
            supported by the RTN66U.
        ssid: Network name
    Returns:
        A hostapd config
    Differences from real RTN66U:
        2.4GHz:
            Rates:
                RTN66U:
                    Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                    Extended: 6, 9, 12, 48
                Simulated:
                    Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                    Extended: 24, 36, 48, 54
        Both:
            HT Info:
                RTN66U: RIFS Permitted
                Simulated: RIFS Prohibited
            HT Capab:
                Info:
                    RTN66U: Green Field supported
                    Simulated: Green Field not supported on Whirlwind.
                A-MPDU
                    RTN66U: MPDU Density 4
                    Simulated: MPDU Density 8
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
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_RX_STBC1,
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935
    ]
    # Broadcom IE
    vendor_elements = {'vendor_elements': 'dd090010180200001c0000'}

    # 2.4GHz
    if channel <= 11:
        interface = iface_wlan_2g
        rates.update(hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
        n_capabilities.append(hostapd_constants.N_CAPABILITY_DSSS_CCK_40)

    # 5GHz
    else:
        interface = iface_wlan_5g
        rates.update(hostapd_constants.OFDM_ONLY_BASIC_RATES)

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
        dtim_period=3,
        short_preamble=False,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config