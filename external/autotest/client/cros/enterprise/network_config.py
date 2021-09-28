# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import string


def generate_random_guid():
    """Create a random 16 character GUID."""
    return ''.join(random.choice(string.hexdigits) for _ in xrange(16))


class NetworkConfig(object):
    """
    NetworkConfig is a client-side representation of a network.

    Its primary purpose is to generate open network configurations.

    """
    def __init__(self, ssid=None, security='None', eap=None, password=None,
                 identity=None, autoconnect=None, ca_cert=None,
                 client_cert=None):
        """
        @param ssid: Service set identifier for wireless local area network.
        @param security: Security of network. Options are:
            'None', 'WEP-PSK', 'WEP-8021X', 'WPA-PSK', and 'WPA-EAP'.
        @param eap: EAP type, required if security is 'WEP-8021X' or 'WPA-EAP'.
        @param identity: Username, if the network type requires it.
        @param password: Password, if the network type requires it.
        @param ca_cert: CA certificate in PEM format. Required
            for EAP networks.
        @param client_cert: Client certificate in base64-encoded PKCS#12
            format. Required for EAP-TLS networks.
        @param autoconnect: True iff network policy should autoconnect.

        """
        self.ssid = ssid
        self.security = security
        self.eap = eap
        self.password = password
        self.identity = identity
        self.autoconnect = autoconnect
        self.ca_cert = ca_cert
        self.client_cert = client_cert
        self.guid = generate_random_guid()


    def policy(self):
        """
        Generate a network configuration policy dictionary.

        @returns conf: A dictionary in the format suitable to setting as a
            network policy.

        """
        conf = {
            'NetworkConfigurations': [
                {'GUID': self.guid,
                 'Name': self.ssid,
                 'Type': 'WiFi',
                 'WiFi': {
                     'SSID': self.ssid,
                     'Security': self.security}
                 }
            ],
        }

        # Generate list of certificate dictionaries.
        certs = []
        if self.ca_cert is not None:
            certs.append(
                {'GUID': 'CA_CERT',
                 'Type': 'Authority',
                 'X509': self.ca_cert}
            )

        if self.client_cert is not None:
            certs.append(
                {'GUID': 'CLIENT_CERT',
                 'Type': 'Client',
                 'PKCS12': self.client_cert}
            )

        if certs:
            conf['Certificates'] = certs

        wifi_conf = conf['NetworkConfigurations'][0]['WiFi']

        if self.autoconnect is not None:
            wifi_conf['AutoConnect'] = self.autoconnect

        if self.security == 'WPA-PSK':
            if self.password is None:
                raise error.TestError(
                        'Password is required for WPA-PSK networks.')
            wifi_conf['Passphrase'] = self.password

        if self.eap is not None:
            eap_conf = {
                'Outer': self.eap,
                'Identity': self.identity,
                'ServerCARefs': ['CA_CERT']
            }

            if self.password is not None:
                eap_conf['Password'] = self.password

            if self.eap == 'EAP-TLS':
                eap_conf['ClientCertType'] = 'Ref'
                eap_conf['ClientCertRef'] = 'CLIENT_CERT'
                eap_conf['UseSystemCAs'] = False

            wifi_conf['EAP'] = eap_conf

        return conf


class ProxyConfig(object):
    """
    ProxyConfig is a client-side representation of a proxy network.

    Its primary purpose is to generate open network configurations.

    """
    def __init__(self, type=None, pac_url=None, host=None, port=None,
                 exclude_urls=None):
        """
        @param type: Proxy type. Direct, Manual, PAC.
        @param pac_url: URL of PAC file.
        @param host: Host URL of proxy.
        @param port: Port of proxy.
        @param exclude_urls: URLs that should not be handled by the proxy.

        """
        self.type = type
        self.pac_url = pac_url
        self.host = host
        self.port = port
        self.exclude_urls = exclude_urls
        self.guid = generate_random_guid()


    def policy(self):
        """
        Generate a network configuration policy dictionary.

        @returns conf: A dictionary in the format suitable to setting as a
            network policy.

        """
        conf = {
            'NetworkConfigurations': [
                {'GUID': self.guid,
                 'Name': 'Managed_Ethernet',
                 'Ethernet': {
                     'Authentication': 'None'},
                 'Type': 'Ethernet',
                 'ProxySettings': {
                     'Type': self.type}
                }
            ]
        }

        proxy = conf['NetworkConfigurations'][0]['ProxySettings']

        if self.pac_url is not None:
            proxy['PAC'] = self.pac_url

        if self.host is not None and self.port is not None:
            proxy['Manual'] = {
                'HTTPProxy': {
                    'Host': self.host,
                    'Port': self.port
                }
            }

        if self.exclude_urls is not None:
            proxy['ExcludeDomains'] = self.exclude_urls

        return conf


    def mode(self):
        """Return ProxyMode consistent with the ProxySettings policy."""
        return {
            'Direct': 'direct',
            'Manual': 'fixed_servers',
            'PAC': 'pac_script'
        }[self.type]
