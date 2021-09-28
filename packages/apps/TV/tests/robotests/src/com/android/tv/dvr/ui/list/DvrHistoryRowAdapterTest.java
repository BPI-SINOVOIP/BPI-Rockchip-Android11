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

package com.android.tv.dvr.ui.list;

import static com.google.common.truth.Truth.assertThat;

import androidx.leanback.widget.ClassPresenterSelector;

import com.android.tv.common.flags.impl.DefaultUiFlags;
import com.android.tv.common.util.Clock;
import com.android.tv.dvr.data.RecordedProgram;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.dvr.DvrDataManagerInMemoryImpl;
import com.android.tv.testing.fakes.FakeClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

/** Test for {@link DvrHistoryRowAdapter}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class DvrHistoryRowAdapterTest {

    private static final ScheduledRecording SCHEDULE_1 =
            buildScheduledRecordingForTest(
                    2,
                    1518249600000L, // 2/10/2018 0:00 PST
                    1518250600000L,
                    ScheduledRecording.STATE_RECORDING_FAILED);
    private static final ScheduledRecording SCHEDULE_1_COPY =
            buildScheduledRecordingForTest(
                    3,
                    1518249600000L, // 2/10/2018 0:00 PST
                    1518250600000L,
                    ScheduledRecording.STATE_RECORDING_FAILED);
    private static final ScheduledRecording SCHEDULE_2 =
            buildScheduledRecordingForTest(
                    4,
                    1518595200000L, // 2/14/2018 0:00 PST
                    1518596200000L,
                    ScheduledRecording.STATE_RECORDING_FAILED);
    private static final ScheduledRecording SCHEDULE_2_COPY =
            buildScheduledRecordingForTest(
                    5,
                    1518595200000L, // 2/14/2018 0:00 PST
                    1518596200000L,
                    ScheduledRecording.STATE_RECORDING_FAILED);

    private static RecordedProgram sRecordedProgram;
    private static final long FAKE_CURRENT_TIME = 1518764400000L; // 2/15/2018 23:00 PST

    private DvrHistoryRowAdapter mDvrHistoryRowAdapter;
    private DvrDataManagerInMemoryImpl mDvrDataManager;

    @Before
    public void setUp() {
        sRecordedProgram =
                buildRecordedProgramForTest(
                        6,
                        1518249600000L, // 2/10/2018 0:00 PST
                        1518250600000L);

        TestSingletonApp app = (TestSingletonApp) RuntimeEnvironment.application;
        Clock clock = FakeClock.createWithTime(FAKE_CURRENT_TIME);

        mDvrDataManager = new DvrDataManagerInMemoryImpl(app, clock);
        app.mDvrDataManager = mDvrDataManager;
        // keep the original IDs instead of creating a new one.
        mDvrDataManager.addScheduledRecording(
                true, SCHEDULE_1, SCHEDULE_1_COPY, SCHEDULE_2, SCHEDULE_2_COPY);

        ClassPresenterSelector presenterSelector = new ClassPresenterSelector();
        mDvrHistoryRowAdapter =
                new DvrHistoryRowAdapter(
                        RuntimeEnvironment.application,
                        presenterSelector,
                        clock,
                        mDvrDataManager,
                        new DefaultUiFlags());
    }

    @Test
    public void testStart() {
        mDvrHistoryRowAdapter.start();
        assertInitialState();
    }

    @Test
    public void testOnScheduledRecordingAdded_existingHeader() {
        mDvrHistoryRowAdapter.start();
        ScheduledRecording toAdd =
                buildScheduledRecordingForTest(
                        6,
                        1518249600000L, // 2/10/2018
                        1518250600000L,
                        ScheduledRecording.STATE_RECORDING_FAILED);
        mDvrHistoryRowAdapter.onScheduledRecordingAdded(toAdd);

        // a schedule row is added
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(7);
        assertThat(getHeaderItemCounts()).containsExactly(2, 3).inOrder();
        assertThat(getScheduleSize()).isEqualTo(5);
    }

    @Test
    public void testOnScheduledRecordingAdded_newHeader_addOldest() {
        mDvrHistoryRowAdapter.start();
        ScheduledRecording toAdd =
                buildScheduledRecordingForTest(
                        6,
                        1518159600000L, // 2/8/2018 PST
                        1518160600000L,
                        ScheduledRecording.STATE_RECORDING_FAILED);
        mDvrHistoryRowAdapter.onScheduledRecordingAdded(toAdd);

        // a header row and a schedule row are added
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(8);
        assertThat(getHeaderItemCounts()).containsExactly(2, 2, 1).inOrder();
        assertThat(getScheduleSize()).isEqualTo(5);
    }

    @Test
    public void testOnScheduledRecordingAdded_newHeader_addBetween() {
        mDvrHistoryRowAdapter.start();
        ScheduledRecording toAdd =
                buildScheduledRecordingForTest(
                        6,
                        1518336000000L, // 2/11/2018 PST
                        1518337000000L,
                        ScheduledRecording.STATE_RECORDING_FAILED);
        mDvrHistoryRowAdapter.onScheduledRecordingAdded(toAdd);

        // a header row and a schedule row are added
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(8);
        assertThat(getHeaderItemCounts()).containsExactly(2, 1, 2).inOrder();
        assertThat(getScheduleSize()).isEqualTo(5);
    }

    @Test
    public void testOnScheduledRecordingAdded_newHeader_addNewest() {
        mDvrHistoryRowAdapter.start();
        ScheduledRecording toAdd =
                buildScheduledRecordingForTest(
                        6,
                        1518681600000L, // 2/15/2018 PST
                        1518682600000L,
                        ScheduledRecording.STATE_RECORDING_FAILED);
        mDvrHistoryRowAdapter.onScheduledRecordingAdded(toAdd);

        // a header row and a schedule row are added
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(8);
        assertThat(getHeaderItemCounts()).containsExactly(1, 2, 2).inOrder();
        assertThat(getScheduleSize()).isEqualTo(5);
    }

    @Test
    public void testOnScheduledRecordingAdded_addRecordedProgram() {
        mDvrHistoryRowAdapter.start();
        mDvrHistoryRowAdapter.onScheduledRecordingAdded(sRecordedProgram);

        // a header row and a schedule row are added
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(7);
        assertThat(getHeaderItemCounts()).containsExactly(2, 3).inOrder();
        assertThat(getScheduleSize()).isEqualTo(5);
    }

    @Test
    public void testOnScheduledRecordingRemoved_keepHeader() {
        mDvrHistoryRowAdapter.start();
        mDvrHistoryRowAdapter.onScheduledRecordingRemoved(SCHEDULE_1);

        // a schedule row is removed
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(5);
        assertThat(getHeaderItemCounts()).containsExactly(2, 1).inOrder();
        assertThat(getScheduleSize()).isEqualTo(3);
    }

    @Test
    public void testOnScheduledRecordingRemoved_removeHeader() {
        mDvrHistoryRowAdapter.start();
        mDvrHistoryRowAdapter.onScheduledRecordingRemoved(SCHEDULE_1);
        mDvrHistoryRowAdapter.onScheduledRecordingRemoved(SCHEDULE_1_COPY);

        // a header row and a schedule row are removed
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(3);
        assertThat(getHeaderItemCounts()).containsExactly(2).inOrder();
        assertThat(getScheduleSize()).isEqualTo(2);
    }

    @Test
    public void testOnScheduledRecordingRemoved_removeRecordedProgram() {
        mDvrDataManager.addRecordedProgramInternal(sRecordedProgram, true);
        mDvrHistoryRowAdapter.start();
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(7);
        assertThat(getHeaderItemCounts()).containsExactly(2, 3).inOrder();
        assertThat(getScheduleSize()).isEqualTo(5);

        mDvrHistoryRowAdapter.onScheduledRecordingRemoved(sRecordedProgram);

        // a schedule row is removed
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(6);
        assertThat(getHeaderItemCounts()).containsExactly(2, 2);
        assertThat(getScheduleSize()).isEqualTo(4);
    }

    private static ScheduledRecording buildScheduledRecordingForTest(
            long id, long startTime, long endTime, int state) {
        ScheduledRecording.Builder builder =
                ScheduledRecording.builder("fakeInput", 1, startTime, endTime)
                        .setId(id)
                        .setState(state);
        return builder.build();
    }

    private static RecordedProgram buildRecordedProgramForTest(
            long id, long startTime, long endTime) {
        RecordedProgram.Builder builder =
                RecordedProgram.builder()
                        .setId(id)
                        .setInputId("fakeInput")
                        .setStartTimeUtcMillis(startTime)
                        .setEndTimeUtcMillis(endTime);
        return builder.build();
    }

    private int getScheduleSize() {
        int size = 0;
        for (int i = 0; i < mDvrHistoryRowAdapter.size(); i++) {
            Object item = mDvrHistoryRowAdapter.get(i);
            if (item instanceof ScheduleRow && ((ScheduleRow) item).getSchedule() != null) {
                size++;
            }
        }
        return size;
    }

    private List<Integer> getHeaderItemCounts() {
        List<Integer> result = new ArrayList<>();
        for (int i = 0; i < mDvrHistoryRowAdapter.size(); i++) {
            Object item = mDvrHistoryRowAdapter.get(i);
            if (item instanceof SchedulesHeaderRow) {
                int count = ((SchedulesHeaderRow) item).getItemCount();
                assertThat(count).isAtLeast(1);
                result.add(count);
            }
        }
        return result;
    }

    private void assertInitialState() {
        assertThat(mDvrHistoryRowAdapter.size()).isEqualTo(6);
        assertThat(getHeaderItemCounts()).containsExactly(2, 2).inOrder();
        assertThat(getScheduleSize()).isEqualTo(4);
    }
}
