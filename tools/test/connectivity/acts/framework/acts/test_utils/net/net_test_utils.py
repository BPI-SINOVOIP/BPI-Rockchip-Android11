#!/usr/bin/env python3
#
#   Copyright 2018 Google, Inc.
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
import logging
import os

from acts.controllers import adb
from acts import asserts
from acts import signals
from acts import utils
from acts.controllers.adb import AdbError
from acts.utils import start_standing_subprocess
from acts.utils import stop_standing_subprocess
from acts.test_utils.net import connectivity_const as cconst
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_data_utils import wait_for_cell_data_connection
from acts.test_utils.tel.tel_test_utils import verify_http_connection
from acts.test_utils.wifi import wifi_test_utils as wutils
from scapy.all import get_if_list

import os
import re
import time
import urllib.request

VPN_CONST = cconst.VpnProfile
VPN_TYPE = cconst.VpnProfileType
VPN_PARAMS = cconst.VpnReqParams
TCPDUMP_PATH = "/data/local/tmp/"
USB_CHARGE_MODE = "svc usb setFunctions"
USB_TETHERING_MODE = "svc usb setFunctions rndis"
DEVICE_IP_ADDRESS = "ip address"

GCE_SSH = "gcloud compute ssh "
GCE_SCP = "gcloud compute scp "


def verify_lte_data_and_tethering_supported(ad):
    """Verify if LTE data is enabled and tethering supported"""
    wutils.wifi_toggle_state(ad, False)
    ad.droid.telephonyToggleDataConnection(True)
    wait_for_cell_data_connection(ad.log, ad, True)
    asserts.assert_true(
        verify_http_connection(ad.log, ad),
        "HTTP verification failed on cell data connection")
    asserts.assert_true(
        ad.droid.connectivityIsTetheringSupported(),
        "Tethering is not supported for the provider")
    wutils.wifi_toggle_state(ad, True)


def set_chrome_browser_permissions(ad):
    """Set chrome browser start with no-first-run verification.
    Give permission to read from and write to storage
    """
    commands = ["pm grant com.android.chrome "
                "android.permission.READ_EXTERNAL_STORAGE",
                "pm grant com.android.chrome "
                "android.permission.WRITE_EXTERNAL_STORAGE",
                "rm /data/local/chrome-command-line",
                "am set-debug-app --persistent com.android.chrome",
                'echo "chrome --no-default-browser-check --no-first-run '
                '--disable-fre" > /data/local/tmp/chrome-command-line']
    for cmd in commands:
        try:
            ad.adb.shell(cmd)
        except AdbError:
            logging.warning("adb command %s failed on %s" % (cmd, ad.serial))


def verify_ping_to_vpn_ip(ad, vpn_ping_addr):
    """ Verify if IP behind VPN server is pingable.
    Ping should pass, if VPN is connected.
    Ping should fail, if VPN is disconnected.

    Args:
      ad: android device object
    """
    ping_result = None
    pkt_loss = "100% packet loss"
    logging.info("Pinging: %s" % vpn_ping_addr)
    try:
        ping_result = ad.adb.shell("ping -c 3 -W 2 %s" % vpn_ping_addr)
    except AdbError:
        pass
    return ping_result and pkt_loss not in ping_result


