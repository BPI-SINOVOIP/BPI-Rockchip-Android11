#
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

"""Test script to test Bluetooth Tethering testcases."""
import time

from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt.bt_test_utils import orchestrate_and_verify_pan_connection
from acts.test_utils.bt.bt_test_utils import setup_multiple_devices_for_bt_test
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.tel.tel_test_utils import wait_for_cell_data_connection
from acts.test_utils.wifi import wifi_test_utils as wutils

DEFAULT_PING_URL = "https://www.google.com/robots.txt"


class BluetoothTetheringTest(BluetoothBaseTest):
  """Tests for Bluetooth Tethering."""

  def setup_class(self):
    """Setup devices for tethering tests."""

    self.tmobile_device = self.android_devices[0]  # T-mobile SIM, only IPv6
    self.verizon_device = self.android_devices[1]  # Verizon SIM, IPv4 & IPv6
    req_params = ("url",)
    self.unpack_userparams(req_params)

    nutils.verify_lte_data_and_tethering_supported(self.tmobile_device)
    nutils.verify_lte_data_and_tethering_supported(self.verizon_device)

  def setup_test(self):
    """Setup devices for tethering before each test."""
    setup_multiple_devices_for_bt_test([self.tmobile_device,
                                        self.verizon_device])

  def on_fail(self, test_name, begin_time):
    self.tmobile_device.take_bug_report(test_name, begin_time)
    self.verizon_device.take_bug_report(test_name, begin_time)

  def _verify_ipv6_tethering(self, dut):
    """Verify IPv6 tethering.

    Args:
      dut: Android device that is being checked
    """

    # httpRequestString() returns the IP address when visiting the URL
    http_response = dut.droid.httpRequestString(self.url)
    self.log.info("IP address %s " % http_response)
    active_link_addrs = dut.droid.connectivityGetAllAddressesOfActiveLink()

    if dut == self.hotspot_device and nutils.carrier_supports_ipv6(dut) \
      or nutils.supports_ipv6_tethering(self.hotspot_device):
      asserts.assert_true(nutils.is_ipaddress_ipv6(http_response),
                          "The http response did not return IPv6 address")
      asserts.assert_true(
          active_link_addrs and http_response in str(active_link_addrs),
          "Could not find IPv6 address in link properties")
      asserts.assert_true(
          dut.droid.connectivityHasIPv6DefaultRoute(),
          "Could not find IPv6 default route in link properties")
    else:
      asserts.assert_false(dut.droid.connectivityHasIPv6DefaultRoute(),
                           "Found IPv6 default route in link properties")

  def _bluetooth_tethering_then_disconnect(self, pan_ad, panu_ad):
    """Test bluetooth PAN tethering connection then disconnect service.

    Test basic PAN tethering connection between two devices then disconnect
    service.

    Steps:
    1. Enable data connection on PAN device
    2. Disable data connection on PANU device
    (steps 3-7: orchestrate_and_verify_pan_connection())
    3. Enable Airplane mode on PANU device. Enable Bluetooth only.
    4. Enable Bluetooth tethering on PAN Service device.
    5. Pair the PAN Service device to the PANU device.
    6. Verify that Bluetooth tethering is enabled on PAN Service device.
    7. Verify HTTP connection on PANU device.
    8. Disable Bluetooth tethering on PAN Service device.
    9. Verify no HTTP connection on PANU device.

    Args:
      pan_ad: Android device providing tethering via Bluetooth
      panu_ad: Android device receiving tethering via Bluetooth

    Expected Result:
    PANU device has internet access only with Bluetooth tethering

    Returns:
      Pass if True
      Fail if False
    """

    if not wait_for_cell_data_connection(self.log, pan_ad, True):
      self.log.error("Failed to enable data connection.")
      return False

    panu_ad.droid.telephonyToggleDataConnection(False)
    if not wait_for_cell_data_connection(self.log, panu_ad, False):
      self.log.error("Failed to disable data connection.")
      return False

    if not orchestrate_and_verify_pan_connection(pan_ad, panu_ad):
      self.log.error("Could not establish a PAN connection.")
      return False
    internet = wutils.validate_connection(panu_ad, DEFAULT_PING_URL)
    if not internet:
      self.log.error("Internet is not connected with Bluetooth")
      return False

    # disable bluetooth tethering and verify internet is not working
    internet = None
    pan_ad.droid.bluetoothPanSetBluetoothTethering(False)
    try:
      internet = wutils.validate_connection(panu_ad, DEFAULT_PING_URL)
    except Exception as e:
      self.log.error(e)
    if internet:
      self.log.error("Internet is working without Bluetooth tethering")
      return False

    return True

  def _do_bluetooth_tethering_then_disconnect(self, hotspot_device,
                                              tethered_device):
    """Test bluetooth tethering.

    Steps:
    1. Enables Data Connection on hotspot device.
    2. Verifies IPv6 tethering is supported
    3. Execute Bluetooth tethering test

    Args:
      hotspot_device: device providing internet service
      tethered_device: device receiving internet service

    Returns:
      True: if tethering test is successful
      False: otherwise
    """

    self.hotspot_device = hotspot_device
    wutils.wifi_toggle_state(self.hotspot_device, False)
    wutils.wifi_toggle_state(tethered_device, False)
    self.hotspot_device.droid.telephonyToggleDataConnection(True)
    time.sleep(20)  # allowing time for Data Connection to stabilize
    operator = nutils.get_operator_name(self.log, self.hotspot_device)
    self.log.info("Carrier is %s" % operator)
    self._verify_ipv6_tethering(self.hotspot_device)
    return self._bluetooth_tethering_then_disconnect(self.hotspot_device,
                                                     tethered_device)

  @test_tracker_info(uuid="433c7b62-3a60-4cae-8f3b-446d60c3ee9a")
  def test_bluetooth_tethering_then_disconnect_source_verizon(self):
    """Test bluetooth tethering from Verizon SIM."""

    return self._do_bluetooth_tethering_then_disconnect(self.verizon_device,
                                                        self.tmobile_device)

  @test_tracker_info(uuid="e12fa5a5-f2c6-49e3-b255-f007be6319b0")
  def test_bluetooth_tethering_then_disconnect_source_tmobile(self):
    """Test bluetooth tethering from T-Mobile SIM."""

    return self._do_bluetooth_tethering_then_disconnect(self.tmobile_device,
                                                        self.verizon_device)

