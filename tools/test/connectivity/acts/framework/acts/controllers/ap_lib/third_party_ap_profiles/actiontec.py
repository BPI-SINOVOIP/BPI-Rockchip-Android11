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


def actiontec_pk5000(iface_wlan_2g=None,
                     channel=None,
                     security=None,
                     ssid=None):
    """A simulated implementation of what a Actiontec PK5000 AP
    Args:
        iface_wlan_2g: The 2.4 interface of the test AP.
        channel: What channel to use.  Only 2.4Ghz is supported for this profile
        security: A security profile.  Must be none or WPA2 as this is what is
            supported by the PK5000.
        ssid: Network name
    Returns:
        A hostapd config

    Differences from real pk5000:
        Supported Rates IE:
            PK5000: Supported: 1, 2, 5.5, 11
                    Extended: 6, 9, 12, 18, 24, 36, 48, 54
            Simulated: Supported: 1, 2, 5.5, 11, 6, 9, 12, 18
                       Extended: 24, 36, 48, 54
    """
    if channel > 11:
        # Technically this should be 14 but since the PK5000 is a US only AP,
        # 11 is the highest allowable channel.
        raise ValueError('The Actiontec PK5000 does not support 5Ghz. '
                         'Invalid channel (%s)' % channel)
    # Verify interface and security
    hostapd_utils.verify_interface(iface_wlan_2g,
                                   hostapd_constants.INTERFACE_2G_LIST)
    hostapd_utils.verify_security_mode(security,
                                       [None, hostapd_constants.WPA2])
    if security:
        hostapd_utils.verify_cipher(security,
                                    [hostapd_constants.WPA2_DEFAULT_CIPER])

    interface = iface_wlan_2g
    short_preamble = False
    force_wmm = False
    beacon_interval = 100
    dtim_period = 3
    # Sets the basic rates and supported rates of the PK5000
    additional_params = utils.merge_dicts(
        hostapd_constants.CCK_AND_OFDM_BASIC_RATES,
        hostapd_constants.CCK_AND_OFDM_DATA_RATES)

    config = hostapd_config.HostapdConfig(
        ssid=ssid,
        channel=channel,
        hidden=False,
        security=security,
        interface=interface,
        mode=hostapd_constants.MODE_11G,
        force_wmm=force_wmm,
        beacon_interval=beacon_interval,
        dtim_period=dtim_period,
        short_preamble=short_preamble,
        additional_parameters=additional_params)

    return config


def actiontec_mi424wr(iface_wlan_2g=None,
                      channel=None,
                      security=None,
                      ssid=None):
    # TODO(b/143104825): Permit RIFS once it is supported
    """A simulated implementation of an Actiontec MI424WR AP.
    Args:
        iface_wlan_2g: The 2.4Ghz interface of the test AP.
        channel: What channel to use (2.4Ghz or 5Ghz).
        security: A security profile.
        ssid: The network name.
    Returns:
        A hostapd config.

    Differences from real MI424WR:
        HT Capabilities:
            MI424WR:
                HT Rx STBC: Support for 1, 2, and 3
            Simulated:
                HT Rx STBC: Support for 1
        HT Information:
            MI424WR:
                RIFS: Premitted
            Simulated:
                RIFS: Prohibited
    """
    if channel > 11:
        raise ValueError('The Actiontec MI424WR does not support 5Ghz. '
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
        hostapd_constants.N_CAPABILITY_TX_STBC,
        hostapd_constants.N_CAPABILITY_DSSS_CCK_40,
        hostapd_constants.N_CAPABILITY_RX_STBC1
    ]
    rates = utils.merge_dicts(hostapd_constants.CCK_AND_OFDM_DATA_RATES,
                              hostapd_constants.CCK_AND_OFDM_BASIC_RATES)
    # Proprietary Atheros Communication: Adv Capability IE
    # Proprietary Atheros Communication: Unknown IE
    # Country Info: US Only IE
    vendor_elements = {
        'vendor_elements':
        'dd0900037f01010000ff7f'
        'dd0a00037f04010000000000'
        '0706555320010b1b'
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
        dtim_period=1,
        short_preamble=True,
        n_capabilities=n_capabilities,
        additional_parameters=additional_params)

    return config
