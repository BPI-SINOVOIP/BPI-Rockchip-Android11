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


def belkin_f9k1001v5(iface_wlan_2g=None,
                     channel=None,
                     security=None,
                     ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    """A simulated implementation of what a Belkin F9K1001v5 AP
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        channel: What channel to use.
        security: A security profile (None or WPA2).
        ssid: The network name.
    Returns:
        A hostapd config.
    Differences from real F9K1001v5:
        Rates:
            F9K1001v5:
                Supported: 1, 2, 5.5, 11, 18, 24, 36, 54
                Extended: 6, 9, 12, 48
            Simulated:
                Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                Extended: 24, 36, 48, 54
        HT Info:
            F9K1001v5:
                RIFS: Permitted
            Simulated:
                RIFS: Prohibited
        RSN Capabilities (w/ WPA2):
            F9K1001v5:
                RSN PTKSA Replay Counter Capab: 1
            Simulated:
                RSN PTKSA Replay Counter Capab: 16
    """
    if channel > 11:
        raise ValueError('The Belkin F9k1001v5 does not support 5Ghz. '
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
        hostapd_constants.N_CAPABILITY_SGI40,
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935,
        hostapd_constants.N_CAPABILITY_DSSS_CCK_40
    ]

    rates = additional_params = utils.merge_dicts(
        hostapd_constants.CCK_AND_OFDM_BASIC_RATES,
        hostapd_constants.CCK_AND_OFDM_DATA_RATES)

    # Broadcom IE
    # WPS IE
    vendor_elements = {
        'vendor_elements':
        'dd090010180200100c0000'
        'dd180050f204104a00011010440001021049000600372a000120'
    }

    additional_params = utils.merge_dicts(rates, vendor_elements)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=iface_wlan_2g,
        mode=hostapd_constants.MODE_11N_MIXED,
        force_wmm=True,
        beacon_interval=100,
        dtim_period=3,
        short_preamble=False,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config
