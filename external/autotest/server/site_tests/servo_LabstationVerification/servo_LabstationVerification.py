# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper test to run verification on a labstation."""

import json
import logging
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test
from autotest_lib.server import utils as server_utils
from autotest_lib.server.hosts import servo_host
from autotest_lib.server.hosts import factory

class servo_LabstationVerification(test.test):
    """Wrapper test to run verifications on a labstation image.

    This test verifies basic servod behavior on the host supplied to it e.g.
    that servod can start etc, before inferring the DUT attached to the servo
    device, and running more comprehensive servod tests by using a full
    cros_host and servo_host setup.
    """
    version = 1

    UL_BIT_MASK = 0x2

    def get_servo_mac(self, servo_proxy):
        """Given a servo's serial retrieve ethernet port mac address.

        @param servo_proxy: proxy to talk to servod

        @returns: mac address of the ethernet port as a string
        @raises: error.TestError: if mac address cannot be inferred
        """
        # TODO(coconutruben): once mac address retrieval through v4 is
        # implemented remove these lines of code, and replace with
        # servo_v4_eth_mac.
        try:
            serial = servo_proxy.get('support.serialname')
            if serial == 'unknown':
                serial = servo_proxy.get('serialname')
        except error.TestFail as e:
            if 'No control named' in e:
                serial = servo_proxy.get('serialname')
            else:
              raise e
        ctrl_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                 'serial_to_mac_map.json')
        with open(ctrl_path, 'r') as f:
            serial_mac_map = json.load(f)
        if not serial in serial_mac_map:
            raise error.TestError('Unable to retrieve mac address for '
                                  'serial %s' % serial)
        return str(serial_mac_map[serial])

    def _flip_UL_bit(self, byte):
        """Helper to flip the Universal/Local bit in a given byte.

        For some IPv6's extended unique identifier (EUI) 64 calculation
        part of the logic is to flip the U/L bit on the first byte.

        Note: it is the callers responsibility to ensure that |byte| is
        only one byte. This function will just flip the 7th bit of whatever
        is supplied and return that.

        @param byte: the byte to flip

        @returns: |byte| with it's U/L bit flipped.
        """
        return byte ^ self.UL_BIT_MASK

    def _from_mac_to_ipv6_eui_64(self, mac):
        """Convert a MAC address (IEEE EUI48) to a IEEE EUI64 node component.

        This follows guidelines to convert a mac address to an IPv6 node
        component by
        - splitting the mac into two parts
        - inserting 0xfffe in between the two parts
        - flipping the U/L bit on the first byte

        @param mac: string containing the mac address

        @returns: string containing the IEEE EUI64 node component to |mac|
        """
        mac_bytes = [b.lower() for b in mac.split(':')]
        # First, flip the 7th bit again. This converts the string coming from
        # the mac (as it's a hex) into an int, flips it, before casting it back
        # to a hex as is expected for the mac address.
        mac_bytes[0] = hex(self._flip_UL_bit(int(mac_bytes[0],16)))[2:]
        mac_bytes = (mac_bytes[:3] + ['ff', 'fe'] + mac_bytes[-3:])
        ipv6_components = []
        while mac_bytes:
            # IPv6 has two bytes between :
            ipv6_components.append('%s%s' % (mac_bytes.pop(0),
                                             mac_bytes.pop(0)))
        # Lastly, remove the leading 0s to have a well formatted concise IPv6.
        return ':'.join([c.lstrip('0') for c in ipv6_components])

    def _mac_to_ipv6_addr(self, mac, ipv6_network_component):
        """Helper to generate an IPv6 address given network component and mac.

        @param mac: the mac address of the target network interface
        @param ipv6_network_component: prefix + subnet id portion of IPv6 [:64]

        @returns: an IPv6 address that could be used to target the network
                  interface at |mac| if it's on the same network as the network
                  component indicates
        """
        # Do not add an extra/miss a ':' when glueing both parts together.
        glue = '' if ipv6_network_component[-1] == ':' else ':'
        return '%s%s%s' % (ipv6_network_component, glue,
                           self._from_mac_to_ipv6_eui_64(mac))

    def _from_ipv6_to_mac_address(self, ipv6):
        """Given an IPv6 address retrieve the mac address.

        Assuming the address at |ipv6| followed the conversion standard layed
        out at _from_mac_to_ipv6_eui_64() above, this helper does the inverse.

        @param ipv6: full IPv6 address to extract the mac address from

        @returns: mac address extracted from node component as a string
        """
        # The node component i.e. the one holding the mac info is the 64 bits.
        components = ipv6.split(':')[-4:]
        # This is reversing the EUI 64 logic.
        mac_bytes = []
        for component in components:
            # Expand the components fully again.
            full_component = component.rjust(4,'0')
            # Mac addresses use one byte components as opposed to the two byte
            # ones for IPv6 - split them up.
            mac_bytes.extend([full_component[:2], full_component[2:]])
        # First, flip the 7th bit again.
        mac_bytes[0] = self._flip_UL_bit(mac_bytes[0])
        # Second, remove the 0xFFFE bytes inserted in the middle again.
        mac_bytes = mac_bytes[:3] + mac_bytes[-3:]
        return ':'.join([c.lower() for c in mac_bytes])

    def _get_dev_from_hostname(self, host):
        """Determine what network device on |host| is associated with its ip.

        @param host: host to inspect

        @returns: dev net for network device handling ip associated with |host|
        """
        devs = [dev.strip() for dev in
                host.run('ls /sys/class/net').stdout.strip().split()]
        host_ip = server_utils.get_ip_address(host.hostname)
        if host_ip is None:
            # TODO(coconutruben): this happens when hostname is IPv6. For now,
            # just use hostname as it is. However, come back to this to either
            # fix get_ip_address to report the right ip address, or
            # use the socket API yourself.
            host_ip = host.hostname
        for dev in devs:
            try:
                host.run('ifconfig %s | grep %s' % (dev, host_ip))
                # If there's a match, then we found the dev name.
                return dev
            except error.AutoservRunError:
              # It's expected that for all but one network device grep will
              # find no match. Ignore it.
              logging.debug('ip %s not on dev %s', host_ip, dev)
              continue
        # If we get to this state, then we failed to find a network device.
        raise error.TestFail('Failed to find network dev corresponding to '
                             'ip %s.' % host.hostname)

    def get_dut_on_servo_ip(self, servo_host_proxy):
        """Retrieve the IPv4 IP of the DUT attached to a servo.

        Note: this will reboot the DUT if it fails initially to get the IP
        Note: for this to work, servo host and dut have to be on the same subnet

        @param servo_host_proxy: proxy to talk to the servo host

        @returns: IPv4 address of DUT attached to servo on |servo_host_proxy|

        @raises error.TestError: if the ip cannot be inferred
        """
        # Note: throughout this method, sh refers to servo host, dh to DUT host.
        # Figure out servo hosts IPv6 address that's based on its mac address.
        servo_proxy = servo_host_proxy._servo
        sh_nic_dev = self._get_dev_from_hostname(servo_host_proxy)
        addr_cmd ='cat /sys/class/net/%s/address' % sh_nic_dev
        sh_dev_addr = servo_host_proxy.run(addr_cmd).stdout.strip()
        logging.debug('Inferred Labstation MAC to be: %s', sh_dev_addr)
        sh_dev_ipv6_stub = self._from_mac_to_ipv6_eui_64(sh_dev_addr)
        # This will get us the IPv6 address that uses the mac address as node id
        cmd = (r'ifconfig %s | grep -oE "([0-9a-f]{0,4}:){4}%s"' %
               (sh_nic_dev, sh_dev_ipv6_stub))
        servo_host_ipv6 = servo_host_proxy.run(cmd).stdout.strip()
        logging.debug('Inferred Labstation IPv6 to be: %s', servo_host_ipv6)
        # Figure out DUTs expected IPv6 address
        # The network component should be shared between the DUT and the servo
        # host as long as they're on the same subnet.
        network_component = ':'.join(servo_host_ipv6.split(':')[:4])
        dut_ipv6 = self._mac_to_ipv6_addr(self.get_servo_mac(servo_proxy),
                                          network_component)
        logging.info('Inferred DUT IPv6 to be: %s', dut_ipv6)
        # Make a temporary cros host using the IPv6
        temp_dut_host = factory.create_host(dut_ipv6)
        # Try to ping the DUT
        if not temp_dut_host.is_up(timeout=20):
            # on failure reboot, wait 7s
            servo_proxy.set('cold_reset', 'on')
            servo_proxy.set('cold_reset', 'off')
            time.sleep(20)
            if not temp_dut_host.is_up(timeout=20):
                temp_dut_host.close()
                raise error.TestFail('Failed to find DUT on IPv6: %s' %
                                     dut_ipv6)
        # Figure out what dev the IPv6 belongs to.
        dh_nic_dev = self._get_dev_from_hostname(temp_dut_host)
        # Use ifconfig to determine the IPv4 address associated with
        # |dh_nic_dev| on the DUT under the inet attribute.
        ipv4_cmd = (r"ifconfig %s | sed -nr 's|\s+inet\s+([0-9.]+)\s.*$|\1|p'" %
                    dh_nic_dev)
        dut_ipv4 = temp_dut_host.run(ipv4_cmd).stdout.strip()
        logging.info('Inferred DUT IPv4 to be: %s', dut_ipv4)
        # Close out this host as a new host to the same DUT with the IPv4 will
        # be created.
        temp_dut_host.close()
        return dut_ipv4

    def initialize(self, host, config=None):
        """Setup servod on |host| to run subsequent tests.

        @param host: CrosHost object representing the DUT.
        @param config: the args argument from test_that in a dict.
        """
        # Save the host.
        self.labstation_host = host
        # Make sure recovery is quick in case of failure.
        self.job.fast = True
        # First, stop all servod instances running on the labstation to test.
        host.run('sudo stop servod PORT=9999', ignore_status=True)
        # Wait for existing servod turned down.
        time.sleep(3)
        # Then, restart servod ourselves.
        host.run_background('start servod BOARD=nami PORT=9999')
        # Give servod plenty of time to come up.
        time.sleep(40)
        try:
            host.run_grep('servodutil show -p 9999',
                          stdout_err_regexp='No servod scratch entry found.')
        except error.AutoservRunError:
            raise error.TestFail('Servod did not come up on labstation.')
        self.dut_ip = None
        if config and 'dut_ip' in config:
            # Retrieve DUT ip from args if caller specified it.
            self.dut_ip = config['dut_ip']

    def run_once(self, local=False):
        """Run through the test sequence.

        This test currently runs through:
        -// ServoLabControlVerification where |host| is treated as a DUT.
        Subsequently, all tests use |host| as a servo host to a generated
        DUT host that's hanging on the servo device.
        - platform_ServoPowerStateController without usb
        - servo_USBMuxVerification
        - platform_InstallTestImage
        - platform_ServoPowerStateController with usb as a test image should
          be on the stick now

        @param local: whether a test image is already on the usb stick.
        """
        success = True
        success &= self.runsubtest('servo_LabControlVerification',
                                   host=self.labstation_host,
                                   disable_sysinfo=True)
        # Servod came up successfully - build a servo host and use it to verify
        # basic functionality.
        servo_args = {servo_host.SERVO_HOST_ATTR: self.labstation_host.hostname,
                      servo_host.SERVO_PORT_ATTR: 9999,
                      'is_in_lab': False}
        # Close out this host as the test will restart it as a servo host.
        self.labstation_host.close()
        self.labstation_host = servo_host.create_servo_host(None, servo_args)
        self.labstation_host.connect_servo()
        servo_proxy = self.labstation_host.get_servo()
        if not self.dut_ip:
            self.dut_ip = self.get_dut_on_servo_ip(self.labstation_host)
        logging.info('Running the DUT side on DUT %r', self.dut_ip)
        dut_host = factory.create_host(self.dut_ip)
        dut_host.set_servo_host(self.labstation_host)
        success &= self.runsubtest('platform_ServoPowerStateController',
                                   host=dut_host, usb_available=False,
                                   subdir_tag='no_usb', disable_sysinfo=True)
        success &= self.runsubtest('servo_USBMuxVerification', host=dut_host,
                                   disable_sysinfo=True)
        # This test needs to run before the power state controller test that
        # uses the USB stick as this test downloads the image onto the stick.
        try:
            # Passing |local| here indicates whether the test should stage and
            # download the test image itself (through a dev server) or whether
            # a test image is already on the usb stick.
            success &= self.runsubtest('platform_InstallTestImage',
                                       host=dut_host, local=local,
                                       disable_sysinfo=True)
        except error.TestBaseException as e:
            # Something went wrong with platform_InstallTestImage.
            # Remove this catch once crbug.com/953113 is fixed.
            raise error.TestNAError('Issue running platform_InstallTestImage: '
                                    '%s', str(e))
        success &= self.runsubtest('platform_ServoPowerStateController',
                                   host=dut_host, usb_available=True,
                                   subdir_tag='usb', disable_sysinfo=True)
        if not success:
            raise error.TestFail('At least one verification test failed. '
                                 'Check the logs.')

    def cleanup(self):
        """Clean up by stopping the servod instance again."""
        self.labstation_host.run_background('stop servod PORT=9999')
