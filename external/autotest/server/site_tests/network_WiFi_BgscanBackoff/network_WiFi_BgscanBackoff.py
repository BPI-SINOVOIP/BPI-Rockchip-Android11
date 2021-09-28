# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros.network import iw_runner
from autotest_lib.client.common_lib.cros.network import ping_runner
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_BgscanBackoff(wifi_cell_test_base.WiFiCellTestBase):
    """Test that background scan backs off when there is foreground traffic."""
    version = 1

    BGSCAN_SAMPLE_PERIOD_SECONDS = 100
    NO_BGSCAN_SAMPLE_PERIOD_SECONDS = 10
    CONFIGURED_BGSCAN_INTERVAL_SECONDS = 7
    PING_INTERVAL_SECONDS = 0.1

    # Dwell time for scanning is usually configured to be around 100 ms (some
    # are higher, around 150 ms), since this is also the standard beacon
    # interval.  Tolerate spikes in latency up to 250 ms as a way of asking
    # that our PHY be servicing foreground traffic regularly during background
    # scans.
    # See also network_WiFi_OverlappingBSSScan for similar parameters.
    LATENCY_MARGIN_MS = 250
    THRESHOLD_BASELINE_LATENCY_MS = 100

    def parse_additional_arguments(self, commandline_args, additional_params):
        """Hook into super class to take control files parameters.

        @param commandline_args dict of parsed parameters from the autotest.
        @param additional_params list of HostapConfig objects and LinuxSystem
           capabilities.

        """
        self._config_first_ap = additional_params[0]
        self._config_second_ap = additional_params[1]

    def _find_bss_matching_mac_addr(self, iw, mac_addr):
        """ Scan and look for a BSS in the scan results with given mac address.

        @param iw iw_runner instantiated on the client
        @param mac_addr the address to look for
        """
        bss_list = iw.scan_dump(self.context.client.wifi_if)
        logging.debug('Found BSSes: %r', bss_list)
        return filter(lambda bss: bss.bss == mac_addr, bss_list)

    def run_once(self):
        """Body of the test."""

        get_assoc_params = lambda conf: xmlrpc_datatypes.AssociationParameters(
                ssid=self.context.router.get_ssid(instance=0),
                bgscan_config=conf)
        get_ping_config = lambda period: ping_runner.PingConfig(
                self.context.get_wifi_addr(),
                interval=self.PING_INTERVAL_SECONDS,
                count=int(period / self.PING_INTERVAL_SECONDS))
        self.context.configure(self._config_first_ap)
        bgscan_config = xmlrpc_datatypes.BgscanConfiguration(
                short_interval=self.CONFIGURED_BGSCAN_INTERVAL_SECONDS,
                long_interval=self.CONFIGURED_BGSCAN_INTERVAL_SECONDS,
                method=xmlrpc_datatypes.BgscanConfiguration.SCAN_METHOD_SIMPLE)
        self.context.assert_connect_wifi(get_assoc_params(bgscan_config))
        logging.info('Pinging router with background scans for %d seconds.',
                     self.BGSCAN_SAMPLE_PERIOD_SECONDS)
        result_bgscan = self.context.client.ping(
                get_ping_config(self.BGSCAN_SAMPLE_PERIOD_SECONDS))
        logging.info('Ping statistics with bgscan: %r', result_bgscan)
        # Bring up a second AP, make sure that it shows up in bgscans.
        self._config_second_ap.ssid = self.context.router.get_ssid()
        self.context.configure(self._config_second_ap,multi_interface=True)
        logging.info('Without a ping running, ensure that bgscans succeed.')
        ap_mac = self.context.router.get_hostapd_mac(ap_num=1)
        logging.debug('Looking for BSS %s', ap_mac)
        iw = iw_runner.IwRunner(remote_host=self.context.client.host)

        utils.poll_for_condition(
            condition=lambda: self._find_bss_matching_mac_addr(iw, ap_mac),
            exception=error.TestFail('Background scans should detect new BSSes '
                                     'within an associated ESS.'),
            timeout=self.BGSCAN_SAMPLE_PERIOD_SECONDS,
            sleep_interval=1)

        self.context.router.deconfig_aps(instance=1)
        self.context.client.shill.disconnect(
                self.context.router.get_ssid(instance=0))
        # Reconfigure AP, so the new bgscan setting can be correctly applied.
        self.context.configure(self._config_first_ap)
        # Gather some statistics about ping latencies without scanning going on.
        self.context.assert_connect_wifi(get_assoc_params(None))
        logging.info('Pinging router without background scans for %d seconds.',
                     self.NO_BGSCAN_SAMPLE_PERIOD_SECONDS)
        result_no_bgscan = self.context.client.ping(
                get_ping_config(self.NO_BGSCAN_SAMPLE_PERIOD_SECONDS))
        logging.info('Ping statistics without bgscan: %r', result_no_bgscan)
        if result_no_bgscan.max_latency > self.THRESHOLD_BASELINE_LATENCY_MS:
            raise error.TestFail('RTT latency is too high even without '
                                 'background scans: %f' %
                                 result_no_bgscan.max_latency)

        if (result_bgscan.max_latency >
                self.LATENCY_MARGIN_MS + result_no_bgscan.avg_latency):
            raise error.TestFail('Significant difference in rtt due to bgscan: '
                                 '%.1f > %.1f + %d' %
                                 (result_bgscan.max_latency,
                                  result_no_bgscan.avg_latency,
                                  self.LATENCY_MARGIN_MS))
