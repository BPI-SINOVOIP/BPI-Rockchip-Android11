#   Copyright 2017 - The Android Open Source Project
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

import acts.controllers.ap_lib.third_party_ap_profiles.actiontec as actiontec
import acts.controllers.ap_lib.third_party_ap_profiles.asus as asus
import acts.controllers.ap_lib.third_party_ap_profiles.belkin as belkin
import acts.controllers.ap_lib.third_party_ap_profiles.linksys as linksys
import acts.controllers.ap_lib.third_party_ap_profiles.netgear as netgear
import acts.controllers.ap_lib.third_party_ap_profiles.securifi as securifi
import acts.controllers.ap_lib.third_party_ap_profiles.tplink as tplink

from acts.controllers.ap_lib import hostapd_config
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_utils


def _get_or_default(var, default_value):
    """Check variable and return non-null value.

   Args:
        var: Any variable.
        default_value: Value to return if the var is None.

   Returns:
        Variable value if not None, default value otherwise.
    """
    return var if var is not None else default_value


def create_ap_preset(profile_name='whirlwind',
                     iface_wlan_2g=None,
                     iface_wlan_5g=None,
                     channel=None,
                     mode=None,
                     frequency=None,
                     security=None,
                     ssid=None,
                     hidden=None,
                     dtim_period=None,
                     frag_threshold=None,
                     rts_threshold=None,
                     force_wmm=None,
                     beacon_interval=None,
                     short_preamble=None,
                     n_capabilities=None,
                     ac_capabilities=None,
                     vht_bandwidth=None,
                     bss_settings=[]):
    """AP preset config generator.  This a wrapper for hostapd_config but
       but supplies the default settings for the preset that is selected.

        You may specify channel or frequency, but not both.  Both options
        are checked for validity (i.e. you can't specify an invalid channel
        or a frequency that will not be accepted).

    Args:
        profile_name: The name of the device want the preset for.
                      Options: whirlwind
        channel: int, channel number.
        dtim: int, DTIM value of the AP, default is 2.
        frequency: int, frequency of channel.
        security: Security, the secuirty settings to use.
        ssid: string, The name of the ssid to brodcast.
        vht_bandwidth: VHT bandwidth for 11ac operation.
        bss_settings: The settings for all bss.
        iface_wlan_2g: the wlan 2g interface name of the AP.
        iface_wlan_5g: the wlan 5g interface name of the AP.
        mode: The hostapd 802.11 mode of operation.
        ssid: The ssid for the wireless network.
        hidden: Whether to include the ssid in the beacons.
        dtim_period: The dtim period for the BSS
        frag_threshold: Max size of packet before fragmenting the packet.
        rts_threshold: Max size of packet before requiring protection for
            rts/cts or cts to self.
        n_capabilities: 802.11n capabilities for for BSS to advertise.
        ac_capabilities: 802.11ac capabilities for for BSS to advertise.

    Returns: A hostapd_config object that can be used by the hostapd object.
    """

    # Verify interfaces
    hostapd_utils.verify_interface(iface_wlan_2g,
                                   hostapd_constants.INTERFACE_2G_LIST)
    hostapd_utils.verify_interface(iface_wlan_5g,
                                   hostapd_constants.INTERFACE_5G_LIST)

    if channel:
        frequency = hostapd_config.get_frequency_for_channel(channel)
    elif frequency:
        channel = hostapd_config.get_channel_for_frequency(frequency)
    else:
        raise ValueError('Specify either frequency or channel.')

    if profile_name == 'whirlwind':
        # profile indicates phy mode is 11bgn for 2.4Ghz or 11acn for 5Ghz
        hidden = _get_or_default(hidden, False)
        force_wmm = _get_or_default(force_wmm, True)
        beacon_interval = _get_or_default(beacon_interval, 100)
        short_preamble = _get_or_default(short_preamble, True)
        dtim_period = _get_or_default(dtim_period, 2)
        frag_threshold = _get_or_default(frag_threshold, 2346)
        rts_threshold = _get_or_default(rts_threshold, 2347)
        if frequency < 5000:
            interface = iface_wlan_2g
            mode = _get_or_default(mode, hostapd_constants.MODE_11N_MIXED)
            n_capabilities = _get_or_default(n_capabilities, [
                hostapd_constants.N_CAPABILITY_LDPC,
                hostapd_constants.N_CAPABILITY_SGI20,
                hostapd_constants.N_CAPABILITY_SGI40,
                hostapd_constants.N_CAPABILITY_TX_STBC,
                hostapd_constants.N_CAPABILITY_RX_STBC1,
                hostapd_constants.N_CAPABILITY_DSSS_CCK_40
            ])
            config = hostapd_config.HostapdConfig(
                ssid=ssid,
                hidden=hidden,
                security=security,
                interface=interface,
                mode=mode,
                force_wmm=force_wmm,
                beacon_interval=beacon_interval,
                dtim_period=dtim_period,
                short_preamble=short_preamble,
                frequency=frequency,
                n_capabilities=n_capabilities,
                frag_threshold=frag_threshold,
                rts_threshold=rts_threshold,
                bss_settings=bss_settings)
        else:
            interface = iface_wlan_5g
            vht_bandwidth = _get_or_default(vht_bandwidth, 80)
            mode = _get_or_default(mode, hostapd_constants.MODE_11AC_MIXED)
            if hostapd_config.ht40_plus_allowed(channel):
                extended_channel = hostapd_constants.N_CAPABILITY_HT40_PLUS
            elif hostapd_config.ht40_minus_allowed(channel):
                extended_channel = hostapd_constants.N_CAPABILITY_HT40_MINUS
            # Channel 165 operates in 20MHz with n or ac modes.
            if channel == 165:
                mode = hostapd_constants.MODE_11N_MIXED
                extended_channel = hostapd_constants.N_CAPABILITY_HT20
            # Define the n capability vector for 20 MHz and higher bandwidth
            if not vht_bandwidth:
                pass
            elif vht_bandwidth >= 40:
                n_capabilities = _get_or_default(n_capabilities, [
                    hostapd_constants.N_CAPABILITY_LDPC, extended_channel,
                    hostapd_constants.N_CAPABILITY_SGI20,
                    hostapd_constants.N_CAPABILITY_SGI40,
                    hostapd_constants.N_CAPABILITY_TX_STBC,
                    hostapd_constants.N_CAPABILITY_RX_STBC1
                ])
            else:
                n_capabilities = _get_or_default(n_capabilities, [
                    hostapd_constants.N_CAPABILITY_LDPC,
                    hostapd_constants.N_CAPABILITY_SGI20,
                    hostapd_constants.N_CAPABILITY_SGI40,
                    hostapd_constants.N_CAPABILITY_TX_STBC,
                    hostapd_constants.N_CAPABILITY_RX_STBC1,
                    hostapd_constants.N_CAPABILITY_HT20
                ])
            ac_capabilities = _get_or_default(ac_capabilities, [
                hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
                hostapd_constants.AC_CAPABILITY_RXLDPC,
                hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
                hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
                hostapd_constants.AC_CAPABILITY_RX_STBC_1,
                hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7,
                hostapd_constants.AC_CAPABILITY_RX_ANTENNA_PATTERN,
                hostapd_constants.AC_CAPABILITY_TX_ANTENNA_PATTERN
            ])
            config = hostapd_config.HostapdConfig(
                ssid=ssid,
                hidden=hidden,
                security=security,
                interface=interface,
                mode=mode,
                force_wmm=force_wmm,
                vht_channel_width=vht_bandwidth,
                beacon_interval=beacon_interval,
                dtim_period=dtim_period,
                short_preamble=short_preamble,
                frequency=frequency,
                frag_threshold=frag_threshold,
                rts_threshold=rts_threshold,
                n_capabilities=n_capabilities,
                ac_capabilities=ac_capabilities,
                bss_settings=bss_settings)
    elif profile_name == 'whirlwind_11ab_legacy':
        if frequency < 5000:
            mode = hostapd_constants.MODE_11B
        else:
            mode = hostapd_constants.MODE_11A

        config = create_ap_preset(iface_wlan_2g=iface_wlan_2g,
                                  iface_wlan_5g=iface_wlan_5g,
                                  ssid=ssid,
                                  channel=channel,
                                  mode=mode,
                                  security=security,
                                  hidden=hidden,
                                  force_wmm=force_wmm,
                                  beacon_interval=beacon_interval,
                                  short_preamble=short_preamble,
                                  dtim_period=dtim_period,
                                  rts_threshold=rts_threshold,
                                  frag_threshold=frag_threshold,
                                  n_capabilities=[],
                                  ac_capabilities=[],
                                  vht_bandwidth=None)
    elif profile_name == 'whirlwind_11ag_legacy':
        if frequency < 5000:
            mode = hostapd_constants.MODE_11G
        else:
            mode = hostapd_constants.MODE_11A

        config = create_ap_preset(iface_wlan_2g=iface_wlan_2g,
                                  iface_wlan_5g=iface_wlan_5g,
                                  ssid=ssid,
                                  channel=channel,
                                  mode=mode,
                                  security=security,
                                  hidden=hidden,
                                  force_wmm=force_wmm,
                                  beacon_interval=beacon_interval,
                                  short_preamble=short_preamble,
                                  dtim_period=dtim_period,
                                  rts_threshold=rts_threshold,
                                  frag_threshold=frag_threshold,
                                  n_capabilities=[],
                                  ac_capabilities=[],
                                  vht_bandwidth=None)
    elif profile_name == 'mistral':
        hidden = _get_or_default(hidden, False)
        force_wmm = _get_or_default(force_wmm, True)
        beacon_interval = _get_or_default(beacon_interval, 100)
        short_preamble = _get_or_default(short_preamble, True)
        dtim_period = _get_or_default(dtim_period, 2)
        frag_threshold = None
        rts_threshold = None

        # Google IE
        # Country Code IE ('us' lowercase)
        vendor_elements = {
            'vendor_elements':
            'dd0cf4f5e80505ff0000ffffffff'
            '070a75732024041e95051e00'
        }
        default_configs = {'bridge': 'br-lan', 'iapp_interface': 'br-lan'}

        if frequency < 5000:
            interface = iface_wlan_2g
            mode = _get_or_default(mode, hostapd_constants.MODE_11N_MIXED)
            n_capabilities = _get_or_default(n_capabilities, [
                hostapd_constants.N_CAPABILITY_LDPC,
                hostapd_constants.N_CAPABILITY_SGI20,
                hostapd_constants.N_CAPABILITY_SGI40,
                hostapd_constants.N_CAPABILITY_TX_STBC,
                hostapd_constants.N_CAPABILITY_RX_STBC1,
                hostapd_constants.N_CAPABILITY_DSSS_CCK_40
            ])

            additional_params = utils.merge_dicts(
                vendor_elements, hostapd_constants.ENABLE_RRM_BEACON_REPORT,
                hostapd_constants.ENABLE_RRM_NEIGHBOR_REPORT, default_configs)
            config = hostapd_config.HostapdConfig(
                ssid=ssid,
                hidden=hidden,
                security=security,
                interface=interface,
                mode=mode,
                force_wmm=force_wmm,
                beacon_interval=beacon_interval,
                dtim_period=dtim_period,
                short_preamble=short_preamble,
                frequency=frequency,
                n_capabilities=n_capabilities,
                frag_threshold=frag_threshold,
                rts_threshold=rts_threshold,
                bss_settings=bss_settings,
                additional_parameters=additional_params,
                set_ap_defaults_profile=profile_name)
        else:
            interface = iface_wlan_5g
            vht_bandwidth = _get_or_default(vht_bandwidth, 80)
            mode = _get_or_default(mode, hostapd_constants.MODE_11AC_MIXED)
            if hostapd_config.ht40_plus_allowed(channel):
                extended_channel = hostapd_constants.N_CAPABILITY_HT40_PLUS
            elif hostapd_config.ht40_minus_allowed(channel):
                extended_channel = hostapd_constants.N_CAPABILITY_HT40_MINUS
            # Channel 165 operates in 20MHz with n or ac modes.
            if channel == 165:
                mode = hostapd_constants.MODE_11N_MIXED
                extended_channel = hostapd_constants.N_CAPABILITY_HT20
            if vht_bandwidth >= 40:
                n_capabilities = _get_or_default(n_capabilities, [
                    hostapd_constants.N_CAPABILITY_LDPC, extended_channel,
                    hostapd_constants.N_CAPABILITY_SGI20,
                    hostapd_constants.N_CAPABILITY_SGI40,
                    hostapd_constants.N_CAPABILITY_TX_STBC,
                    hostapd_constants.N_CAPABILITY_RX_STBC1
                ])
            else:
                n_capabilities = _get_or_default(n_capabilities, [
                    hostapd_constants.N_CAPABILITY_LDPC,
                    hostapd_constants.N_CAPABILITY_SGI20,
                    hostapd_constants.N_CAPABILITY_SGI40,
                    hostapd_constants.N_CAPABILITY_TX_STBC,
                    hostapd_constants.N_CAPABILITY_RX_STBC1,
                    hostapd_constants.N_CAPABILITY_HT20
                ])
            ac_capabilities = _get_or_default(ac_capabilities, [
                hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
                hostapd_constants.AC_CAPABILITY_RXLDPC,
                hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
                hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
                hostapd_constants.AC_CAPABILITY_RX_STBC_1,
                hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7,
                hostapd_constants.AC_CAPABILITY_RX_ANTENNA_PATTERN,
                hostapd_constants.AC_CAPABILITY_TX_ANTENNA_PATTERN,
                hostapd_constants.AC_CAPABILITY_SU_BEAMFORMER,
                hostapd_constants.AC_CAPABILITY_SU_BEAMFORMEE,
                hostapd_constants.AC_CAPABILITY_MU_BEAMFORMER,
                hostapd_constants.AC_CAPABILITY_SOUNDING_DIMENSION_4,
                hostapd_constants.AC_CAPABILITY_BF_ANTENNA_4
            ])

            additional_params = utils.merge_dicts(
                vendor_elements, hostapd_constants.ENABLE_RRM_BEACON_REPORT,
                hostapd_constants.ENABLE_RRM_NEIGHBOR_REPORT, default_configs)
            config = hostapd_config.HostapdConfig(
                ssid=ssid,
                hidden=hidden,
                security=security,
                interface=interface,
                mode=mode,
                force_wmm=force_wmm,
                vht_channel_width=vht_bandwidth,
                beacon_interval=beacon_interval,
                dtim_period=dtim_period,
                short_preamble=short_preamble,
                frequency=frequency,
                frag_threshold=frag_threshold,
                rts_threshold=rts_threshold,
                n_capabilities=n_capabilities,
                ac_capabilities=ac_capabilities,
                bss_settings=bss_settings,
                additional_parameters=additional_params,
                set_ap_defaults_profile=profile_name)
    elif profile_name == 'actiontec_pk5000':
        config = actiontec.actiontec_pk5000(iface_wlan_2g=iface_wlan_2g,
                                            channel=channel,
                                            ssid=ssid,
                                            security=security)
    elif profile_name == 'actiontec_mi424wr':
        config = actiontec.actiontec_mi424wr(iface_wlan_2g=iface_wlan_2g,
                                             channel=channel,
                                             ssid=ssid,
                                             security=security)
    elif profile_name == 'asus_rtac66u':
        config = asus.asus_rtac66u(iface_wlan_2g=iface_wlan_2g,
                                   iface_wlan_5g=iface_wlan_5g,
                                   channel=channel,
                                   ssid=ssid,
                                   security=security)
    elif profile_name == 'asus_rtac86u':
        config = asus.asus_rtac86u(iface_wlan_2g=iface_wlan_2g,
                                   iface_wlan_5g=iface_wlan_5g,
                                   channel=channel,
                                   ssid=ssid,
                                   security=security)
    elif profile_name == 'asus_rtac5300':
        config = asus.asus_rtac5300(iface_wlan_2g=iface_wlan_2g,
                                    iface_wlan_5g=iface_wlan_5g,
                                    channel=channel,
                                    ssid=ssid,
                                    security=security)
    elif profile_name == 'asus_rtn56u':
        config = asus.asus_rtn56u(iface_wlan_2g=iface_wlan_2g,
                                  iface_wlan_5g=iface_wlan_5g,
                                  channel=channel,
                                  ssid=ssid,
                                  security=security)
    elif profile_name == 'asus_rtn66u':
        config = asus.asus_rtn66u(iface_wlan_2g=iface_wlan_2g,
                                  iface_wlan_5g=iface_wlan_5g,
                                  channel=channel,
                                  ssid=ssid,
                                  security=security)
    elif profile_name == 'belkin_f9k1001v5':
        config = belkin.belkin_f9k1001v5(iface_wlan_2g=iface_wlan_2g,
                                         channel=channel,
                                         ssid=ssid,
                                         security=security)
    elif profile_name == 'linksys_ea4500':
        config = linksys.linksys_ea4500(iface_wlan_2g=iface_wlan_2g,
                                        iface_wlan_5g=iface_wlan_5g,
                                        channel=channel,
                                        ssid=ssid,
                                        security=security)
    elif profile_name == 'linksys_ea9500':
        config = linksys.linksys_ea9500(iface_wlan_2g=iface_wlan_2g,
                                        iface_wlan_5g=iface_wlan_5g,
                                        channel=channel,
                                        ssid=ssid,
                                        security=security)
    elif profile_name == 'linksys_wrt1900acv2':
        config = linksys.linksys_wrt1900acv2(iface_wlan_2g=iface_wlan_2g,
                                             iface_wlan_5g=iface_wlan_5g,
                                             channel=channel,
                                             ssid=ssid,
                                             security=security)
    elif profile_name == 'netgear_r7000':
        config = netgear.netgear_r7000(iface_wlan_2g=iface_wlan_2g,
                                       iface_wlan_5g=iface_wlan_5g,
                                       channel=channel,
                                       ssid=ssid,
                                       security=security)
    elif profile_name == 'netgear_wndr3400':
        config = netgear.netgear_wndr3400(iface_wlan_2g=iface_wlan_2g,
                                          iface_wlan_5g=iface_wlan_5g,
                                          channel=channel,
                                          ssid=ssid,
                                          security=security)
    elif profile_name == 'securifi_almond':
        config = securifi.securifi_almond(iface_wlan_2g=iface_wlan_2g,
                                          channel=channel,
                                          ssid=ssid,
                                          security=security)
    elif profile_name == 'tplink_archerc5':
        config = tplink.tplink_archerc5(iface_wlan_2g=iface_wlan_2g,
                                        iface_wlan_5g=iface_wlan_5g,
                                        channel=channel,
                                        ssid=ssid,
                                        security=security)
    elif profile_name == 'tplink_archerc7':
        config = tplink.tplink_archerc7(iface_wlan_2g=iface_wlan_2g,
                                        iface_wlan_5g=iface_wlan_5g,
                                        channel=channel,
                                        ssid=ssid,
                                        security=security)
    elif profile_name == 'tplink_c1200':
        config = tplink.tplink_c1200(iface_wlan_2g=iface_wlan_2g,
                                     iface_wlan_5g=iface_wlan_5g,
                                     channel=channel,
                                     ssid=ssid,
                                     security=security)
    elif profile_name == 'tplink_tlwr940n':
        config = tplink.tplink_tlwr940n(iface_wlan_2g=iface_wlan_2g,
                                        channel=channel,
                                        ssid=ssid,
                                        security=security)
    else:
        raise ValueError('Invalid ap model specified (%s)' % profile_name)

    return config