def legacy_vpn_connection_test_logic(ad, vpn_profile, vpn_ping_addr):
    """ Test logic for each legacy VPN connection

    Steps:
      1. Generate profile for the VPN type
      2. Establish connection to the server
      3. Verify that connection is established using LegacyVpnInfo
      4. Verify the connection by pinging the IP behind VPN
      5. Stop the VPN connection
      6. Check the connection status
      7. Verify that ping to IP behind VPN fails

    Args:
      1. ad: Android device object
      2. VpnProfileType (1 of the 6 types supported by Android)
    """
    # Wait for sometime so that VPN server flushes all interfaces and
    # connections after graceful termination
    time.sleep(10)

    ad.adb.shell("ip xfrm state flush")
    ad.log.info("Connecting to: %s", vpn_profile)
    ad.droid.vpnStartLegacyVpn(vpn_profile)
    time.sleep(cconst.VPN_TIMEOUT)

    connected_vpn_info = ad.droid.vpnGetLegacyVpnInfo()
    asserts.assert_equal(connected_vpn_info["state"],
                         cconst.VPN_STATE_CONNECTED,
                         "Unable to establish VPN connection for %s"
                         % vpn_profile)

    ping_result = verify_ping_to_vpn_ip(ad, vpn_ping_addr)
    ip_xfrm_state = ad.adb.shell("ip xfrm state")
    match_obj = re.search(r'hmac(.*)', "%s" % ip_xfrm_state)
    if match_obj:
        ip_xfrm_state = format(match_obj.group(0)).split()
        ad.log.info("HMAC for ESP is %s " % ip_xfrm_state[0])

    ad.droid.vpnStopLegacyVpn()
    asserts.assert_true(ping_result,
                        "Ping to the internal IP failed. "
                        "Expected to pass as VPN is connected")

    connected_vpn_info = ad.droid.vpnGetLegacyVpnInfo()
    asserts.assert_true(not connected_vpn_info,
                        "Unable to terminate VPN connection for %s"
                        % vpn_profile)


def download_load_certs(ad, vpn_params, vpn_type, vpn_server_addr,
                        ipsec_server_type, log_path):
    """ Download the certificates from VPN server and push to sdcard of DUT

    Args:
      ad: android device object
      vpn_params: vpn params from config file
      vpn_type: 1 of the 6 VPN types
      vpn_server_addr: server addr to connect to
      ipsec_server_type: ipsec version - strongswan or openswan
      log_path: log path to download cert

    Returns:
      Client cert file name on DUT's sdcard
    """
    url = "http://%s%s%s" % (vpn_server_addr,
                             vpn_params['cert_path_vpnserver'],
                             vpn_params['client_pkcs_file_name'])
    local_cert_name = "%s_%s_%s" % (vpn_type.name,
                                    ipsec_server_type,
                                    vpn_params['client_pkcs_file_name'])

    local_file_path = os.path.join(log_path, local_cert_name)
    logging.info("URL is: %s" % url)
    try:
        ret = urllib.request.urlopen(url)
        with open(local_file_path, "wb") as f:
            f.write(ret.read())
    except Exception:
        asserts.fail("Unable to download certificate from the server")

    ad.adb.push("%s sdcard/" % local_file_path)
    return local_cert_name


def generate_legacy_vpn_profile(ad,
                                vpn_params,
                                vpn_type,
                                vpn_server_addr,
                                ipsec_server_type,
                                log_path):
    """ Generate legacy VPN profile for a VPN

    Args:
      ad: android device object
      vpn_params: vpn params from config file
      vpn_type: 1 of the 6 VPN types
      vpn_server_addr: server addr to connect to
      ipsec_server_type: ipsec version - strongswan or openswan
      log_path: log path to download cert

    Returns:
      Vpn profile
    """
    vpn_profile = {VPN_CONST.USER: vpn_params['vpn_username'],
                   VPN_CONST.PWD: vpn_params['vpn_password'],
                   VPN_CONST.TYPE: vpn_type.value,
                   VPN_CONST.SERVER: vpn_server_addr, }
    vpn_profile[VPN_CONST.NAME] = "test_%s_%s" % (vpn_type.name,
                                                  ipsec_server_type)
    if vpn_type.name == "PPTP":
        vpn_profile[VPN_CONST.NAME] = "test_%s" % vpn_type.name

    psk_set = set(["L2TP_IPSEC_PSK", "IPSEC_XAUTH_PSK"])
    rsa_set = set(["L2TP_IPSEC_RSA", "IPSEC_XAUTH_RSA", "IPSEC_HYBRID_RSA"])

    if vpn_type.name in psk_set:
        vpn_profile[VPN_CONST.IPSEC_SECRET] = vpn_params['psk_secret']
    elif vpn_type.name in rsa_set:
        cert_name = download_load_certs(ad,
                                        vpn_params,
                                        vpn_type,
                                        vpn_server_addr,
                                        ipsec_server_type,
                                        log_path)
        vpn_profile[VPN_CONST.IPSEC_USER_CERT] = cert_name.split('.')[0]
        vpn_profile[VPN_CONST.IPSEC_CA_CERT] = cert_name.split('.')[0]
        ad.droid.installCertificate(vpn_profile, cert_name,
                                    vpn_params['cert_password'])
    else:
        vpn_profile[VPN_CONST.MPPE] = "mppe"

    return vpn_profile


