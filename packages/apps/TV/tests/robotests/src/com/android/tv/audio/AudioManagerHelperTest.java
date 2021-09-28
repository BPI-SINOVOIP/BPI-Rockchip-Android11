/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tv.audio;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.media.AudioManager;
import android.os.Build;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.ui.api.TunableTvViewPlayingApi;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

/** Tests for {@link AudioManagerHelper}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class AudioManagerHelperTest {

    private AudioManagerHelper mAudioManagerHelper;
    private TestTvView mTvView;
    private AudioManager mAudioManager;

    @Before
    public void setup() {
        Activity testActivity = Robolectric.buildActivity(Activity.class).get();
        mTvView = new TestTvView();
        mAudioManager = RuntimeEnvironment.application.getSystemService(AudioManager.class);

        mAudioManagerHelper = new AudioManagerHelper(testActivity, mTvView);
    }

    @Test
    public void onAudioFocusChange_none_noTimeShift() {
        mTvView.mTimeShiftAvailable = false;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_NONE);

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isZero();
    }

    @Test
    public void onAudioFocusChange_none_TimeShift() {
        mTvView.mTimeShiftAvailable = true;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_NONE);

        assertThat(mTvView.mPaused).isTrue();
        assertThat(mTvView.mVolume).isNull();
    }

    @Test
    public void onAudioFocusChange_gain_noTimeShift() {
        mTvView.mTimeShiftAvailable = false;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isEqualTo(1.0f);
    }

    @Test
    public void onAudioFocusChange_gain_timeShift() {
        mTvView.mTimeShiftAvailable = true;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);

        assertThat(mTvView.mPaused).isFalse();
        assertThat(mTvView.mVolume).isNull();
    }

    @Test
    public void onAudioFocusChange_loss_noTimeShift() {
        mTvView.mTimeShiftAvailable = false;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isEqualTo(0.0f);
    }

    @Test
    public void onAudioFocusChange_loss_timeShift() {
        mTvView.mTimeShiftAvailable = true;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);

        assertThat(mTvView.mPaused).isTrue();
        assertThat(mTvView.mVolume).isNull();
    }

    @Test
    public void onAudioFocusChange_lossTransient_noTimeShift() {
        mTvView.mTimeShiftAvailable = false;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isEqualTo(0.0f);
    }

    @Test
    public void onAudioFocusChange_lossTransient_timeShift() {
        mTvView.mTimeShiftAvailable = true;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);

        assertThat(mTvView.mPaused).isTrue();
        assertThat(mTvView.mVolume).isNull();
    }

    @Test
    public void onAudioFocusChange_lossTransientCanDuck_noTimeShift() {
        mTvView.mTimeShiftAvailable = false;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isEqualTo(0.3f);
    }

    @Test
    public void onAudioFocusChange_lossTransientCanDuck_timeShift() {
        mTvView.mTimeShiftAvailable = true;

        mAudioManagerHelper.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        assertThat(mTvView.mPaused).isTrue();
        assertThat(mTvView.mVolume).isNull();
    }

    @Test
    @Config(sdk = {ConfigConstants.SDK, Build.VERSION_CODES.O})
    public void requestAudioFocus_granted() {
        Shadows.shadowOf(mAudioManager)
                .setNextFocusRequestResponse(AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
        mAudioManagerHelper.requestAudioFocus();

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isEqualTo(1.0f);
    }

    @Test
    @Config(sdk = {ConfigConstants.SDK, Build.VERSION_CODES.O})
    public void requestAudioFocus_failed() {
        Shadows.shadowOf(mAudioManager)
                .setNextFocusRequestResponse(AudioManager.AUDIOFOCUS_REQUEST_FAILED);

        mAudioManagerHelper.requestAudioFocus();

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isZero();
    }

    @Test
    @Config(sdk = {ConfigConstants.SDK, Build.VERSION_CODES.O})
    public void requestAudioFocus_delayed() {
        Shadows.shadowOf(mAudioManager)
                .setNextFocusRequestResponse(AudioManager.AUDIOFOCUS_REQUEST_DELAYED);

        mAudioManagerHelper.requestAudioFocus();

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isZero();
    }

    @Test
    public void setVolumeByAudioFocusStatus_started() {
        mAudioManagerHelper.setVolumeByAudioFocusStatus();

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isZero();
    }

    @Test
    public void setVolumeByAudioFocusStatus_notStarted() {
        mTvView.mStarted = false;
        mAudioManagerHelper.setVolumeByAudioFocusStatus();

        assertThat(mTvView.mPaused).isNull();
        assertThat(mTvView.mVolume).isNull();
    }

    private static class TestTvView implements TunableTvViewPlayingApi {
        private boolean mStarted = true;
        private boolean mTimeShiftAvailable = false;
        private Float mVolume = null;
        private Boolean mPaused = null;

        @Override
        public boolean isPlaying() {
            return mStarted;
        }

        @Override
        public void setStreamVolume(float volume) {
            mVolume = volume;
        }

        @Override
        public void setTimeShiftListener(TimeShiftListener listener) {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isTimeShiftAvailable() {
            return mTimeShiftAvailable;
        }

        @Override
        public void timeShiftPlay() {
            mPaused = false;
        }

        @Override
        public void timeShiftPause() {
            mPaused = true;
        }

        @Override
        public void timeShiftRewind(int speed) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void timeShiftFastForward(int speed) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void timeShiftSeekTo(long timeMs) {
            throw new UnsupportedOperationException();
        }

        @Override
        public long timeShiftGetCurrentPositionMs() {
            throw new UnsupportedOperationException();
        }
    }
}
