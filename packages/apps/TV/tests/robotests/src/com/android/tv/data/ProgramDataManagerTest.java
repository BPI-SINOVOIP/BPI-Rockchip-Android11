/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.data;

import static android.os.Looper.getMainLooper;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.robolectric.Shadows.shadowOf;

import android.content.ContentResolver;
import android.media.tv.TvContract;

import com.android.tv.common.flags.impl.DefaultBackendKnobsFlags;
import com.android.tv.data.api.Program;
import com.android.tv.perf.stub.StubPerformanceMonitor;
import com.android.tv.testing.FakeTvInputManagerHelper;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.constants.Constants;
import com.android.tv.testing.data.ProgramInfo;
import com.android.tv.testing.data.ProgramUtils;
import com.android.tv.testing.fakes.FakeClock;
import com.android.tv.testing.fakes.FakeTvProvider;
import com.android.tv.testing.robo.ContentProviders;
import com.android.tv.testing.testdata.TestData;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Test for {@link ProgramDataManager} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class ProgramDataManagerTest {

    // Wait time for expected success.
    private static final long WAIT_TIME_OUT_MS = 1000L;
    // Wait time for expected failure.
    private static final long FAILURE_TIME_OUT_MS = 300L;

    private ProgramDataManager mProgramDataManager;
    private FakeClock mClock;
    private TestProgramDataManagerCallback mCallback;

    @Before
    public void setUp() {
        mClock = FakeClock.createWithCurrentTime();
        mCallback = new TestProgramDataManagerCallback();
        ContentProviders.register(FakeTvProvider.class, TvContract.AUTHORITY);
        TestData.DEFAULT_10_CHANNELS.init(
                RuntimeEnvironment.application, mClock, TimeUnit.DAYS.toMillis(1));
        FakeTvInputManagerHelper tvInputManagerHelper =
                new FakeTvInputManagerHelper(RuntimeEnvironment.application);
        RoboExecutorService executor = new RoboExecutorService();
        ContentResolver contentResolver = RuntimeEnvironment.application.getContentResolver();
        ChannelDataManager channelDataManager =
                new ChannelDataManager(
                        RuntimeEnvironment.application,
                        tvInputManagerHelper,
                        executor,
                        contentResolver);
        mProgramDataManager =
                new ProgramDataManager(
                        RuntimeEnvironment.application,
                        executor,
                        RuntimeEnvironment.application.getContentResolver(),
                        mClock,
                        getMainLooper(),
                        new DefaultBackendKnobsFlags(),
                        new StubPerformanceMonitor(),
                        channelDataManager,
                        tvInputManagerHelper);

        mProgramDataManager.setPrefetchEnabled(true);
        mProgramDataManager.addCallback(mCallback);
    }

    @After
    public void tearDown() {
        mProgramDataManager.stop();
    }

    private void startAndWaitForComplete() throws InterruptedException {
        mProgramDataManager.start();
        shadowOf(getMainLooper()).idle();
        assertThat(mCallback.channelUpdatedLatch.await(WAIT_TIME_OUT_MS, TimeUnit.MILLISECONDS))
                .isTrue();
    }

    /** Test for {@link ProgramInfo#getIndex} and {@link ProgramInfo#getStartTimeMs}. */
    @Test
    public void testProgramUtils() {
        ProgramInfo stub = ProgramInfo.create();
        for (long channelId = 1; channelId < Constants.UNIT_TEST_CHANNEL_COUNT; channelId++) {
            int index = stub.getIndex(mClock.currentTimeMillis(), channelId);
            long startTimeMs = stub.getStartTimeMs(index, channelId);
            ProgramInfo programAt = stub.build(RuntimeEnvironment.application, index);
            assertThat(startTimeMs).isAtMost(mClock.currentTimeMillis());
            assertThat(mClock.currentTimeMillis()).isLessThan(startTimeMs + programAt.durationMs);
        }
    }

    /**
     * Test for following methods.
     *
     * <p>{@link ProgramDataManager#getCurrentProgram(long)}, {@link
     * ProgramDataManager#getPrograms(long, long)}, {@link
     * ProgramDataManager#setPrefetchTimeRange(long)}.
     */
    @Test
    public void testGetPrograms() throws InterruptedException {
        // Initial setup to test {@link ProgramDataManager#setPrefetchTimeRange(long)}.
        long preventSnapDelayMs = ProgramDataManager.PROGRAM_GUIDE_SNAP_TIME_MS * 2;
        long prefetchTimeRangeStartMs = System.currentTimeMillis() + preventSnapDelayMs;
        mClock.setCurrentTimeMillis(prefetchTimeRangeStartMs + preventSnapDelayMs);
        mProgramDataManager.setPrefetchTimeRange(prefetchTimeRangeStartMs);

        startAndWaitForComplete();

        for (long channelId = 1; channelId <= Constants.UNIT_TEST_CHANNEL_COUNT; channelId++) {
            Program currentProgram = mProgramDataManager.getCurrentProgram(channelId);
            // Test {@link ProgramDataManager#getCurrentProgram(long)}.
            assertThat(currentProgram).isNotNull();
            assertWithMessage("currentProgramStartTime")
                    .that(currentProgram.getStartTimeUtcMillis())
                    .isLessThan(mClock.currentTimeMillis());
            assertWithMessage("currentProgramEndTime")
                    .that(currentProgram.getEndTimeUtcMillis())
                    .isGreaterThan(mClock.currentTimeMillis());

            // Test {@link ProgramDataManager#getPrograms(long)}.
            // Case #1: Normal case
            List<Program> programs =
                    mProgramDataManager.getPrograms(channelId, mClock.currentTimeMillis());
            ProgramInfo stub = ProgramInfo.create();
            int index = stub.getIndex(mClock.currentTimeMillis(), channelId);
            for (Program program : programs) {
                ProgramInfo programInfoAt = stub.build(RuntimeEnvironment.application, index);
                long startTimeMs = stub.getStartTimeMs(index, channelId);
                assertProgramEquals(startTimeMs, programInfoAt, program);
                index++;
            }
            // Case #2: Corner cases where there's a program that starts at the start of the range.
            long startTimeMs = programs.get(0).getStartTimeUtcMillis();
            programs = mProgramDataManager.getPrograms(channelId, startTimeMs);
            assertThat(programs.get(0).getStartTimeUtcMillis()).isEqualTo(startTimeMs);

            // Test {@link ProgramDataManager#setPrefetchTimeRange(long)}.
            programs =
                    mProgramDataManager.getPrograms(
                            channelId, prefetchTimeRangeStartMs - TimeUnit.HOURS.toMillis(1));
            for (Program program : programs) {
                assertThat(program.getEndTimeUtcMillis()).isAtLeast(prefetchTimeRangeStartMs);
            }
        }
    }

    /**
     * Test for following methods.
     *
     * <p>{@link ProgramDataManager#addOnCurrentProgramUpdatedListener}, {@link
     * ProgramDataManager#removeOnCurrentProgramUpdatedListener}.
     */
    @Test
    public void testCurrentProgramListener() throws InterruptedException {
        final long testChannelId = 1;
        ProgramInfo stub = ProgramInfo.create();
        int index = stub.getIndex(mClock.currentTimeMillis(), testChannelId);
        // Set current time to few seconds before the current program ends,
        // so we can see if callback is called as expected.
        long nextProgramStartTimeMs = stub.getStartTimeMs(index + 1, testChannelId);
        ProgramInfo nextProgramInfo = stub.build(RuntimeEnvironment.application, index + 1);
        mClock.setCurrentTimeMillis(nextProgramStartTimeMs - (WAIT_TIME_OUT_MS / 2));

        startAndWaitForComplete();
        // Note that changing current time doesn't affect the current program
        // because current program is updated after waiting for the program's duration.
        // See {@link ProgramDataManager#updateCurrentProgram}.
        TestProgramDataManagerOnCurrentProgramUpdatedListener listener =
                new TestProgramDataManagerOnCurrentProgramUpdatedListener();
        mClock.setCurrentTimeMillis(mClock.currentTimeMillis() + WAIT_TIME_OUT_MS);
        mProgramDataManager.addOnCurrentProgramUpdatedListener(testChannelId, listener);
        shadowOf(getMainLooper()).runToEndOfTasks();
        assertThat(
                        listener.currentProgramUpdatedLatch.await(
                                WAIT_TIME_OUT_MS, TimeUnit.MILLISECONDS))
                .isTrue();
        assertThat(listener.updatedChannelId).isEqualTo(testChannelId);
        Program currentProgram = mProgramDataManager.getCurrentProgram(testChannelId);
        assertProgramEquals(nextProgramStartTimeMs, nextProgramInfo, currentProgram);
        assertThat(currentProgram).isEqualTo(listener.updatedProgram);
    }

    /** Test if program data is refreshed after the program insertion. */
    @Test
    public void testContentProviderUpdate() throws InterruptedException {
        final long testChannelId = 1;
        startAndWaitForComplete();
        // Force program data manager to update program data whenever it's changes.
        mProgramDataManager.setProgramPrefetchUpdateWait(0);
        mCallback.reset();
        List<Program> programList =
                mProgramDataManager.getPrograms(testChannelId, mClock.currentTimeMillis());
        assertThat(programList).isNotNull();
        long lastProgramEndTime = programList.get(programList.size() - 1).getEndTimeUtcMillis();
        // Make change in content provider
        ProgramUtils.populatePrograms(
                RuntimeEnvironment.application,
                TvContract.buildChannelUri(testChannelId),
                ProgramInfo.create(),
                mClock,
                TimeUnit.DAYS.toMillis(2));
        shadowOf(getMainLooper()).runToEndOfTasks();
        assertThat(mCallback.programUpdatedLatch.await(WAIT_TIME_OUT_MS, TimeUnit.MILLISECONDS))
                .isTrue();
        programList = mProgramDataManager.getPrograms(testChannelId, mClock.currentTimeMillis());
        assertThat(lastProgramEndTime)
                .isLessThan(programList.get(programList.size() - 1).getEndTimeUtcMillis());
    }

    /** Test for {@link ProgramDataManager#setPauseProgramUpdate(boolean)}. */
    @Test
    public void testSetPauseProgramUpdate() throws InterruptedException {
        final long testChannelId = 1;
        startAndWaitForComplete();
        // Force program data manager to update program data whenever it's changes.
        mProgramDataManager.setProgramPrefetchUpdateWait(0);
        mCallback.reset();
        mProgramDataManager.setPauseProgramUpdate(true);
        ProgramUtils.populatePrograms(
                RuntimeEnvironment.application,
                TvContract.buildChannelUri(testChannelId),
                ProgramInfo.create(),
                mClock,
                TimeUnit.DAYS.toMillis(2));
        shadowOf(getMainLooper()).runToEndOfTasks();
        assertThat(mCallback.programUpdatedLatch.await(FAILURE_TIME_OUT_MS, TimeUnit.MILLISECONDS))
                .isFalse();
    }

    public static void assertProgramEquals(
            long expectedStartTime, ProgramInfo expectedInfo, Program actualProgram) {
        assertWithMessage("title").that(actualProgram.getTitle()).isEqualTo(expectedInfo.title);
        assertWithMessage("episode")
                .that(actualProgram.getEpisodeTitle())
                .isEqualTo(expectedInfo.episode);
        assertWithMessage("description")
                .that(actualProgram.getDescription())
                .isEqualTo(expectedInfo.description);
        assertWithMessage("startTime")
                .that(actualProgram.getStartTimeUtcMillis())
                .isEqualTo(expectedStartTime);
        assertWithMessage("endTime")
                .that(actualProgram.getEndTimeUtcMillis())
                .isEqualTo(expectedStartTime + expectedInfo.durationMs);
    }

    private static class TestProgramDataManagerCallback implements ProgramDataManager.Callback {
        public CountDownLatch programUpdatedLatch = new CountDownLatch(1);
        public CountDownLatch channelUpdatedLatch = new CountDownLatch(1);

        @Override
        public void onProgramUpdated() {
            programUpdatedLatch.countDown();
        }

        @Override
        public void onChannelUpdated() {
            channelUpdatedLatch.countDown();
        }

        public void reset() {
            programUpdatedLatch = new CountDownLatch(1);
            channelUpdatedLatch = new CountDownLatch(1);
        }
    }

    private static class TestProgramDataManagerOnCurrentProgramUpdatedListener
            implements OnCurrentProgramUpdatedListener {
        public final CountDownLatch currentProgramUpdatedLatch = new CountDownLatch(1);
        public long updatedChannelId = -1;
        public Program updatedProgram = null;

        @Override
        public void onCurrentProgramUpdated(long channelId, Program program) {
            updatedChannelId = channelId;
            updatedProgram = program;
            currentProgramUpdatedLatch.countDown();
        }
    }
}
