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

package com.android.tv.dvr.recorder;

import static com.google.common.truth.Truth.assertThat;

import android.os.Build;
import android.util.LongSparseArray;

import com.android.tv.common.feature.CommonFeatures;
import com.android.tv.common.feature.TestableFeature;
import com.android.tv.data.ProgramImpl;
import com.android.tv.data.api.Program;
import com.android.tv.dvr.data.SeriesRecording;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.dvr.DvrDataManagerInMemoryImpl;
import com.android.tv.testing.fakes.FakeClock;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for {@link SeriesRecordingScheduler} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.N, application = TestSingletonApp.class)
public class SeriesRecordingSchedulerTest {
    private static final String PROGRAM_TITLE = "MyProgram";
    private static final long CHANNEL_ID = 123;
    private static final long SERIES_RECORDING_ID1 = 1;
    private static final String SERIES_ID = "SERIES_ID";
    private static final String SEASON_NUMBER1 = "SEASON NUMBER1";
    private static final String SEASON_NUMBER2 = "SEASON NUMBER2";
    private static final String EPISODE_NUMBER1 = "EPISODE NUMBER1";
    private static final String EPISODE_NUMBER2 = "EPISODE NUMBER2";

    private final SeriesRecording mBaseSeriesRecording =
            new SeriesRecording.Builder()
                    .setTitle(PROGRAM_TITLE)
                    .setChannelId(CHANNEL_ID)
                    .setSeriesId(SERIES_ID)
                    .build();
    private final ProgramImpl mBaseProgram =
            new ProgramImpl.Builder()
                    .setTitle(PROGRAM_TITLE)
                    .setChannelId(CHANNEL_ID)
                    .setSeriesId(SERIES_ID)
                    .build();
    private final TestableFeature mDvrFeature = CommonFeatures.DVR;

    private DvrDataManagerInMemoryImpl mDataManager;

    @Before
    public void setUp() {
        mDvrFeature.enableForTest();
        FakeClock fakeClock = FakeClock.createWithCurrentTime();
        mDataManager = new DvrDataManagerInMemoryImpl(RuntimeEnvironment.application, fakeClock);
    }

    @After
    public void tearDown() {
        mDvrFeature.resetForTests();
    }

    @Test
    public void testPickOneProgramPerEpisode_onePerEpisode() {
        SeriesRecording seriesRecording =
                SeriesRecording.buildFrom(mBaseSeriesRecording).setId(SERIES_RECORDING_ID1).build();
        mDataManager.addSeriesRecording(seriesRecording);
        List<Program> programs = new ArrayList<>();
        Program program1 =
                new ProgramImpl.Builder(mBaseProgram)
                        .setSeasonNumber(SEASON_NUMBER1)
                        .setEpisodeNumber(EPISODE_NUMBER1)
                        .build();
        programs.add(program1);
        Program program2 =
                new ProgramImpl.Builder(mBaseProgram)
                        .setSeasonNumber(SEASON_NUMBER2)
                        .setEpisodeNumber(EPISODE_NUMBER2)
                        .build();
        programs.add(program2);
        LongSparseArray<List<Program>> result =
                SeriesRecordingScheduler.pickOneProgramPerEpisode(
                        mDataManager, Collections.singletonList(seriesRecording), programs);
        assertThat(result.get(SERIES_RECORDING_ID1)).containsExactly(program1, program2);
    }

    @Test
    public void testPickOneProgramPerEpisode_manyPerEpisode() {
        SeriesRecording seriesRecording =
                SeriesRecording.buildFrom(mBaseSeriesRecording).setId(SERIES_RECORDING_ID1).build();
        mDataManager.addSeriesRecording(seriesRecording);
        List<Program> programs = new ArrayList<>();
        ProgramImpl program1 =
                new ProgramImpl.Builder(mBaseProgram)
                        .setSeasonNumber(SEASON_NUMBER1)
                        .setEpisodeNumber(EPISODE_NUMBER1)
                        .setStartTimeUtcMillis(0)
                        .build();
        programs.add(program1);
        Program program2 = new ProgramImpl.Builder(program1).setStartTimeUtcMillis(1).build();
        programs.add(program2);
        Program program3 =
                new ProgramImpl.Builder(mBaseProgram)
                        .setSeasonNumber(SEASON_NUMBER2)
                        .setEpisodeNumber(EPISODE_NUMBER2)
                        .build();
        programs.add(program3);
        Program program4 = new ProgramImpl.Builder(program1).setStartTimeUtcMillis(1).build();
        programs.add(program4);
        LongSparseArray<List<Program>> result =
                SeriesRecordingScheduler.pickOneProgramPerEpisode(
                        mDataManager, Collections.singletonList(seriesRecording), programs);
        assertThat(result.get(SERIES_RECORDING_ID1)).containsExactly(program1, program3);
    }

    @Test
    public void testPickOneProgramPerEpisode_nullEpisode() {
        SeriesRecording seriesRecording =
                SeriesRecording.buildFrom(mBaseSeriesRecording).setId(SERIES_RECORDING_ID1).build();
        mDataManager.addSeriesRecording(seriesRecording);
        List<Program> programs = new ArrayList<>();
        Program program1 = new ProgramImpl.Builder(mBaseProgram).setStartTimeUtcMillis(0).build();
        programs.add(program1);
        Program program2 = new ProgramImpl.Builder(mBaseProgram).setStartTimeUtcMillis(1).build();
        programs.add(program2);
        LongSparseArray<List<Program>> result =
                SeriesRecordingScheduler.pickOneProgramPerEpisode(
                        mDataManager, Collections.singletonList(seriesRecording), programs);
        assertThat(result.get(SERIES_RECORDING_ID1)).containsExactly(program1, program2);
    }
}
