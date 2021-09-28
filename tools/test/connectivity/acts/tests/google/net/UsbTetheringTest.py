from acts import asserts
from acts import base_test
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.wifi import wifi_test_utils as wutils
from scapy.all import get_if_list
from scapy.all import get_if_raw_hwaddr

DUMSYS_CMD = "dumpsys connectivity tethering"
UPSTREAM_WANTED_STRING = "Upstream wanted"
CURRENT_UPSTREAM_STRING = "Current upstream interface"
SSID = wutils.WifiEnums.SSID_KEY


class UsbTetheringTest(base_test.BaseTestClass):
  """Tests for tethering."""

  def setup_class(self):
    self.dut = self.android_devices[0]
    self.USB_TETHERED = False

    nutils.verify_lte_data_and_tethering_supported(self.dut)
    req_params = ("wifi_network",)
    self.unpack_userparams(req_params)

  def teardown_class(self):
    nutils.stop_usb_tethering(self.dut)
    self.USB_TETHERED = False
    wutils.reset_wifi(self.dut)

  def on_fail(self, test_name, begin_time):
    self.dut.take_bug_report(test_name, begin_time)

  @test_tracker_info(uuid="140a064b-1ab0-4a92-8bdb-e52dde03d5b8")
  def test_usb_tethering_over_wifi(self):
    """Tests USB tethering over wifi.

    Steps:
    1. Connects to a wifi network
    2. Enables USB tethering
    3. Verifies wifi is preferred upstream over data connection
    """

    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wifi_network[SSID])
    wutils.wifi_connect(self.dut, self.wifi_network)
    wifi_network = self.dut.droid.connectivityGetActiveNetwork()
    self.log.info("wifi network %s" % wifi_network)

    iflist_before = get_if_list()
    nutils.start_usb_tethering(self.dut)
    self.USB_TETHERED = True
    self.iface = nutils.wait_for_new_iface(iflist_before)
    self.real_hwaddr = get_if_raw_hwaddr(self.iface)

    output = self.dut.adb.shell(DUMSYS_CMD)
    for line in output.split("\n"):
      if UPSTREAM_WANTED_STRING in line:
        asserts.assert_true("true" in line, "Upstream interface is not active")
        self.log.info("Upstream interface is active")
      if CURRENT_UPSTREAM_STRING in line:
        asserts.assert_true("wlan" in line, "WiFi is not the upstream "
                            "interface")
        self.log.info("WiFi is the upstream interface")

