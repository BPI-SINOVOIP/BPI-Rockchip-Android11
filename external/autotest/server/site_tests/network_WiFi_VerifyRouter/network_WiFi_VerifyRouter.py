# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_VerifyRouter(wifi_cell_test_base.WiFiCellTestBase):
    """Test that a dual radio router can use both radios."""
    version = 1
    MAX_ASSOCIATION_RETRIES = 8  # Super lucky number.  Not science.

    # Antenna bitmap constants.
    ANTENNAS_1 = 0x1
    ANTENNAS_2 = 0x2
    ANTENNAS_BOTH = ANTENNAS_1 | ANTENNAS_2

    # We don't want to accept really low signal strength, so we pick an
    # arbitrary threshold.
    SIGNAL_THRESHOLD = -60

    # Antennas on a device should have similar signal stength, so we pick
    # another arbitrary threshold.
    ANTENNA_VARIANCE_THRESHOLD = 15

    def _connect(self, wifi_params):
        assoc_result = xmlrpc_datatypes.deserialize(
                self.context.client.shill.connect_wifi(wifi_params))
        logging.info('Finished connection attempt to %s with times: '
                     'discovery=%.2f, association=%.2f, configuration=%.2f.',
                     wifi_params.ssid,
                     assoc_result.discovery_time,
                     assoc_result.association_time,
                     assoc_result.configuration_time)
        return assoc_result.success

    def _check_signal_levels(self, instance, bitmap, channel):
        signal_level = self.context.client.wifi_signal_level
        if signal_level is None:
            return 'Could not retrieve signal info from device.'

        logging.info('Signal level for AP %d with bitmap %d is %d',
                     instance, bitmap, signal_level)
        self.write_perf_keyval(
                {'signal_for_ap_%d_bm_%d_ch_%d' %
                         (instance, bitmap, channel): signal_level})
        # Don't accept very low signal strength.
        if signal_level < self.SIGNAL_THRESHOLD:
            return 'Signal too weak (%s dBm)' % (signal_level)

        # In our conductive testbeds, AP antennas are connected 1:1 with DUT
        # antennas. This means that when broadcasting from one AP antenna
        # we will only see signal on one DUT antenna. Thus, don't test per
        # antenna DUT signal when only using one AP antenna.
        if bitmap != self.ANTENNAS_BOTH:
            return None

        antenna_signal_levels = self.context.client.wifi_signal_level_all_chains
        # Some devices don't report per antenna signal levels. This is not an
        # error so we log our inability to retrieve the data and return without
        # failure.
        if antenna_signal_levels is None:
            logging.info('Could not retrieve per antenna signal info from'
                    ' device.')
            return None

        max_signal = max(antenna_signal_levels)
        min_signal = min(antenna_signal_levels)
        if min_signal < self.SIGNAL_THRESHOLD:
            return ('Signal too weak on at least one antenna (%s dBm)' %
                    antenna_signal_levels)

        if max_signal - min_signal > self.ANTENNA_VARIANCE_THRESHOLD:
            return ('Antenna signals vary significantly (%s dBm)' %
                    antenna_signal_levels)

        return None


    def _antenna_test(self, bitmap, channel):
        """Test that we can connect on |channel|, with given antenna |bitmap|.

        Sets up two radios on |channel|, configures both radios with the
        given antenna |bitmap|, and then verifies that a client can connect
        to the AP on each radio and that the DUT doesn't report unreasonably
        low signal strength.

        Why do we run the two radios concurrently, instead of iterating over
        them? That's simply because our lower-layer code doesn't provide an
        interface for specifiying which PHY to run an AP on.

        To work around the API limitaiton, we bring up multiple APs, and let
        the lower-layer code spread them across radios. For stumpy/panther,
        this works in an obvious way. That is, each call to this method
        exercises phy0 and phy1.

        For whirlwind, we do not cover the 3rd radio. phy0 is 2.4 GHz, phy1 is
        5 GHz, and phy2 is a 1x1 radio that covers both. Because phy2 isn't
        normally used (and validated) as a transmitter, and because our
        conductive setups don't even wire up its antennas, we try not to use it
        in practice. So in effect, this test will be double-testing phy0 and
        phy1 with the 2.4 GHz and 5 GHz portions of the test, respectively.

        Gale is similar to whirlwind, except that it has no phy2 radio.

        @param bitmap: int bitmask controlling which antennas to enable.
        @param channel: int Wifi channel to conduct test on

        """
        # Antenna can only be configured when the wireless interface is down.
        self.context.router.deconfig()
        self.context.router.disable_antennas_except(bitmap)
        # This seems to increase the probability that our association
        # attempts pass.  It is the very definition of a dark incantation.
        time.sleep(5)
        # Setup two APs on |channel|. configure() will spread these across
        # radios.
        n_mode = hostap_config.HostapConfig.MODE_11N_MIXED
        ap_config = hostap_config.HostapConfig(channel=channel, mode=n_mode)
        self.context.configure(ap_config)
        self.context.configure(ap_config, multi_interface=True)
        failures = []
        # Verify connectivity to both APs. As the APs are spread
        # across radios, this exercises multiple radios.
        for instance in range(2):
            context_message = ('bitmap=%d, ap_instance=%d, channel=%d' %
                               (bitmap, instance, channel))
            logging.info('Connecting to AP with settings %s.',
                         context_message)
            client_conf = xmlrpc_datatypes.AssociationParameters(
                    ssid=self.context.router.get_ssid(instance=instance))
            if self._connect(client_conf):
                failure = self._check_signal_levels(instance, bitmap, channel)
                if failure:
                    failures.append('%s: %s' % (context_message, failure))
            else:
                failures.append('%s: Failed to connect.' % context_message)
            # Don't automatically reconnect to this AP.
            self.context.client.shill.disconnect(
                    self.context.router.get_ssid(instance=instance))
        return failures


    def cleanup(self):
        """Clean up after the test is completed

        Perform additional cleanups after the test, the important thing is
        to re-enable all antennas.
        """
        self.context.router.deconfig()
        self.context.router.enable_all_antennas()
        super(network_WiFi_VerifyRouter, self).cleanup()


    def run_once(self):
        """Verify that all radios on this router are functional."""
        all_failures = []

        # ath10k doesn't support support non-contiguous antenna masks. The
        # driver complains:
        #   ath10k_ahb a000000.wifi: mac tx antenna chainmask may be invalid:
        #   0x2.  Suggested values: 15, 7, 3, 1 or 0.
        # And on gale, the firmware crashes eventually. Whirlwind seems OK for
        # now.
        # TODO: communicate this back from the driver better, so we don't have
        # to build an exception list.
        if self.context.router.board == "gale":
            bitmaps = (self.ANTENNAS_BOTH, self.ANTENNAS_1)
        else:
            bitmaps = (self.ANTENNAS_BOTH, self.ANTENNAS_1, self.ANTENNAS_2)

        # Run antenna test for 2GHz band and 5GHz band
        for channel in (6, 149):
            # First connect with both antennas enabled. Then connect with just
            # one antenna enabled at a time.
            for bitmap in bitmaps:
                failures = set()
                for _ in range(self.MAX_ASSOCIATION_RETRIES):
                    new_failures = self._antenna_test(bitmap, channel)
                    if not new_failures:
                        break
                    failures.update(new_failures)
                else:
                    all_failures += failures

        if all_failures:
            failure_message = ', '.join(
                    ['(' + message + ')' for message in all_failures])
            raise error.TestFail("Failed the following configurations: %s." %
                    failure_message)
