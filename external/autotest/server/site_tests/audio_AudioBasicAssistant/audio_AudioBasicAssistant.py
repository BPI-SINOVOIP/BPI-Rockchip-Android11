# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import time

from autotest_lib.server.cros.audio import audio_test
from autotest_lib.client.cros.audio import audio_test_data
from autotest_lib.client.cros.chameleon import chameleon_audio_helper
from autotest_lib.client.cros.chameleon import chameleon_audio_ids
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.server import test
from autotest_lib.server.cros.multimedia import remote_facade_factory

class audio_AudioBasicAssistant(audio_test.AudioTest):
    """A basic assistant voice command test."""
    version = 1
    DELAY_AFTER_BINDING_SECS = 0.5
    DELAY_AFTER_COMMAND_SECS = 3
    GOOGLE_URL = 'google.com'

    def run_once(self, host, enable_dsp_hotword=False):
        """Run Basic Audio Assistant Test.

        @param host: Device under test CrosHost
        @param enable_dsp_hotword: A bool to control the usage of dsp for
                hotword.
        """
        # TODO(paulhsia): Use gtts to generate command in runtime
        hotword_file = audio_test_data.HOTWORD_OPEN_TAB_TEST_FILE
        chameleon_board = host.chameleon
        factory = remote_facade_factory.RemoteFacadeFactory(
                host, results_dir=self.resultsdir)

        # Prepares chameleon speaker resource
        chameleon_board.setup_and_reset(self.outputdir)
        widget_factory = chameleon_audio_helper.AudioWidgetFactory(
                factory, host)
        source = widget_factory.create_widget(
                chameleon_audio_ids.ChameleonIds.LINEOUT)
        sink = widget_factory.create_widget(
                chameleon_audio_ids.PeripheralIds.SPEAKER)
        binder = widget_factory.create_binder(source, sink)

        # Restarts Chrome with assistant enabled and enables hotword
        assistant_facade = factory.create_assistant_facade()
        assistant_facade.restart_chrome_for_assistant(enable_dsp_hotword)
        assistant_facade.enable_hotword()

        browser_facade = factory.create_browser_facade()

        # Tests voice command with chameleon
        with chameleon_audio_helper.bind_widgets(binder):
            time.sleep(self.DELAY_AFTER_BINDING_SECS)
            logging.info('Setting hotword playback data on Chameleon')
            remote_hotword_file_path = source.set_playback_data(hotword_file)
            source.start_playback_with_path(remote_hotword_file_path)
            time.sleep(hotword_file.duration_secs)
        time.sleep(self.DELAY_AFTER_COMMAND_SECS)

        # Checks urls in open tabs
        urls = browser_facade.get_tab_urls()
        if len(urls) != 2:
            raise error.TestFail('There should be an empty tab and a tab with '
                     'Google.')
        # Checks if google.com exists in tabs
        for url in urls:
            logging.debug(url)
            if self.GOOGLE_URL in url.lower():
                break
        else:
            raise error.TestFail('There is no google.com opened in tabs.')
