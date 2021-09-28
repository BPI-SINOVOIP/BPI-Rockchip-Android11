# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import ui_utils
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.graphics import graphics_utils
from autotest_lib.client.cros.bluetooth import bluetooth_device_xmlrpc_server


class bluetooth_TurnOnOffUI(graphics_utils.GraphicsTest):

    """Go to Status Tray and turn BT On/Off"""
    version = 1

    # Node roles
    BUTTON_ROLE = "button"
    SWITCH_ROLE = "switch"

    # Node names
    STATUS_TRAY_REGEXP = "/Status tray, /i"
    SHOW_BLUETOOTH_SETTINGS = "/Show Bluetooth settings./i"
    BLUETOOTH = "Bluetooth"
    DELAY_BW_TOGGLE_ON_OFF = 5

    def initialize(self):
        """Autotest initialize function"""
        self.xmlrpc_delegate = \
            bluetooth_device_xmlrpc_server.BluetoothDeviceXmlRpcDelegate()
        super(bluetooth_TurnOnOffUI, self).initialize(raise_error_on_hang=True)

    def cleanup(self):
        """Autotest cleanup function"""
        if self._GSC:
            keyvals = self._GSC.get_memory_difference_keyvals()
            for key, val in keyvals.iteritems():
                self.output_perf_value(
                    description=key,
                    value=val,
                    units='bytes',
                    higher_is_better=False)
            self.write_perf_keyval(keyvals)
        super(bluetooth_TurnOnOffUI, self).cleanup()

    def open_status_tray(self, ui):
        """Open status tray

        @param ui: ui object
        """
        logging.info("Opening status tray")
        ui.doDefault_on_obj(self.STATUS_TRAY_REGEXP, True, self.BUTTON_ROLE)
        ui.wait_for_ui_obj(self.SHOW_BLUETOOTH_SETTINGS, True,
                           role=self.BUTTON_ROLE)

    def open_bluetooth_page(self, ui):
        """Opens bluetooth settings in tray

        @param ui: ui object
        """
        logging.info("Opening bluetooth settings in tray")
        ui.doDefault_on_obj(self.SHOW_BLUETOOTH_SETTINGS, True,
                            self.BUTTON_ROLE)
        ui.wait_for_ui_obj(self.BLUETOOTH, False, role=self.SWITCH_ROLE)

    def is_bluetooth_enabled(self):
        """Returns True if bluetoothd is powered on, otherwise False"""

        return self.xmlrpc_delegate._is_powered_on()

    def turn_on_bluetooth(self, ui):
        """Turn on BT in status tray

        @param ui: ui object
        """
        if self.is_bluetooth_enabled():
            logging.info('Bluetooth is turned on already..')
        else:
            logging.info("Turning on bluetooth")
            ui.doDefault_on_obj(self.BLUETOOTH, False, self.SWITCH_ROLE)
            time.sleep(self.DELAY_BW_TOGGLE_ON_OFF)
            if self.is_bluetooth_enabled():
                logging.info('Turned on BT successfully..')
            else:
                raise error.TestFail('BT is not turned on..')

    def turn_off_bluetooth(self, ui):
        """Turn off BT in status tray

        @param ui: ui object
        """
        if not self.is_bluetooth_enabled():
            logging.info('Bluetooth is turned off already')
        else:
            logging.info("Turning off bluetooth")
            ui.doDefault_on_obj(self.BLUETOOTH, False, self.SWITCH_ROLE)
            time.sleep(self.DELAY_BW_TOGGLE_ON_OFF)
            if not self.is_bluetooth_enabled():
                logging.info('Turned off BT successfully..')
            else:
                raise error.TestFail('Bluetooth is not turned off within time')

    def run_once(self, iteration_count=3):
        """Turn on/off bluetooth in status tray

        @param iteration_count: Number of iterations to toggle on/off

        """
        try:
            with chrome.Chrome(autotest_ext=True) as cr:
                ui = ui_utils.UI_Handler()
                ui.start_ui_root(cr)
                self.open_status_tray(ui)
                self.open_bluetooth_page(ui)
                logging.info("Turning off bluetooth before start test")
                self.turn_off_bluetooth(ui)
                for iteration in range(1, iteration_count + 1):
                    logging.info("Iteration: %d", iteration)
                    self.turn_on_bluetooth(ui)
                    self.turn_off_bluetooth(ui)
        except error.TestFail:
            raise
        except Exception as e:
            logging.error('Exception "%s" seen during test', e)
            raise error.TestFail('Exception "%s" seen during test' % e)
        finally:
            self.xmlrpc_delegate.reset_on()
