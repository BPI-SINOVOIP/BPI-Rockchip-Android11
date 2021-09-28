#!/usr/bin/env python3
#
#   Copyright 2016 - Google, Inc.
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

import collections
import ipaddress
import os

from acts import logger
from acts.controllers.ap_lib import ap_get_interface
from acts.controllers.ap_lib import ap_iwconfig
from acts.controllers.ap_lib import bridge_interface
from acts.controllers.ap_lib import dhcp_config
from acts.controllers.ap_lib import dhcp_server
from acts.controllers.ap_lib import hostapd
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.utils_lib.commands import ip
from acts.controllers.utils_lib.commands import route
from acts.controllers.utils_lib.commands import shell
from acts.controllers.utils_lib.ssh import connection
from acts.controllers.utils_lib.ssh import settings
from acts.libs.proc import job

ACTS_CONTROLLER_CONFIG_NAME = 'AccessPoint'
ACTS_CONTROLLER_REFERENCE_NAME = 'access_points'
_BRCTL = 'brctl'

LIFETIME = 180
PROC_NET_SNMP6 = '/proc/net/snmp6'
SCAPY_INSTALL_COMMAND = 'sudo python setup.py install'
RA_MULTICAST_ADDR = '33:33:00:00:00:01'
RA_SCRIPT = 'sendra.py'


def create(configs):
    """Creates ap controllers from a json config.

    Creates an ap controller from either a list, or a single
    element. The element can either be just the hostname or a dictionary
    containing the hostname and username of the ap to connect to over ssh.

    Args:
        The json configs that represent this controller.

    Returns:
        A new AccessPoint.
    """
    return [AccessPoint(c) for c in configs]


def destroy(aps):
    """Destroys a list of access points.

    Args:
        aps: The list of access points to destroy.
    """
    for ap in aps:
        ap.close()


def get_info(aps):
    """Get information on a list of access points.

    Args:
        aps: A list of AccessPoints.

    Returns:
        A list of all aps hostname.
    """
    return [ap.ssh_settings.hostname for ap in aps]


class Error(Exception):
    """Error raised when there is a problem with the access point."""


_ApInstance = collections.namedtuple('_ApInstance', ['hostapd', 'subnet'])

# These ranges were split this way since each physical radio can have up
# to 8 SSIDs so for the 2GHz radio the DHCP range will be
# 192.168.1 - 8 and the 5Ghz radio will be 192.168.9 - 16
_AP_2GHZ_SUBNET_STR_DEFAULT = '192.168.1.0/24'
_AP_5GHZ_SUBNET_STR_DEFAULT = '192.168.9.0/24'

# The last digit of the ip for the bridge interface
BRIDGE_IP_LAST = '100'


