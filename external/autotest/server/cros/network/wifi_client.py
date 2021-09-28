# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging
import math
import re
import time

from contextlib import contextmanager
from collections import namedtuple

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros.network import interface
from autotest_lib.client.common_lib.cros.network import iw_runner
from autotest_lib.client.common_lib.cros.network import ping_runner
from autotest_lib.client.cros import constants
from autotest_lib.server import autotest
from autotest_lib.server import site_linux_system
from autotest_lib.server.cros.network import wpa_cli_proxy
from autotest_lib.server.hosts import cast_os_host

# Wake-on-WiFi feature strings
WAKE_ON_WIFI_NONE = 'none'
WAKE_ON_WIFI_PACKET = 'packet'
WAKE_ON_WIFI_DARKCONNECT = 'darkconnect'
WAKE_ON_WIFI_PACKET_DARKCONNECT = 'packet_and_darkconnect'
WAKE_ON_WIFI_NOT_SUPPORTED = 'not_supported'

# Wake-on-WiFi test timing constants
SUSPEND_WAIT_TIME_SECONDS = 10
RECEIVE_PACKET_WAIT_TIME_SECONDS = 10
DARK_RESUME_WAIT_TIME_SECONDS = 25
WAKE_TO_SCAN_PERIOD_SECONDS = 30
NET_DETECT_SCAN_WAIT_TIME_SECONDS = 15
WAIT_UP_TIMEOUT_SECONDS = 10
DISCONNECT_WAIT_TIME_SECONDS = 10
INTERFACE_DOWN_WAIT_TIME_SECONDS = 10

ConnectTime = namedtuple('ConnectTime', 'state, time')

XMLRPC_BRINGUP_TIMEOUT_SECONDS = 60
SHILL_XMLRPC_LOG_PATH = '/var/log/shill_xmlrpc_server.log'


def _is_eureka_host(host):
    return host.get_os_type() == cast_os_host.OS_TYPE_CAST_OS


def get_xmlrpc_proxy(host):
    """Get a shill XMLRPC proxy for |host|.

    The returned object has no particular type.  Instead, when you call
    a method on the object, it marshalls the objects passed as arguments
    and uses them to make RPCs on the remote server.  Thus, you should
    read shill_xmlrpc_server.py to find out what methods are supported.

    @param host: host object representing a remote device.
    @return proxy object for remote XMLRPC server.

    """
    # Make sure the client library is on the device so that the proxy
    # code is there when we try to call it.
    if host.is_client_install_supported:
        client_at = autotest.Autotest(host)
        client_at.install()
    # This is the default port for shill xmlrpc server.
    server_port = constants.SHILL_XMLRPC_SERVER_PORT
    xmlrpc_server_command = constants.SHILL_XMLRPC_SERVER_COMMAND
    log_path = SHILL_XMLRPC_LOG_PATH
    command_name = constants.SHILL_XMLRPC_SERVER_CLEANUP_PATTERN
    rpc_server_host = host

    # Start up the XMLRPC proxy on the client
    proxy = rpc_server_host.rpc_server_tracker.xmlrpc_connect(
            xmlrpc_server_command,
            server_port,
            command_name=command_name,
            ready_test_name=constants.SHILL_XMLRPC_SERVER_READY_METHOD,
            timeout_seconds=XMLRPC_BRINGUP_TIMEOUT_SECONDS,
            logfile=log_path
      )
    return proxy


def _is_conductive(host):
    """Determine if the host is conductive based on AFE labels.

    @param host: A Host object.
    """
    info = host.host_info_store.get()
    conductive = info.get_label_value('conductive')
    return conductive.lower() == 'true'


