#!/usr/bin/python2
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mox
import pexpect
import unittest

import dli

import rpm_controller

import common
from autotest_lib.site_utils.rpm_control_system import utils


class TestRPMControllerQueue(mox.MoxTestBase):
    """Test request can be queued and processed in controller."""

    def setUp(self):
        super(TestRPMControllerQueue, self).setUp()
        self.rpm = rpm_controller.SentryRPMController('chromeos-rack1-host8')
        self.powerunit_info = utils.PowerUnitInfo(
                device_hostname='chromos-rack1-host8',
                powerunit_hostname='chromeos-rack1-rpm1',
                powerunit_type=utils.PowerUnitInfo.POWERUNIT_TYPES.RPM,
                outlet='.A100',
                hydra_hostname=None)


    def testQueueRequest(self):
        """Should create a new process to handle request."""
        new_state = 'ON'
        process = self.mox.CreateMockAnything()
        rpm_controller.multiprocessing.Process = self.mox.CreateMockAnything()
        rpm_controller.multiprocessing.Process(target=mox.IgnoreArg(),
                args=mox.IgnoreArg()).AndReturn(process)
        process.start()
        process.join()
        self.mox.ReplayAll()
        self.assertFalse(self.rpm.queue_request(self.powerunit_info, new_state))
        self.mox.VerifyAll()


class TestSentryRPMController(mox.MoxTestBase):
    """Test SentryRPMController."""


    def setUp(self):
        super(TestSentryRPMController, self).setUp()
        self.ssh = self.mox.CreateMockAnything()
        rpm_controller.pexpect.spawn = self.mox.CreateMockAnything()
        rpm_controller.pexpect.spawn(mox.IgnoreArg()).AndReturn(self.ssh)
        self.rpm = rpm_controller.SentryRPMController('chromeos-rack1-host8')
        self.powerunit_info = utils.PowerUnitInfo(
                device_hostname='chromos-rack1-host8',
                powerunit_hostname='chromeos-rack1-rpm1',
                powerunit_type=utils.PowerUnitInfo.POWERUNIT_TYPES.RPM,
                outlet='.A100',
                hydra_hostname=None)


    def testSuccessfullyChangeOutlet(self):
        """Should return True if change was successful."""
        prompt = ['Switched CDU:', 'Switched PDU:']
        password = 'admn'
        new_state = 'ON'
        self.ssh.expect('Password:', timeout=60)
        self.ssh.sendline(password)
        self.ssh.expect(prompt, timeout=60)
        self.ssh.sendline('%s %s' % (new_state, self.powerunit_info.outlet))
        self.ssh.expect('Command successful', timeout=60)
        self.ssh.sendline('logout')
        self.ssh.close(force=True)
        self.mox.ReplayAll()
        self.assertTrue(self.rpm.set_power_state(
                self.powerunit_info, new_state))
        self.mox.VerifyAll()


    def testUnsuccessfullyChangeOutlet(self):
        """Should return False if change was unsuccessful."""
        prompt = ['Switched CDU:', 'Switched PDU:']
        password = 'admn'
        new_state = 'ON'
        self.ssh.expect('Password:', timeout=60)
        self.ssh.sendline(password)
        self.ssh.expect(prompt, timeout=60)
        self.ssh.sendline('%s %s' % (new_state, self.powerunit_info.outlet))
        self.ssh.expect('Command successful',
                        timeout=60).AndRaise(pexpect.TIMEOUT('Timed Out'))
        self.ssh.sendline('logout')
        self.ssh.close(force=True)
        self.mox.ReplayAll()
        self.assertFalse(
            self.rpm.set_power_state(self.powerunit_info, new_state))
        self.mox.VerifyAll()


