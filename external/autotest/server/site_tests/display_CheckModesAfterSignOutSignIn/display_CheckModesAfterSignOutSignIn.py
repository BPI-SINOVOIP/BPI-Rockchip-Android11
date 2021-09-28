# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.server import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.chameleon import chameleon_port_finder
from autotest_lib.client.cros.chameleon import chameleon_screen_test
from autotest_lib.server.cros.multimedia import remote_facade_factory


class display_CheckModesAfterSignOutSignIn(test.test):
    """ To Check the modes after sign out/sign in"""
    version = 1
    KEY_DELAY = 0.3
    LOGOUT_DELAY = 20

    def logout(self):
        """Log out the User"""
        logging.info("Signing out")

        # Pressing the keys twice to logout
        self.input_facade.blocking_playback_of_default_file("keyboard",
                                                            'keyboard_ctrl+shift+q')
        time.sleep(self.KEY_DELAY)
        self.input_facade.blocking_playback_of_default_file("keyboard",
                                                            'keyboard_ctrl+shift+q')

    def is_logged_out(self):
        """Will check whether user logged out"""
        logging.debug("checking /home/chronos/user/Downloads/ to know "
                      "whether user logged out or not!")
        return self.host.path_exists('/home/chronos/user/Downloads/') is False

    def check_mode(self, test_mirrored_mode=True):
        """Checks the display mode is as expected

        @param test_mirrored: True if mirror mode
        @raise error.TestFail if the display mode is not preserved.
        """
        logging.info("Is Mirrored mode enabled?: %s",
                     self.display_facade.is_mirrored_enabled())
        if self.display_facade.is_mirrored_enabled() != test_mirrored_mode:
            raise error.TestFail('Display mode %s is not preserved!' %
                                 ('mirrored' if test_mirrored_mode
                                     else 'extended'))

    def check_external_display(self, test_mirrored):
        """Display status check

        @param test_mirrored: True if mirror mode
        """

        # Check connector
        if self.screen_test.check_external_display_connected(
                self.connector_used, self.errors) is None:

            # Check mode is same as beginning of the test
            self.check_mode(test_mirrored)

            # Check test image
            resolution = self.chameleon_port.get_resolution()
            self.screen_test.test_screen_with_image(
                    resolution, test_mirrored, self.errors)

    def run_test_on_port(self, chameleon_port, test_mirrored):
        """Run the test on the given Chameleon port.

        @param chameleon_port: a ChameleonPorts object.
        @param test_mirrored: True if mirror mode
        @raise error.TestFail if any display errors
        """
        self.chameleon_port = chameleon_port
        self.screen_test = chameleon_screen_test.ChameleonScreenTest(
                self.host, chameleon_port, self.display_facade, self.outputdir)

        # Get connector type used (HDMI,DP,...)
        self.connector_used = self.display_facade.get_external_connector_name()

        # Set main display mode for the test
        logging.info("Setting Mirrored display mode")
        self.display_facade.set_mirrored(test_mirrored)
        self.logout()
        utils.poll_for_condition(
                self.is_logged_out,
                exception=error.TestFail('User is not logged out'),
                timeout=self.LOGOUT_DELAY,
                sleep_interval=1)
        logging.info("Restarting the chrome again!")
        self.browser_facade.start_default_chrome(restart=True)
        logging.info("Checking the external display mode and image!")
        self.check_external_display(test_mirrored)
        if self.errors:
            raise error.TestFail('; '.join(set(self.errors)))

    def run_once(self, host, test_mirrored=True):
        """Checks the mode is preserved after logout

        @param host: DUT object
        @param test_mirrored: True if mirror mode
        """
        self.host = host
        self.errors = list()
        chameleon_board = host.chameleon
        factory = remote_facade_factory.RemoteFacadeFactory(
                self.host, results_dir=self.resultsdir)
        self.display_facade = factory.create_display_facade()
        self.browser_facade = factory.create_browser_facade()
        self.input_facade = factory.create_input_facade()
        self.input_facade.initialize_input_playback("keyboard")
        chameleon_board.setup_and_reset(self.outputdir)
        finder = chameleon_port_finder.ChameleonVideoInputFinder(
                chameleon_board, self.display_facade)

        # Iterates all connected video ports and ensures every of them plugged
        for chameleon_port in finder.iterate_all_ports():
            self.run_test_on_port(chameleon_port, test_mirrored)
