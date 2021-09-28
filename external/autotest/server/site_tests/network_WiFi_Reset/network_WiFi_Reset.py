# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_cell_test_base

class network_WiFi_Reset(wifi_cell_test_base.WiFiCellTestBase):
    """Test that the WiFi interface can be reset successfully, and that WiFi
    comes back up properly. Also run a few suspend/resume cycles along the way.
    Supports only Marvell (mwifiex) and Qualcomm/Atheros (ath10k) Wifi drivers.
    """

    version = 1

    _MWIFIEX_RESET_PATH = "/sys/kernel/debug/mwifiex/%s/reset"
    _MWIFIEX_RESET_TIMEOUT = 20
    _MWIFIEX_RESET_INTERVAL = 0.5

    _ATH10K_RESET_PATH = \
        '/sys/kernel/debug/ieee80211/%s/ath10k/simulate_fw_crash'

    # Possible reset paths for Intel wireless NICs are:
    # 1. '/sys/kernel/debug/iwlwifi/*/iwlmvm/fw_restart'
    # Logs look like: iwlwifi 0000:00:0c.0: 0x00000038 | BAD_COMMAND
    # This also triggers a register dump after the restart.
    # 2. '/sys/kernel/debug/iwlwifi/\*/iwlmvm/fw_nmi'
    # Logs look like: iwlwifi 0000:00:0c.0: 0x00000084 | NMI_INTERRUPT_UNKNOWN
    # This triggers a "hardware restart" once the NMI is processed
    # 3. '/sys/kernel/debug/iwlwifi/\*/iwlmvm/fw_dbg_collect'
    # The  third one is a mechanism to collect firmware debug dumps, that
    # effectively causes a restart, but we'll leave it aside for now.
    _IWLWIFI_RESET_PATH = '/sys/kernel/debug/iwlwifi/%s/iwlmvm/fw_restart'

    _NUM_RESETS = 15
    _NUM_SUSPENDS = 5
    _SUSPEND_DELAY = 10

    # Implement these 3 functions for each driver reset mechanism we want to
    # support:
    #  @supported: return True if this driver is supported (e.g., check for
    #    available debugfs/sysfs triggers)
    #  @do_reset: perform a Wifi reset, to simulate a firmware crash/restart.
    #    Different drivers may handle restart in different ways, but this
    #    method should at least ensure the network device is available before
    #    returning.
    DriverReset = collections.namedtuple('DriverReset', ['supported',
                                                         'do_reset'])


    @property
    def mwifiex_reset_path(self):
        """Get path to the Wifi interface's reset file."""
        return self._MWIFIEX_RESET_PATH % self.context.client.wifi_if

    def mwifiex_reset_exists(self):
        """Check if the mwifiex reset file is present (i.e., a mwifiex
        interface is present).
        """
        return self.context.client.host.path_exists(self.mwifiex_reset_path)

    def mwifiex_reset(self):
        """Perform mwifiex reset and wait for the interface to come back up."""

        ssid = self.context.router.get_ssid()

        # Adapter will asynchronously reset.
        self.context.client.host.run('echo 1 > ' + self.mwifiex_reset_path)

        # Wait for disconnect. We aren't guaranteed to receive a disconnect
        # event, but shill will at least notice the adapter went away.
        self.context.client.wait_for_service_states(ssid, ['idle'],
                timeout_seconds=20)

        # Now wait for the reset interface file to come back.
        utils.poll_for_condition(
                condition=self.mwifiex_reset_exists,
                exception=error.TestFail(
                        'Failed to reset device %s' %
                        self.context.client.wifi_if),
                timeout=self._MWIFIEX_RESET_TIMEOUT,
                sleep_interval=self._MWIFIEX_RESET_INTERVAL)

    @property
    def ath10k_reset_path(self):
        """Get path to ath10k debugfs reset file"""
        phy_name = self.context.client.wifi_phy_name
        return self._ATH10K_RESET_PATH % phy_name

    def ath10k_reset_exists(self):
        """@return True if ath10k debugfs reset file exists"""
        return self.context.client.host.path_exists(self.ath10k_reset_path)

    def ath10k_reset(self):
        """
        Simulate ath10k firmware crash. mac80211 handles firmware crashes
        transparently, so we don't expect a full disconnect/reconnet event.

        From ath10k debugfs:
        To simulate firmware crash write one of the keywords to this file:
        `soft` - this will send WMI_FORCE_FW_HANG_ASSERT to firmware if FW
            supports that command.
        `hard` - this will send to firmware command with illegal parameters
            causing firmware crash.
        `assert` - this will send special illegal parameter to firmware to
            cause assert failure and crash.
        `hw-restart` - this will simply queue hw restart without fw/hw actually
            crashing.
        """
        self.context.client.host.run('echo soft > ' + self.ath10k_reset_path)

    def iwlwifi_reset_path(self):
        """Get path to iwlwifi debugfs reset file"""
        pci_dev_name = self.context.client.parent_device_name
        return self._IWLWIFI_RESET_PATH % pci_dev_name

    def iwlwifi_reset_exists(self):
        """@return True if iwlwifi debugfs reset file exists"""
        return self.context.client.host.path_exists(self.iwlwifi_reset_path())

    def iwlwifi_reset(self):
        """
        Simulate iwlwifi firmware crash.
        """
        self.context.client.host.run('echo 1 > ' + self.iwlwifi_reset_path())

    def get_reset_driver(self):
        DRIVER_LIST = [
            self.DriverReset(
                supported=self.mwifiex_reset_exists,
                do_reset=self.mwifiex_reset,
            ),
            self.DriverReset(
                supported=self.ath10k_reset_exists,
                do_reset=self.ath10k_reset,
            ),
            self.DriverReset(
                supported=self.iwlwifi_reset_exists,
                do_reset=self.iwlwifi_reset,
            ),
        ]

        for driver in DRIVER_LIST:
            if driver.supported():
                return driver
        else:
            raise error.TestNAError('DUT does not support device reset')

    def run_once(self):
        """Body of the test."""
        self._passed = False

        client = self.context.client

        self.boot_id = client.host.get_boot_id()
        self.reset_driver = self.get_reset_driver()

        ap_config = hostap_config.HostapConfig(channel=1)
        ssid = self.configure_and_connect_to_ap(ap_config)

        self.context.assert_ping_from_dut()

        router = self.context.router
        ssid = router.get_ssid()

        logging.info("Running %d suspends", self._NUM_SUSPENDS)
        for _ in range(self._NUM_SUSPENDS):
            logging.info("Running %d resets", self._NUM_RESETS)
            for __ in range(self._NUM_RESETS):
                self.reset_driver.do_reset()
                client.wait_for_connection(ssid)
                self.context.assert_ping_from_dut()

            client.do_suspend(self._SUSPEND_DELAY)
            client.host.test_wait_for_resume(self.boot_id)
            client.wait_for_connection(ssid)

        self._passed = True

    def cleanup(self):
        """Performs cleanup at exit. May reboot the DUT, to keep the system
        functioning for the next test.
        """
        # TODO: Technically, we should be able to handle both
        # super(...).cleanup() and arbitrary reboots (either driver crashes or
        # forced reboot). This would require fixing up some of WiFiCellTestBase
        # (e.g., not to assume a persistent xmlrpc connection in cleanup()).
        # But cleanup() is not absolutely critical -- subsequent tests should
        # handle re-initializing state.
        if not self._passed:
            logging.info('Test failed: may have left DUT in bad state; '
                         'rebooting')
            self.context.client.reboot(timeout=60)
        elif self.context.client.host.get_boot_id() == self.boot_id:
            super(network_WiFi_Reset, self).cleanup()
        else:
            logging.info('May have rebooted during test; skipping cleanup')