class WiFiClient(site_linux_system.LinuxSystem):
    """WiFiClient is a thin layer of logic over a remote DUT in wifitests."""

    DEFAULT_PING_COUNT = 10
    COMMAND_PING = 'ping'

    MAX_SERVICE_GONE_TIMEOUT_SECONDS = 60

    # List of interface names we won't consider for use as "the" WiFi interface
    # on Android or CastOS hosts.
    WIFI_IF_BLACKLIST = ['p2p0', 'wfd0']

    UNKNOWN_BOARD_TYPE = 'unknown'

    # DBus device properties. Wireless interfaces should support these.
    WAKE_ON_WIFI_FEATURES = 'WakeOnWiFiFeaturesEnabled'
    NET_DETECT_SCAN_PERIOD = 'NetDetectScanPeriodSeconds'
    WAKE_TO_SCAN_PERIOD = 'WakeToScanPeriodSeconds'
    FORCE_WAKE_TO_SCAN_TIMER = 'ForceWakeToScanTimer'
    MAC_ADDRESS_RANDOMIZATION_SUPPORTED = 'MACAddressRandomizationSupported'
    MAC_ADDRESS_RANDOMIZATION_ENABLED = 'MACAddressRandomizationEnabled'

    CONNECTED_STATES = ['portal', 'no-connectivity', 'redirect-found',
                        'portal-suspected', 'online', 'ready']

    @property
    def machine_id(self):
        """@return string unique to a particular board/cpu configuration."""
        if self._machine_id:
            return self._machine_id

        uname_result = self.host.run('uname -m', ignore_status=True)
        kernel_arch = ''
        if not uname_result.exit_status and uname_result.stdout.find(' ') < 0:
            kernel_arch = uname_result.stdout.strip()
        cpu_info = self.host.run('cat /proc/cpuinfo').stdout.splitlines()
        cpu_count = len(filter(lambda x: x.lower().startswith('bogomips'),
                               cpu_info))
        cpu_count_str = ''
        if cpu_count:
            cpu_count_str = 'x%d' % cpu_count
        ghz_value = ''
        ghz_pattern = re.compile('([0-9.]+GHz)')
        for line in cpu_info:
            match = ghz_pattern.search(line)
            if match is not None:
                ghz_value = '_' + match.group(1)
                break

        return '%s_%s%s%s' % (self.board, kernel_arch, ghz_value, cpu_count_str)


    @property
    def powersave_on(self):
        """@return bool True iff WiFi powersave mode is enabled."""
        result = self.host.run("iw dev %s get power_save" % self.wifi_if)
        output = result.stdout.rstrip()       # NB: chop \n
        # Output should be either "Power save: on" or "Power save: off".
        find_re = re.compile('([^:]+):\s+(\w+)')
        find_results = find_re.match(output)
        if not find_results:
            raise error.TestFail('Failed to find power_save parameter '
                                 'in iw results.')

        return find_results.group(2) == 'on'


    @property
    def shill(self):
        """@return shill RPCProxy object."""
        return self._shill_proxy


    @property
    def client(self):
        """Deprecated accessor for the client host.

        The term client is used very loosely in old autotests and this
        accessor should not be used in new code.  Use host() instead.

        @return host object representing a remote DUT.

        """
        return self.host


    @property
    def command_ip(self):
        """@return string path to ip command."""
        return self._command_ip


    @property
    def command_iptables(self):
        """@return string path to iptables command."""
        return self._command_iptables


    @property
    def command_ping6(self):
        """@return string path to ping6 command."""
        return self._command_ping6


    @property
    def command_wpa_cli(self):
        """@return string path to wpa_cli command."""
        return self._command_wpa_cli


    @property
    def conductive(self):
        """@return True if the rig is conductive; False otherwise."""
        if self._conductive is None:
            self._conductive = _is_conductive(self.host)
        return self._conductive


    @conductive.setter
    def conductive(self, value):
        """Set the conductive member to True or False.

        @param value: boolean value to set the conductive member to.
        """
        self._conductive = value


    @property
    def module_name(self):
        """@return Name of kernel module in use by this interface."""
        return self._interface.module_name

    @property
    def parent_device_name(self):
        """
        @return Path of the parent device for the net device"""
        return self._interface.parent_device_name

    @property
    def wifi_if(self):
        """@return string wifi device on machine (e.g. mlan0)."""
        return self._wifi_if


    @property
    def wifi_mac(self):
        """@return string MAC address of self.wifi_if."""
        return self._interface.mac_address


    @property
    def wifi_ip(self):
        """@return string IPv4 address of self.wifi_if."""
        return self._interface.ipv4_address


    @property
    def wifi_ip_subnet(self):
        """@return string IPv4 subnet prefix of self.wifi_if."""
        return self._interface.ipv4_subnet


    @property
    def wifi_phy_name(self):
        """@return wiphy name (e.g., 'phy0') or None"""
        return self._interface.wiphy_name

    @property
    def wifi_signal_level(self):
        """Returns the signal level of this DUT's WiFi interface.

        @return int signal level of connected WiFi interface or None (e.g. -67).

        """
        return self._interface.signal_level

    @property
    def wifi_signal_level_all_chains(self):
        """Returns the signal level of all chains of this DUT's WiFi interface.

        @return int array signal level of each chain of connected WiFi interface
                or None (e.g. [-67, -60]).

        """
        return self._interface.signal_level_all_chains

    @staticmethod
    def assert_bsses_include_ssids(found_bsses, expected_ssids):
        """Verifies that |found_bsses| includes |expected_ssids|.

        @param found_bsses list of IwBss objects.
        @param expected_ssids list of string SSIDs.
        @raise error.TestFail if any element of |expected_ssids| is not found.

        """
        for ssid in expected_ssids:
            if not ssid:
                continue

            for bss in found_bsses:
                if bss.ssid == ssid:
                    break
            else:
                raise error.TestFail('SSID %s is not in scan results: %r' %
                                     (ssid, found_bsses))


    def wifi_noise_level(self, frequency_mhz):
        """Returns the noise level of this DUT's WiFi interface.

        @param frequency_mhz: frequency at which the noise level should be
               measured and reported.
        @return int signal level of connected WiFi interface in dBm (e.g. -67)
                or None if the value is unavailable.

        """
        return self._interface.noise_level(frequency_mhz)


    def __init__(self, client_host, result_dir, use_wpa_cli):
        """
        Construct a WiFiClient.

        @param client_host host object representing a remote host.
        @param result_dir string directory to store test logs/packet caps.
        @param use_wpa_cli bool True if we want to use |wpa_cli| commands for
               Android testing.

        """
        super(WiFiClient, self).__init__(client_host, 'client',
                                         inherit_interfaces=True)
        self._command_ip = 'ip'
        self._command_iptables = 'iptables'
        self._command_ping6 = 'ping6'
        self._command_wpa_cli = 'wpa_cli'
        self._machine_id = None
        self._result_dir = result_dir
        self._conductive = None

        if _is_eureka_host(self.host) and use_wpa_cli:
            # Look up the WiFi device (and its MAC) on the client.
            devs = self.iw_runner.list_interfaces(desired_if_type='managed')
            devs = [dev for dev in devs
                    if dev.if_name not in self.WIFI_IF_BLACKLIST]
            if not devs:
                raise error.TestFail('No wlan devices found on %s.' %
                                     self.host.hostname)

            if len(devs) > 1:
                logging.warning('Warning, found multiple WiFi devices on '
                                '%s: %r', self.host.hostname, devs)
            self._wifi_if = devs[0].if_name
            self._shill_proxy = wpa_cli_proxy.WpaCliProxy(
                    self.host, self._wifi_if)
            self._wpa_cli_proxy = self._shill_proxy
        else:
            self._shill_proxy = get_xmlrpc_proxy(self.host)
            interfaces = self._shill_proxy.list_controlled_wifi_interfaces()
            if not interfaces:
                logging.debug('No interfaces managed by shill. Rebooting host')
                self.host.reboot()
                raise error.TestError('No interfaces managed by shill on %s' %
                                      self.host.hostname)
            self._wifi_if = interfaces[0]
            self._wpa_cli_proxy = wpa_cli_proxy.WpaCliProxy(
                    self.host, self._wifi_if)
            self._raise_logging_level()
        self._interface = interface.Interface(self._wifi_if, host=self.host)
        logging.debug('WiFi interface is: %r',
                      self._interface.device_description)
        self._firewall_rules = []
        # All tests that use this object assume the interface starts enabled.
        self.set_device_enabled(self._wifi_if, True)
        # Turn off powersave mode by default.
        self.powersave_switch(False)
        # Invoke the |capabilities| property defined in the parent |Linuxsystem|
        # to workaround the lazy loading of the capabilities cache and supported
        # frequency list. This is needed for tests that may need access to these
        # when the DUT is unreachable (for ex: suspended).
        #pylint: disable=pointless-statement
        self.capabilities


    def _assert_method_supported(self, method_name):
        """Raise a TestNAError if the XMLRPC proxy has no method |method_name|.

        @param method_name: string name of method that should exist on the
                XMLRPC proxy.

        """
        if not self._supports_method(method_name):
            raise error.TestNAError('%s() is not supported' % method_name)


    def _raise_logging_level(self):
        """Raises logging levels for WiFi on DUT."""
        self.host.run('ff_debug --level -2', ignore_status=True)
        self.host.run('ff_debug +wifi', ignore_status=True)


    def is_vht_supported(self):
        """Returns True if VHT supported; False otherwise"""
        return self.CAPABILITY_VHT in self.capabilities


    def is_5ghz_supported(self):
        """Returns True if 5Ghz bands are supported; False otherwise."""
        return self.CAPABILITY_5GHZ in self.capabilities


    def is_ibss_supported(self):
        """Returns True if IBSS mode is supported; False otherwise."""
        return self.CAPABILITY_IBSS in self.capabilities


    def is_frequency_supported(self, frequency):
        """Returns True if the given frequency is supported; False otherwise.

        @param frequency: int Wifi frequency to check if it is supported by
                          DUT.
        """
        return frequency in self.phys_for_frequency


    def _supports_method(self, method_name):
        """Checks if |method_name| is supported on the remote XMLRPC proxy.

        autotest will, for their own reasons, install python files in the
        autotest client package that correspond the version of the build
        rather than the version running on the autotest drone.  This
        creates situations where we call methods on the client XMLRPC proxy
        that don't exist in that version of the code.  This detects those
        situations so that we can degrade more or less gracefully.

        @param method_name: string name of method that should exist on the
                XMLRPC proxy.
        @return True if method is available, False otherwise.

        """
        supported = (_is_eureka_host(self.host)
                     or method_name in self._shill_proxy.system.listMethods())
        if not supported:
            logging.warning('%s() is not supported on older images',
                            method_name)
        return supported


    def close(self):
        """Tear down state associated with the client."""
        self.stop_capture()
        self.powersave_switch(False)
        self.shill.clean_profiles()
        super(WiFiClient, self).close()


    def firewall_open(self, proto, src):
        """Opens up firewall to run netperf tests.

        By default, we have a firewall rule for NFQUEUE (see crbug.com/220736).
        In order to run netperf test, we need to add a new firewall rule BEFORE
        this NFQUEUE rule in the INPUT chain.

        @param proto a string, test traffic protocol, e.g. udp, tcp.
        @param src a string, subnet/mask.

        @return a string firewall rule added.

        """
        rule = 'INPUT -s %s/32 -p %s -m %s -j ACCEPT' % (src, proto, proto)
        self.host.run('%s -I %s' % (self._command_iptables, rule))
        self._firewall_rules.append(rule)
        return rule


    def firewall_cleanup(self):
        """Cleans up firewall rules."""
        for rule in self._firewall_rules:
            self.host.run('%s -D %s' % (self._command_iptables, rule))
        self._firewall_rules = []


    def sync_host_times(self):
        """Set time on our DUT to match local time."""
        epoch_seconds = time.time()
        self.shill.sync_time_to(epoch_seconds)


    def collect_debug_info(self, local_save_dir_prefix):
        """Collect any debug information needed from the DUT

        This invokes the |collect_debug_info| RPC method to trigger
        bugreport/logcat collection and then transfers the logs to the
        server.

        @param local_save_dir_prefix Used as a prefix for local save directory.
        """
        pass


    def check_iw_link_value(self, iw_link_key, desired_value):
        """Assert that the current wireless link property is |desired_value|.

        @param iw_link_key string one of IW_LINK_KEY_* defined in iw_runner.
        @param desired_value string desired value of iw link property.

        """
        actual_value = self.get_iw_link_value(iw_link_key)
        desired_value = str(desired_value)
        if actual_value != desired_value:
            raise error.TestFail('Wanted iw link property %s value %s, but '
                                 'got %s instead.' % (iw_link_key,
                                                      desired_value,
                                                      actual_value))


    def get_iw_link_value(self, iw_link_key):
        """Get the current value of a link property for this WiFi interface.

        @param iw_link_key string one of IW_LINK_KEY_* defined in iw_runner.

        """
        return self.iw_runner.get_link_value(self.wifi_if, iw_link_key)


    def powersave_switch(self, turn_on):
        """Toggle powersave mode for the DUT.

        @param turn_on bool True iff powersave mode should be turned on.

        """
        mode = 'off'
        if turn_on:
            mode = 'on'
        # Turn ON interface and set power_save option.
        self.host.run('ifconfig %s up' % self.wifi_if)
        self.host.run('iw dev %s set power_save %s' % (self.wifi_if, mode))


    def timed_scan(self, frequencies, ssids, scan_timeout_seconds=10,
                   retry_timeout_seconds=10):
        """Request timed scan to discover given SSIDs.

        This method will retry for a default of |retry_timeout_seconds| until it
        is able to successfully kick off a scan.  Sometimes, the device on the
        DUT claims to be busy and rejects our requests. It will raise error
        if the scan did not complete within |scan_timeout_seconds| or it was
        not able to discover the given SSIDs.

        @param frequencies list of int WiFi frequencies to scan for.
        @param ssids list of string ssids to probe request for.
        @param scan_timeout_seconds: float number of seconds the scan
                operation not to exceed.
        @param retry_timeout_seconds: float number of seconds to retry scanning
                if the interface is busy.  This does not retry if certain
                SSIDs are missing from the results.
        @return time in seconds took to complete scan request.

        """
        # the poll method returns the result of the func
        scan_result = utils.poll_for_condition(
                condition=lambda: self.iw_runner.timed_scan(
                                          self.wifi_if,
                                          frequencies=frequencies,
                                          ssids=ssids),
                exception=error.TestFail('Unable to trigger scan on client'),
                timeout=retry_timeout_seconds,
                sleep_interval=0.5)
        # Verify scan operation completed within given timeout
        if scan_result.time > scan_timeout_seconds:
            raise error.TestFail('Scan time %.2fs exceeds the scan timeout' %
                                 (scan_result.time))

        # Verify all ssids are discovered
        self.assert_bsses_include_ssids(scan_result.bss_list, ssids)

        logging.info('Wifi scan completed in %.2f seconds', scan_result.time)
        return scan_result.time


    def scan(self, frequencies, ssids, timeout_seconds=10, require_match=True):
        """Request a scan and (optionally) check that requested SSIDs appear in
        the results.

        This method will retry for a default of |timeout_seconds| until it is
        able to successfully kick off a scan.  Sometimes, the device on the DUT
        claims to be busy and rejects our requests.

        If |ssids| is non-empty, we will speficially probe for those SSIDs.

        If |require_match| is True, we will verify that every element
        of |ssids| was found in scan results.

        @param frequencies list of int WiFi frequencies to scan for.
        @param ssids list of string ssids to probe request for.
        @param timeout_seconds: float number of seconds to retry scanning
                if the interface is busy.  This does not retry if certain
                SSIDs are missing from the results.
        @param require_match: bool True if we must find |ssids|.

        """
        bss_list = utils.poll_for_condition(
                condition=lambda: self.iw_runner.scan(
                        self.wifi_if,
                        frequencies=frequencies,
                        ssids=ssids),
                exception=error.TestFail('Unable to trigger scan on client'),
                timeout=timeout_seconds,
                sleep_interval=0.5)

        if require_match:
            self.assert_bsses_include_ssids(bss_list, ssids)


    def wait_for_bsses(self, ssid, num_bss_expected, timeout_seconds=15):
      """Wait for all BSSes associated with given SSID to be discovered in the
      scan.

      @param ssid string name of network being queried
      @param num_bss_expected int number of BSSes expected
      @param timeout_seconds int seconds to wait for BSSes to be discovered

      """
      # If the scan returns None, return 0, else return the matching count
      num_bss_actual = 0
      def are_all_bsses_discovered():
          """Determine if all BSSes associated with the SSID from parent
          function are discovered in the scan

          @return boolean representing whether the expected bss count matches
          how many in the scan match the given ssid
          """
          self.claim_wifi_if() # Stop shill/supplicant scans
          try:
            scan_results = self.iw_runner.scan(
                    self.wifi_if,
                    frequencies=[],
                    ssids=[ssid])
            if scan_results is None:
                return False
            num_bss_actual = sum(ssid == bss.ssid for bss in scan_results)
            return num_bss_expected == num_bss_actual
          finally:
            self.release_wifi_if()

      utils.poll_for_condition(
              condition=are_all_bsses_discovered,
              exception=error.TestFail('Failed to discover all BSSes. Found %d,'
                                      ' wanted %d with SSID %s' %
                                      (num_bss_actual, num_bss_expected, ssid)),
              timeout=timeout_seconds,
              sleep_interval=0.5)

    def wait_for_service_states(self, ssid, states, timeout_seconds):
        """Waits for a WiFi service to achieve one of |states|.

        @param ssid string name of network being queried
        @param states tuple list of states for which the caller is waiting
        @param timeout_seconds int seconds to wait for a state in |states|

        """
        logging.info('Waiting for %s to reach one of %r...', ssid, states)
        success, state, duration  = self._shill_proxy.wait_for_service_states(
                ssid, states, timeout_seconds)
        logging.info('...ended up in state \'%s\' (%s) after %f seconds.',
                     state, 'success' if success else 'failure', duration)
        return success, state, duration


    def do_suspend(self, seconds):
        """Puts the DUT in suspend power state for |seconds| seconds.

        @param seconds: The number of seconds to suspend the device.

        """
        logging.info('Suspending DUT for %d seconds...', seconds)
        self._shill_proxy.do_suspend(seconds)
        logging.info('...done suspending')


    def do_suspend_bg(self, seconds):
        """Suspend DUT using the power manager - non-blocking.

        @param seconds: The number of seconds to suspend the device.

        """
        logging.info('Suspending DUT (in background) for %d seconds...',
                     seconds)
        self._shill_proxy.do_suspend_bg(seconds)


    def clear_supplicant_blacklist(self):
        """Clear's the AP blacklist on the DUT.

        @return stdout and stderror returns passed from wpa_cli command.

        """
        result = self._wpa_cli_proxy.run_wpa_cli_cmd('blacklist clear',
                                                     check_result=False);
        logging.info('wpa_cli blacklist clear: out:%r err:%r', result.stdout,
                     result.stderr)
        return result.stdout, result.stderr


    def get_active_wifi_SSIDs(self):
        """Get a list of visible SSID's around the DUT

        @return list of string SSIDs

        """
        self._assert_method_supported('get_active_wifi_SSIDs')
        return self._shill_proxy.get_active_wifi_SSIDs()


    def set_device_enabled(self, wifi_interface, value,
                           fail_on_unsupported=False):
        """Enable or disable the WiFi device.

        @param wifi_interface: string name of interface being modified.
        @param enabled: boolean; true if this device should be enabled,
                false if this device should be disabled.
        @return True if it worked; False, otherwise.

        """
        if fail_on_unsupported:
            self._assert_method_supported('set_device_enabled')
        elif not self._supports_method('set_device_enabled'):
            return False
        return self._shill_proxy.set_device_enabled(wifi_interface, value)


    def add_arp_entry(self, ip_address, mac_address):
        """Add an ARP entry to the table associated with the WiFi interface.

        @param ip_address: string IP address associated with the new ARP entry.
        @param mac_address: string MAC address associated with the new ARP
                entry.

        """
        self.host.run('ip neigh add %s lladdr %s dev %s nud perm' %
                      (ip_address, mac_address, self.wifi_if))

    def add_wake_packet_source(self, source_ip):
        """Add |source_ip| as a source that can wake us up with packets.

        @param source_ip: IP address from which to wake upon receipt of packets

        @return True if successful, False otherwise.

        """
        return self._shill_proxy.add_wake_packet_source(
            self.wifi_if, source_ip)


    def remove_wake_packet_source(self, source_ip):
        """Remove |source_ip| as a source that can wake us up with packets.

        @param source_ip: IP address to stop waking on packets from

        @return True if successful, False otherwise.

        """
        return self._shill_proxy.remove_wake_packet_source(
            self.wifi_if, source_ip)


    def remove_all_wake_packet_sources(self):
        """Remove all IPs as sources that can wake us up with packets.

        @return True if successful, False otherwise.

        """
        return self._shill_proxy.remove_all_wake_packet_sources(self.wifi_if)


    def wake_on_wifi_features(self, features):
        """Shill supports programming the NIC to wake on special kinds of
        incoming packets, or on changes to the available APs (disconnect,
        coming in range of a known SSID). This method allows you to configure
        what wake-on-WiFi mechanisms are active. It returns a context manager,
        because this is a system-wide setting and we don't want it to persist
        across different tests.

        If you enable wake-on-packet, then the IPs registered by
        add_wake_packet_source will be able to wake the system from suspend.

        The correct way to use this method is:

        with client.wake_on_wifi_features(WAKE_ON_WIFI_DARKCONNECT):
            ...

        @param features: string from the WAKE_ON_WIFI constants above.

        @return a context manager for the features.

        """
        return TemporaryDeviceDBusProperty(self._shill_proxy,
                                           self.wifi_if,
                                           self.WAKE_ON_WIFI_FEATURES,
                                           features)


    def net_detect_scan_period_seconds(self, period):
        """Sets the period between net detect scans performed by the NIC to look
        for whitelisted SSIDs to |period|. This setting only takes effect if the
        NIC is programmed to wake on SSID.

        The correct way to use this method is:

        with client.net_detect_scan_period_seconds(60):
            ...

        @param period: integer number of seconds between NIC net detect scans

        @return a context manager for the net detect scan period

        """
        return TemporaryDeviceDBusProperty(self._shill_proxy,
                                           self.wifi_if,
                                           self.NET_DETECT_SCAN_PERIOD,
                                           period)


    def wake_to_scan_period_seconds(self, period):
        """Sets the period between RTC timer wakeups where the system is woken
        from suspend to perform scans. This setting only takes effect if the
        NIC is programmed to wake on SSID. Use as with
        net_detect_scan_period_seconds.

        @param period: integer number of seconds between wake to scan RTC timer
                wakes.

        @return a context manager for the net detect scan period

        """
        return TemporaryDeviceDBusProperty(self._shill_proxy,
                                           self.wifi_if,
                                           self.WAKE_TO_SCAN_PERIOD,
                                           period)


    def force_wake_to_scan_timer(self, is_forced):
        """Sets the boolean value determining whether or not to force the use of
        the wake to scan RTC timer, which wakes the system from suspend
        periodically to scan for networks.

        @param is_forced: boolean whether or not to force the use of the wake to
                scan timer

        @return a context manager for the net detect scan period

        """
        return TemporaryDeviceDBusProperty(self._shill_proxy,
                                           self.wifi_if,
                                           self.FORCE_WAKE_TO_SCAN_TIMER,
                                           is_forced)


    def mac_address_randomization(self, enabled):
        """Sets the boolean value determining whether or not to enable MAC
        address randomization. This instructs the NIC to randomize the last
        three octets of the MAC address used in probe requests while
        disconnected to make the DUT harder to track.

        If MAC address randomization is not supported on this DUT and the
        caller tries to turn it on, this raises a TestNAError.

        @param enabled: boolean whether or not to enable MAC address
                randomization

        @return a context manager for the MAC address randomization property

        """
        if not self._shill_proxy.get_dbus_property_on_device(
                self.wifi_if, self.MAC_ADDRESS_RANDOMIZATION_SUPPORTED):
            if enabled:
                raise error.TestNAError(
                    'MAC address randomization not supported')
            else:
                # Return a no-op context manager.
                return contextlib.nested()

        return TemporaryDeviceDBusProperty(
                self._shill_proxy,
                self.wifi_if,
                self.MAC_ADDRESS_RANDOMIZATION_ENABLED,
                enabled)


    def request_roam(self, bssid):
        """Request that we roam to the specified BSSID.

        Note that this operation assumes that:

        1) We're connected to an SSID for which |bssid| is a member.
        2) There is a BSS with an appropriate ID in our scan results.

        This method does not check for success of either the command or
        the roaming operation.

        @param bssid: string MAC address of bss to roam to.

        """
        self._wpa_cli_proxy.run_wpa_cli_cmd('roam %s' % bssid,
                                            check_result=False);
        return True


    def request_roam_dbus(self, bssid, iface):
        """Request that we roam to the specified BSSID through dbus.

        Note that this operation assumes that:

        1) We're connected to an SSID for which |bssid| is a member.
        2) There is a BSS with an appropriate ID in our scan results.

        @param bssid: string MAC address of bss to roam to.
        @param iface: interface to use

        """
        self._assert_method_supported('request_roam_dbus')
        self._shill_proxy.request_roam_dbus(bssid, iface)


    def wait_for_roam(self, bssid, timeout_seconds=10.0):
        """Wait for a roam to the given |bssid|.

        @param bssid: string bssid to expect a roam to
                (e.g.  '00:11:22:33:44:55').
        @param timeout_seconds: float number of seconds to wait for a roam.
        @return True if we detect an association to the given |bssid| within
                |timeout_seconds|.

        """
        def attempt_roam():
            """Perform a roam to the given |bssid|

            @return True if there is an assocation between the given |bssid|
                    and that association is detected within the timeout frame
            """
            current_bssid = self.iw_runner.get_current_bssid(self.wifi_if)
            logging.debug('Current BSSID is %s.', current_bssid)
            return current_bssid == bssid

        start_time = time.time()
        duration = 0.0
        try:
            success = utils.poll_for_condition(
                    condition=attempt_roam,
                    timeout=timeout_seconds,
                    sleep_interval=0.5,
                    desc='Wait for a roam to the given bssid')
        # wait_for_roam should return False on timeout
        except utils.TimeoutError:
            success = False

        duration = time.time() - start_time
        logging.debug('%s to %s in %f seconds.',
                      'Roamed ' if success else 'Failed to roam ',
                      bssid,
                      duration)
        return success


    def wait_for_ssid_vanish(self, ssid):
        """Wait for shill to notice that there are no BSS's for an SSID present.

        Raise a test failing exception if this does not come to pass.

        @param ssid: string SSID of the network to require be missing.

        """
        def is_missing_yet():
            """Determine if the ssid is removed from the service list yet

            @return True if the ssid is not found in the service list
            """
            visible_ssids = self.get_active_wifi_SSIDs()
            logging.info('Got service list: %r', visible_ssids)
            if ssid not in visible_ssids:
                return True
            self.scan(frequencies=[], ssids=[], timeout_seconds=30)

        utils.poll_for_condition(
                condition=is_missing_yet, # func
                exception=error.TestFail('shill should mark BSS not present'),
                timeout=self.MAX_SERVICE_GONE_TIMEOUT_SECONDS,
                sleep_interval=0)

    def reassociate(self, timeout_seconds=10):
        """Reassociate to the connected network.

        @param timeout_seconds: float number of seconds to wait for operation
                to complete.

        """
        logging.info('Attempt to reassociate')
        with self.iw_runner.get_event_logger() as logger:
            logger.start()
            # Issue reattach command to wpa_supplicant
            self._wpa_cli_proxy.run_wpa_cli_cmd('reattach');

            # Wait for the timeout seconds for association to complete
            time.sleep(timeout_seconds)

            # Stop iw event logger
            logger.stop()

            # Get association time based on the iw event log
            reassociate_time = logger.get_reassociation_time()
            if reassociate_time is None or reassociate_time > timeout_seconds:
                raise error.TestFail(
                        'Failed to reassociate within given timeout')
            logging.info('Reassociate time: %.2f seconds', reassociate_time)


    def wait_for_connection(self, ssid, timeout_seconds=30, freq=None,
                            ping_ip=None, desired_subnet=None):
        """Verifies a connection to network ssid, optionally verifying
        frequency, ping connectivity and subnet.

        @param ssid string ssid of the network to check.
        @param timeout_seconds int number of seconds to wait for
                connection on the given frequency.
        @param freq int frequency of network to check.
        @param ping_ip string ip address to ping for verification.
        @param desired_subnet string expected subnet in which client
                ip address should reside.

        @returns a named tuple of (state, time)
        """
        start_time = time.time()
        duration = lambda: time.time() - start_time
        state = [None] # need mutability for the nested method to save state

        def verify_connection():
            """Verify the connection and perform optional operations
            as defined in the parent function

            @return False if there is a failure in the connection or
                    the prescribed verification steps
                    The named tuple ConnectTime otherwise, with the state
                    and connection time from waiting for service states
            """
            success, state[0], conn_time = self.wait_for_service_states(
                    ssid,
                    self.CONNECTED_STATES,
                    int(math.ceil(timeout_seconds - duration())))
            if not success:
                return False

            if freq:
                actual_freq = self.get_iw_link_value(
                        iw_runner.IW_LINK_KEY_FREQUENCY)
                if str(freq) != actual_freq:
                    logging.debug(
                            'Waiting for desired frequency %s (got %s).',
                            freq,
                            actual_freq)
                    return False

            if desired_subnet:
                actual_subnet = self.wifi_ip_subnet
                if actual_subnet != desired_subnet:
                    logging.debug(
                            'Waiting for desired subnet %s (got %s).',
                            desired_subnet,
                            actual_subnet)
                    return False

            if ping_ip:
                ping_config = ping_runner.PingConfig(ping_ip)
                self.ping(ping_config)

            return ConnectTime(state[0], conn_time)

        freq_error_str = (' on frequency %d Mhz' % freq) if freq else ''

        return utils.poll_for_condition(
                condition=verify_connection,
                exception=error.TestFail(
                        'Failed to connect to "%s"%s in %f seconds (state=%s)' %
                        (ssid,
                        freq_error_str,
                        duration(),
                        state[0])),
                timeout=timeout_seconds,
                sleep_interval=0)

    @contextmanager
    def assert_disconnect_count(self, count):
        """Context asserting |count| disconnects for the context lifetime.

        Creates an iw logger during the lifetime of the context and asserts
        that the client disconnects exactly |count| times.

        @param count int the expected number of disconnections.

        """
        with self.iw_runner.get_event_logger() as logger:
            logger.start()
            yield
            logger.stop()
            if logger.get_disconnect_count() != count:
                raise error.TestFail(
                    'Client disconnected %d times; expected %d' %
                    (logger.get_disconnect_count(), count))


    def assert_no_disconnects(self):
        """Context asserting no disconnects for the context lifetime."""
        return self.assert_disconnect_count(0)


    @contextmanager
    def assert_disconnect_event(self):
        """Context asserting at least one disconnect for the context lifetime.

        Creates an iw logger during the lifetime of the context and asserts
        that the client disconnects at least one time.

        """
        with self.iw_runner.get_event_logger() as logger:
            logger.start()
            yield
            logger.stop()
            if logger.get_disconnect_count() == 0:
                raise error.TestFail('Client did not disconnect')


    def get_num_card_resets(self):
        """Get card reset count."""
        reset_msg = '[m]wifiex_sdio_card_reset'
        result = self.host.run('grep -c %s /var/log/messages' % reset_msg,
                               ignore_status=True)
        if result.exit_status == 1:
            return 0
        count = int(result.stdout.strip())
        return count


    def get_disconnect_reasons(self):
        """Get disconnect reason codes."""
        disconnect_reason_msg = "updated DisconnectReason "
        disconnect_reason_cleared = "clearing DisconnectReason for "
        result = self.host.run('grep -E "(%s|%s)" /var/log/net.log' %
                               (disconnect_reason_msg,
                               disconnect_reason_cleared),
                               ignore_status=True)
        if result.exit_status == 1:
            return None

        lines = result.stdout.strip().split('\n')
        disconnect_reasons = []
        disconnect_reason_regex = re.compile(' to (\D?\d+)')

        found = False
        for line in reversed(lines):
          match = disconnect_reason_regex.search(line)
          if match is not None:
            disconnect_reasons.append(match.group(1))
            found = True
          else:
            if (found):
                break
        return list(reversed(disconnect_reasons))


    def release_wifi_if(self):
        """Release the control over the wifi interface back to normal operation.

        This will give the ownership of the wifi interface back to shill and
        wpa_supplicant.

        """
        self.set_device_enabled(self._wifi_if, True)


    def claim_wifi_if(self):
        """Claim the control over the wifi interface from this wifi client.

        This claim the ownership of the wifi interface from shill and
        wpa_supplicant. The wifi interface should be UP when this call returns.

        """
        # Disabling a wifi device in shill will remove that device from
        # wpa_supplicant as well.
        self.set_device_enabled(self._wifi_if, False)

        # Wait for shill to bring down the wifi interface.
        is_interface_down = lambda: not self._interface.is_up
        utils.poll_for_condition(
                is_interface_down,
                timeout=INTERFACE_DOWN_WAIT_TIME_SECONDS,
                sleep_interval=0.5,
                desc='Timeout waiting for interface to go down.')
        # Bring up the wifi interface to allow the test to use the interface.
        self.host.run('%s link set %s up' % (self.cmd_ip, self.wifi_if))


    def set_sched_scan(self, enable, fail_on_unsupported=False):
        """enable/disable scheduled scan.

        @param enable bool flag indicating to enable/disable scheduled scan.

        """
        if fail_on_unsupported:
            self._assert_method_supported('set_sched_scan')
        elif not self._supports_method('set_sched_scan'):
            return False
        return self._shill_proxy.set_sched_scan(enable)


    def check_connected_on_last_resume(self):
        """Checks whether the DUT was connected on its last resume.

        Checks that the DUT was connected after waking from suspend by parsing
        the last instance shill log message that reports shill's connection
        status on resume. Fails the test if this log message reports that
        the DUT woke up disconnected.

        """
        # As of build R43 6913.0.0, the shill log message from the function
        # OnAfterResume is called as soon as shill resumes from suspend, and
        # will report whether or not shill is connected. The log message will
        # take one of the following two forms:
        #
        #       [...] [INFO:wifi.cc(1941)] OnAfterResume: connected
        #       [...] [INFO:wifi.cc(1941)] OnAfterResume: not connected
        #
        # where 1941 is an arbitrary PID number. By checking if the last
        # instance of this message contains the substring "not connected", we
        # can determine whether or not shill was connected on its last resume.
        connection_status_log_regex_str = 'INFO:wifi\.cc.*OnAfterResume'
        not_connected_substr = 'not connected'
        connected_substr = 'connected'

        cmd = ('grep -E %s /var/log/net.log | tail -1' %
               connection_status_log_regex_str)
        connection_status_log = self.host.run(cmd).stdout
        if not connection_status_log:
            raise error.TestFail('Could not find resume connection status log '
                                 'message.')

        logging.debug('Connection status message:\n%s', connection_status_log)
        if not_connected_substr in connection_status_log:
            raise error.TestFail('Client was not connected upon waking from '
                                 'suspend.')

        if not connected_substr in connection_status_log:
            raise error.TestFail('Last resume log message did not contain '
                                 'connection status.')

        logging.info('Client was connected upon waking from suspend.')


    def check_wake_on_wifi_throttled(self):
        """
        Checks whether wake on WiFi was throttled on the DUT on the last dark
        resume. Check for this by parsing shill logs for a throttling message.

        """
        # We are looking for an dark resume error log message indicating that
        # wake on WiFi was throttled. This is an example of the error message:
        #     [...] [ERROR:wake_on_wifi.cc(1304)] OnDarkResume: Too many dark \
        #       resumes; disabling wake on WiFi temporarily
        dark_resume_log_regex_str = 'ERROR:wake_on_wifi\.cc.*OnDarkResume:.*'
        throttled_msg_substr = ('Too many dark resumes; disabling wake on '
                                   'WiFi temporarily')

        cmd = ('grep -E %s /var/log/net.log | tail -1' %
               dark_resume_log_regex_str)
        last_dark_resume_error_log = self.host.run(cmd).stdout
        if not last_dark_resume_error_log:
            raise error.TestFail('Could not find a dark resume log message.')

        logging.debug('Last dark resume log message:\n%s',
                last_dark_resume_error_log)
        if not throttled_msg_substr in last_dark_resume_error_log:
            raise error.TestFail('Wake on WiFi was not throttled on the last '
                                 'dark resume.')

        logging.info('Wake on WiFi was throttled on the last dark resume.')


    def shill_debug_log(self, message):
        """Logs a message to the shill log (i.e. /var/log/net.log). This
           message will be logged at the DEBUG level.

        @param message: the message to be logged.

        """
        logging.info(message)
        logger_command = ('/usr/bin/logger'
                          ' --tag shill'
                          ' --priority daemon.debug'
                          ' "%s"' % utils.sh_escape(message))
        self.host.run(logger_command)


    def is_wake_on_wifi_supported(self):
        """Returns true iff wake-on-WiFi is supported by the DUT."""

        if (self.shill.get_dbus_property_on_device(
                    self.wifi_if, self.WAKE_ON_WIFI_FEATURES) ==
             WAKE_ON_WIFI_NOT_SUPPORTED):
            return False
        return True


    def set_manager_property(self, prop_name, prop_value):
        """Sets the given manager property to the value provided.

        @param prop_name: the property to be set
        @param prop_value: value to assign to the prop_name
        @return a context manager for the setting

        """
        return TemporaryManagerDBusProperty(self._shill_proxy,
                                            prop_name,
                                            prop_value)


