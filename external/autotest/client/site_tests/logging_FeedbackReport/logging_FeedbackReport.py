# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.input_playback import input_playback


class logging_FeedbackReport(test.test):
    """Tests if feedback report can be opened with no crashes in browser."""
    version = 1
    _FEEDBACK_ID = 'gfdkimpbcpahaombhbimeihdjnejgicl'
    _FEEDBACK_STATE_TIMEOUT = 40
    _WAIT = 2
    _FEEDBACK_SENT_URL = 'support.google.com/chromebook/answer/3142217'

    def warmup(self):
        """Test setup."""
        # Emulate keyboard to open feedback app.
        # See input_playback. The keyboard is used to play back shortcuts.
        self._player = input_playback.InputPlayback()
        self._player.emulate(input_type='keyboard')
        self._player.find_connected_inputs()

    def _open_feedback(self):
        """Use keyboard shortcut to emulate input to open feedback app."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_alt+shift+i')

    def _check_feedback_elements(self):
        """
        Return whether feedback app is open or not.

        @returns: True if all elements are present, else False.

        """
        # Verifying feedback app window is open.
        if not self.feedback_app.EvaluateJavaScript('document.body != null'):
            logging.info('Window not enabled.')
            return False

        # Verifying UI elements in window are enabled.
        elements = ['cancel-button', 'send-report-button',
                    'description-text']
        for element in elements:
            js = "document.getElementById('%s') != null" % element
            if not self.feedback_app.EvaluateJavaScript(js):
                logging.info("%s not enabled.", element)
                return False

        return True

    def _confirm_feedback_state(self):
        """
        Fail test if feedback elements have not been found.

        @raises: error.TestFail if feedback app not found.

        """
        utils.poll_for_condition(
                lambda: self._check_feedback_elements(),
                exception=error.TestFail('Feedback elements not enabled.'),
                timeout=self._FEEDBACK_STATE_TIMEOUT)

    def _enter_feedback_text(self):
        """Enter Feedback message in the Text field"""
        time.sleep(self._WAIT)
        self._player.blocking_playback_of_default_file(
               input_type='keyboard', filename='keyboard_T+e+s+t')

    def _press_enter(self):
        """Use keyboard shortcut to press Enter."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_enter')

    def _press_shift_tab(self):
        """Use keyboard shortcut to press Shift-Tab."""
        self._player.blocking_playback_of_default_file(
            input_type='keyboard', filename='keyboard_shift+tab')

    def _submit_feedback(self):
        """Click on Send button to submit Feedback Report using keyboard input"""
        self._enter_feedback_text()
        self._press_shift_tab()
        self._press_enter()

    def _is_feedback_sent(self, start_time, timeout):
        """Checks feedback is sent within timeout

        @param start_time: beginning timestamp
        @param timeout: duration of URL checks

        @returns: True if feedback sent page is present
        """
        while True:
            time.sleep(self._WAIT)
            for tab in self.cr.browser.tabs:
                if self._FEEDBACK_SENT_URL in tab.url:
                    return True
            if time.time() - start_time >= timeout:
                break;
        return False

    def run_once(self):
        """Run the test."""
        with chrome.Chrome(disable_default_apps=False) as self.cr:
            # Open and confirm feedback app is working.
            self._open_feedback()
            cr_exts = self.cr.browser.extensions
            self.feedback_app = None
            for extension in cr_exts.GetByExtensionId(self._FEEDBACK_ID):
                url = extension.EvaluateJavaScript('location.href;')
                if url.endswith('default.html'):
                    self.feedback_app = extension
                    break

            if self.feedback_app is None:
                raise error.TestError("Incorrect feedback id list.")
            self._confirm_feedback_state()
            self._submit_feedback()

            start_time = time.time()
            if not self._is_feedback_sent(start_time, self._WAIT * 30):
                raise error.TestFail("Feedback NOT sent!")

    def cleanup(self):
        """Test cleanup."""
        self._player.close()
