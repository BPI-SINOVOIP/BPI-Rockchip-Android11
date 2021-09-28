# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test multiple WebGL windows spread across internal and external displays."""

import collections
import logging
import os
import tarfile
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import constants
from autotest_lib.client.cros.chameleon import chameleon_port_finder
from autotest_lib.client.cros.chameleon import chameleon_screen_test
from autotest_lib.server import test
from autotest_lib.server import utils
from autotest_lib.server.cros.multimedia import remote_facade_factory


class graphics_MultipleDisplays(test.test):
    """Loads multiple WebGL windows on internal and external displays.

    This test first initializes the extended Chameleon display. It then
    launches four WebGL windows, two on each display.
    """
    version = 1
    WAIT_AFTER_SWITCH = 5
    FPS_MEASUREMENT_DURATION = 15
    STUCK_FPS_THRESHOLD = 2
    MAXIMUM_STUCK_MEASUREMENTS = 5

    # Running the HTTP server requires starting Chrome with
    # init_network_controller set to True.
    CHROME_KWARGS = {'extension_paths': [constants.AUDIO_TEST_EXTENSION,
                                          constants.DISPLAY_TEST_EXTENSION],
                     'autotest_ext': True,
                     'init_network_controller': True}

    # Local WebGL tarballs to populate the webroot.
    STATIC_CONTENT = ['webgl_aquarium_static.tar.bz2',
                      'webgl_blob_static.tar.bz2']
    # Client directory for the root of the HTTP server
    CLIENT_TEST_ROOT = \
        '/usr/local/autotest/tests/graphics_MultipleDisplays/webroot'
    # Paths to later convert to URLs
    WEBGL_AQUARIUM_PATH = \
        CLIENT_TEST_ROOT + '/webgl_aquarium_static/aquarium.html'
    WEBGL_BLOB_PATH = CLIENT_TEST_ROOT + '/webgl_blob_static/blob.html'

    MEDIA_CONTENT_BASE = ('https://commondatastorage.googleapis.com'
                          '/chromiumos-test-assets-public')
    H264_URL = MEDIA_CONTENT_BASE + '/Shaka-Dash/1080_60.mp4'
    VP9_URL = MEDIA_CONTENT_BASE + '/Shaka-Dash/1080_60.webm'

    # Simple configuration to capture window position, content URL, or local
    # path. Positioning is either internal or external and left or right half
    # of the display. As an example, to open the newtab page on the left
    # half: WindowConfig(True, True, 'chrome://newtab', None).
    WindowConfig = collections.namedtuple(
        'WindowConfig', 'internal_display, snap_left, url, path')

    WINDOW_CONFIGS = \
        {'aquarium+blob': [WindowConfig(True, True, None, WEBGL_AQUARIUM_PATH),
                           WindowConfig(True, False, None, WEBGL_BLOB_PATH),
                           WindowConfig(False, True, None, WEBGL_AQUARIUM_PATH),
                           WindowConfig(False, False, None, WEBGL_BLOB_PATH)],
         'aquarium+vp9+blob+h264':
              [WindowConfig(True, True, None, WEBGL_AQUARIUM_PATH),
               WindowConfig(True, False, VP9_URL, None),
               WindowConfig(False, True, None, WEBGL_BLOB_PATH),
               WindowConfig(False, False, H264_URL, None)]}


    def _prepare_test_assets(self):
        """Create a local test bundle and send it to the client.

        @raise ValueError if the HTTP server does not start.
        """
        # Create a directory to unpack archives.
        temp_bundle_dir = utils.get_tmp_dir()

        for static_content in self.STATIC_CONTENT:
            archive_path = os.path.join(self.bindir, 'files', static_content)

            with tarfile.open(archive_path, 'r') as tar:
                tar.extractall(temp_bundle_dir)

        # Send bundle to client. The extra slash is to send directory contents.
        self._host.run('mkdir -p {}'.format(self.CLIENT_TEST_ROOT))
        self._host.send_file(temp_bundle_dir + '/', self.CLIENT_TEST_ROOT,
                             delete_dest=True)

        # Start the HTTP server
        res = self._browser_facade.set_http_server_directories(
            self.CLIENT_TEST_ROOT)
        if not res:
            raise ValueError('HTTP server failed to start.')

    def _calculate_new_bounds(self, config):
        """Calculates bounds for 'snapping' to the left or right of a display.

        @param config: WindowConfig specifying which display and side.

        @return Dictionary with keys top, left, width, and height for the new
                window boundaries.
        """
        new_bounds = {'top': 0, 'left': 0, 'width': 0, 'height': 0}
        display_info = filter(
            lambda d: d.is_internal == config.internal_display,
            self._display_facade.get_display_info())
        display_info = display_info[0]

        # Since we are "snapping" windows left and right, set the width to half
        # and set the height to the full working area.
        new_bounds['width'] = int(display_info.work_area.width / 2)
        new_bounds['height'] = display_info.work_area.height

        # To specify the left or right "snap", first set the left edge to the
        # display boundary. Note that for the internal display this will be 0.
        # For the external display it will already include the offset from the
        # internal display. Finally, if we are positioning to the right half
        # of the display also add in the width.
        new_bounds['left'] = display_info.bounds.left
        if not config.snap_left:
            new_bounds['left'] = new_bounds['left'] + new_bounds['width']

        return new_bounds

    def _measure_external_display_fps(self, chameleon_port):
        """Measure the update rate of the external display.

        @param chameleon_port: ChameleonPort object for recording.

        @raise ValueError if Chameleon FPS measurements indicate the external
               display was not changing.
        """
        chameleon_port.start_capturing_video()
        time.sleep(self.FPS_MEASUREMENT_DURATION)
        chameleon_port.stop_capturing_video()

        # FPS information for saving later
        self._fps_list = chameleon_port.get_captured_fps_list()

        stuck_fps_list = filter(lambda fps: fps < self.STUCK_FPS_THRESHOLD,
                                self._fps_list)
        if len(stuck_fps_list) > self.MAXIMUM_STUCK_MEASUREMENTS:
            msg = 'Too many measurements {} are < {} FPS. GPU hang?'.format(
                self._fps_list, self.STUCK_FPS_THRESHOLD)
            raise ValueError(msg)

    def _setup_windows(self):
        """Create windows and update their positions.

        @raise ValueError if the selected subtest is not a valid configuration.
        @raise ValueError if a window configurations is invalid.
        """

        if self._subtest not in self.WINDOW_CONFIGS:
            msg = '{} is not a valid subtest. Choices are {}.'.format(
                self._subtest, self.WINDOW_CONFIGS.keys())
            raise ValueError(msg)

        for window_config in self.WINDOW_CONFIGS[self._subtest]:
            url = window_config.url
            if not url:
                if not window_config.path:
                    msg = 'Path & URL not configured. {}'.format(window_config)
                    raise ValueError(msg)

                # Convert the locally served content path to a URL.
                url =  self._browser_facade.http_server_url_of(
                    window_config.path)

            new_bounds = self._calculate_new_bounds(window_config)
            new_id = self._display_facade.create_window(url)
            self._display_facade.update_window(new_id, 'normal', new_bounds)
            time.sleep(self.WAIT_AFTER_SWITCH)

    def run_once(self, host, subtest, test_duration=60):
        self._host = host
        self._subtest = subtest

        factory = remote_facade_factory.RemoteFacadeFactory(host)
        self._browser_facade = factory.create_browser_facade()
        self._browser_facade.start_custom_chrome(self.CHROME_KWARGS)
        self._display_facade = factory.create_display_facade()
        self._graphics_facade = factory.create_graphics_facade()

        logging.info('Preparing local WebGL test assets.')
        self._prepare_test_assets()

        chameleon_board = host.chameleon
        chameleon_board.setup_and_reset(self.outputdir)
        finder = chameleon_port_finder.ChameleonVideoInputFinder(
                chameleon_board, self._display_facade)

        # Snapshot the DUT system logs for any prior GPU hangs
        self._graphics_facade.graphics_state_checker_initialize()

        for chameleon_port in finder.iterate_all_ports():
            logging.info('Setting Chameleon screen to extended mode.')
            self._display_facade.set_mirrored(False)
            time.sleep(self.WAIT_AFTER_SWITCH)

            logging.info('Launching WebGL windows.')
            self._setup_windows()

            logging.info('Measuring the external display update rate.')
            self._measure_external_display_fps(chameleon_port)

            logging.info('Running test for {}s.'.format(test_duration))
            time.sleep(test_duration)

            # Raise an error on new GPU hangs
            self._graphics_facade.graphics_state_checker_finalize()

    def postprocess_iteration(self):
        desc = 'Display update rate {}'.format(self._subtest)
        self.output_perf_value(description=desc, value=self._fps_list,
                               units='FPS', higher_is_better=True, graph=None)
