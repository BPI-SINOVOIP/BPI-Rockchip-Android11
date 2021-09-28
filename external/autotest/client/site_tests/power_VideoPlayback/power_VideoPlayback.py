# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os

from autotest_lib.client.common_lib import file_utils
from autotest_lib.client.cros.power import power_videotest


class power_VideoPlayback(power_videotest.power_VideoTest):
    """class for power_VideoPlayback test."""
    version = 1

    # Time in seconds to measure power per video file.
    _MEASUREMENT_DURATION = 120

    _BASE_URL='http://storage.googleapis.com/chromiumos-test-assets-public/tast/cros/video/power/2m/'

    # list of video name and url.
    _VIDEOS = [
        ('h264_720_30fps',
         _BASE_URL + '720p30fpsH264_foodmarket_sync_2m.mp4'
        ),
        ('h264_720_60fps',
         _BASE_URL + '720p60fpsH264_boat_sync_2m.mp4'
        ),
        ('h264_1080_30fps',
         _BASE_URL + '1080p30fpsH264_foodmarket_sync_2m.mp4'
        ),
        ('h264_1080_60fps',
         _BASE_URL + '1080p60fpsH264_boat_sync_2m.mp4'
        ),
        ('h264_4k_30fps',
         _BASE_URL + '4k30fpsH264_foodmarket_sync_vod_2m.mp4'
        ),
        ('h264_4k_60fps',
         _BASE_URL + '4k60fpsH264_boat_sync_vod_2m.mp4'
        ),
        ('vp8_720_30fps',
         _BASE_URL + '720p30fpsVP8_foodmarket_sync_2m.webm'
        ),
        ('vp8_720_60fps',
         _BASE_URL + '720p60fpsVP8_boat_sync_2m.webm'
        ),
        ('vp8_1080_30fps',
         _BASE_URL + '1080p30fpsVP8_foodmarket_sync_2m.webm'
        ),
        ('vp8_1080_60fps',
         _BASE_URL + '1080p60fpsVP8_boat_sync_2m.webm'
        ),
        ('vp8_4k_30fps',
         _BASE_URL + '4k30fpsVP8_foodmarket_sync_2m.webm'
        ),
        ('vp8_4k_60fps',
         _BASE_URL + '4k60fpsVP8_boat_sync_2m.webm'
        ),
        ('vp9_720_30fps',
         _BASE_URL + '720p30fpsVP9_foodmarket_sync_2m.webm'
        ),
        ('vp9_720_60fps',
         _BASE_URL + '720p60fpsVP9_boat_sync_2m.webm'
        ),
        ('vp9_1080_30fps',
         _BASE_URL + '1080p30fpsVP9_foodmarket_sync_2m.webm'
        ),
        ('vp9_1080_60fps',
         _BASE_URL + '1080p60fpsVP9_boat_sync_2m.webm'
        ),
        ('vp9_4k_30fps',
         _BASE_URL + '4k30fpsVP9_foodmarket_sync_2m.webm'
        ),
        ('vp9_4k_60fps',
         _BASE_URL + '4k60fpsVP9_boat_sync_2m.webm'
        ),
        ('av1_720_30fps',
         _BASE_URL + '720p30fpsAV1_foodmarket_sync_2m.mp4'
        ),
        ('av1_720_60fps',
         _BASE_URL + '720p60fpsAV1_boat_sync_2m.mp4'
        ),
        ('av1_1080_30fps',
         _BASE_URL + '1080p30fpsAV1_foodmarket_sync_2m.mp4'
        ),
        ('av1_1080_60fps',
         _BASE_URL + '1080p60fpsAV1_boat_sync_2m.mp4'
        ),
    ]


    _FAST_BASE_URL = 'http://storage.googleapis.com/chromiumos-test-assets-public/tast/cros/video/power/10s/'
    _FAST_VIDEOS = [
        ('h264_720_30fps',
            _FAST_BASE_URL + '720p30fpsH264_foodmarket_sync_10s.mp4'
        ),
        ('vp8_720_30fps',
            _FAST_BASE_URL + '720p30fpsVP8_foodmarket_sync_10s.webm'
        ),
        ('vp9_720_30fps',
            _FAST_BASE_URL + '720p30fpsVP9_foodmarket_sync_10s.webm'
        ),
        ('av1_720_30fps',
            _FAST_BASE_URL + '720p30fpsAV1_foodmarket_sync_10s.mp4'
        ),
    ]

    def _prepare_video(self, cr, url):
        """Prepare browser session before playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        # Download video to ramdisk
        local_path = os.path.join(self._RAMDISK, os.path.basename(url))
        logging.info('Downloading %s to %s', url, local_path)
        file_utils.download_file(url, local_path)
        self.cr.browser.platform.SetHTTPServerDirectories(self._RAMDISK)

    def _start_video(self, cr, url):
        """Start playing video.

        @param cr: Autotest Chrome instance.
        @param local_path: path to the local video file to play.
        """
        local_path = os.path.join(self._RAMDISK, os.path.basename(url))
        tab = cr.browser.tabs[0]
        tab.Navigate(cr.browser.platform.http_server.UrlOf(local_path))
        tab.WaitForDocumentReadyStateToBeComplete()

    def _teardown_video(self, cr, url):
        """Teardown browser session after playing video.

        @param cr: Autotest Chrome instance.
        @param url: url of video file to play.
        """
        self.cr.browser.platform.StopAllLocalServers()
        local_path = os.path.join(self._RAMDISK, os.path.basename(url))
        os.remove(local_path)

    def run_once(self, videos=None, secs_per_video=_MEASUREMENT_DURATION,
                 use_hw_decode=True, fast=False):
        """run_once method.

        @param videos: list of tuple of tagname and video url to test.
        @param secs_per_video: time in seconds to play video and measure power.
        @param use_hw_decode: if False, disable hw video decoding.
        @param fast: Use smaller set of videos when videos is None.
        """
        if not videos:
            videos = self._FAST_VIDEOS if fast else self._VIDEOS

        super(power_VideoPlayback, self).run_once(
            videos, secs_per_video, use_hw_decode)