class TemporaryDeviceDBusProperty:
    """Utility class to temporarily change a dbus property for the WiFi device.

    Since dbus properties are global and persistent settings, we want
    to make sure that we change them back to what they were before the test
    started.

    """

    def __init__(self, shill_proxy, iface, prop_name, value):
        """Construct a TemporaryDeviceDBusProperty context manager.


        @param shill_proxy: the shill proxy to use to communicate via dbus
        @param iface: device whose property to change (e.g. 'wlan0')
        @param prop_name: the name of the property we want to set
        @param value: the desired value of the property

        """
        self._shill = shill_proxy
        self._interface = iface
        self._prop_name = prop_name
        self._value = value
        self._saved_value = None


    def __enter__(self):
        logging.info('- Setting property %s on device %s',
                     self._prop_name,
                     self._interface)

        self._saved_value = self._shill.get_dbus_property_on_device(
                self._interface, self._prop_name)
        if self._saved_value is None:
            raise error.TestFail('Device or property not found.')
        if not self._shill.set_dbus_property_on_device(self._interface,
                                                       self._prop_name,
                                                       self._value):
            raise error.TestFail('Could not set property')

        logging.info('- Changed value from %s to %s',
                     self._saved_value,
                     self._value)


    def __exit__(self, exception, value, traceback):
        logging.info('- Resetting property %s', self._prop_name)

        if not self._shill.set_dbus_property_on_device(self._interface,
                                                       self._prop_name,
                                                       self._saved_value):
            raise error.TestFail('Could not reset property')


