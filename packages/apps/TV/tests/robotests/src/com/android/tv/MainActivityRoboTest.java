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
 * limitations under the License.
 */

package com.android.tv;

import static com.google.common.truth.Truth.assertThat;

import android.media.tv.TvTrackInfo;
import android.os.Bundle;
import android.view.LayoutInflater;

import com.android.tv.common.flags.impl.DefaultLegacyFlags;
import com.android.tv.data.ProgramDataManager;
import com.android.tv.data.StreamInfo;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.ui.TunableTvView;
import com.android.tv.util.TvInputManagerHelper;

import java.util.Arrays;
import java.util.List;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.Arrays;

/** Tests for {@link TunableTvView} */
@RunWith(TvRobolectricTestRunner.class)
@Config(
        sdk = ConfigConstants.SDK,
        application = TestSingletonApp.class,
        shadows = {ShadowTvView.class})
public class MainActivityRoboTest {
    private ShadowTvView mShadowTvView;
    private FakeMainActivity mMainActivity;

    @Before
    public void setUp() {
        mMainActivity = Robolectric.buildActivity(FakeMainActivity.class).create().get();
        mShadowTvView = Shadow.extract(mMainActivity.getTvView().getTvView());
        mShadowTvView.listener = mMainActivity.getListener();
    }

    @Test
    public void testSelectAudioTrack_autoSelect() {
        mShadowTvView.mAudioTrackCountChanged = false;
        setTracks(
                TvTrackInfo.TYPE_AUDIO,
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "EN audio 1", "EN"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 1", "FR"));
        mMainActivity.selectAudioTrack("FR audio 1");
        assertThat(mMainActivity.getSelectedTrack(TvTrackInfo.TYPE_AUDIO)).isEqualTo("FR audio 1");

        setTracks(
                TvTrackInfo.TYPE_AUDIO,
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "EN audio 2", "EN"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 2", "FR"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 3", "FR"));
        mMainActivity.applyMultiAudio(null);
        // FR audio 2 is selected according the previously selected track.
        assertThat(mMainActivity.getSelectedTrack(TvTrackInfo.TYPE_AUDIO)).isEqualTo("FR audio 2");
    }

    @Test
    public void testSelectAudioTrack_audioTrackCountChanged() {
        mShadowTvView.mAudioTrackCountChanged = true;
        setTracks(
                TvTrackInfo.TYPE_AUDIO,
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "EN audio 1", "EN"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 1", "FR"));
        mMainActivity.selectAudioTrack("FR audio 1");
        assertThat(mMainActivity.getSelectedTrack(TvTrackInfo.TYPE_AUDIO)).isEqualTo("FR audio 1");

        setTracks(
                TvTrackInfo.TYPE_AUDIO,
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "EN audio 2", "EN"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 2", "FR"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 3", "FR"));
        mMainActivity.selectAudioTrack("FR audio 3");
        // FR audio 3 is selected even if the track info has been changed
        assertThat(mMainActivity.getSelectedTrack(TvTrackInfo.TYPE_AUDIO)).isEqualTo("FR audio 3");
    }