class AccessPoint(object):
    """An access point controller.

    Attributes:
        ssh: The ssh connection to this ap.
        ssh_settings: The ssh settings being used by the ssh connection.
        dhcp_settings: The dhcp server settings being used.
    """
    def __init__(self, configs):
        """
        Args:
            configs: configs for the access point from config file.
        """
        self.ssh_settings = settings.from_config(configs['ssh_config'])
        self.log = logger.create_logger(lambda msg: '[Access Point|%s] %s' %
                                        (self.ssh_settings.hostname, msg))

        if 'ap_subnet' in configs:
            self._AP_2G_SUBNET_STR = configs['ap_subnet']['2g']
            self._AP_5G_SUBNET_STR = configs['ap_subnet']['5g']
        else:
            self._AP_2G_SUBNET_STR = _AP_2GHZ_SUBNET_STR_DEFAULT
            self._AP_5G_SUBNET_STR = _AP_5GHZ_SUBNET_STR_DEFAULT

        self._AP_2G_SUBNET = dhcp_config.Subnet(
            ipaddress.ip_network(self._AP_2G_SUBNET_STR))
        self._AP_5G_SUBNET = dhcp_config.Subnet(
            ipaddress.ip_network(self._AP_5G_SUBNET_STR))

        self.ssh = connection.SshConnection(self.ssh_settings)

        # Singleton utilities for running various commands.
        self._ip_cmd = ip.LinuxIpCommand(self.ssh)
        self._route_cmd = route.LinuxRouteCommand(self.ssh)

        # A map from network interface name to _ApInstance objects representing
        # the hostapd instance running against the interface.
        self._aps = dict()
        self._dhcp = None
        self._dhcp_bss = dict()
        self.bridge = bridge_interface.BridgeInterface(self)
        self.interfaces = ap_get_interface.ApInterfaces(self)
        self.iwconfig = ap_iwconfig.ApIwconfig(self)

        # Get needed interface names and initialize the unneccessary ones.
        self.wan = self.interfaces.get_wan_interface()
        self.wlan = self.interfaces.get_wlan_interface()
        self.wlan_2g = self.wlan[0]
        self.wlan_5g = self.wlan[1]
        self.lan = self.interfaces.get_lan_interface()
        self.__initial_ap()
        self.scapy_install_path = None
        self.setup_bridge = False

    def __initial_ap(self):
        """Initial AP interfaces.

        Bring down hostapd if instance is running, bring down all bridge
        interfaces.
        """
        # This is necessary for Gale/Whirlwind flashed with dev channel image
        # Unused interfaces such as existing hostapd daemon, guest, mesh
        # interfaces need to be brought down as part of the AP initialization
        # process, otherwise test would fail.
        try:
            self.ssh.run('stop wpasupplicant')
            self.ssh.run('stop hostapd')
        except job.Error:
            self.log.debug('No hostapd running')
        # Bring down all wireless interfaces
        for iface in self.wlan:
            WLAN_DOWN = 'ifconfig {} down'.format(iface)
            self.ssh.run(WLAN_DOWN)
        # Bring down all bridge interfaces
        bridge_interfaces = self.interfaces.get_bridge_interface()
        if bridge_interfaces:
            for iface in bridge_interfaces:
                BRIDGE_DOWN = 'ifconfig {} down'.format(iface)
                BRIDGE_DEL = 'brctl delbr {}'.format(iface)
                self.ssh.run(BRIDGE_DOWN)
                self.ssh.run(BRIDGE_DEL)

    def start_ap(self,
                 hostapd_config,
                 setup_bridge=False,
                 additional_parameters=None):
        """Starts as an ap using a set of configurations.

        This will start an ap on this host. To start an ap the controller
        selects a network interface to use based on the configs given. It then
        will start up hostapd on that interface. Next a subnet is created for
        the network interface and dhcp server is refreshed to give out ips
        for that subnet for any device that connects through that interface.

        Args:
            hostapd_config: hostapd_config.HostapdConfig, The configurations
                to use when starting up the ap.
            setup_bridge: Whether to bridge the LAN interface WLAN interface.
                Only one WLAN interface can be bridged with the LAN interface
                and none of the guest networks can be bridged.
            additional_parameters: A dictionary of parameters that can sent
                directly into the hostapd config file.  This can be used for
                debugging and or adding one off parameters into the config.

        Returns:
            An identifier for each ssid being started. These identifiers can be
            used later by this controller to control the ap.

        Raises:
            Error: When the ap can't be brought up.
        """
        if hostapd_config.frequency < 5000:
            interface = self.wlan_2g
            subnet = self._AP_2G_SUBNET
        else:
            interface = self.wlan_5g
            subnet = self._AP_5G_SUBNET

        # In order to handle dhcp servers on any interface, the initiation of
        # the dhcp server must be done after the wlan interfaces are figured
        # out as opposed to being in __init__
        self._dhcp = dhcp_server.DhcpServer(self.ssh, interface=interface)

        # For multi bssid configurations the mac address
        # of the wireless interface needs to have enough space to mask out
        # up to 8 different mac addresses. So in for one interface the range is
        # hex 0-7 and for the other the range is hex 8-f.
        interface_mac_orig = None
        cmd = "ifconfig %s|grep ether|awk -F' ' '{print $2}'" % interface
        interface_mac_orig = self.ssh.run(cmd)
        if interface == self.wlan_5g:
            hostapd_config.bssid = interface_mac_orig.stdout[:-1] + '0'
            last_octet = 1
        if interface == self.wlan_2g:
            hostapd_config.bssid = interface_mac_orig.stdout[:-1] + '8'
            last_octet = 9
        if interface in self._aps:
            raise ValueError('No WiFi interface available for AP on '
                             'channel %d' % hostapd_config.channel)

        apd = hostapd.Hostapd(self.ssh, interface)
        new_instance = _ApInstance(hostapd=apd, subnet=subnet)
        self._aps[interface] = new_instance

        # Turn off the DHCP server, we're going to change its settings.
        self.stop_dhcp()
        # Clear all routes to prevent old routes from interfering.
        self._route_cmd.clear_routes(net_interface=interface)

        if hostapd_config.bss_lookup:
            # The self._dhcp_bss dictionary is created to hold the key/value
            # pair of the interface name and the ip scope that will be
            # used for the particular interface.  The a, b, c, d
            # variables below are the octets for the ip address.  The
            # third octet is then incremented for each interface that
            # is requested.  This part is designed to bring up the
            # hostapd interfaces and not the DHCP servers for each
            # interface.
            self._dhcp_bss = dict()
            counter = 1
            for bss in hostapd_config.bss_lookup:
                if interface_mac_orig:
                    hostapd_config.bss_lookup[bss].bssid = (
                        interface_mac_orig.stdout[:-1] + hex(last_octet)[-1:])
                self._route_cmd.clear_routes(net_interface=str(bss))
                if interface is self.wlan_2g:
                    starting_ip_range = self._AP_2G_SUBNET_STR
                else:
                    starting_ip_range = self._AP_5G_SUBNET_STR
                a, b, c, d = starting_ip_range.split('.')
                self._dhcp_bss[bss] = dhcp_config.Subnet(
                    ipaddress.ip_network('%s.%s.%s.%s' %
                                         (a, b, str(int(c) + counter), d)))
                counter = counter + 1
                last_octet = last_octet + 1

        apd.start(hostapd_config, additional_parameters=additional_parameters)

        # The DHCP serer requires interfaces to have ips and routes before
        # the server will come up.
        interface_ip = ipaddress.ip_interface(
            '%s/%s' % (subnet.router, subnet.network.netmask))
        if setup_bridge is True:
            bridge_interface_name = 'br_lan'
            self.create_bridge(bridge_interface_name, [interface, self.lan])
            self._ip_cmd.set_ipv4_address(bridge_interface_name, interface_ip)
        else:
            self._ip_cmd.set_ipv4_address(interface, interface_ip)
        if hostapd_config.bss_lookup:
            # This loop goes through each interface that was setup for
            # hostapd and assigns the DHCP scopes that were defined but
            # not used during the hostapd loop above.  The k and v
            # variables represent the interface name, k, and dhcp info, v.
            for k, v in self._dhcp_bss.items():
                bss_interface_ip = ipaddress.ip_interface(
                    '%s/%s' % (self._dhcp_bss[k].router,
                               self._dhcp_bss[k].network.netmask))
                self._ip_cmd.set_ipv4_address(str(k), bss_interface_ip)

        # Restart the DHCP server with our updated list of subnets.
        configured_subnets = [x.subnet for x in self._aps.values()]
        if hostapd_config.bss_lookup:
            for k, v in self._dhcp_bss.items():
                configured_subnets.append(v)

        self.start_dhcp(subnets=configured_subnets)
        self.start_nat()

        bss_interfaces = [bss for bss in hostapd_config.bss_lookup]
        bss_interfaces.append(interface)

        return bss_interfaces

    def start_dhcp(self, subnets):
        """Start a DHCP server for the specified subnets.

        This allows consumers of the access point objects to control DHCP.

        Args:
            subnets: A list of Subnets.
        """
        return self._dhcp.start(config=dhcp_config.DhcpConfig(subnets))

    def stop_dhcp(self):
        """Stop DHCP for this AP object.

        This allows consumers of the access point objects to control DHCP.
        """
        return self._dhcp.stop()

    def start_nat(self):
        """Start NAT on the AP.

        This allows consumers of the access point objects to enable NAT
        on the AP.

        Note that this is currently a global setting, since we don't
        have per-interface masquerade rules.
        """
        # The following three commands are needed to enable NAT between
        # the WAN and LAN/WLAN ports.  This means anyone connecting to the
        # WLAN/LAN ports will be able to access the internet if the WAN port
        # is connected to the internet.
        self.ssh.run('iptables -t nat -F')
        self.ssh.run('iptables -t nat -A POSTROUTING -o %s -j MASQUERADE' %
                     self.wan)
        self.ssh.run('echo 1 > /proc/sys/net/ipv4/ip_forward')
        self.ssh.run('echo 1 > /proc/sys/net/ipv6/conf/all/forwarding')

    def stop_nat(self):
        """Stop NAT on the AP.

        This allows consumers of the access point objects to disable NAT on the
        AP.

        Note that this is currently a global setting, since we don't have
        per-interface masquerade rules.
        """
        self.ssh.run('iptables -t nat -F')
        self.ssh.run('echo 0 > /proc/sys/net/ipv4/ip_forward')
        self.ssh.run('echo 0 > /proc/sys/net/ipv6/conf/all/forwarding')

    def create_bridge(self, bridge_name, interfaces):
        """Create the specified bridge and bridge the specified interfaces.

        Args:
            bridge_name: The name of the bridge to create.
            interfaces: A list of interfaces to add to the bridge.
        """

        # Create the bridge interface
        self.ssh.run(
            'brctl addbr {bridge_name}'.format(bridge_name=bridge_name))

        for interface in interfaces:
            self.ssh.run('brctl addif {bridge_name} {interface}'.format(
                bridge_name=bridge_name, interface=interface))

    def remove_bridge(self, bridge_name):
        """Removes the specified bridge

        Args:
            bridge_name: The name of the bridge to remove.
        """
        # Check if the bridge exists.
        #
        # Cases where it may not are if we failed to initialize properly
        #
        # Or if we're doing 2.4Ghz and 5Ghz SSIDs and we've already torn
        # down the bridge once, but we got called for each band.
        result = self.ssh.run(
            'brctl show {bridge_name}'.format(bridge_name=bridge_name),
            ignore_status=True)

        # If the bridge exists, we'll get an exit_status of 0, indicating
        # success, so we can continue and remove the bridge.
        if result.exit_status == 0:
            self.ssh.run('ip link set {bridge_name} down'.format(
                bridge_name=bridge_name))
            self.ssh.run(
                'brctl delbr {bridge_name}'.format(bridge_name=bridge_name))

    def get_bssid_from_ssid(self, ssid, band):
        """Gets the BSSID from a provided SSID

        Args:
            ssid: An SSID string.
            band: 2G or 5G Wifi band.
        Returns: The BSSID if on the AP or None if SSID could not be found.
        """
        if band == hostapd_constants.BAND_2G:
            interfaces = [self.wlan_2g, ssid]
        else:
            interfaces = [self.wlan_5g, ssid]

        # Get the interface name associated with the given ssid.
        for interface in interfaces:
            cmd = "iw dev %s info|grep ssid|awk -F' ' '{print $2}'" % (
                str(interface))
            iw_output = self.ssh.run(cmd)
            if 'command failed: No such device' in iw_output.stderr:
                continue
            else:
                # If the configured ssid is equal to the given ssid, we found
                # the right interface.
                if iw_output.stdout == ssid:
                    cmd = "iw dev %s info|grep addr|awk -F' ' '{print $2}'" % (
                        str(interface))
                    iw_output = self.ssh.run(cmd)
                    return iw_output.stdout
        return None

    def stop_ap(self, identifier):
        """Stops a running ap on this controller.

        Args:
            identifier: The identify of the ap that should be taken down.
        """

        if identifier not in list(self._aps.keys()):
            raise ValueError('Invalid identifier %s given' % identifier)

        instance = self._aps.get(identifier)

        instance.hostapd.stop()
        self.stop_dhcp()
        self._ip_cmd.clear_ipv4_addresses(identifier)

        # DHCP server needs to refresh in order to tear down the subnet no
        # longer being used. In the event that all interfaces are torn down
        # then an exception gets thrown. We need to catch this exception and
        # check that all interfaces should actually be down.
        configured_subnets = [x.subnet for x in self._aps.values()]
        del self._aps[identifier]
        if configured_subnets:
            self.start_dhcp(subnets=configured_subnets)
        bridge_interfaces = self.interfaces.get_bridge_interface()
        if bridge_interfaces:
            for iface in bridge_interfaces:
                BRIDGE_DOWN = 'ifconfig {} down'.format(iface)
                BRIDGE_DEL = 'brctl delbr {}'.format(iface)
                self.ssh.run(BRIDGE_DOWN)
                self.ssh.run(BRIDGE_DEL)

    def stop_all_aps(self):
        """Stops all running aps on this device."""

        for ap in list(self._aps.keys()):
            try:
                self.stop_ap(ap)
            except dhcp_server.NoInterfaceError:
                pass

    def close(self):
        """Called to take down the entire access point.

        When called will stop all aps running on this host, shutdown the dhcp
        server, and stop the ssh connection.
        """

        if self._aps:
            self.stop_all_aps()
        self.ssh.close()

    def generate_bridge_configs(self, channel):
        """Generate a list of configs for a bridge between LAN and WLAN.

        Args:
            channel: the channel WLAN interface is brought up on
            iface_lan: the LAN interface to bridge
        Returns:
            configs: tuple containing iface_wlan, iface_lan and bridge_ip
        """

        if channel < 15:
            iface_wlan = self.wlan_2g
            subnet_str = self._AP_2G_SUBNET_STR
        else:
            iface_wlan = self.wlan_5g
            subnet_str = self._AP_5G_SUBNET_STR

        iface_lan = self.lan

        a, b, c, _ = subnet_str.strip('/24').split('.')
        bridge_ip = "%s.%s.%s.%s" % (a, b, c, BRIDGE_IP_LAST)

        configs = (iface_wlan, iface_lan, bridge_ip)

        return configs

    def install_scapy(self, scapy_path, send_ra_path):
        """Install scapy

        Args:
            scapy_path: path where scapy tar file is located on server
            send_ra_path: path where sendra path is located on server
        """
        self.scapy_install_path = self.ssh.run('mktemp -d').stdout.rstrip()
        self.log.info("Scapy install path: %s" % self.scapy_install_path)
        self.ssh.send_file(scapy_path, self.scapy_install_path)
        self.ssh.send_file(send_ra_path, self.scapy_install_path)

        scapy = os.path.join(self.scapy_install_path,
                             scapy_path.split('/')[-1])

        untar_res = self.ssh.run('tar -xvf %s -C %s' %
                                 (scapy, self.scapy_install_path))

        instl_res = self.ssh.run(
            'cd %s; %s' % (self.scapy_install_path, SCAPY_INSTALL_COMMAND))

    def cleanup_scapy(self):
        """ Cleanup scapy """
        if self.scapy_install_path:
            cmd = 'rm -rf %s' % self.scapy_install_path
            self.log.info("Cleaning up scapy %s" % cmd)
            output = self.ssh.run(cmd)
            self.scapy_install_path = None

    def send_ra(self,
                iface,
                mac=RA_MULTICAST_ADDR,
                interval=1,
                count=None,
                lifetime=LIFETIME,
                rtt=0):
        """Invoke scapy and send RA to the device.

        Args:
          iface: string of the WiFi interface to use for sending packets.
          mac: string HWAddr/MAC address to send the packets to.
          interval: int Time to sleep between consecutive packets.
          count: int Number of packets to be sent.
          lifetime: int original RA's router lifetime in seconds.
          rtt: retrans timer of the RA packet
        """
        scapy_command = os.path.join(self.scapy_install_path, RA_SCRIPT)
        options = ' -m %s -i %d -c %d -l %d -in %s -rtt %s' % (
            mac, interval, count, lifetime, iface, rtt)
        self.log.info("Scapy cmd: %s" % scapy_command + options)
        res = self.ssh.run(scapy_command + options)

    def get_icmp6intype134(self):
        """Read the value of Icmp6InType134 and return integer.

        Returns:
            Integer value >0 if grep is successful; 0 otherwise.
        """
        ra_count_str = self.ssh.run('grep Icmp6InType134 %s || true' %
                                    PROC_NET_SNMP6).stdout
        if ra_count_str:
            return int(ra_count_str.split()[1])
