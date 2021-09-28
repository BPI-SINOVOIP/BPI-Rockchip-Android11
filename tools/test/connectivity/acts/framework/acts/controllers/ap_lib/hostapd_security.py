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

import collections
import string

from acts.controllers.ap_lib import hostapd_constants


class Security(object):
    """The Security class for hostapd representing some of the security
       settings that are allowed in hostapd.  If needed more can be added.
    """
    def __init__(self,
                 security_mode=None,
                 password=None,
                 wpa_cipher=hostapd_constants.WPA_DEFAULT_CIPHER,
                 wpa2_cipher=hostapd_constants.WPA2_DEFAULT_CIPER,
                 wpa_group_rekey=hostapd_constants.WPA_GROUP_KEY_ROTATION_TIME,
                 wpa_strict_rekey=hostapd_constants.WPA_STRICT_REKEY_DEFAULT,
                 wep_default_key=hostapd_constants.WEP_DEFAULT_KEY,
                 radius_server_ip=None,
                 radius_server_port=None,
                 radius_server_secret=None):
        """Gather all of the security settings for WPA-PSK.  This could be
           expanded later.

        Args:
            security_mode: Type of security modes.
                           Options: wep, wpa, wpa2, wpa/wpa2, wpa3
            password: The PSK or passphrase for the security mode.
            wpa_cipher: The cipher to be used for wpa.
                        Options: TKIP, CCMP, TKIP CCMP
                        Default: TKIP
            wpa2_cipher: The cipher to be used for wpa2.
                         Options: TKIP, CCMP, TKIP CCMP
                         Default: CCMP
            wpa_group_rekey: How often to refresh the GTK regardless of network
                             changes.
                             Options: An integrer in seconds, None
                             Default: 600 seconds
            wpa_strict_rekey: Whether to do a group key update when client
                              leaves the network or not.
                              Options: True, False
                              Default: True
            wep_default_key: The wep key number to use when transmitting.
            radius_server_ip: Radius server IP for Enterprise auth.
            radius_server_port: Radius server port for Enterprise auth.
            radius_server_secret: Radius server secret for Enterprise auth.
        """
        self.wpa_cipher = wpa_cipher
        self.wpa2_cipher = wpa2_cipher
        self.wpa3 = security_mode == hostapd_constants.WPA3_STRING
        self.wpa_group_rekey = wpa_group_rekey
        self.wpa_strict_rekey = wpa_strict_rekey
        self.wep_default_key = wep_default_key
        self.radius_server_ip = radius_server_ip
        self.radius_server_port = radius_server_port
        self.radius_server_secret = radius_server_secret
        if security_mode == hostapd_constants.WPA_STRING:
            security_mode = hostapd_constants.WPA1
        # Both wpa2 and wpa3 use hostapd security mode 2, and are distinguished
        # by their key management field (WPA-PSK and SAE, respectively)
        elif (security_mode == hostapd_constants.WPA2_STRING
              or security_mode == hostapd_constants.WPA3_STRING):
            security_mode = hostapd_constants.WPA2
        elif security_mode == hostapd_constants.WPA_MIXED_STRING:
            security_mode = hostapd_constants.MIXED
        elif security_mode == hostapd_constants.WEP_STRING:
            security_mode = hostapd_constants.WEP
        elif security_mode == hostapd_constants.ENT_STRING:
            security_mode = hostapd_constants.ENT
        else:
            security_mode = None
        self.security_mode = security_mode
        if password:
            if security_mode == hostapd_constants.WEP:
                if len(password) in hostapd_constants.WEP_STR_LENGTH:
                    self.password = '"%s"' % password
                elif len(password) in hostapd_constants.WEP_HEX_LENGTH and all(
                        c in string.hexdigits for c in password):
                    self.password = password
                else:
                    raise ValueError(
                        'WEP key must be a hex string of %s characters' %
                        hostapd_constants.WEP_HEX_LENGTH)
            else:
                if len(password) < hostapd_constants.MIN_WPA_PSK_LENGTH or len(
                        password) > hostapd_constants.MAX_WPA_PSK_LENGTH:
                    raise ValueError(
                        'Password must be a minumum of %s characters and a maximum of %s'
                        % (hostapd_constants.MIN_WPA_PSK_LENGTH,
                           hostapd_constants.MAX_WPA_PSK_LENGTH))
                else:
                    self.password = password

    def generate_dict(self):
        """Returns: an ordered dictionary of settings"""
        settings = collections.OrderedDict()
        if self.security_mode is not None:
            if self.security_mode == hostapd_constants.WEP:
                settings['wep_default_key'] = self.wep_default_key
                settings['wep_key' + str(self.wep_default_key)] = self.password
            elif self.security_mode == hostapd_constants.ENT:
                settings['auth_server_addr'] = self.radius_server_ip
                settings['auth_server_port'] = self.radius_server_port
                settings[
                    'auth_server_shared_secret'] = self.radius_server_secret
                settings['wpa_key_mgmt'] = hostapd_constants.ENT_KEY_MGMT
                settings['ieee8021x'] = hostapd_constants.IEEE8021X
                settings['wpa'] = hostapd_constants.WPA2
            else:
                settings['wpa'] = self.security_mode
                if len(self.password) == hostapd_constants.MAX_WPA_PSK_LENGTH:
                    settings['wpa_psk'] = self.password
                else:
                    settings['wpa_passphrase'] = self.password
                if self.security_mode == hostapd_constants.MIXED:
                    settings['wpa_pairwise'] = self.wpa_cipher
                    settings['rsn_pairwise'] = self.wpa2_cipher
                elif self.security_mode == hostapd_constants.WPA1:
                    settings['wpa_pairwise'] = self.wpa_cipher
                elif self.security_mode == hostapd_constants.WPA2:
                    settings['rsn_pairwise'] = self.wpa2_cipher
                if self.wpa3:
                    settings['wpa_key_mgmt'] = 'SAE'
                if self.wpa_group_rekey:
                    settings['wpa_group_rekey'] = self.wpa_group_rekey
                if self.wpa_strict_rekey:
                    settings[
                        'wpa_strict_rekey'] = hostapd_constants.WPA_STRICT_REKEY
        return settings