def generate_ikev2_vpn_profile(ad, vpn_params, vpn_type, server_addr, log_path):
    """Generate VPN profile for IKEv2 VPN.

    Args:
        ad: android device object.
        vpn_params: vpn params from config file.
        vpn_type: ikev2 vpn type.
        server_addr: vpn server addr.
        log_path: log path to download cert.

    Returns:
        Vpn profile.
    """
    vpn_profile = {
        VPN_CONST.TYPE: vpn_type.value,
        VPN_CONST.SERVER: server_addr,
    }

    if vpn_type.name == "IKEV2_IPSEC_USER_PASS":
        vpn_profile[VPN_CONST.USER] = vpn_params["vpn_username"]
        vpn_profile[VPN_CONST.PWD] = vpn_params["vpn_password"]
        vpn_profile[VPN_CONST.IPSEC_ID] = vpn_params["vpn_identity"]
        cert_name = download_load_certs(
            ad, vpn_params, vpn_type, server_addr, "IKEV2_IPSEC_USER_PASS",
            log_path)
        vpn_profile[VPN_CONST.IPSEC_CA_CERT] = cert_name.split('.')[0]
        ad.droid.installCertificate(
            vpn_profile, cert_name, vpn_params['cert_password'])
    elif vpn_type.name == "IKEV2_IPSEC_PSK":
        vpn_profile[VPN_CONST.IPSEC_ID] = vpn_params["vpn_identity"]
        vpn_profile[VPN_CONST.IPSEC_SECRET] = vpn_params["psk_secret"]
    else:
        vpn_profile[VPN_CONST.IPSEC_ID] = "%s@%s" % (
            vpn_params["vpn_identity"], vpn_params["server_addr"])
        cert_name = download_load_certs(
            ad, vpn_params, vpn_type, server_addr, "IKEV2_IPSEC_RSA", log_path)
        vpn_profile[VPN_CONST.IPSEC_USER_CERT] = cert_name.split('.')[0]
        vpn_profile[VPN_CONST.IPSEC_CA_CERT] = cert_name.split('.')[0]
        ad.droid.installCertificate(
            vpn_profile, cert_name, vpn_params['cert_password'])

    return vpn_profile


def start_tcpdump(ad, test_name):
    """Start tcpdump on all interfaces

    Args:
        ad: android device object.
        test_name: tcpdump file name will have this
    """
    ad.log.info("Starting tcpdump on all interfaces")
    ad.adb.shell("killall -9 tcpdump", ignore_status=True)
    ad.adb.shell("mkdir %s" % TCPDUMP_PATH, ignore_status=True)
    ad.adb.shell("rm -rf %s/*" % TCPDUMP_PATH, ignore_status=True)

    file_name = "%s/tcpdump_%s_%s.pcap" % (TCPDUMP_PATH, ad.serial, test_name)
    ad.log.info("tcpdump file is %s", file_name)
    cmd = "adb -s {} shell tcpdump -i any -s0 -w {}".format(ad.serial,
                                                            file_name)
    try:
        return start_standing_subprocess(cmd, 5)
    except Exception:
        ad.log.exception('Could not start standing process %s' % repr(cmd))

    return None

