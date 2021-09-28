# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the servo `power_state` control.

## Purpose
This test exists to ensure that servo's `power_state` control provides a
consistent, hardware-independent implementation across all models of
Chrome hardware.

    ==== NOTE NOTE NOTE ====
    YOU ARE NOT ALLOWED TO CHANGE THIS TEST TO TREAT DIFFERENT HARDWARE
    DIFFERENTLY.

In particular, importing from `faft.config` in order to customize this
test for different hardware is never allowed.  This has been tried twice
before; anyone making a third attempt is liable to be taken out and
shot.  Or at least, be given a good talking to.

This test is intended to enforce a contract between hardware-independent
parts of Autotest (especially the repair code) and hardware-dependent
implementations of servo in `hdctools`.  The contract imposes requirements
and limitations on both sides.

## Requirements on the servo implementation in `hdctools`
### Setting `power_state:off`
Setting `power_state:off` must turn the DUT "off" unconditionally.  The
operation must have the same effect regardless of the state of the DUT
prior to the operation.  The operation is not allowed to fail.

The meaning of "off" does not require any specific component to be
actually powered off, provided that the DUT does not respond on the
network, and does not respond to USB devices being plugged or unplugged.
Some examples of "off" that are acceptable:
  * A system in ACPI power state S5.
  * A system held in reset indefinitely by asserting `cold_reset:on`.
  * In general, any system where power is not supplied to the AP.

While a DUT is turned off, it is allowed to respond to the power button,
the lid, and to the reset signals in ways that are hardware dependent.
The signals may be ignored, or they may have any other effect that's
appropriate to the hardware, such as turning the DUT on.

### Setting `power_state:on` and `power_state:rec`
After applying `power_state:off` to turn a DUT off, setting
`power_state:on` or `power_state:rec` will turn the DUT on.
  * Setting "on" turns the DUT on in normal mode.  The DUT will
    boot normally as for a cold start.
  * Setting "rec" turns the DUT on in recovery mode.  The DUT
    will go to the recovery screen and wait for a USB stick to be
    inserted.

If a DUT is not off because of setting `power_state:off`, the response
to "on" and "rec" may be hardware dependent.  The settings may turn the
device on as specified, they may have no effect, or they may provide any
other response that's appropriate to the hardware.

### Setting `power_state:reset`
Applying `power_state:reset` has the same visible effects as setting
`power_state:off` followed by `power_state:on`.  The operation must have
the same effect regardless of the state of the DUT prior to the
operation.  The operation is not allowed to fail.

Additionally, applying reset will assert and deassert `cold_reset` to
ensure a full hardware reset.

### Timing Constraints
The servo implementation for `power_state` cannot impose timing
constraints on Autotest.  If the hardware imposes constraints, the servo
implemention must provide the necessary time delays by sleeping.

In particular:
  * After `power_state:off`, it is allowed to immediately apply either
    `on` or `rec`.
  * After `rec`, USB stick insertion must be recognized immediately.
  * Setting `power_state:reset` twice in a row must work reliably.

### Requirements on Autotest
### Setting `power_state:off`
Once a DUT has been turned "off", changing signals such as the power
button, the lid, or reset signals isn't allowed.  The only allowed
state changes are these:
  * USB muxes may be switched at will.
  * The `power_state` control can be set to any valid setting.

Any other operation may cause the DUT to change state unpredictably
(e.g. by powering the DUT on).

## Setting `power_state:on` and `power_state:rec`
Autotest can only apply `power_state:on` or `power_state:rec` if the DUT
was previously turned off with `power_state:off`.

