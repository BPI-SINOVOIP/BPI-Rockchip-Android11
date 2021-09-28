# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.graphics import graphics_utils


PLAYBACK_TEST_TIME_S = 10
PLAYER_ENDED_STATE = 'Ended'
PLAYER_PAUSE_STATE = 'Paused'
PLAYER_PLAYING_STATE = 'Playing'
WAIT_TIMEOUT_S = 20


class YouTubeHelper(object):
    """A helper class contains YouTube related utility functions.

       To use this class, please call wait_for_player_state(playing) as below
       before calling set_video_duration. Please note that set_video_duration
       must be called in order to access few functions which uses the video
       length member variable.

       yh = youtube_helper.YouTubeHelper(tab)
       yh.wait_for_player_state(PLAYER_PLAYING_STATE)
       yh.set_video_duration()

    """

    def __init__(self, youtube_tab):
        self._tab = youtube_tab
        self._video_duration = 0


    def set_video_duration(self):
        """Sets the video duration."""
        self._video_duration = (int(self._tab.EvaluateJavaScript(
            'player.getDuration()')))

    def video_current_time(self):
        """Returns video's current playback time.

        Returns:
            returns the current playback location in seconds (int).

        """
        return int(self._tab.EvaluateJavaScript('player.getCurrentTime()'))

    def get_player_status(self):
        """Returns the player status."""
        return self._tab.EvaluateJavaScript(
            '(typeof playerStatus !== \'undefined\') && '
            'playerStatus.innerHTML')

    def get_playback_quality(self):
        """Returns the playback quality."""
        return self._tab.EvaluateJavaScript('player.getPlaybackQuality()')

    def wait_for_player_state(self, expected_status):
        """Wait till the player status changes to expected_status.

        If the status doesn't change for long, the test will time out after
        WAIT_TIMEOUT_S and fails.

        @param expected_status: status which is expected for the test
                                to continue.

        """
        utils.poll_for_condition(
            lambda: self.get_player_status() == expected_status,
            exception=error.TestError(
                'Video failed to load. Player expected status: %s'
                ' and current status: %s.'
                % (expected_status, self.get_player_status())),
            timeout=WAIT_TIMEOUT_S,
            sleep_interval=1)

    def verify_video_playback(self):
        """Verify the video playback."""
        logging.info('Verifying the YouTube video playback.')
        playback = 0  # seconds
        prev_playback = 0
        count = 0
        while (self.video_current_time() < self._video_duration
               and playback < PLAYBACK_TEST_TIME_S):
            time.sleep(1)
            if self.video_current_time() <= prev_playback:
                if count < 2:
                    logging.info('Retrying to video playback test.')
                    self._tab.ExecuteJavaScript(
                        'player.seekTo(%d, true)'
                        % (self.video_current_time() + 2))
                    time.sleep(1)
                    count = count + 1
                else:
                    player_status = self.get_player_status()
                    raise error.TestError(
                        'Video is not playing. Player status: %s.' %
                        player_status)
            prev_playback = self.video_current_time()
            playback = playback + 1

    def verify_player_states(self):
        """Verify the player states like play, pause, ended and seek."""
        logging.info('Verifying the player states.')
        self._tab.ExecuteJavaScript('player.pauseVideo()')
        self.wait_for_player_state(PLAYER_PAUSE_STATE)
        self._tab.ExecuteJavaScript('player.playVideo()')
        self.wait_for_player_state(PLAYER_PLAYING_STATE)
        # We are seeking the player position to (video length - 2 seconds).
        # Since the player waits for WAIT_TIMEOUT_S for the status change,
        # the video should be ended before we hit the timeout.
        video_end_test_duration = (self._video_duration -
                                   self.video_current_time() - 2)
        if video_end_test_duration >= WAIT_TIMEOUT_S:
            self._tab.ExecuteJavaScript(
                'player.seekTo(%d, true)' % (self._video_duration - 5))
            self.wait_for_player_state(PLAYER_ENDED_STATE)
        else:
            raise error.TestError(
                'Test video is not long enough for the video end test.')
        # Verifying seek back from the end position.
        self._tab.ExecuteJavaScript('player.seekTo(%d, true)'
                                    % (self._video_duration / 2))
        self.wait_for_player_state(PLAYER_PLAYING_STATE)
        # So the playback doesn't stay at the mid.
        seek_test = False
        for _ in range(WAIT_TIMEOUT_S):
            logging.info('Waiting for seek position to change.')
            time.sleep(1)
            seek_position = self.video_current_time()
            if (seek_position > self._video_duration / 2
                    and seek_position < self._video_duration):
                seek_test = True
                break
        if not seek_test:
            raise error.TestError(
                'Seek location is wrong. '
                'Video length: %d, seek position: %d.' %
                (self._video_duration, seek_position))
