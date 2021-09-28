# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Login with test account and display chart file using telemetry.'''

# This sets up import paths for autotest.
import common

import argparse
import contextlib
import logging
import os
import signal
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.input_playback import keyboard

DISPLAY_LEVEL = 96.0


@contextlib.contextmanager
def set_display_brightness(display_level):
    SET_BRIGHTNESS_CMD = 'backlight_tool --set_brightness_percent=%s'

    original_display_level = utils.system_output(
            'backlight_tool --get_brightness_percent')
    logging.info('Save original display brightness %r '
                 'and fix display brightness to %r', original_display_level,
                 display_level)
    utils.system(SET_BRIGHTNESS_CMD % display_level)
    utils.system('stop powerd', ignore_status=True)
    yield
    logging.info('Restore display brightness %r', original_display_level)
    utils.system('start powerd', ignore_status=True)
    utils.system(SET_BRIGHTNESS_CMD % original_display_level)


def display(filepath):
    """Display chart with filepath on device by using telemetry."""
    assert os.path.isfile(filepath), 'filepath %r not found.' % filepath
    filepath = os.path.abspath(filepath)

    logging.info('Setup SIGINT listener for stop displaying.')
    displaying = [True]

    def handler(signum, frame):
        """Wait signal to clear running flag."""
        if signum == signal.SIGINT:
            displaying.pop()

    signal.signal(signal.SIGINT, handler)

    with chrome.Chrome() as cr, set_display_brightness(DISPLAY_LEVEL):
        logging.info('Display chart file of path %r.', filepath)
        tab = cr.browser.tabs[0]
        tab.Navigate('file://' + filepath)

        logging.info('Set chart tab fullscreen.')
        kb = keyboard.Keyboard()
        kb.press_key('f4')
        kb.close()

        while displaying:
            time.sleep(1)


if __name__ == '__main__':
    argparser = argparse.ArgumentParser(
            description='Display chart file on chrome by using telemetry.'
            ' Send SIGINT or keyboard interrupt to stop displaying.')
    argparser.add_argument('filepath', help='Path of displayed chart file.')

    args = argparser.parse_args()
    display(args.filepath)
