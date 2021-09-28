# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a server side hotwording test using the Chameleon board."""

import logging
import os
import time
import threading

from autotest_lib.client.cros.audio import audio_test_data
from autotest_lib.client.cros.chameleon import audio_test_utils
from autotest_lib.client.cros.chameleon import chameleon_audio_helper
from autotest_lib.client.cros.chameleon import chameleon_audio_ids
from autotest_lib.server.cros.audio import audio_test
from autotest_lib.server.cros.multimedia import remote_facade_factory


class audio_AudioBasicHotwording(audio_test.AudioTest):
    """Server side hotwording test.

    This test talks to a Chameleon board and a Cros device to verify
    hotwording function of the Cros device.

    """
    version = 1
    DELAY_AFTER_BINDING_SECS = 0.5
    DELAY_AFTER_START_LISTENING_SECS = 1
    DELAY_AFTER_SUSPEND_SECS = 5

    RECORD_SECONDS = 5
    SUSPEND_SECONDS = 20
    RESUME_TIMEOUT_SECS = 60

    def run_once(self, host, suspend=False):
        """Runs Basic Audio Hotwording test.

        @param host: device under test CrosHost

        @param suspend: True for suspend the device before playing hotword.
                        False for hotwording test suspend.

        """
        if (not audio_test_utils.has_hotwording(host)):
            return

        hotword_file = audio_test_data.HOTWORD_TEST_FILE
        golden_file = audio_test_data.SIMPLE_FREQUENCY_TEST_1330_FILE

        chameleon_board = host.chameleon
        factory = remote_facade_factory.RemoteFacadeFactory(
                host, results_dir=self.resultsdir)

        chameleon_board.setup_and_reset(self.outputdir)

        widget_factory = chameleon_audio_helper.AudioWidgetFactory(
                factory, host)

        source = widget_factory.create_widget(
            chameleon_audio_ids.ChameleonIds.LINEOUT)
        sink = widget_factory.create_widget(
            chameleon_audio_ids.PeripheralIds.SPEAKER)
        binder = widget_factory.create_binder(source, sink)

        listener = widget_factory.create_widget(
            chameleon_audio_ids.CrosIds.HOTWORDING)

        with chameleon_audio_helper.bind_widgets(binder):
            time.sleep(self.DELAY_AFTER_BINDING_SECS)
            audio_facade = factory.create_audio_facade()

            audio_test_utils.dump_cros_audio_logs(
                    host, audio_facade, self.resultsdir, 'after_binding')

            logging.info('Start listening from Cros device.')
            listener.start_listening()
            time.sleep(self.DELAY_AFTER_START_LISTENING_SECS)

            audio_test_utils.dump_cros_audio_logs(
                    host, audio_facade, self.resultsdir,
                    'after_start_listening')

            if suspend:
                def suspend_host():
                    logging.info('Suspend the DUT for %d secs',
                                 self.SUSPEND_SECONDS)
                    host.suspend(suspend_time=self.SUSPEND_SECONDS,
                                 allow_early_resume=True)

                # Folk a thread to suspend the host
                boot_id = host.get_boot_id()
                thread = threading.Thread(target=suspend_host)
                thread.start()
                suspend_start_time = time.time()
                time.sleep(self.DELAY_AFTER_SUSPEND_SECS)

            # Starts playing hotword and golden file.
            # Sleep for 5s for DUT to detect and record the sounds
            logging.info('Setting hotword playback data on Chameleon')
            remote_hotword_file_path = source.set_playback_data(hotword_file)

            logging.info('Setting golden playback data on Chameleon')
            remote_golden_file_path = source.set_playback_data(golden_file)

            logging.info('Start playing %s from Chameleon',
                         hotword_file.path)
            source.start_playback_with_path(remote_hotword_file_path)
            time.sleep(hotword_file.duration_secs)

            logging.info('Start playing %s from Chameleon',
                         golden_file.path)
            source.start_playback_with_path(remote_golden_file_path)

            time.sleep(self.RECORD_SECONDS)

            # If the DUT suspended, the server will reconnect to DUT
            if suspend:
                host.test_wait_for_resume(boot_id, self.RESUME_TIMEOUT_SECS)
                real_suspend_time = time.time() - suspend_start_time
                logging.info('Suspend for %f time.', real_suspend_time)

                # Check the if real suspend time is less than SUSPEND_SECOND
                if real_suspend_time < self.SUSPEND_SECONDS:
                    logging.info('Real suspend time is less than '
                                 'SUSPEND_SECONDS. Hotwording succeeded.')
                else:
                    logging.error('Real suspend time is larger than or equal to'
                                  'SUSPEND_SECONDS. Hostwording failed.')

            listener.stop_listening()
            logging.info('Stopped listening from Cros device.')

            audio_test_utils.dump_cros_audio_logs(
                    host, audio_facade, self.resultsdir, 'after_listening')

            listener.read_recorded_binary()
            logging.info('Read recorded binary from Cros device.')

        recorded_file = os.path.join(self.resultsdir, "recorded.raw")
        logging.info('Saving recorded data to %s', recorded_file)
        listener.save_file(recorded_file)

        # Removes the first 5s of recorded data, which included hotword and
        # the data before hotword.
        listener.remove_head(5.0)
        short_recorded_file = os.path.join(self.resultsdir,
                                           "short_recorded.raw")
        listener.save_file(short_recorded_file)

        # Cros device only records one channel data. Here we set the channel map
        # of the recorder to compare the recorded data with left channel of the
        # test data. This is fine as left and right channel of test data are
        # identical.
        listener.channel_map = [0]

        # Compares data by frequency. Audio signal from Chameleon Line-Out to
        # speaker and finally recorded on Cros device using hotwording chip
        # has gone through analog processing and through the air.
        # This suffers from codec artifacts and noise on the path.
        # Comparing data by frequency is more robust than comparing them by
        # correlation, which is suitable for fully-digital audio path like USB
        # and HDMI.
        audio_test_utils.check_recorded_frequency(golden_file, listener,
                                                  second_peak_ratio=0.2)
