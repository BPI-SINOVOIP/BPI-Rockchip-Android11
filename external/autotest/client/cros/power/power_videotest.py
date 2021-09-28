# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import abc
import logging
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.audio import audio_helper
from autotest_lib.client.cros.input_playback import keyboard
from autotest_lib.client.cros.power import power_test


class power_VideoTest(power_test.power_Test):
    """Optional base class for power related video tests."""
    version = 1

    # Ram disk location to download video file.
    # We use ram disk to avoid power hit from network / disk usage.
    _RAMDISK = '/tmp/ramdisk'

    # Time in seconds to wait after set up before starting each video.
    _WAIT_FOR_IDLE = 15

    # Time in seconds to measure power per video file.
    _MEASUREMENT_DURATION = 120

    # Chrome arguments to disable HW video decode
    _DISABLE_HW_VIDEO_DECODE_ARGS = '--disable-accelerated-video-decode'


    def initialize(self, seconds_period=3, pdash_note='',
                   force_discharge=False):
        """Create and mount ram disk to download video."""
        super(power_VideoTest, self).initialize(
                seconds_period=seconds_period, pdash_note=pdash_note,
                force_discharge=force_discharge)
        utils.run('mkdir -p %s' % self._RAMDISK)
        # Don't throw an exception on errors.
        result = utils.run('mount -t ramfs -o context=u:object_r:tmpfs:s0 '
                           'ramfs %s' % self._RAMDISK, ignore_status=True)
        if result.exit_status:
            logging.info('cannot mount ramfs with context=u:object_r:tmpfs:s0,'
                         ' trying plain mount')
            # Try again without selinux options.  This time fail on error.
            utils.run('mount -t ramfs ramfs %s' % self._RAMDISK)
        audio_helper.set_volume_levels(10, 10)

    @abc.abstractmethod
    def _prepare_video(self, cr, url):
        """Prepare browser session before playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        raise NotImplementedError()

    @abc.abstractmethod
    def _start_video(self, cr, url):
        """Open the video and play it.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        raise NotImplementedError()

    @abc.abstractmethod
    def _teardown_video(self, cr, url):
        """Teardown browser session after playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        raise NotImplementedError()

    def _calculate_dropped_frame_percent(self, tab):
        """Calculate percent of dropped frame.

        @param tab: tab object that played video in Autotest Chrome instance.
        """
        decoded_frame_count = tab.EvaluateJavaScript(
                "document.getElementsByTagName"
                "('video')[0].webkitDecodedFrameCount")
        dropped_frame_count = tab.EvaluateJavaScript(
                "document.getElementsByTagName"
                "('video')[0].webkitDroppedFrameCount")
        if decoded_frame_count != 0:
            dropped_frame_percent = \
                    100.0 * dropped_frame_count / decoded_frame_count
        else:
            logging.error("No frame is decoded. Set drop percent to 100.")
            dropped_frame_percent = 100.0

        logging.info("Decoded frames=%d, dropped frames=%d, percent=%f",
                decoded_frame_count, dropped_frame_count, dropped_frame_percent)
        return dropped_frame_percent

    def run_once(self, videos=None, secs_per_video=_MEASUREMENT_DURATION,
                 use_hw_decode=True):
        """run_once method.

        @param videos: list of tuple of tagname and video url to test.
        @param secs_per_video: time in seconds to play video and measure power.
        @param use_hw_decode: if False, disable hw video decoding.
        """
        extra_browser_args = []
        if not use_hw_decode:
            extra_browser_args.append(self._DISABLE_HW_VIDEO_DECODE_ARGS)

        with chrome.Chrome(extra_browser_args=extra_browser_args,
                           init_network_controller=True) as self.cr:
            tab = self.cr.browser.tabs.New()
            tab.Activate()

            # Just measure power in full-screen.
            fullscreen = tab.EvaluateJavaScript('document.webkitIsFullScreen')
            if not fullscreen:
                with keyboard.Keyboard() as keys:
                    keys.press_key('f4')

            self.start_measurements()
            idle_start = time.time()

            for name, url in videos:
                self._prepare_video(self.cr, url)
                time.sleep(self._WAIT_FOR_IDLE)

                logging.info('Playing video: %s', name)
                self._start_video(self.cr, url)
                self.checkpoint_measurements('idle', idle_start)

                loop_start = time.time()
                time.sleep(secs_per_video)
                self.checkpoint_measurements(name, loop_start)

                idle_start = time.time()
                self.keyvals[name + '_dropped_frame_percent'] = \
                        self._calculate_dropped_frame_percent(tab)
                self._teardown_video(self.cr, url)

    def cleanup(self):
        """Unmount ram disk."""
        utils.run('umount %s' % self._RAMDISK)
        super(power_VideoTest, self).cleanup()