class TestWebPoweredRPMController(mox.MoxTestBase):
    """Test WebPoweredRPMController."""


    def setUp(self):
        super(TestWebPoweredRPMController, self).setUp()
        self.dli_ps = self.mox.CreateMock(dli.powerswitch)
        hostname = 'chromeos-rack8a-rpm1'
        self.web_rpm = rpm_controller.WebPoweredRPMController(hostname,
                                                              self.dli_ps)
        outlet = 8
        dut = 'chromeos-rack8a-host8'
        # Outlet statuses are in the format "u'ON'"
        initial_state = 'u\'ON\''
        self.test_status_list_initial = [[outlet, dut, initial_state]]
        self.powerunit_info = utils.PowerUnitInfo(
                device_hostname=dut,
                powerunit_hostname=hostname,
                powerunit_type=utils.PowerUnitInfo.POWERUNIT_TYPES.RPM,
                outlet=outlet,
                hydra_hostname=None)


    def testSuccessfullyChangeOutlet(self):
        """Should return True if change was successful."""
        test_status_list_final = [[8, 'chromeos-rack8a-host8', 'u\'OFF\'']]
        self.dli_ps.off(8)
        self.dli_ps.statuslist().AndReturn(test_status_list_final)
        self.mox.ReplayAll()
        self.assertTrue(self.web_rpm.set_power_state(
                self.powerunit_info, 'OFF'))
        self.mox.VerifyAll()


    def testUnsuccessfullyChangeOutlet(self):
        """Should return False if Outlet State does not change."""
        test_status_list_final = [[8, 'chromeos-rack8a-host8', 'u\'ON\'']]
        self.dli_ps.off(8)
        self.dli_ps.statuslist().AndReturn(test_status_list_final)
        self.mox.ReplayAll()
        self.assertFalse(self.web_rpm.set_power_state(
                self.powerunit_info, 'OFF'))
        self.mox.VerifyAll()


    def testNoOutlet(self):
        """Should return False if DUT hostname is not on the RPM device."""
        self.powerunit_info.outlet=None
        self.assertFalse(self.web_rpm.set_power_state(
                self.powerunit_info, 'OFF'))


