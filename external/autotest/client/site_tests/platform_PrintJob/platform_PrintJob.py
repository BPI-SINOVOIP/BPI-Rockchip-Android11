# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.chameleon import chameleon
from autotest_lib.client.cros.input_playback import input_playback

_CHECK_TIMEOUT = 20
_PRINTER_NAME = "HP OfficeJet g55"
_PRINTING_COMPLETE_NOTIF = "Printing complete"
_PRINTING_NOTIF = "Printing"
_STEPS_BETWEEN_CONTROLS = 4
_USB_PRINTER_CONNECTED_NOTIF = "USB printer connected"

_SHORT_WAIT = 2

class platform_PrintJob(test.test):
    """
    E2E test - Chrome is brought up, local pdf file open, print dialog open,
    chameleon gadget driver printer selected and print job executed. The test
    verifies the print job finished successfully.
    """
    version = 1

    def warmup(self):
        """Test setup."""
        # Emulate keyboard to play back shortcuts and navigate.
        # See input_playback README.
        self._player = input_playback.InputPlayback()
        self._player.emulate(input_type='keyboard')
        self._player.find_connected_inputs()


    def cleanup(self):
        if hasattr(self, 'browser'):
            self.browser.close()
        self._player.close()



    def _open_print_dialog(self):
        """Use keyboard shortcut to open print dialog."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_ctrl+p')
        time.sleep(_SHORT_WAIT)


    def _press_tab(self):
        """Use keyboard shortcut to press Tab."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_tab')


    def _press_down(self):
        """Use keyboard shortcut to press down."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_down')


    def _press_shift_tab(self):
        """Use keyboard shortcut to press Shift-Tab."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_shift+tab')


    def _press_enter(self):
        """Use keyboard shortcut to press Enter."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_enter')
        time.sleep(_SHORT_WAIT)


    def execute_print_job(self):
        """Using keyboard imput navigate to print dialog and execute a job."""

        # Open dialog and select 'See more'
        self._open_print_dialog()
        time.sleep(_SHORT_WAIT)

        # Navigate to printer selection
        for x in range(_STEPS_BETWEEN_CONTROLS):
            self._press_tab()
        self._press_down()
        self._press_tab()
        self._press_down()
        self._press_enter()

        # Go back to Print button
        for x in range(_STEPS_BETWEEN_CONTROLS):
            self._press_shift_tab()

        # Send the print job
        self._press_enter()


    def check_notification(self, notification):
        """Polls for successful print job notification"""

        def find_notification(title=None):
            notifications = self.cr.get_visible_notifications()
            if notifications is None:
                return False
            for n in notifications:
                if title in n['title']:
                    return True
            return False

        utils.poll_for_condition(
                condition=lambda: find_notification(notification),
                desc='Notification %s NOT found' % notification,
                timeout=_CHECK_TIMEOUT, sleep_interval=_SHORT_WAIT)


    def navigate_to_pdf(self):
        """Navigate to the pdf page to print"""
        self.cr.browser.platform.SetHTTPServerDirectories(self.bindir)
        tab = self.cr.browser.tabs.New()
        pdf_path = os.path.join(self.bindir, 'to_print.pdf')
        tab.Navigate(self.cr.browser.platform.http_server.UrlOf(pdf_path))
        tab.WaitForDocumentReadyStateToBeInteractiveOrBetter(
                timeout=_CHECK_TIMEOUT);
        time.sleep(_SHORT_WAIT)


    def run_once(self, host, args):
        """Run the test."""

        # Make chameleon host known to the DUT host crbug.com/862646
        chameleon_args = 'chameleon_host=' + host.hostname + '-chameleon'
        args.append(chameleon_args)

        chameleon_board = chameleon.create_chameleon_board(host.hostname, args)
        chameleon_board.setup_and_reset(self.outputdir)
        usb_printer = chameleon_board.get_usb_printer()
        usb_printer.SetPrinterModel(1008, 17, _PRINTER_NAME)

        with chrome.Chrome(autotest_ext=True,
                           init_network_controller=True) as self.cr:
            usb_printer.Plug()
            self.check_notification(_USB_PRINTER_CONNECTED_NOTIF)
            logging.info('Chameleon printer connected!')
            self.navigate_to_pdf()
            time.sleep(_SHORT_WAIT)
            logging.info('PDF file opened in browser!')
            self.execute_print_job()
            usb_printer.StartCapturingPrinterData()
            self.check_notification(_PRINTING_NOTIF)
            self.check_notification(_PRINTING_COMPLETE_NOTIF)
            usb_printer.StopCapturingPrinterData()
            usb_printer.Unplug()