class TemporaryManagerDBusProperty:
    """Utility class to temporarily change a Manager dbus property.

    Since dbus properties are global and persistent settings, we want
    to make sure that we change them back to what they were before the test
    started.

    """

    def __init__(self, shill_proxy, prop_name, value):
        """Construct a TemporaryManagerDBusProperty context manager.

        @param shill_proxy: the shill proxy to use to communicate via dbus
        @param prop_name: the name of the property we want to set
        @param value: the desired value of the property

        """
        self._shill = shill_proxy
        self._prop_name = prop_name
        self._value = value
        self._saved_value = None


    def __enter__(self):
        logging.info('- Setting Manager property: %s', self._prop_name)

        self._saved_value = self._shill.get_manager_property(
                self._prop_name)
        if self._saved_value is None:
            self._saved_value = ""
            if not self._shill.set_optional_manager_property(self._prop_name,
                                                             self._value):
                raise error.TestFail('Could not set optional manager property.')
        else:
            setprop_result = self._shill.set_manager_property(self._prop_name,
                                                              self._value)
            if not setprop_result:
                raise error.TestFail('Could not set manager property')

        logging.info('- Changed value from [%s] to [%s]',
                     self._saved_value,
                     self._value)


    def __exit__(self, exception, value, traceback):
        logging.info('- Resetting property %s to [%s]',
                     self._prop_name,
                     self._saved_value)

        if not self._shill.set_manager_property(self._prop_name,
                                                self._saved_value):
            raise error.TestFail('Could not reset manager property')
