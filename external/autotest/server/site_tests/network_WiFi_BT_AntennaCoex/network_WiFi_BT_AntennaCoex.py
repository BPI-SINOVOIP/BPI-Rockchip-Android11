# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests
from autotest_lib.server.cros.multimedia import bluetooth_le_facade_adapter
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_test_context_manager

class network_WiFi_BT_AntennaCoex(
        bluetooth_adapter_tests.BluetoothAdapterTests):
    """Test various sequences that mix wifi off/on, BT off/on
    and ensure that wifi/BT functionality is intact during
    and after these sequences.

    """

    # Sequence being tested:
    # 1. wifi goes down
    #    - verify bt discovery works
    # 2. bt goes down
    # 3. wifi comes up (test scan and connect)
    #    - b/79233533 will cause step 3 to fail.
    # 4. bt comes up (verify bt discovery again)
    def _test_bringup_wifi_with_bt_off(self):
        # Turn off wifi
        client = self.wifi_context.client
        client.set_device_enabled(client.wifi_if, False)

        # Check that BT is up
        self.test_power_on_adapter()
        self.test_bluetoothd_running()

        # Run a BT scan.
        self.test_start_discovery()

        utils.poll_for_condition(self.bluetooth_facade.is_discovering,
                                 timeout=5,
                                 sleep_interval=1,
                                 desc="Checking BT scan in progress")
        self.test_stop_discovery()

        # Turn off BT
        self.test_power_off_adapter()
        self.test_bluetoothd_running()

        # Turn wifi back on
        client.set_device_enabled(client.wifi_if, True)

        # Expect that the DUT will scan, find the AP again.
        client.wait_for_bsses(self.ssid, 1)
        self.wifi_context.wait_for_connection(self.ssid)

        # Bring BT back up
        self.test_power_on_adapter()
        self.test_bluetoothd_running()

    # Sequence being tested:
    # 1. bt goes down
    #    - verify wifi scanning works
    # 2. wifi goes down
    # 3. bt comes up (test discovery works)
    # 4. wifi comes up (verify by connecting to AP)
    def _test_bringup_bt_with_wifi_off(self):
        # Turn off BT
        self.test_power_off_adapter()
        self.test_bluetoothd_running()

        # Test that we can scan on wifi
        client = self.wifi_context.client
        client.wait_for_bsses(self.ssid, 1, timeout_seconds=10)

        # Turn off wifi
        client = self.wifi_context.client
        client.set_device_enabled(client.wifi_if, False)

        # Turn on BT
        self.test_power_on_adapter()
        self.test_bluetoothd_running()

        # Run a BT scan.
        self.test_start_discovery()

        utils.poll_for_condition(self.bluetooth_facade.is_discovering,
                                 timeout=5,
                                 sleep_interval=1,
                                 desc="Checking BT scan in progress")
        self.test_stop_discovery()

        # Turn wifi back on
        client.set_device_enabled(client.wifi_if, True)

        # Expect that the DUT will scan, find the AP again.
        client.wait_for_bsses(self.ssid, 1)
        self.wifi_context.wait_for_connection(self.ssid)

    def warmup(self, host, raw_cmdline_args):
        """Stashes away parameters for use by run_once().

        @param host Host object representing the client DUT.
        @param raw_cmdline_args Raw input from autotest.
        @param additional_params One item from CONFIGS in control file.

        """
        # pylint: disable=attribute-defined-outside-init
        self.host = host
        self.cmdline_args = utils.args_to_dict(raw_cmdline_args)

        # Initialize BT.
        self.ble_adapter = \
                bluetooth_le_facade_adapter.BluetoothLEFacadeRemoteAdapter
        self.bluetooth_facade = self.ble_adapter(self.host)

        # Initialize wifi
        self.wifi_context = wifi_test_context_manager.WiFiTestContextManager(
                self.__class__.__name__, self.host, self.cmdline_args,
                self.debugdir)
        self.wifi_context.setup()

    def run_once(self):
        """Run a series of WiFi on/off, BT on/off sequences."""

        # Try the co-ex tests on both 2.4 and 5 GHz bands.
        # Run 5 GHz first so that if the test throws an
        # exception in the flow with 2.4 GHz and terminates abruptly,
        # we have the 5 GHz data-point to compare against. WiFi is
        # alone on 5 GHz, 2.4 GHz is the band with wifi-bt co-ex.
        frequencies = [5180, 5745, 2412, 2462]

        for freq in frequencies:
            mode_n = hostap_config.HostapConfig.MODE_11N_PURE
            self.wifi_context.configure(
                    hostap_config.HostapConfig(frequency=freq, mode=mode_n))
            # pylint: disable=attribute-defined-outside-init
            self.ssid = self.wifi_context.router.get_ssid()
            self.wifi_context.assert_connect_wifi(
                    xmlrpc_datatypes.AssociationParameters(ssid=self.ssid))

            self._test_bringup_wifi_with_bt_off()
            self._test_bringup_bt_with_wifi_off()

            self.wifi_context.client.shill.disconnect(self.ssid)
            self.wifi_context.router.deconfig()

    def cleanup(self):
        super(network_WiFi_BT_AntennaCoex, self).cleanup()
        if hasattr(self, 'wifi_context'):
            self.wifi_context.teardown()
