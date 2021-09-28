# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import iw_runner
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_Prefer5Ghz(wifi_cell_test_base.WiFiCellTestBase):
    """Test that if we see two APs in the same network, we take the 5Ghz one."""
    version = 1


    def run_once(self):
        """Body of the test."""
        client = self.context.client
        router = self.context.router
        mode_n = hostap_config.HostapConfig.MODE_11N_PURE
        ssid = router.build_unique_ssid()
        ap_config_2G = hostap_config.HostapConfig(channel=1, ssid=ssid,
                mode=mode_n)
        ap_config_5G = hostap_config.HostapConfig(ssid=ssid, channel=36,
                mode=mode_n)

        # Make sure neither the 2.4 GHz nor the 5 GHz BSS is blacklisted.
        client.clear_supplicant_blacklist()

        # Bring up a 2.4 GHz and 5 GHz BSS, both with the same SSID.
        self.context.configure(ap_config_2G)
        self.context.configure(ap_config_5G, multi_interface=True)

        # Uncomment the below for testing/debugging: Lowers the tx power of the
        # 5 GHz AP to make its signal weaker, so the DUT is more likely to
        # choose 2.4 GHz. 100 mBm or 1 dbM is the most we can lower it to.
        # _5G_interface = self.context.router.get_hostapd_interface(1)
        # self.context.router.iw_runner.set_tx_power(_5G_interface, 'fixed 100')

        # Wait for both BSSes to be discovered - if the DUT doesn't discover
        # both BSSes in the scan, it may end up connecting to the wrong BSS.
        client.wait_for_bsses(ssid, 2)
        self.context.assert_connect_wifi(
                xmlrpc_datatypes.AssociationParameters(ssid=ssid))

        # Cache the frequency to signal strength mapping for debugging purposes.
        wanted_freq = ap_config_5G.frequency
        actual_freq = int(client.get_iw_link_value(\
            iw_runner.IW_LINK_KEY_FREQUENCY))
        signal_strengths = {}

        bss_list_full = client.iw_runner.scan_dump(client.wifi_if)
        bss_list = filter(lambda bss: bss.ssid == ssid, bss_list_full)
        for bss in bss_list:
            signal_strengths[bss.frequency] = bss.signal
            logging.info('Freq: %d Signal: %d' , bss.frequency, bss.signal)

        if actual_freq != wanted_freq:
            raise error.TestFail('Connected to 2.4GHz at %r dBm, wanted 5 GHz '
                                  ' at %r dBm' %
                                  (signal_strengths.get(actual_freq),
                                  signal_strengths.get(wanted_freq)))
