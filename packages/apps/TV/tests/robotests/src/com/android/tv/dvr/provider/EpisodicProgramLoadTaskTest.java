/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.dvr.provider;

import static com.google.common.truth.Truth.assertThat;

import android.os.Build.VERSION_CODES;

import com.android.tv.dvr.data.SeasonEpisodeNumber;
import com.android.tv.testing.TestSingletonApp;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link EpisodicProgramLoadTask} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.N, application = TestSingletonApp.class)
public class EpisodicProgramLoadTaskTest {
    private static final long SERIES_RECORDING_ID1 = 1;
    private static final long SERIES_RECORDING_ID2 = 2;
    private static final String SEASON_NUMBER1 = "SEASON NUMBER1";
    private static final String SEASON_NUMBER2 = "SEASON NUMBER2";
    private static final String EPISODE_NUMBER1 = "EPISODE NUMBER1";
    private static final String EPISODE_NUMBER2 = "EPISODE NUMBER2";

    @Test
    public void testEpisodeAlreadyScheduled_true() {
        List<SeasonEpisodeNumber> seasonEpisodeNumbers = new ArrayList<>();
        SeasonEpisodeNumber seasonEpisodeNumber =
                new SeasonEpisodeNumber(SERIES_RECORDING_ID1, SEASON_NUMBER1, EPISODE_NUMBER1);
        seasonEpisodeNumbers.add(seasonEpisodeNumber);
        assertThat(seasonEpisodeNumbers)
                .contains(
                        new SeasonEpisodeNumber(
                                SERIES_RECORDING_ID1, SEASON_NUMBER1, EPISODE_NUMBER1));
    }

    @Test
    public void testEpisodeAlreadyScheduled_false() {
        List<SeasonEpisodeNumber> seasonEpisodeNumbers = new ArrayList<>();
        SeasonEpisodeNumber seasonEpisodeNumber =
                new SeasonEpisodeNumber(SERIES_RECORDING_ID1, SEASON_NUMBER1, EPISODE_NUMBER1);
        seasonEpisodeNumbers.add(seasonEpisodeNumber);
        assertThat(seasonEpisodeNumbers)
                .doesNotContain(
                        new SeasonEpisodeNumber(
                                SERIES_RECORDING_ID2, SEASON_NUMBER1, EPISODE_NUMBER1));
        assertThat(seasonEpisodeNumbers)
                .doesNotContain(
                        new SeasonEpisodeNumber(
                                SERIES_RECORDING_ID1, SEASON_NUMBER2, EPISODE_NUMBER1));
        assertThat(seasonEpisodeNumbers)
                .doesNotContain(
                        new SeasonEpisodeNumber(
                                SERIES_RECORDING_ID1, SEASON_NUMBER1, EPISODE_NUMBER2));
    }

    @Test
    public void testEpisodeAlreadyScheduled_null() {
        List<SeasonEpisodeNumber> seasonEpisodeNumbers = new ArrayList<>();
        SeasonEpisodeNumber seasonEpisodeNumber =
                new SeasonEpisodeNumber(SERIES_RECORDING_ID1, SEASON_NUMBER1, EPISODE_NUMBER1);
        seasonEpisodeNumbers.add(seasonEpisodeNumber);
        assertThat(seasonEpisodeNumbers)
                .doesNotContain(
                        new SeasonEpisodeNumber(SERIES_RECORDING_ID1, null, EPISODE_NUMBER1));
        assertThat(seasonEpisodeNumbers)
                .doesNotContain(
                        new SeasonEpisodeNumber(SERIES_RECORDING_ID1, SEASON_NUMBER1, null));
        assertThat(seasonEpisodeNumbers)
                .doesNotContain(new SeasonEpisodeNumber(SERIES_RECORDING_ID1, null, null));
    }
}
