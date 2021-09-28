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

package com.android.tv.dvr.ui.playback;

import static com.google.common.truth.Truth.assertThat;

import android.media.tv.TvTrackInfo;

import com.android.tv.ShadowTvView;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.ui.AppLayerTvView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.ArrayList;
import java.util.Collections;

/** Test for {@link DvrPlayer}. */
@RunWith(RobolectricTestRunner.class)
@Config(
        sdk = ConfigConstants.SDK,
        application = TestSingletonApp.class,
        shadows = {ShadowTvView.class})
public class DvrPlayerTest {
    private ShadowTvView mShadowTvView;
    private DvrPlayer mDvrPlayer;

    @Before
    public void setUp() {
        AppLayerTvView tvView = new AppLayerTvView(RuntimeEnvironment.application);
        mShadowTvView = Shadow.extract(tvView);
        mDvrPlayer = new DvrPlayer(tvView, RuntimeEnvironment.application);
    }

    @Test
    public void testGetAudioTracks_null() {
        mShadowTvView.mTracks.put(TvTrackInfo.TYPE_AUDIO, null);
        assertThat(mDvrPlayer.getAudioTracks()).isNotNull();
        assertThat(mDvrPlayer.getAudioTracks()).isEmpty();
    }

    @Test
    public void testGetAudioTracks_empty() {
        mShadowTvView.mTracks.put(TvTrackInfo.TYPE_AUDIO, new ArrayList<>());
        assertThat(mDvrPlayer.getAudioTracks()).isNotNull();
        assertThat(mDvrPlayer.getAudioTracks()).isEmpty();
    }

    @Test
    public void testGetAudioTracks_nonEmpty() {
        TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, "fake id").build();
        mShadowTvView.mTracks.put(TvTrackInfo.TYPE_AUDIO, Collections.singletonList(info));
        assertThat(mDvrPlayer.getAudioTracks()).containsExactly(info);
    }
}
