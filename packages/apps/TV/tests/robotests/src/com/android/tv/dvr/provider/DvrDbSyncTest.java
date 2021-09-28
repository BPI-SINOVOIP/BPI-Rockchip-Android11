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
 * limitations under the License
 */

package com.android.tv.dvr.provider;

import static com.google.common.truth.Truth.assertThat;
import static java.lang.Math.abs;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.tv.common.flags.impl.DefaultDvrFlags;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.ProgramImpl;
import com.android.tv.data.api.Program;
import com.android.tv.dvr.DvrManager;
import com.android.tv.dvr.WritableDvrDataManager;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.dvr.data.SeriesRecording;
import com.android.tv.dvr.recorder.SeriesRecordingScheduler;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.common.flags.DvrFlags;
import java.util.concurrent.TimeUnit;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;

/** Tests for {@link com.android.tv.dvr.DvrScheduleManager} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class DvrDbSyncTest {
    private static final String INPUT_ID = "input_id";
    private static final long BASE_PROGRAM_ID = 1;
    private static final long BASE_TIME_MS =
            System.currentTimeMillis() + TimeUnit.MINUTES.toMillis(1);
    private static final long BASE_START_TIME_MS = BASE_TIME_MS;
    private static final long BASE_END_TIME_MS = BASE_TIME_MS + 1;
    private static final long BASE_OFFSET_TIME_MS = 1;
    private static final long BASE_START_TIME_WITH_OFFSET_MS =
            BASE_START_TIME_MS - BASE_OFFSET_TIME_MS;
    private static final long BASE_END_TIME_WITH_OFFSET_MS = BASE_END_TIME_MS + BASE_OFFSET_TIME_MS;
    private static final long BASE_LARGE_OFFSET_TIME_MS = TimeUnit.MINUTES.toMillis(2);
    private static final long RECORD_MARGIN_MS = TimeUnit.SECONDS.toMillis(10);
    private static final String BASE_SEASON_NUMBER = "2";
    private static final String BASE_EPISODE_NUMBER = "3";
    private ProgramImpl baseProgram;
    private ProgramImpl baseSeriesProgram;
    private ScheduledRecording baseSchedule;
    private ScheduledRecording baseSeriesSchedule;

    private DvrDbSync mDbSync;
    @Mock private DvrManager mDvrManager;
    private DvrFlags mDvrFlags = new DefaultDvrFlags();
    @Mock private WritableDvrDataManager mDataManager;
    @Mock private ChannelDataManager mChannelDataManager;
    @Mock private SeriesRecordingScheduler mSeriesRecordingScheduler;

    @Before
    public void setUp() {
        // TODO(b/69843199): make these static finals
        baseProgram =
                new ProgramImpl.Builder()
                        .setId(BASE_PROGRAM_ID)
                        .setStartTimeUtcMillis(BASE_START_TIME_MS)
                        .setEndTimeUtcMillis(BASE_END_TIME_MS)
                        .build();
        baseSeriesProgram =
                new ProgramImpl.Builder()
                        .setId(BASE_PROGRAM_ID)
                        .setStartTimeUtcMillis(BASE_START_TIME_MS)
                        .setEndTimeUtcMillis(BASE_END_TIME_MS)
                        .setSeasonNumber(BASE_SEASON_NUMBER)
                        .setEpisodeNumber(BASE_EPISODE_NUMBER)
                        .build();
        baseSchedule = ScheduledRecording.builder(INPUT_ID, baseProgram).build();
        baseSeriesSchedule = ScheduledRecording.builder(INPUT_ID, baseSeriesProgram).build();

        MockitoAnnotations.initMocks(this);
        when(mChannelDataManager.isDbLoadFinished()).thenReturn(true);
        when(mDvrManager.addSeriesRecording(any(), any(), anyInt()))
                .thenReturn(SeriesRecording.builder(INPUT_ID, baseProgram).build());
        mDbSync =
                new DvrDbSync(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mDataManager,
                        mDvrFlags,
                        mChannelDataManager,
                        mDvrManager,
                        mSeriesRecordingScheduler,
                        new RoboExecutorService());
    }

    @Test
    public void testHandleUpdateProgram_null() {
        addSchedule(BASE_PROGRAM_ID, baseSchedule);
        mDbSync.handleUpdateProgram(null, BASE_PROGRAM_ID);
        verify(mDataManager).removeScheduledRecording(baseSchedule);
    }

    @Test
    public void testHandleUpdateProgram_changeTimeNotStarted() {
        addSchedule(BASE_PROGRAM_ID, baseSchedule);
        long startTimeMs = BASE_START_TIME_MS + 1;
        long endTimeMs = BASE_END_TIME_MS + 1;
        Program program =
                new ProgramImpl.Builder(baseProgram)
                        .setStartTimeUtcMillis(startTimeMs)
                        .setEndTimeUtcMillis(endTimeMs)
                        .build();
        mDbSync.handleUpdateProgram(program, BASE_PROGRAM_ID);
        assertUpdateScheduleCalled(program);
    }

    @Test
    public void testHandleUpdateProgram_changeTimeInProgressNotCalled() {
        addSchedule(
                BASE_PROGRAM_ID,
                ScheduledRecording.buildFrom(baseSchedule)
                        .setState(ScheduledRecording.STATE_RECORDING_IN_PROGRESS)
                        .build());
        long startTimeMs = BASE_START_TIME_MS + 1;
        Program program =
                new ProgramImpl.Builder(baseProgram).setStartTimeUtcMillis(startTimeMs).build();
        mDbSync.handleUpdateProgram(program, BASE_PROGRAM_ID);
        verify(mDataManager, never()).updateScheduledRecording(any());
    }

    @Test
    public void testHandleUpdateProgram_changeSeason() {
        addSchedule(BASE_PROGRAM_ID, baseSeriesSchedule);
        String seasonNumber = BASE_SEASON_NUMBER + "1";
        String episodeNumber = BASE_EPISODE_NUMBER + "1";
        Program program =
                new ProgramImpl.Builder(baseSeriesProgram)
                        .setSeasonNumber(seasonNumber)
                        .setEpisodeNumber(episodeNumber)
                        .build();
        mDbSync.handleUpdateProgram(program, BASE_PROGRAM_ID);
        assertUpdateScheduleCalled(program);
    }

    @Test
    public void testHandleUpdateProgram_finished() {
        addSchedule(
                BASE_PROGRAM_ID,
                ScheduledRecording.buildFrom(baseSeriesSchedule)
                        .setState(ScheduledRecording.STATE_RECORDING_FINISHED)
                        .build());
        String seasonNumber = BASE_SEASON_NUMBER + "1";
        String episodeNumber = BASE_EPISODE_NUMBER + "1";
        Program program =
                new ProgramImpl.Builder(baseSeriesProgram)
                        .setSeasonNumber(seasonNumber)
                        .setEpisodeNumber(episodeNumber)
                        .build();
        mDbSync.handleUpdateProgram(program, BASE_PROGRAM_ID);
        verify(mDataManager, never()).updateScheduledRecording(any());
    }

    @Test
    public void testHandleUpdateProgram_addOffsetNotStarted() {
        Assume.assumeTrue(mDvrFlags.startEarlyEndLateEnabled());
        ScheduledRecording schedule = ScheduledRecording.buildFrom(baseSchedule)
                .setStartOffsetMs(BASE_OFFSET_TIME_MS)
                .setEndOffsetMs(BASE_OFFSET_TIME_MS)
                .build();
        addSchedule(BASE_PROGRAM_ID, schedule);
        mDbSync.handleUpdateProgram(baseProgram, BASE_PROGRAM_ID);
        ScheduledRecording expectedSchedule =
                ScheduledRecording.buildFrom(schedule)
                        .setStartTimeMs(BASE_START_TIME_WITH_OFFSET_MS)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS)
                        .build();
        assertUpdateScheduleCalled(expectedSchedule);
    }

    @Test
    public void testHandleUpdateProgram_addOffsetInProgress() {
        Assume.assumeTrue(mDvrFlags.startEarlyEndLateEnabled());
        ScheduledRecording schedule =
                ScheduledRecording.buildFrom(baseSchedule)
                        .setState(ScheduledRecording.STATE_RECORDING_IN_PROGRESS)
                        .setStartOffsetMs(BASE_OFFSET_TIME_MS)
                        .setEndOffsetMs(BASE_OFFSET_TIME_MS)
                        .build();
        addSchedule(BASE_PROGRAM_ID, schedule);
        mDbSync.handleUpdateProgram(baseProgram, BASE_PROGRAM_ID);
        ScheduledRecording expectedSchedule =
                ScheduledRecording.buildFrom(schedule)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS)
                        .build();
        assertUpdateScheduleCalled(expectedSchedule);
    }

    @Test
    public void testHandleUpdateProgram_changeTimeNotStartedWithScheduleOffsets() {
        Assume.assumeTrue(mDvrFlags.startEarlyEndLateEnabled());
        ScheduledRecording schedule =
                ScheduledRecording.buildFrom(baseSchedule)
                        .setStartTimeMs(BASE_START_TIME_WITH_OFFSET_MS)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS)
                        .setStartOffsetMs(BASE_OFFSET_TIME_MS)
                        .setEndOffsetMs(BASE_OFFSET_TIME_MS)
                        .build();
        addSchedule(BASE_PROGRAM_ID, schedule);
        long startTimeMs = BASE_START_TIME_MS + 1;
        long endTimeMs = BASE_END_TIME_MS + 1;
        Program program =
                new ProgramImpl.Builder(baseProgram)
                        .setStartTimeUtcMillis(startTimeMs)
                        .setEndTimeUtcMillis(endTimeMs)
                        .build();
        mDbSync.handleUpdateProgram(program, BASE_PROGRAM_ID);
        ScheduledRecording expectedSchedule =
                ScheduledRecording.buildFrom(schedule)
                        .setStartTimeMs(BASE_START_TIME_WITH_OFFSET_MS + 1)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS + 1)
                        .build();
        assertUpdateScheduleCalled(expectedSchedule);
    }

    @Test
    public void testHandleUpdateProgram_changeTimeInProgressWithScheduleOffsets() {
        Assume.assumeTrue(mDvrFlags.startEarlyEndLateEnabled());
        ScheduledRecording schedule =
                ScheduledRecording.buildFrom(baseSchedule)
                        .setState(ScheduledRecording.STATE_RECORDING_IN_PROGRESS)
                        .setStartTimeMs(BASE_START_TIME_WITH_OFFSET_MS)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS)
                        .setStartOffsetMs(BASE_OFFSET_TIME_MS)
                        .setEndOffsetMs(BASE_OFFSET_TIME_MS)
                        .build();
        addSchedule(BASE_PROGRAM_ID, schedule);
        long startTimeMs = BASE_START_TIME_MS + 1;
        long endTimeMs = BASE_END_TIME_MS + 1;
        Program program =
                new ProgramImpl.Builder(baseProgram)
                        .setStartTimeUtcMillis(startTimeMs)
                        .setEndTimeUtcMillis(endTimeMs)
                        .build();
        mDbSync.handleUpdateProgram(program, BASE_PROGRAM_ID);
        ScheduledRecording expectedSchedule =
                ScheduledRecording.buildFrom(schedule)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS + 1)
                        .build();
        assertUpdateScheduleCalled(expectedSchedule);
    }

    @Test
    public void testHandleUpdateProgram_addStartTimeInPastOffsetNotStarted() {
        Assume.assumeTrue(mDvrFlags.startEarlyEndLateEnabled());
        ScheduledRecording schedule =
                ScheduledRecording.buildFrom(baseSchedule)
                        .setStartOffsetMs(BASE_LARGE_OFFSET_TIME_MS)
                        .setEndOffsetMs(BASE_OFFSET_TIME_MS)
                        .build();
        addSchedule(BASE_PROGRAM_ID, schedule);
        mDbSync.handleUpdateProgram(baseProgram, BASE_PROGRAM_ID);
        long startTimeMs = System.currentTimeMillis() + RECORD_MARGIN_MS;
        long startOffsetMs = BASE_START_TIME_MS - startTimeMs;
        ScheduledRecording expectedSchedule =
                ScheduledRecording.buildFrom(schedule)
                        .setStartTimeMs(startTimeMs)
                        .setStartOffsetMs(startOffsetMs)
                        .setEndTimeMs(BASE_END_TIME_WITH_OFFSET_MS)
                        .build();
        assertUpdateScheduleCalledWithinRange(expectedSchedule, RECORD_MARGIN_MS);
    }

    private void addSchedule(long programId, ScheduledRecording schedule) {
        when(mDataManager.getScheduledRecordingForProgramId(programId)).thenReturn(schedule);
    }

    private void assertUpdateScheduleCalled(Program program) {
        verify(mDataManager)
                .updateScheduledRecording(
                        eq(ScheduledRecording.builder(INPUT_ID, program).build()));
    }

    private void assertUpdateScheduleCalled(ScheduledRecording schedule) {
        verify(mDataManager).updateScheduledRecording(eq(schedule));
    }

    private void assertUpdateScheduleCalledWithinRange(ScheduledRecording schedule, long range) {
        // Compare the schedules excluding startTimeMs and startOffsetMs
        verify(mDataManager).updateScheduledRecording(
                refEq(schedule,"start_time_utc_millis", "start_offset_millis"));
        // Fetch the actual schedule
        ArgumentCaptor<ScheduledRecording> actualArgument =
                ArgumentCaptor.forClass(ScheduledRecording.class);
        verify(mDataManager).updateScheduledRecording(actualArgument.capture());
        ScheduledRecording actualSchedule = actualArgument.getValue();
        // Assert that values of startTimeMs and startOffsetMs are within expected range
        long startTimeDelta = abs(actualSchedule.getStartTimeMs() - schedule.getStartTimeMs());
        long startOffsetDelta =
                abs(actualSchedule.getStartOffsetMs() - schedule.getStartOffsetMs());
        assertThat(startTimeDelta).isAtMost(range);
        assertThat(startOffsetDelta).isAtMost(range);
    }
}
