/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package com.android.tv;

import static com.google.common.truth.Truth.assertThat;

import android.app.PendingIntent;
import android.content.Intent;
import android.media.MediaMetadata;
import android.media.session.PlaybackState;

import com.android.tv.testing.EpgTestData;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.shadows.ShadowMediaSession;

import com.google.common.collect.Maps;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.Map;

/** Tests fpr {@link MediaSessionWrapper}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(
        sdk = ConfigConstants.SDK,
        application = TestSingletonApp.class,
        shadows = {ShadowMediaSession.class})
public class MediaSessionWrapperTest {

    private static final int TEST_REQUEST_CODE = 1337;

    private ShadowMediaSession mediaSession;
    private MediaSessionWrapper mediaSessionWrapper;
    private PendingIntent pendingIntent;

    @Before
    public void setUp() {
        pendingIntent =
                PendingIntent.getActivity(
                        RuntimeEnvironment.application, TEST_REQUEST_CODE, new Intent(), 0);
        mediaSessionWrapper =
                new MediaSessionWrapper(RuntimeEnvironment.application, pendingIntent) {
                    @Override
                    void initMediaController() {
                        // Not use MediaController for tests here because:
                        // 1. it's not allow to shadow MediaController
                        // 2. The Context TestSingletonApp is not an instance of Activity so
                        // Activity.setMediaController cannot be called.
                        // onPlaybackStateChanged() is called in #setPlaybackState instead.
                    }

                    @Override
                    void unregisterMediaControllerCallback() {}
                };
        mediaSession = Shadow.extract(mediaSessionWrapper.getMediaSession());
    }

    @Test
    public void setSessionActivity() {
        assertThat(mediaSession.mSessionActivity).isEqualTo(this.pendingIntent);
    }

    @Test
    public void setPlaybackState_true() {
        setPlaybackState(true);
        assertThat(mediaSession.mActive).isTrue();
        assertThat(mediaSession.mPlaybackState.getState()).isEqualTo(PlaybackState.STATE_PLAYING);
    }

    @Test
    public void setPlaybackState_false() {
        setPlaybackState(false);
        assertThat(mediaSession.mActive).isFalse();
        assertThat(mediaSession.mPlaybackState).isNull();
    }

    @Test
    public void setPlaybackState_trueThenFalse() {
        setPlaybackState(true);
        setPlaybackState(false);
        assertThat(mediaSession.mActive).isFalse();
        assertThat(mediaSession.mPlaybackState.getState()).isEqualTo(PlaybackState.STATE_STOPPED);
    }

    @Test
    public void update_channel10() {

        mediaSessionWrapper.update(false, EpgTestData.toTvChannel(EpgTestData.CHANNEL_10), null);
        assertThat(asMap(mediaSession.mMediaMetadata))
                .containsExactly(MediaMetadata.METADATA_KEY_TITLE, "Channel TEN");
    }

    @Test
    public void update_blockedChannel10() {
        mediaSessionWrapper.update(true, EpgTestData.toTvChannel(EpgTestData.CHANNEL_10), null);
        assertThat(asMap(mediaSession.mMediaMetadata))
                .containsExactly(
                        MediaMetadata.METADATA_KEY_TITLE,
                        "Channel blocked",
                        MediaMetadata.METADATA_KEY_ART,
                        null);
    }

    @Test
    public void update_channel10Program2() {
        mediaSessionWrapper.update(
                false, EpgTestData.toTvChannel(EpgTestData.CHANNEL_10), EpgTestData.PROGRAM_2);
        assertThat(asMap(mediaSession.mMediaMetadata))
                .containsExactly(MediaMetadata.METADATA_KEY_TITLE, "Program 2");
    }

    @Test
    public void update_blockedChannel10Program2() {
        mediaSessionWrapper.update(
                true, EpgTestData.toTvChannel(EpgTestData.CHANNEL_10), EpgTestData.PROGRAM_2);
        assertThat(asMap(mediaSession.mMediaMetadata))
                .containsExactly(
                        MediaMetadata.METADATA_KEY_TITLE,
                        "Channel blocked",
                        MediaMetadata.METADATA_KEY_ART,
                        null);
        // TODO(b/70559407): test async loading of images.
    }

    @Test
    public void release() {
        mediaSessionWrapper.release();
        assertThat(mediaSession.mReleased).isTrue();
    }

    private Map<String, Object> asMap(MediaMetadata mediaMetadata) {
        return Maps.asMap(mediaMetadata.keySet(), key -> mediaMetadata.getString(key));
    }

    private void setPlaybackState(boolean isPlaying) {
        mediaSessionWrapper.setPlaybackState(isPlaying);
        mediaSessionWrapper
                .getMediaControllerCallback()
                .onPlaybackStateChanged(
                        isPlaying
                                ? MediaSessionWrapper.MEDIA_SESSION_STATE_PLAYING
                                : MediaSessionWrapper.MEDIA_SESSION_STATE_STOPPED);
    }
}
