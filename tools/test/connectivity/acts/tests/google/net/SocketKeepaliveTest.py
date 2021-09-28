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

import random
import re
import time
from acts import asserts
from acts import base_test
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_test_utils as cutils
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.net import socket_test_utils as sutils
from acts.test_utils.net.net_test_utils import start_tcpdump
from acts.test_utils.net.net_test_utils import stop_tcpdump
from acts.test_utils.wifi import wifi_test_utils as wutils
from scapy.all import rdpcap
from scapy.all import Scapy_Exception
from scapy.all import TCP
from scapy.all import UDP


ACK = "A"
DROPPED_IPV4_KA_ACK = "DROPPED_IPV4_KEEPALIVE_ACK"
MIN_TEST_KA_INTERVAL = 10
# max keepalive interval is kept to 60 for test purposes
MAX_TEST_KA_INTERVAL = 60
# sleep time to test keepalives. this is set to a prime number to prevent
# race conditions and as a result flaky tests.
# Ex: if keepalive is started with 42s time interval, we expect 4 keepalives
# after 181s.
SLEEP_TIME = 181
STRESS_COUNT = 5
SUPPORTED_KERNEL_VERSION = 4.8
TCP_SERVER_PORT = 853
UDP_SERVER_PORT = 4500


class SocketKeepaliveTest(base_test.BaseTestClass):
    """Tests for Socket keepalive."""

    def __init__(self, controllers):
        """List and order of tests to run."""
        base_test.BaseTestClass.__init__(self, controllers)
        self.tests = (
            "test_tcp_socket_keepalive_start_stop_wifi",
            "test_natt_socket_keepalive_start_stop_wifi",
            "test_tcp_socket_keepalive_error_client_socket_close_wifi",
            "test_tcp_socket_keepalive_over_max_keepalive_limit_wifi",
            "test_tcp_socket_keepalive_invalid_interval",
            "test_natt_socket_keepalive_invalid_interval",
            "test_tcp_socket_keepalive_start_stop_multiple_keepalives_wifi",
            "test_tcp_socket_keepalive_start_stop_stress_wifi",
            "test_tcp_socket_keepalive_on_data_received_wifi",
            "test_natt_socket_keepalive_start_stop_lte",)

    def setup_class(self):
        """Setup devices for tests and unpack params."""

        self.dut = self.android_devices[1]
        # remote_server_2 is the host machine to test OnDataCallback and Error
        # callbacks. The server program sends data to the DUT while keepalive
        # is enabled. It also closes the server socket to verify Error callback
        # 'test_tcp_socket_keepalive_on_data_received_wifi' requires this.
        # remote_server is the host machine to test all other test cases.
        # This host listens, accepts connections and verifies the keepalive
        # intervals.
        req_params = ("wifi_network", "remote_server", "remote_server_2")
        self.unpack_userparams(req_params)
        nutils.verify_lte_data_and_tethering_supported(self.dut)
        self.max_ka_net = self.dut.droid.getSupportedKeepalivesForNetwork()
        self.log.info("Max Keepalives for mobile network: %s" % self.max_ka_net)
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, self.wifi_network["SSID"])
        wutils.wifi_connect(self.dut, self.wifi_network)
        self.max_ka_wifi = self.dut.droid.getSupportedKeepalivesForNetwork()
        self.log.info("Max Keepalives on wifi network: %s" % self.max_ka_wifi)
        self.kernel_version = self._get_kernel_version(self.dut)
        self.log.info("Kernel version: %s" % self.kernel_version)

        self.host_ip = self.remote_server["ip_addr"]
        self.tcpdump_pid = None
        self.tcp_sockets = []
        self.socket_keepalives = []
        self.gce_tcpdump_pid = None
        self.udp_encap = None

    def setup_test(self):
        asserts.skip_if(
            self.test_name.startswith("test_tcp") and \
                self.kernel_version < SUPPORTED_KERNEL_VERSION,
            "TCP Keepalive is not supported on kernel %s. Need at least %s" %
            (self.kernel_version, SUPPORTED_KERNEL_VERSION))

        if self.test_name.endswith("_lte"):
            wutils.wifi_toggle_state(self.dut, False)
        time.sleep(3)
        link_prop = self.dut.droid.connectivityGetActiveLinkProperties()
        iface = link_prop["InterfaceName"]
        self.log.info("Iface: %s" % iface)
        self.dut_ip = self.dut.droid.connectivityGetIPv4Addresses(iface)[0]
        self.tcpdump_pid = start_tcpdump(self.dut, self.test_name)

    def teardown_test(self):
        if self.tcpdump_pid:
            stop_tcpdump(self.dut, self.tcpdump_pid, self.test_name)
            self.tcpdump_pid = None
        if self.gce_tcpdump_pid:
            host = self.remote_server
            if "_on_data_" in self.test_name:
                host = self.remote_server_2
            nutils.stop_tcpdump_gce_server(
                self.dut, self.gce_tcpdump_pid, None, host)
            self.gce_tcpdump_pid = None
        for ska in self.socket_keepalives:
            cutils.stop_socket_keepalive(self.dut, ska)
        self.socket_keepalives = []
        for sock in self.tcp_sockets:
            sutils.shutdown_socket(self.dut, sock)
        self.tcp_sockets = []
        if self.udp_encap:
            self.dut.droid.ipSecCloseUdpEncapsulationSocket(self.udp_encap)
        self.udp_encap = None
        wutils.wifi_toggle_state(self.dut, True)

    def teardown_class(self):
        wutils.reset_wifi(self.dut)

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)

    ### Helper functions

    def _get_kernel_version(self, ad):
        """Get the kernel version on the device.

        Args:
            ad: android device object.

        Returns:
            Kernel version on the device.
        """
        cmd_out = ad.adb.shell("cat /proc/version")
        pattern_match = re.findall(r"^Linux version \d.\d", cmd_out)
        return float(pattern_match[0].split()[-1]) if pattern_match else None

    def _verify_tcp_keepalives(self,
                               pcap,
                               interval,
                               cport,
                               beg,
                               end,
                               expected_ka_count,
                               verify_ka_interval=True):
        """Verify TCP keepalives received by host.

        Args:
            pcap: tcpdump file from host.
            interval: keepalive time interval.
            cport: client port after NAT on the host.
            beg: beginning time for keepalives.
            end: end time for keepalives.
            expected_ka_count: expected no. of KA packets.
            verify_ka_interval: if true, verify the keepalive time interval.

        Returns:
            True/False if keepalives are successful.
        """
        try:
            packets = rdpcap(pcap)
        except Scapy_Exception:
            asserts.fail("Not a valid pcap file")

        seq = None
        time_stamps = []
        for pkt in packets:
            if pkt.time < beg:
                continue
            if pkt.time > end:
                break
            if TCP in pkt and pkt[TCP].dport == TCP_SERVER_PORT and \
                    pkt[TCP].sport == cport and pkt[TCP].flags == ACK and \
                    not seq or pkt[TCP].seq == seq:

                seq = pkt[TCP].seq
                time_stamps.append(pkt.time)

        self.log.info("KA count: %s, expected %s" % (len(time_stamps),
                                                     expected_ka_count))
        if len(time_stamps) != expected_ka_count:
            return False
        if not verify_ka_interval:
            return True
        return self._verify_keepalive_interval(time_stamps, interval)

    def _verify_natt_keepalives(self,
                                pcap,
                                interval,
                                beg,
                                end,
                                expected_ka_count):
        """Verify Natt keepalives received by host.

        Args:
            pcap: tcpdump file from host.
            interval: expected time difference between keepalives.
            beg: beginning time for keepalives.
            end: end time for keepalives.
            expected_ka_count: expected keepalive count.

        Returns:
            True/False if keepalives are successful.
        """
        try:
            packets = rdpcap(pcap)
        except Scapy_Exception:
            asserts.fail("Not a valid pcap file")

        ka_dict = {}
        for pkt in packets:
            if pkt.time < beg:
                continue
            if pkt.time > end:
                break
            if UDP in pkt and pkt[UDP].dport == UDP_SERVER_PORT:
                if pkt[UDP].sport not in ka_dict:
                    ka_dict[pkt[UDP].sport] = [float(pkt.time)]
                else:
                    ka_dict[pkt[UDP].sport].append(float(pkt.time))

        for sport in ka_dict:
            self.log.info("Source port: %s" % sport)
            self.log.info("KA count: %s, expected: %s" % (len(ka_dict[sport]),
                                                          expected_ka_count))
            if len(ka_dict[sport]) == expected_ka_count and \
                    self._verify_keepalive_interval(ka_dict[sport], interval):
                return True
        self.log.error("Keepalive time interval verification failed")
        return False

    def _verify_keepalive_interval(self, time_stamps, interval):
        """Verify time difference between keepalive packets.

        Args:
            time_stamps: List of timestamps of each keepalive.
            interval: Expected time difference.

        Returns:
            True if Keepalive interval matches with expected value.
        """
        i = 0
        for t in time_stamps:
            if i == 0:
                prev = t
                i += 1
                continue
            diff = int(round(t - prev))
            self.log.info("KA interval: %s, expected: %s" % (diff, interval))
            if diff != interval:
                return False
            prev = t
            i += 1
        return True

    def _get_expected_ka_count(self, elapsed_time, ka_time_interval):
        """Get expected keepalive count.

        Args:
            elapsed_time: time in seconds within which to count the keepalives.
            ka_time_interval: time interval between keepalives.

        Returns:
            no. of keepalives.
        """
        return int(elapsed_time/ka_time_interval)

    def _get_client_port_and_time_interval(self):
        """Generate a random tcp port and keepalive time interval.

        Returns:
            client_port and keepalive time interval.
        """
        # (TODO: @gmoturu) Change this to autobind instead of generating a port.
        client_port = random.randint(8000, 9000)
        time_interval = random.randint(MIN_TEST_KA_INTERVAL,
                                       MAX_TEST_KA_INTERVAL)
        self.log.info("Socket Keepalive time interval: %s" % time_interval)
        return client_port, time_interval

    def _open_tcp_socket_and_connect(self, client_port, server_ip=None):
        """Open a TCP socket and connect to server for keepalive testing.

        Args:
            client_port: port to open tcp socket on.
            server_ip: IP addr of remote server.

        Returns:
            socket object and client port seen by server.
        """
        if not server_ip:
            server_ip = self.host_ip
        sock = self.dut.droid.openTcpSocket(server_ip,
                                            TCP_SERVER_PORT,
                                            self.dut_ip,
                                            client_port)
        asserts.assert_true(sock, "Failed to open TCP socket")
        self.log.info("Socket key: %s" % sock)
        self.tcp_sockets.append(sock)
        cport_server = self.dut.droid.tcpSocketRecv(sock)
        asserts.assert_true(cport_server, "Received null message from server")
        self.log.info("Client port after NAT on host: %s" % cport_server)
        return sock, cport_server

    def _start_tcp_keepalive(self, sock, time_interval, expect_ka_start=True):
        """Start TCP keepalive.

        Args:
            sock: TCP socket object.
            time_interval: keepalive time interval.
            expect_ka_start: if True, KA should Start, else Error outcome.

        Returns:
            key of socket keepalive object.
        """
        ska_key = cutils.start_tcp_socket_keepalive(self.dut,
                                                    sock,
                                                    time_interval)

        if not expect_ka_start:
            asserts.assert_false(ska_key,
                                 "Expected starting socket keepalive to fail")
            return None

        asserts.assert_true(ska_key, "Failed to start socket keepalive")
        self.socket_keepalives.append(ska_key)
        return ska_key

    def _get_dropped_keepalive_acks(self, ack=DROPPED_IPV4_KA_ACK):
        """Get dropped keepalive acks from dumpsys output.

        Args:
            ack: Type of keepalive ack to check.

        Returns:
            Number of dropped acks.
        """
        dropped_acks = self.dut.adb.shell(
            "dumpsys network_stack | grep '%s'" % ack)
        dropped_acks = dropped_acks.rstrip()
        self.log.info("Dropped keepalive acks: %s" % dropped_acks)
        return 0 if not dropped_acks else int(dropped_acks.split(":")[-1])

    def _test_socket_keepalive_start_stop(self):
        """Test NATT and TCP socket keepalives for wifi and mobile networks."""
        tcp_keepalive = True if "_tcp_" in self.test_name else False
        ad = self.dut
        port = TCP_SERVER_PORT if tcp_keepalive else UDP_SERVER_PORT
        self.log.info("Client IP: %s" % self.dut_ip)
        self.log.info("Server IP: %s" % self.host_ip)

        # start tcpdump on the server
        self.gce_tcpdump_pid, pcap_path = nutils.start_tcpdump_gce_server(
            ad, self.test_name, port, self.remote_server)

        # start keepalive
        client_port, time_interval = self._get_client_port_and_time_interval()
        ska_key = None
        if tcp_keepalive:
            self.log.info("Client port: %s" % client_port)
            self.log.info("Server port: %s" % TCP_SERVER_PORT)
            sock, cport_server = self._open_tcp_socket_and_connect(client_port)
            ska_key = self._start_tcp_keepalive(sock, time_interval)
        else:
            self.log.info("UDP encap port on server: %s" % UDP_SERVER_PORT)
            self.udp_encap = self.dut.droid.ipSecOpenUdpEncapsulationSocket()
            ska_key = cutils.start_natt_socket_keepalive(self.dut,
                                                         self.udp_encap,
                                                         self.dut_ip,
                                                         self.host_ip,
                                                         time_interval)
            self.socket_keepalives.append(ska_key)
        asserts.assert_true(ska_key, "Failed to start socket keepalive")

        # sleep for sometime to verify keepalives
        beg_time = time.time()
        self.log.info("Sleeping for %s seconds" % SLEEP_TIME)
        time.sleep(SLEEP_TIME)
        end_time = time.time()

        # stop tcpdump
        pcap_file = nutils.stop_tcpdump_gce_server(
            ad, self.gce_tcpdump_pid, pcap_path, self.remote_server)
        self.gce_tcpdump_pid = None

        # verify keepalives
        expected_ka_count = self._get_expected_ka_count(end_time-beg_time,
                                                        time_interval)

        if tcp_keepalive:
            result = self._verify_tcp_keepalives(
                pcap_file, time_interval, int(cport_server), beg_time, end_time,
                expected_ka_count)
        else:
            result = self._verify_natt_keepalives(
                pcap_file, time_interval, beg_time, end_time, expected_ka_count)
        asserts.assert_true(result, "Keepalive verification failed")

    ### Tests begin

    @test_tracker_info(uuid="537e13ef-2366-40a3-86d4-596266d21eda")
    def test_tcp_socket_keepalive_start_stop_wifi(self):
        """Test TCP keepalive for onStart and onStop callbacks.

        Steps:
            1. Open a TCP socket on DUT and connect to server.
            2. Start TCP keepalive and verify onStarted callback is received.
            3. Verify that server received the keepalives.
            4. Stop TCP keepalive and verify onStopped callback is received.
        """
        self._test_socket_keepalive_start_stop()

    @test_tracker_info(uuid="8f23f3a0-7b65-4b5c-bc91-5decaddb7c9c")
    def test_tcp_socket_keepalive_on_data_received_wifi(self):
        """Test Error callback for tcp socket keepalive.

        Steps:
            1. Open a TCP socket on DUT and connect ot server on port 853.
            2. Start TCP keepalive and verify on Start callback.
            3. Send message from server and verify OnDataReceived Callback.
            4. Send data and recv data from server.
            5. Start keeaplive again and verify server receives them.
        """
        # start tcpdump on the server
        host = self.remote_server_2
        host_ip = host["ip_addr"]
        tcpdump_pid, pcap_path = nutils.start_tcpdump_gce_server(
            self.dut, self.test_name, TCP_SERVER_PORT, host)

        # open socket, connect to server, start keepalive
        time_interval = 11
        sleep_time = 37
        keepalive_count = 3
        self.log.info("Client IP: %s" % self.dut_ip)
        self.log.info("Server IP: %s" % host_ip)
        client_port = random.randint(8000, 9000)
        self.log.info("Client port: %s" % client_port)
        sock, cport_server = self._open_tcp_socket_and_connect(client_port,
                                                               host_ip)
        ska_key = self._start_tcp_keepalive(sock, time_interval)

        # wait for data to be received from server
        begin_time = time.time()
        self.log.info("Sleeping for %s seconds" % sleep_time)
        time.sleep(sleep_time)
        recv_msg = self.dut.droid.tcpSocketRecv(sock)
        self.log.info("Received message: %s" % recv_msg)

        # verify onDataReceived callback
        status = cutils.socket_keepalive_data_received(self.dut, ska_key)
        asserts.assert_true(status, "Failed to receive onDataReceived callback")
        self.socket_keepalives = []

        # wait for some time before starting keepalives
        self.log.info("Sleeping for %s seconds" % sleep_time)
        time.sleep(sleep_time)

        # re-start keepalives and sleep for sometime to verify keepalives
        ska_key = self._start_tcp_keepalive(sock, time_interval)
        self.log.info("Sleeping for %s seconds" % sleep_time)
        time.sleep(sleep_time)
        end_time = time.time()
        dropped_acks = self._get_dropped_keepalive_acks()

        # expected ka count here is not based on beg_time and end_time as we
        # wait some time before re-starting keepalives after OnDataReceived
        # callback. the elapsed time will be sleep_time * 2. the extra keepalive
        # accounts for the one sent by kernel. b/132924144 for reference.
        expected_ka_count = 1+self._get_expected_ka_count(sleep_time*2,
                                                          time_interval)

        # stop tcpdump
        pcap_file = nutils.stop_tcpdump_gce_server(
            self.dut, tcpdump_pid, pcap_path, host)

        # server closes socket - verify error callback
        time.sleep(60)
        status = cutils.socket_keepalive_error(self.dut, ska_key)
        asserts.assert_true(status, "Failed to receive onError callback")
        self.socket_keepalives = []
        self.tcp_sockets = []

        # verify keepalives
        result = self._verify_tcp_keepalives(pcap_file,
                                             time_interval,
                                             int(cport_server),
                                             begin_time,
                                             end_time,
                                             expected_ka_count,
                                             False)
        asserts.assert_true(result and keepalive_count == dropped_acks,
                            "Keepalive verification failed")
        asserts.assert_true(result, "Keepalive verification failed")

    @test_tracker_info(uuid="c02bcc1c-f86b-4905-973a-b4200a2b86c1")
    def test_tcp_socket_keepalive_start_stop_multiple_keepalives_wifi(self):
        """Test TCP keepalive for onStart and onStop callbacks.

        Steps:
            1. Open 3 TCP socket on DUT and connect to server.
            2. Start TCP keepalive and verify onStarted callback is received.
            3. Verify that server received the keepalives.
            4. Stop TCP keepalive and verify onStopped callback is received.
        """
        # start tcpdump on the server
        tcpdump_pid, pcap_path = nutils.start_tcpdump_gce_server(
            self.dut, self.test_name, TCP_SERVER_PORT, self.remote_server)

        # open 3 sockets and start keepalives
        interval_list = []
        cport_list = []
        cport_server_list = []
        self.log.info("Client IP: %s" % self.dut_ip)
        self.log.info("Server IP: %s" % self.host_ip)
        for _ in range(self.max_ka_wifi):
            cport, interval = self._get_client_port_and_time_interval()
            cport_list.append(cport)
            interval_list.append(interval)

        for _ in range(self.max_ka_wifi):
            sock, cport_server = self._open_tcp_socket_and_connect(
                cport_list[_])
            self._start_tcp_keepalive(sock, interval_list[_])
            cport_server_list.append(cport_server)

        # sleep for sometime to verify keepalives
        begin_time = time.time()
        self.log.info("Sleeping for %s seconds" % SLEEP_TIME)
        time.sleep(SLEEP_TIME)
        end_time = time.time()
        dropped_acks = self._get_dropped_keepalive_acks()

        # stop tcpdump
        pcap_file = nutils.stop_tcpdump_gce_server(
            self.dut, tcpdump_pid, pcap_path, self.remote_server)

        # verify keepalives
        total_dropped_acks = 0
        for _ in range(self.max_ka_wifi):
            total_dropped_acks += int(SLEEP_TIME/interval_list[_])
            expected_ka_count = self._get_expected_ka_count(end_time-begin_time,
                                                            interval_list[_])
            result = self._verify_tcp_keepalives(pcap_file,
                                                 interval_list[_],
                                                 int(cport_server_list[_]),
                                                 begin_time,
                                                 end_time,
                                                 expected_ka_count)
            asserts.assert_true(result, "Keepalive verification failed")

    @test_tracker_info(uuid="64374896-7a04-4a5b-b27c-e27b5f0e7159")
    def test_tcp_socket_keepalive_start_stop_stress_wifi(self):
        """Test TCP keepalive for onStart and onStop callbacks.

        Steps:
            1. Open 3 TCP socket on DUT and connect to server.
            2. Start TCP keepalive and verify onStarted callback is received.
            3. Verify that server received the keepalives.
            4. Stop TCP keepalive and verify onStopped callback is received.
            5. Repeat steps 1-4 few times.
        """
        # open 3 sockets
        interval_list = []
        cport_server_list = []
        sock_list = []

        self.log.info("Client IP: %s" % self.dut_ip)
        self.log.info("Server IP: %s" % self.host_ip)
        for _ in range(self.max_ka_wifi):
            client_port = random.randint(8000, 9000)
            self.log.info("Client port: %s" % client_port)
            sock, cport_server = self._open_tcp_socket_and_connect(client_port)
            sock_list.append(sock)
            cport_server_list.append(cport_server)

        for _ in range(STRESS_COUNT):

            # start tcpdump on the server
            tcpdump_pid, pcap_path = nutils.start_tcpdump_gce_server(
                self.dut, self.test_name, TCP_SERVER_PORT, self.remote_server)

            # start keepalives on sockets
            for _ in range(self.max_ka_wifi):
                time_interval = random.randint(MIN_TEST_KA_INTERVAL,
                                               MAX_TEST_KA_INTERVAL)
                self.log.info("TCP socket Keepalive time interval: %s" %
                              time_interval)
                interval_list.append(time_interval)
                self._start_tcp_keepalive(sock_list[_], interval_list[_])

            # sleep for sometime to verify keepalives
            begin_time = time.time()
            self.log.info("Sleeping for %s seconds" % SLEEP_TIME)
            time.sleep(SLEEP_TIME)
            end_time = time.time()
            dropped_acks = self._get_dropped_keepalive_acks()

            # stop keepalives
            for ska in self.socket_keepalives:
                self.log.info("Stopping keepalive %s" % ska)
                cutils.stop_socket_keepalive(self.dut, ska)
            self.socket_keepalives = []

            # stop tcpdump
            pcap_file = nutils.stop_tcpdump_gce_server(
                self.dut, tcpdump_pid, pcap_path, self.remote_server)

            # verify keepalives
            total_dropped_acks = 0
            for _ in range(self.max_ka_wifi):
                total_dropped_acks += int(SLEEP_TIME/interval_list[_])
                expected_ka_count = self._get_expected_ka_count(
                    end_time-begin_time, interval_list[_])
                result = self._verify_tcp_keepalives(pcap_file,
                                                     interval_list[_],
                                                     int(cport_server_list[_]),
                                                     begin_time,
                                                     end_time,
                                                     expected_ka_count)
                asserts.assert_true(result, "Keepalive verification failed")

            interval_list = []

    @test_tracker_info(uuid="de3970c1-23d1-4d52-b3c9-4a2f1e53e7f6")
    def test_tcp_socket_keepalive_error_client_socket_close_wifi(self):
        """Test Error callback for tcp socket keepalive.

        Steps:
            1. Open a TCP socket on DUT and connect to server on port 853.
            2. Start TCP keepalive and verify on Start callback.
            3. Wait for some time and shutdown the client socket.
            4. Verify on Error callback is received.
        """
        # open socket, connect to server, start keepalive
        client_port, time_interval = self._get_client_port_and_time_interval()
        sock, _ = self._open_tcp_socket_and_connect(client_port)
        ska_key = self._start_tcp_keepalive(sock, time_interval)

        # wait for atleast 1 keepalive
        self.log.info("Sleeping for %s seconds" % (time_interval+1))
        time.sleep(time_interval+1)

        # shutdown client socket
        sutils.shutdown_socket(self.dut, sock)
        self.tcp_sockets.remove(sock)
        self.socket_keepalives = []

        # verify Error callback received
        result = cutils.socket_keepalive_error(self.dut, ska_key)
        asserts.assert_true(result, "Failed to receive error callback")

    @test_tracker_info(uuid="7c902e51-725a-41f1-aebc-8528197c450c")
    def test_tcp_socket_keepalive_over_max_keepalive_limit_wifi(self):
        """Test TCP socket keepalive over max keepalive limit.

        Steps:
            1. Start 4 TCP socket keepalives.
            2. 3 Should be successful and 4th should fail with Error.
        """
        # open sockets and connect to server
        max_ka_net = self.dut.droid.getSupportedKeepalivesForNetwork()
        self.log.info("Max Keepalives supported: %s" % max_ka_net)
        state = True
        for _ in range(1, max_ka_net+1):
            cport, interval = self._get_client_port_and_time_interval()
            sock, _ = self._open_tcp_socket_and_connect(cport)
            if _ == max_ka_net:
                state = False
            self._start_tcp_keepalive(sock, interval, state)

    @test_tracker_info(uuid="308004c5-8e40-4e8b-9408-4a90e85a2261")
    def test_tcp_socket_keepalive_invalid_interval(self):
        """Test TCP socket keepalive with invalid interval.

        Steps:
            1. Start TCP socket keepalive with time interval <10 seconds.
            2. Verify Error callback is received.
        """
        # keepalive time interval, client port
        client_port = random.randint(8000, 9000)
        time_interval = random.randint(1, 9)
        self.log.info("Client port: %s" % client_port)
        self.log.info("Server port: %s" % TCP_SERVER_PORT)

        # open tcp socket
        sock, _ = self._open_tcp_socket_and_connect(client_port)

        # start TCP socket keepalive
        self.log.info("TCP socket Keepalive time interval: %s" % time_interval)
        self._start_tcp_keepalive(sock, time_interval, False)

    @test_tracker_info(uuid="c9012da2-656f-44ef-bad6-26892335d4bd")
    def test_natt_socket_keepalive_start_stop_wifi(self):
        """Test Natt socket keepalive for onStart and onStop callbacks.

        Steps:
            1. Open a UDP encap socket on DUT.
            2. Start Natt socket keepalive.
            3. Verify that server received the keepalives.
            4. Stop keepalive and verify onStopped callback is received.
        """
        self._test_socket_keepalive_start_stop()

    @test_tracker_info(uuid="646bfd5d-936f-4069-8dea-fdd02582550d")
    def test_natt_socket_keepalive_start_stop_lte(self):
        """Test Natt socket keepalive for onStart and onStop callbacks.

        Steps:
            1. Open a UDP encap socket on DUT.
            2. Start Natt socket keepalive.
            3. Verify that server received the keepalives.
            4. Stop keepalive and verify onStopped callback is received.
        """
        self._test_socket_keepalive_start_stop()

    @test_tracker_info(uuid="8ab20733-4a9e-4e4d-a46f-4d32a9f221c5")
    def test_natt_socket_keepalive_invalid_interval(self):
        """Test invalid natt socket keepalive time interval.

        Steps:
          1. Start NATT socket keepalive with time interval <10 seconds.
          2. Verify Error callback is recevied.
        """
        # keepalive time interval, client port
        time_interval = random.randint(1, 9)
        self.log.info("Natt socket keepalive time interval: %s" % time_interval)
        self.log.info("UDP encap port on server: %s" % UDP_SERVER_PORT)

        # open udp encap socket
        self.udp_encap = self.dut.droid.ipSecOpenUdpEncapsulationSocket()

        # start Natt socket keepalive
        ska_key = cutils.start_natt_socket_keepalive(
            self.dut, self.udp_encap, self.dut_ip, self.host_ip, time_interval)
        asserts.assert_true(not ska_key, "Failed to receive error callback")

    ### Tests End