"""

import logging
import time

# STOP!  You are not allowed to import anything from FAFT.  Read the
# comments above.
from autotest_lib.client.common_lib import error
from autotest_lib.server import test


class platform_ServoPowerStateController(test.test):
    """Test servo can power on and off DUT in recovery and non-recovery mode."""
    version = 1


    def initialize(self, host):
        """Initialize DUT for testing."""
        pass


    def cleanup(self):
        """Clean up DUT after servo actions."""
        if not self.host.is_up():
            # Power off, then power on DUT from internal storage.
            self.controller.power_off()
            self.host.servo.switch_usbkey('off')
            self.controller.power_on(self.controller.REC_OFF)
            self.host.wait_up(timeout=300)


    def assert_dut_on(self, rec_on=False):
        """Confirm DUT is powered on, claim test failure if DUT is off.

        @param rec_on: True if DUT should boot from external USB stick as in
                       recovery mode.

        @raise TestFail: If DUT is off or DUT boot from wrong source.
        """
        if not self.host.wait_up(timeout=300):
            raise error.TestFail('power_state:%s did not turn DUT on.' %
                                 ('rec' if rec_on else 'on'))

        # Check boot source. Raise TestFail if DUT boot from wrong source.
        boot_from_usb = self.host.is_boot_from_usb()
        if boot_from_usb != rec_on:
            boot_source = ('USB' if boot_from_usb else
                           'non-removable storage')
            raise error.TestFail('power_state:%s booted from %s.' %
                                 ('rec' if rec_on else 'on', boot_source))


    def assert_dut_off(self, error_message):
        """Confirm DUT is off and does not turn back on after 30 seconds.

        @param error_message: Error message to raise if DUT stays on.
        @raise TestFail: If DUT stays on.
        """
        if not self.host.ping_wait_down(timeout=10):
            raise error.TestFail(error_message)

        if self.host.ping_wait_up(timeout=30):
            raise error.TestFail('%s. %s' % (error_message, 'DUT turns back on'
                                             ' after it is turned off.'))


    def test_with_usb_plugged_in(self):
        """Run test when USB stick is plugged in to servo."""
        logging.info('Power off DUT')
        self.controller.power_off()
        self.assert_dut_off('power_state:off did not turn DUT off.')

        logging.info('Power DUT on in recovery mode, DUT shall boot from USB.')
        self.host.servo.switch_usbkey('off')
        self.controller.power_on(self.controller.REC_ON)
        self.assert_dut_off('power_state:rec didn\'t stay at recovery screen.')

        self.host.servo.switch_usbkey('dut')
        time.sleep(30)
        self.assert_dut_on(rec_on=True)

        logging.info('Power off DUT which is up in recovery mode.')
        self.controller.power_off()
        self.assert_dut_off('power_state:off failed after boot from external '
                            'USB stick.')

        logging.info('Power DUT off in recovery mode without booting.')
        self.host.servo.switch_usbkey('off')
        self.controller.power_on(self.controller.REC_ON)
        self.controller.power_off()
        self.assert_dut_off('power_state:off failed at recovery screen ')

        # Power DUT on in non-recovery mode with USB stick plugged in.
        # DUT shall boot from internal storage.
        logging.info('Power on DUT in non-recovery mode.')
        self.host.servo.switch_usbkey('dut')
        self.controller.power_on(self.controller.REC_OFF)
        self.assert_dut_on()
        self.host.servo.switch_usbkey('off')


    def test_with_usb_unplugged(self):
        """Run test when USB stick is not plugged in servo."""
        # Power off DUT regardless its current status.
        logging.info('Power off DUT.')
        self.controller.power_off()
        self.assert_dut_off('power_state:off did not turn DUT off.')

        # Try to power off the DUT again, make sure the DUT stays off.
        logging.info('Power off DUT which is already off.')
        self.controller.power_off()
        self.assert_dut_off('power_state:off turned DUT on.')

        # USB stick should be unplugged before the test.
        self.host.servo.switch_usbkey('off')

        logging.info('Power on in non-recovery mode.')
        self.controller.power_on(self.controller.REC_OFF)
        self.assert_dut_on(rec_on=False)

        logging.info('Power DUT off and on without delay. DUT should be '
                     'on after power_on is completed.')
        self.controller.power_off()
        self.controller.power_on(self.controller.REC_OFF)
        self.assert_dut_on(rec_on=False)

        logging.info('Power off DUT which is up in non-recovery mode.')
        self.controller.power_off()
        self.assert_dut_off('power_state:off failed after boot from '
                            'internal storage.')

        logging.info('Power DUT off and reset. DUT should be on after '
                     'reset is completed.')
        self.controller.reset()
        self.assert_dut_on(rec_on=False)

        logging.info('Reset DUT when it\'s on. DUT should be on after '
                     'reset is completed.')
        boot_id = self.host.get_boot_id()
        self.controller.reset()
        self.assert_dut_on(rec_on=False)
        new_boot_id = self.host.get_boot_id()
        if not new_boot_id or boot_id == new_boot_id:
            raise error.TestFail('power_state:reset failed to reboot DUT.')


    def run_once(self, host, usb_available=True):
        """Run the test.

        @param host: host object of tested DUT.
        @param usb_plugged_in: True if USB stick is plugged in servo.
        """
        self.host = host
        self.controller = host.servo.get_power_state_controller()

        self.test_with_usb_unplugged()
        if usb_available:
            self.test_with_usb_plugged_in()
