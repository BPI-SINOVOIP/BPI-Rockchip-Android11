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

package com.android.tv.dvr;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.os.Build;
import android.util.Range;

import com.android.tv.dvr.DvrScheduleManager.ConflictInfo;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.dvr.RecordingTestUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link DvrScheduleManager} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.N, application = TestSingletonApp.class)
public class DvrScheduleManagerTest {
    private static final String INPUT_ID = "input_id";

    @Test
    public void testGetConflictingSchedules_emptySchedule() {
        List<ScheduledRecording> schedules = new ArrayList<>();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1)).isEmpty();
    }

    @Test
    public void testGetConflictingSchedules_noConflict() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1)).isEmpty();

        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();

        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();
    }

    @Test
    public void testGetConflictingSchedules_noTuner() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 0)).isEmpty();

        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 0)).isEqualTo(schedules);
        schedules.add(
                0,
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 0)).isEqualTo(schedules);
    }

    @Test
    public void testGetConflictingSchedules_conflict() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1)).isEmpty();

        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r2);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r3);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        ScheduledRecording r4 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r4);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();

        ScheduledRecording r5 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r5);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();

        ScheduledRecording r6 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 10L, 90L);
        schedules.add(r6);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 4)).isEmpty();

        ScheduledRecording r7 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 110L, 190L);
        schedules.add(r7);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r5, r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 4)).isEmpty();

        ScheduledRecording r8 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 50L, 150L);
        schedules.add(r8);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r7, r6, r5, r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r5, r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3))
                .containsExactly(r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 4))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 5)).isEmpty();
    }

    @Test
    public void testGetConflictingSchedules_conflict2() {
        // The case when there is a long schedule.
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 1000L);
        schedules.add(r1);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1)).isEmpty();

        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r2);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r3);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();
    }

    @Test
    public void testGetConflictingSchedules_reverseOrder() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(0, r1);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1)).isEmpty();

        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(0, r2);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(0, r3);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2)).isEmpty();

        ScheduledRecording r4 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(0, r4);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();

        ScheduledRecording r5 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(0, r5);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();

        ScheduledRecording r6 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 10L, 90L);
        schedules.add(0, r6);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 4)).isEmpty();

        ScheduledRecording r7 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 110L, 190L);
        schedules.add(0, r7);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r5, r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 4)).isEmpty();

        ScheduledRecording r8 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 50L, 150L);
        schedules.add(0, r8);
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r7, r6, r5, r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 2))
                .containsExactly(r5, r4, r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3))
                .containsExactly(r3, r2, r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 4))
                .containsExactly(r1)
                .inOrder();
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 5)).isEmpty();
    }

    @Test
    public void testGetConflictingSchedules_period1() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(10L, 20L))))
                .containsExactly(r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(110L, 120L))))
                .containsExactly(r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedules_period2() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(10L, 20L))))
                .containsExactly(r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(110L, 120L))))
                .containsExactly(r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedules_period3() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r2);
        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r3);
        ScheduledRecording r4 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r4);
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(10L, 20L))))
                .containsExactly(r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(110L, 120L))))
                .containsExactly(r2)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                schedules, 1, Collections.singletonList(new Range<>(50L, 150L))))
                .containsExactly(r2, r1)
                .inOrder();
        List<Range<Long>> ranges = new ArrayList<>();
        ranges.add(new Range<>(10L, 20L));
        ranges.add(new Range<>(110L, 120L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1, ranges))
                .containsExactly(r2, r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedules_addSchedules1() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 100L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                Collections.singletonList(
                                        ScheduledRecording.builder(INPUT_ID, ++channelId, 10L, 20L)
                                                .setPriority(++priority)
                                                .build()),
                                schedules,
                                1))
                .containsExactly(r2, r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                Collections.singletonList(
                                        ScheduledRecording.builder(
                                                        INPUT_ID, ++channelId, 110L, 120L)
                                                .setPriority(++priority)
                                                .build()),
                                schedules,
                                1))
                .containsExactly(r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedules_addSchedules2() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                Collections.singletonList(
                                        ScheduledRecording.builder(INPUT_ID, ++channelId, 10L, 20L)
                                                .setPriority(++priority)
                                                .build()),
                                schedules,
                                1))
                .containsExactly(r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                Collections.singletonList(
                                        ScheduledRecording.builder(
                                                        INPUT_ID, ++channelId, 110L, 120L)
                                                .setPriority(++priority)
                                                .build()),
                                schedules,
                                1))
                .containsExactly(r2, r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedules_addLowestPriority() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();

        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 400L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r2);
        // Returning r1 even though r1 has the higher priority than the new one. That's because r1
        // starts at 0 and stops at 100, and the new one will be recorded successfully.
        assertThat(
                        DvrScheduleManager.getConflictingSchedules(
                                Collections.singletonList(
                                        ScheduledRecording.builder(
                                                        INPUT_ID, ++channelId, 200L, 300L)
                                                .setPriority(0)
                                                .build()),
                                schedules,
                                1))
                .containsExactly(r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedules_sameChannel() {
        long priority = 0;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelId, ++priority, 0L, 200L));
        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelId, ++priority, 0L, 200L));
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 3)).isEmpty();
    }

    @Test
    public void testGetConflictingSchedule_startEarlyAndFail() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 200L, 300L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 400L);
        schedules.add(r2);
        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 200L);
        schedules.add(r3);
        // r2 starts recording and fails when r3 starts. r1 is recorded successfully.
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r2)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedule_startLate() {
        long priority = 0;
        long channelId = 0;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 200L, 400L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 100L, 300L);
        schedules.add(r2);
        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r3);
        // r2 and r1 are clipped.
        assertThat(DvrScheduleManager.getConflictingSchedules(schedules, 1))
                .containsExactly(r2, r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedulesForTune_canTune() {
        // Can tune to the recorded channel if tuner count is 1.
        long priority = 0;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelId, ++priority, 0L, 200L));
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForTune(
                                INPUT_ID, channelId, 0L, priority + 1, schedules, 1))
                .isEmpty();
    }

    @Test
    public void testGetConflictingSchedulesForTune_cannotTune() {
        // Can't tune to a channel if other channel is recording and tuner count is 1.
        long priority = 0;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        schedules.add(
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelId, ++priority, 0L, 200L));
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForTune(
                                INPUT_ID, channelId + 1, 0L, priority + 1, schedules, 1))
                .containsExactly(schedules.get(0))
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedulesForWatching_otherChannels() {
        // The other channels are to be recorded.
        long priority = 0;
        long channelToWatch = 1;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 3))
                .isEmpty();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 2))
                .containsExactly(r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedulesForWatching_sameChannel1() {
        long priority = 0;
        long channelToWatch = 1;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelToWatch, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 2))
                .isEmpty();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 1))
                .containsExactly(r2)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedulesForWatching_sameChannel2() {
        long priority = 0;
        long channelToWatch = 1;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelToWatch, ++priority, 0L, 200L);
        schedules.add(r2);
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 2))
                .isEmpty();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 1))
                .containsExactly(r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedulesForWatching_sameChannelConflict1() {
        long priority = 0;
        long channelToWatch = 1;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelToWatch, ++priority, 0L, 200L);
        schedules.add(r2);
        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelToWatch, ++priority, 0L, 200L);
        schedules.add(r3);
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 3))
                .containsExactly(r2)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 2))
                .containsExactly(r2)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 1))
                .containsExactly(r2, r1)
                .inOrder();
    }

    @Test
    public void testGetConflictingSchedulesForWatching_sameChannelConflict2() {
        long priority = 0;
        long channelToWatch = 1;
        long channelId = 1;
        List<ScheduledRecording> schedules = new ArrayList<>();
        ScheduledRecording r1 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelToWatch, ++priority, 0L, 200L);
        schedules.add(r1);
        ScheduledRecording r2 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        channelToWatch, ++priority, 0L, 200L);
        schedules.add(r2);
        ScheduledRecording r3 =
                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                        ++channelId, ++priority, 0L, 200L);
        schedules.add(r3);
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 3))
                .containsExactly(r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 2))
                .containsExactly(r1)
                .inOrder();
        assertThat(
                        DvrScheduleManager.getConflictingSchedulesForWatching(
                                INPUT_ID, channelToWatch, 0L, ++priority, schedules, 1))
                .containsExactly(r3, r1)
                .inOrder();
    }

    @Test
    public void testPartiallyConflictingSchedules() {
        long priority = 100;
        long channelId = 0;
        List<ScheduledRecording> schedules =
                new ArrayList<>(
                        Arrays.asList(
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 0L, 400L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 0L, 200L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 200L, 500L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 400L, 600L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 700L, 800L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 600L, 900L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 800L, 900L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 800L, 900L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 750L, 850L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 300L, 450L),
                                RecordingTestUtils.createTestRecordingWithPriorityAndPeriod(
                                        ++channelId, --priority, 50L, 900L)));
        List<ConflictInfo> conflicts = DvrScheduleManager.getConflictingSchedulesInfo(schedules, 1);

        assertNotInList(schedules.get(0), conflicts);
        assertFullConflict(schedules.get(1), conflicts);
        assertPartialConflict(schedules.get(2), conflicts);
        assertPartialConflict(schedules.get(3), conflicts);
        assertNotInList(schedules.get(4), conflicts);
        assertPartialConflict(schedules.get(5), conflicts);
        assertNotInList(schedules.get(6), conflicts);
        assertFullConflict(schedules.get(7), conflicts);
        assertFullConflict(schedules.get(8), conflicts);
        assertFullConflict(schedules.get(9), conflicts);
        assertFullConflict(schedules.get(10), conflicts);

        conflicts = DvrScheduleManager.getConflictingSchedulesInfo(schedules, 2);

        assertNotInList(schedules.get(0), conflicts);
        assertNotInList(schedules.get(1), conflicts);
        assertNotInList(schedules.get(2), conflicts);
        assertNotInList(schedules.get(3), conflicts);
        assertNotInList(schedules.get(4), conflicts);
        assertNotInList(schedules.get(5), conflicts);
        assertNotInList(schedules.get(6), conflicts);
        assertFullConflict(schedules.get(7), conflicts);
        assertFullConflict(schedules.get(8), conflicts);
        assertFullConflict(schedules.get(9), conflicts);
        assertPartialConflict(schedules.get(10), conflicts);

        conflicts = DvrScheduleManager.getConflictingSchedulesInfo(schedules, 3);

        assertNotInList(schedules.get(0), conflicts);
        assertNotInList(schedules.get(1), conflicts);
        assertNotInList(schedules.get(2), conflicts);
        assertNotInList(schedules.get(3), conflicts);
        assertNotInList(schedules.get(4), conflicts);
        assertNotInList(schedules.get(5), conflicts);
        assertNotInList(schedules.get(6), conflicts);
        assertNotInList(schedules.get(7), conflicts);
        assertPartialConflict(schedules.get(8), conflicts);
        assertNotInList(schedules.get(9), conflicts);
        assertPartialConflict(schedules.get(10), conflicts);
    }

    private void assertNotInList(ScheduledRecording schedule, List<ConflictInfo> conflicts) {
        for (ConflictInfo conflictInfo : conflicts) {
            if (conflictInfo.schedule.equals(schedule)) {
                fail(schedule + " conflicts with others.");
            }
        }
    }

    private void assertPartialConflict(ScheduledRecording schedule, List<ConflictInfo> conflicts) {
        for (ConflictInfo conflictInfo : conflicts) {
            if (conflictInfo.schedule.equals(schedule)) {
                if (conflictInfo.partialConflict) {
                    return;
                } else {
                    fail(schedule + " fully conflicts with others.");
                }
            }
        }
        fail(schedule + " doesn't conflict");
    }

    private void assertFullConflict(ScheduledRecording schedule, List<ConflictInfo> conflicts) {
        for (ConflictInfo conflictInfo : conflicts) {
            if (conflictInfo.schedule.equals(schedule)) {
                if (!conflictInfo.partialConflict) {
                    return;
                } else {
                    fail(schedule + " partially conflicts with others.");
                }
            }
        }
        fail(schedule + " doesn't conflict");
    }
}