def stop_tcpdump(ad,
                 proc,
                 test_name,
                 pull_dump=True,
                 adb_pull_timeout=adb.DEFAULT_ADB_PULL_TIMEOUT):
    """Stops tcpdump on any iface
       Pulls the tcpdump file in the tcpdump dir if necessary

    Args:
        ad: android device object.
        proc: need to know which pid to stop
        test_name: test name to save the tcpdump file
        pull_dump: pull tcpdump file or not
        adb_pull_timeout: timeout for adb_pull

    Returns:
      log_path of the tcpdump file
    """
    ad.log.info("Stopping and pulling tcpdump if any")
    if proc is None:
        return None
    try:
        stop_standing_subprocess(proc)
    except Exception as e:
        ad.log.warning(e)
    if pull_dump:
        log_path = os.path.join(ad.device_log_path, "TCPDUMP_%s" % ad.serial)
        os.makedirs(log_path, exist_ok=True)
        ad.adb.pull("%s/. %s" % (TCPDUMP_PATH, log_path),
                timeout=adb_pull_timeout)
        ad.adb.shell("rm -rf %s/*" % TCPDUMP_PATH, ignore_status=True)
        file_name = "tcpdump_%s_%s.pcap" % (ad.serial, test_name)
        return "%s/%s" % (log_path, file_name)
    return None

def start_tcpdump_gce_server(ad, test_name, dest_port, gce):
    """ Start tcpdump on gce server

    Args:
        ad: android device object
        test_name: test case name
        dest_port: port to collect tcpdump
        gce: dictionary of gce instance

    Returns:
       process id and pcap file path from gce server
    """
    ad.log.info("Starting tcpdump on gce server")

    # pcap file name
    fname = "/tmp/%s_%s_%s_%s" % \
        (test_name, ad.model, ad.serial,
         time.strftime('%Y-%m-%d_%H-%M-%S', time.localtime(time.time())))

    # start tcpdump
    tcpdump_cmd = "sudo bash -c \'tcpdump -i %s -w %s.pcap port %s > \
        %s.txt 2>&1 & echo $!\'" % (gce["interface"], fname, dest_port, fname)
    gcloud_ssh_cmd = "%s --project=%s --zone=%s %s@%s --command " % \
        (GCE_SSH, gce["project"], gce["zone"], gce["username"], gce["hostname"])
    gce_ssh_cmd = '%s "%s"' % (gcloud_ssh_cmd, tcpdump_cmd)
    utils.exe_cmd(gce_ssh_cmd)

    # get process id
    ps_cmd = '%s "ps aux | grep tcpdump | grep %s"' % (gcloud_ssh_cmd, fname)
    tcpdump_pid = utils.exe_cmd(ps_cmd).decode("utf-8", "ignore").split()
    if not tcpdump_pid:
        raise signals.TestFailure("Failed to start tcpdump on gce server")
    return tcpdump_pid[1], fname

