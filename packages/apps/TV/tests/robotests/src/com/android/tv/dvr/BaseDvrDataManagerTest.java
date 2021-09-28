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

package com.android.tv.dvr;

import android.os.Build;
import android.support.annotation.NonNull;

import com.android.tv.common.feature.CommonFeatures;
import com.android.tv.common.feature.TestableFeature;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.dvr.DvrDataManagerInMemoryImpl;
import com.android.tv.testing.dvr.RecordingTestUtils;
import com.android.tv.testing.fakes.FakeClock;

import com.google.common.truth.Truth;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link BaseDvrDataManager} using {@link DvrDataManagerInMemoryImpl}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.N, application = TestSingletonApp.class)
public class BaseDvrDataManagerTest {
    private static final String INPUT_ID = "input_id";
    private static final int CHANNEL_ID = 273;

    private final TestableFeature mDvrFeature = CommonFeatures.DVR;
    private DvrDataManagerInMemoryImpl mDvrDataManager;
    private FakeClock mFakeClock;

    @Before
    public void setUp() {
        mDvrFeature.enableForTest();
        mFakeClock = FakeClock.createWithCurrentTime();
        mDvrDataManager =
                new DvrDataManagerInMemoryImpl(RuntimeEnvironment.application, mFakeClock);
    }

    @After
    public void tearDown() {
        mDvrFeature.resetForTests();
    }

    @Test
    public void testGetNonStartedScheduledRecordings() {
        ScheduledRecording recording =
                mDvrDataManager.addScheduledRecordingInternal(
                        createNewScheduledRecordingStartingNow());
        List<ScheduledRecording> result = mDvrDataManager.getNonStartedScheduledRecordings();
        Truth.assertThat(result).containsExactly(recording);
    }

    @Test
    public void testGetNonStartedScheduledRecordings_past() {
        mDvrDataManager.addScheduledRecordingInternal(createNewScheduledRecordingStartingNow());
        mFakeClock.increment(TimeUnit.MINUTES, 6);
        List<ScheduledRecording> result = mDvrDataManager.getNonStartedScheduledRecordings();
        Truth.assertThat(result).isEmpty();
    }

    @NonNull
    private ScheduledRecording createNewScheduledRecordingStartingNow() {
        return ScheduledRecording.buildFrom(
                        RecordingTestUtils.createTestRecordingWithIdAndPeriod(
                                ScheduledRecording.ID_NOT_SET,
                                INPUT_ID,
                                CHANNEL_ID,
                                mFakeClock.currentTimeMillis(),
                                mFakeClock.currentTimeMillis() + TimeUnit.MINUTES.toMillis(5)))
                .setState(ScheduledRecording.STATE_RECORDING_NOT_STARTED)
                .build();
    }
}