    @Test
    public void testSelectAudioTrack_audioTrackCountNotChanged() {
        mShadowTvView.mAudioTrackCountChanged = false;
        setTracks(
                TvTrackInfo.TYPE_AUDIO,
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "EN audio 1", "EN"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 1", "FR"));
        mMainActivity.selectAudioTrack("FR audio 1");
        assertThat(mMainActivity.getSelectedTrack(TvTrackInfo.TYPE_AUDIO)).isEqualTo("FR audio 1");

        setTracks(
                TvTrackInfo.TYPE_AUDIO,
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "EN audio 2", "EN"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 2", "FR"),
                buildTrackForTesting(TvTrackInfo.TYPE_AUDIO, "FR audio 3", "FR"));
        mMainActivity.selectAudioTrack("FR audio 3");
        assertThat(mMainActivity.getSelectedTrack(TvTrackInfo.TYPE_AUDIO)).isEqualTo("FR audio 3");
    }

    @Test
    public void testFindBestCaptionTrack_bestTrackMatch() {
        List<TvTrackInfo> tracks = Arrays.asList(
                buildCaptionTrackForTesting("EN subtitle 1", "EN"),
                buildCaptionTrackForTesting("EN subtitle 2", "EN"),
                buildCaptionTrackForTesting("FR subtitle 1", "FR"),
                buildCaptionTrackForTesting("FR subtitle 2", "FR"));
        List<String> preferredLanguages = Arrays.asList("DE", "FR");

        int trackIndex =
                MainActivity.findBestCaptionTrackIndex(
                        tracks, "EN", preferredLanguages, "EN subtitle 2");
        assertThat(trackIndex).isEqualTo(1);
    }

    @Test
    public void testFindBestCaptionTrack_selectedLanguageMatch() {
        List<TvTrackInfo> tracks = Arrays.asList(
                buildCaptionTrackForTesting("EN subtitle 1", "EN"),
                buildCaptionTrackForTesting("EN subtitle 2", "EN"),
                buildCaptionTrackForTesting("FR subtitle 1", "FR"),
                buildCaptionTrackForTesting("FR subtitle 2", "FR"));
        List<String> preferredLanguages = Arrays.asList("DE", "FR");

        int trackIndex =
                MainActivity.findBestCaptionTrackIndex(
                        tracks, "EN", preferredLanguages, "EN subtitle 3");
        assertThat(trackIndex).isEqualTo(0);
    }

    @Test
    public void testFindBestCaptionTrack_preferredLanguageMatch() {
        List<TvTrackInfo> tracks = Arrays.asList(
                buildCaptionTrackForTesting("EN subtitle 1", "EN"),
                buildCaptionTrackForTesting("EN subtitle 2", "EN"),
                buildCaptionTrackForTesting("FR subtitle 1", "FR"),
                buildCaptionTrackForTesting("FR subtitle 2", "FR"));
        List<String> preferredLanguages = Arrays.asList("DE", "FR");

        int trackIndex =
                MainActivity.findBestCaptionTrackIndex(
                        tracks, "JA", preferredLanguages, "JA subtitle 4");
        assertThat(trackIndex).isEqualTo(2);
    }

    @Test
    public void testFindBestCaptionTrack_higherPriorityPreferredLanguageMatch() {
        List<TvTrackInfo> tracks = Arrays.asList(
                buildCaptionTrackForTesting("EN subtitle 1", "EN"),
                buildCaptionTrackForTesting("EN subtitle 2", "EN"),
                buildCaptionTrackForTesting("FR subtitle 1", "FR"),
                buildCaptionTrackForTesting("FR subtitle 2", "FR"));
        List<String> preferredLanguages = Arrays.asList("FR", "EN");

        int trackIndex =
                MainActivity.findBestCaptionTrackIndex(
                        tracks, "JA", preferredLanguages, "JA subtitle 4");
        assertThat(trackIndex).isEqualTo(2);
    }

    @Test
    public void testFindBestCaptionTrack_noMatch() {
        List<TvTrackInfo> tracks = Arrays.asList(
                buildCaptionTrackForTesting("EN subtitle 1", "EN"),
                buildCaptionTrackForTesting("EN subtitle 2", "EN"),
                buildCaptionTrackForTesting("FR subtitle 1", "FR"),
                buildCaptionTrackForTesting("FR subtitle 2", "FR"));
        List<String> preferredLanguages = Arrays.asList("DE", "IT");

        int trackIndex =
                MainActivity.findBestCaptionTrackIndex(
                        tracks, "JA", preferredLanguages, "JA subtitle 4");
        assertThat(trackIndex).isEqualTo(0);
    }

    private void setTracks(int type, TvTrackInfo... tracks) {
        mShadowTvView.mTracks.put(type, Arrays.asList(tracks));
        mShadowTvView.mSelectedTracks.put(type, null);
    }

    private TvTrackInfo buildTrackForTesting(int type, String id, String language) {
        return new TvTrackInfo.Builder(type, id)
                .setLanguage(language)
                .setAudioChannelCount(0)
                .build();
    }

    private TvTrackInfo buildCaptionTrackForTesting(String id, String language) {
        return new TvTrackInfo.Builder(TvTrackInfo.TYPE_SUBTITLE, id)
                .setLanguage(language)
                .build();
    }

    /** A {@link MainActivity} class for tests */
    public static class FakeMainActivity extends MainActivity {
        private MyOnTuneListener mListener;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            // Override onCreate() to omit unnecessary member variables
            mTvView =
                    (TunableTvView)
                            LayoutInflater.from(RuntimeEnvironment.application)
                                    .inflate(R.layout.activity_tv, null)
                                    .findViewById(R.id.main_tunable_tv_view);
            DefaultLegacyFlags legacyFlags = DefaultLegacyFlags.DEFAULT;
            mTvView.initialize(
                    new ProgramDataManager(RuntimeEnvironment.application),
                    new TvInputManagerHelper(RuntimeEnvironment.application, legacyFlags),
                    legacyFlags);
            mTvView.start();
            mListener =
                    new MyOnTuneListener() {
                        @Override
                        public void onStreamInfoChanged(
                                StreamInfo info, boolean allowAutoSelectionOfTrack) {
                            applyMultiAudio(
                                    allowAutoSelectionOfTrack
                                            ? null
                                            : getSelectedTrack(TvTrackInfo.TYPE_AUDIO));
                        }
                    };
            mTvView.setOnTuneListener(mListener);
        }

        public TunableTvView getTvView() {
            return mTvView;
        }

        public MyOnTuneListener getListener() {
            return mListener;
        }
    }
}