def stop_tcpdump_gce_server(ad, tcpdump_pid, fname, gce):
    """ Stop and pull tcpdump file from gce server

    Args:
        ad: android device object
        tcpdump_pid: process id for tcpdump file
        fname: tcpdump file path
        gce: dictionary of gce instance

    Returns:
       pcap file from gce server
    """
    ad.log.info("Stop and pull pcap file from gce server")

    # stop tcpdump
    tcpdump_cmd = "sudo kill %s" % tcpdump_pid
    gcloud_ssh_cmd = "%s --project=%s --zone=%s %s@%s --command " % \
        (GCE_SSH, gce["project"], gce["zone"], gce["username"], gce["hostname"])
    gce_ssh_cmd = '%s "%s"' % (gcloud_ssh_cmd, tcpdump_cmd)
    utils.exe_cmd(gce_ssh_cmd)

    # verify tcpdump is stopped
    ps_cmd = '%s "ps aux | grep tcpdump"' % gcloud_ssh_cmd
    res = utils.exe_cmd(ps_cmd).decode("utf-8", "ignore")
    if tcpdump_pid in res.split():
        raise signals.TestFailure("Failed to stop tcpdump on gce server")
    if not fname:
        return None

    # pull pcap file
    gcloud_scp_cmd = "%s --project=%s --zone=%s %s@%s:" % \
        (GCE_SCP, gce["project"], gce["zone"], gce["username"], gce["hostname"])
    pull_file = '%s%s.pcap %s/' % (gcloud_scp_cmd, fname, ad.device_log_path)
    utils.exe_cmd(pull_file)
    if not os.path.exists(
        "%s/%s.pcap" % (ad.device_log_path, fname.split('/')[-1])):
        raise signals.TestFailure("Failed to pull tcpdump from gce server")

    # delete pcaps
    utils.exe_cmd('%s "sudo rm %s.*"' % (gcloud_ssh_cmd, fname))

    # return pcap file
    pcap_file = "%s/%s.pcap" % (ad.device_log_path, fname.split('/')[-1])
    return pcap_file

def is_ipaddress_ipv6(ip_address):
    """Verify if the given string is a valid IPv6 address.

    Args:
        ip_address: string containing the IP address

    Returns:
        True: if valid ipv6 address
        False: if not
    """
    try:
        socket.inet_pton(socket.AF_INET6, ip_address)
        return True
    except socket.error:
        return False

def carrier_supports_ipv6(dut):
    """Verify if carrier supports ipv6.

    Args:
        dut: Android device that is being checked

    Returns:
        True: if carrier supports ipv6
        False: if not
    """

    carrier_supports_ipv6 = ["vzw", "tmo", "Far EasTone", "Chunghwa Telecom"]
    operator = get_operator_name("log", dut)
    return operator in carrier_supports_ipv6

def supports_ipv6_tethering(self, dut):
    """ Check if provider supports IPv6 tethering.

    Returns:
        True: if provider supports IPv6 tethering
        False: if not
    """
    carrier_supports_tethering = ["vzw", "tmo", "Far EasTone", "Chunghwa Telecom"]
    operator = get_operator_name(self.log, dut)
    return operator in carrier_supports_tethering


def start_usb_tethering(ad):
    """Start USB tethering.

    Args:
        ad: android device object
    """
    # TODO: test USB tethering by #startTethering API - b/149116235
    ad.log.info("Starting USB Tethering")
    ad.stop_services()
    ad.adb.shell(USB_TETHERING_MODE, ignore_status=True)
    ad.adb.wait_for_device()
    ad.start_services()
    if "rndis" not in ad.adb.shell(DEVICE_IP_ADDRESS):
        raise signals.TestFailure("Unable to enable USB tethering.")


def stop_usb_tethering(ad):
    """Stop USB tethering.

    Args:
        ad: android device object
    """
    ad.log.info("Stopping USB Tethering")
    ad.stop_services()
    ad.adb.shell(USB_CHARGE_MODE)
    ad.adb.wait_for_device()
    ad.start_services()


def wait_for_new_iface(old_ifaces):
    """Wait for the new interface to come up.

    Args:
        old_ifaces: list of old interfaces
    """
    old_set = set(old_ifaces)
    # Try 10 times to find a new interface with a 1s sleep every time
    # (equivalent to a 9s timeout)
    for i in range(0, 10):
        new_ifaces = set(get_if_list()) - old_set
        asserts.assert_true(len(new_ifaces) < 2,
                            "Too many new interfaces after turning on "
                            "tethering")
        if len(new_ifaces) == 1:
            return new_ifaces.pop()
        time.sleep(1)
    asserts.fail("Timeout waiting for tethering interface on host")