class TestCiscoPOEController(mox.MoxTestBase):
    """Test CiscoPOEController."""

    DEVICE = 'chromeos2-poe-sw8#'
    MATCHER = 'Port\\s+.*%s(\\s+(\\S+)){6,6}.*%s'
    PORT = 'fa32'
    PWD = 'Password:'
    SERVO = 'chromeos1-rack3-host12-servo'
    SWITCH = 'chromeos2-poe-switch8'
    POWERUNIT_INFO = utils.PowerUnitInfo(
            device_hostname=PORT,
            powerunit_hostname=SERVO,
            powerunit_type=utils.PowerUnitInfo.POWERUNIT_TYPES.POE,
            outlet=PORT,
            hydra_hostname=None)

    def testLogin(self):
        """Test we can log into the switch."""
        rpm_controller.pexpect.spawn = self.mox.CreateMockAnything()
        mock_ssh = self.mox.CreateMockAnything()
        rpm_controller.pexpect.spawn(mox.IgnoreArg()).AndReturn(mock_ssh)
        sut = rpm_controller.CiscoPOEController(self.SWITCH)
        mock_ssh.expect(sut.POE_USERNAME_PROMPT, timeout=sut.LOGIN_TIMEOUT)
        mock_ssh.sendline(mox.IgnoreArg())
        mock_ssh.expect(self.PWD, timeout=sut.LOGIN_TIMEOUT)
        mock_ssh.sendline(mox.IgnoreArg())
        mock_ssh.expect(self.DEVICE, timeout=sut.LOGIN_TIMEOUT)
        self.mox.ReplayAll()
        self.assertIsNotNone(sut._login())
        self.mox.VerifyAll()

    def testSuccessfullyChangePowerState(self):
        """Should return True if change was successful."""
        sut = rpm_controller.CiscoPOEController(self.SWITCH)
        mock_ssh = self.mox.CreateMockAnything()
        self.mox.StubOutWithMock(sut, '_login')
        sut._login().AndReturn(mock_ssh)
        self.mox.StubOutWithMock(sut, '_verify_state')
        sut._verify_state(self.PORT, 'ON', mock_ssh).AndReturn(True)
        # _enter_configuration_terminal
        mock_ssh.sendline(sut.CONFIG)
        mock_ssh.expect(sut.config_prompt, timeout=sut.CMD_TIMEOUT)
        mock_ssh.sendline(sut.CONFIG_IF % self.PORT)
        mock_ssh.expect(sut.config_if_prompt, timeout=sut.CMD_TIMEOUT)
        # _change_state
        mock_ssh.sendline(sut.SET_STATE_ON)
        # _exit_configuration_terminal
        mock_ssh.sendline(sut.END_CMD)
        mock_ssh.expect(sut.poe_prompt, timeout=sut.CMD_TIMEOUT)
        # exit
        mock_ssh.sendline(sut.EXIT_CMD)
        mock_ssh.close(force=True)
        self.mox.ReplayAll()
        self.assertTrue(sut.set_power_state(self.POWERUNIT_INFO, 'ON'))
        self.mox.VerifyAll()

    def testUnableToEnterConfigurationTerminal(self):
        """Should return False if unable to enter configuration terminal."""
        exception = pexpect.TIMEOUT('Could not enter configuration terminal.')
        sut = rpm_controller.CiscoPOEController(self.SWITCH)
        timeout = sut.CMD_TIMEOUT
        mock_ssh = self.mox.CreateMockAnything()
        self.mox.StubOutWithMock(sut, '_login')
        sut._login().AndReturn(mock_ssh)
        mock_ssh.sendline(sut.CONFIG)
        mock_ssh.expect(sut.config_prompt, timeout=timeout)
        mock_ssh.sendline(sut.CONFIG_IF % self.PORT)
        config_if_prompt = sut.config_if_prompt
        mock_ssh.expect(config_if_prompt, timeout=timeout).AndRaise(exception)
        mock_ssh.sendline(sut.END_CMD)
        mock_ssh.sendline(sut.EXIT_CMD)
        mock_ssh.close(force=True)
        self.mox.ReplayAll()
        self.assertFalse(sut.set_power_state(self.POWERUNIT_INFO, mock_ssh))
        self.mox.VerifyAll()

    def testUnableToExitConfigurationTerminal(self):
        """Should return False if unable to exit configuration terminal."""
        exception = pexpect.TIMEOUT('Could not exit configuration terminal.')
        sut = rpm_controller.CiscoPOEController(self.SWITCH)
        mock_ssh = self.mox.CreateMockAnything()
        self.mox.StubOutWithMock(sut, '_login')
        self.mox.StubOutWithMock(sut, '_enter_configuration_terminal')
        sut._login().AndReturn(mock_ssh)
        sut._enter_configuration_terminal(self.PORT, mock_ssh).AndReturn(True)
        mock_ssh.sendline(sut.SET_STATE_ON)
        mock_ssh.sendline(sut.END_CMD)
        mock_ssh.expect(
                self.DEVICE, timeout=sut.CMD_TIMEOUT).AndRaise(exception)
        mock_ssh.sendline(sut.EXIT_CMD)
        mock_ssh.close(force=True)
        self.mox.ReplayAll()
        self.assertFalse(sut.set_power_state(self.POWERUNIT_INFO, 'ON'))
        self.mox.VerifyAll()

    def testUnableToVerifyState(self):
        """Should return False if unable to verify current state."""
        sut = rpm_controller.CiscoPOEController(self.SWITCH)
        mock_ssh = self.mox.CreateMockAnything()
        self.mox.StubOutWithMock(sut, '_login')
        self.mox.StubOutWithMock(sut, '_enter_configuration_terminal')
        self.mox.StubOutWithMock(sut, '_exit_configuration_terminal')
        sut._login().AndReturn(mock_ssh)
        sut._enter_configuration_terminal(self.PORT, mock_ssh).AndReturn(True)
        sut._exit_configuration_terminal(mock_ssh).AndReturn(True)
        mock_ssh.sendline(sut.SET_STATE_ON)
        mock_ssh.sendline(sut.CHECK_INTERFACE_STATE % self.PORT)
        exception = pexpect.TIMEOUT('Could not verify state.')
        matcher = self.MATCHER % (self.PORT, self.DEVICE)
        mock_ssh.expect(matcher, timeout=sut.CMD_TIMEOUT).AndRaise(exception)
        mock_ssh.sendline(sut.EXIT_CMD)
        mock_ssh.close(force=True)
        self.mox.ReplayAll()
        self.assertFalse(sut.set_power_state(self.POWERUNIT_INFO, 'ON'))
        self.mox.VerifyAll()


if __name__ == "__main__":
    unittest.main()
