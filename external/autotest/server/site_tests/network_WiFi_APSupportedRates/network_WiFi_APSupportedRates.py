# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import tcpdump_analyzer
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_APSupportedRates(wifi_cell_test_base.WiFiCellTestBase):
    """Test that the WiFi chip honors the SupportedRates IEs sent by the AP."""
    version = 1

    def check_bitrates_in_capture(self, pcap_result, supported_rates):
        """
        Check that bitrates look like we expect in a packet capture.

        The DUT should not send packets at bitrates that were disabled by the
        AP.

        @param pcap_result: RemoteCaptureResult tuple.
        @param supported_rates: List of upported legacy bitrates (Mbps).

        """
        # Filter notes:
        # (a) Some chips use self-addressed frames to tune channel performance.
        #     They don't carry host-generated traffic, so filter them out.
        # (b) Use TA (not SA), because multicast may retransmit our
        #     "Source-Addressed" frames at rates we don't control.
        # (c) BSSID filter: non-BSSID frames include Ack and BlockAck frames;
        #     these rates tend to match the frames to which they're responding
        #     (i.e., not under DUT's control).
        # (d) QoS null filter: these frames are short (no data payload), and
        #     it's more important that they be reliable (e.g., for PS
        #     transitions) than fast. See b/132825853#comment40,
        #     for example.
        # Items (b) and (c) wouldn't be much problem if our APs actually
        # respected the Supported Rates IEs that we're configuring, but current
        # (2019-06-28) test AP builds do not appear to.
        frame_filter = ('wlan.ta==%s and (not wlan.da==%s) and wlan.bssid==%s'
                        ' and wlan.fc.type_subtype!=%s' %
                        (self.context.client.wifi_mac,
                         self.context.client.wifi_mac,
                         self.context.router.get_hostapd_mac(0),
                         tcpdump_analyzer.WLAN_QOS_NULL_TYPE))
        frames = tcpdump_analyzer.get_frames(pcap_result.local_pcap_path,
                                             frame_filter, reject_bad_fcs=False)
        if not frames:
            raise error.TestError('Failed to capture any relevant frames')

        # Some frames don't have bitrate fields -- for example, if they are
        # using MCS rates (not legacy rates). For MCS rates, that's OK, since
        # that satisfies this test requirement (not using "unsupported legacy
        # rates"). So ignore them.
        bad_frames = [f for f in frames
                      if f.bit_rate is not None and
                         f.bit_rate not in supported_rates]
        if bad_frames:
            # Remove duplicates.
            bad_rates = list(set(f.bit_rate for f in bad_frames))
            logging.error('Unexpected rate for frames:')
            for f in bad_frames:
                logging.error('%s', f)
            raise error.TestFail('Saw frames at rates %r (expected %r).' %
                                 (bad_rates, supported_rates))

    def parse_additional_arguments(self, commandline_args, additional_params):
        """Hook into super class to take control files parameters.

        @param commandline_args dict of parsed parameters from the autotest.
        @param additional_params HostapConfig object.

        """
        self._ap_config = additional_params

    def run_once(self):
        """Verify that we respond sanely to APs that disable legacy bitrates.
        """
        ap_config = self._ap_config
        self.context.configure(ap_config)
        self.context.capture_host.start_capture(
                ap_config.frequency,
                width_type=ap_config.packet_capture_mode)
        assoc_params = xmlrpc_datatypes.AssociationParameters(
                ssid=self.context.router.get_ssid())
        self.context.assert_connect_wifi(assoc_params)
        self.context.assert_ping_from_dut()
        results = self.context.capture_host.stop_capture()
        if len(results) != 1:
            raise error.TestError('Expected to generate one packet '
                                  'capture but got %d captures instead.' %
                                  len(results))
        self.check_bitrates_in_capture(results[0],
                                       ap_config.supported_rates)
