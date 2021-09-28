# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.cros.power import power_videotest

import py_utils

class power_VideoDRMPlayback(power_videotest.power_VideoTest):
    """class for power_VideoDRMPlayback test."""
    version = 1

    _BASE_URL='https://ats.sandbox.google.com/videostack/media_test_page.html?file='

    # list of video name and url.
    _VIDEOS = [
        ('h264_720_30fps_cenc',
         _BASE_URL + 'EME_720p30fpsH264_foodmarket_sync_L3_video_clear_audio.mp4.mpd'
        ),
        ('h264_1080_30fps_cenc',
         _BASE_URL + 'EME_1080p30fpsH264_foodmarket_sync_L3_video_clear_audio.mp4.mpd'
        ),
        ('vp9_720_30fps_cenc',
         _BASE_URL + 'EME_720p30fpsVP9_foodmarket_sync_L3_video_clear_audio.webm.mpd'
        ),
        ('vp9_1080_30fps_cenc',
         _BASE_URL + 'EME_1080p30fpsVP9_foodmarket_sync_L3_video_clear_audio.webm.mpd'
        ),
        ('av1_720_30fps_cenc',
         _BASE_URL + 'EME_720p30fpsAV1_foodmarket_sync_L3_video_clear_audio.mp4.mpd'
        ),
        ('av1_1080_30fps_cenc',
         _BASE_URL + 'EME_1080p30fpsAV1_foodmarket_sync_L3_video_clear_audio.mp4.mpd'
        ),
        ('h264_720_30fps_cbcs',
         _BASE_URL + 'EME_720p30fpsH264_foodmarket_sync_cbcs_video_clear_audio.mp4.mpd'
        ),
        ('h264_1080_30fps_cbcs',
         _BASE_URL + 'EME_1080p30fpsH264_foodmarket_sync_cbcs_video_clear_audio.mp4.mpd'
        ),
        ('av1_720_30fps_cbcs',
         _BASE_URL + 'EME_720p30fpsAV1_foodmarket_sync_cbcs_video_clear_audio.mp4.mpd'
        ),
        ('av1_1080_30fps_cbcs',
         _BASE_URL + 'EME_1080p30fpsAV1_foodmarket_sync_cbcs_video_clear_audio.mp4.mpd'
        ),
    ]

    # Time in seconds to measure power per video file.
    _MEASUREMENT_DURATION = 120

    def _prepare_video(self, cr, url):
        """Prepare browser session before playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        tab = cr.browser.tabs[0]
        tab.Navigate(url)
        tab.WaitForDocumentReadyStateToBeComplete()

    def _start_video(self, cr, url):
        """Start playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        tab = cr.browser.tabs[0]

        # Chrome prevents making an element fullscreen if the request doesn't
        # initiated by user gesture. https://CrOSPower.page.link/noFullScreen
        # Fake the user gesture by evaluate javascript from URL bar.
        try:
            tab.Navigate("javascript:TestFrameworkApp.FullScreen()", timeout=0)
            tab.WaitForDocumentReadyStateToBeComplete()
        except py_utils.TimeoutException:
            # tab.Navigate always raise TimeoutException because we used it to
            # execute javascript and didn't navigate to anywhere.
            pass

        tab.EvaluateJavaScript("TestFrameworkApp.getInstance().startTest()")

    def _teardown_video(self, cr, url):
        """Teardown browser session after playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        pass

    def run_once(self, videos=None, secs_per_video=_MEASUREMENT_DURATION,
                 use_hw_decode=True):
        """run_once method.

        @param videos: list of tuple of tagname and video url to test.
        @param secs_per_video: time in seconds to play video and measure power.
        @param use_hw_decode: if False, disable hw video decoding.
        """
        if not videos:
            videos = self._VIDEOS

        super(power_VideoDRMPlayback, self).run_once(
            videos, secs_per_video, use_hw_decode)

