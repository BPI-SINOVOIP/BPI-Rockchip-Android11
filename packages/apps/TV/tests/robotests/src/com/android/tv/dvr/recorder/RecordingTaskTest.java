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
 * limitations under the License
 */

package com.android.tv.dvr.recorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import com.android.tv.InputSessionManager;
import com.android.tv.InputSessionManager.RecordingSession;
import com.android.tv.common.feature.CommonFeatures;
import com.android.tv.common.feature.TestableFeature;
import com.android.tv.data.ChannelImpl;
import com.android.tv.data.api.Channel;
import com.android.tv.dvr.DvrManager;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.dvr.recorder.RecordingTask.State;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.dvr.DvrDataManagerInMemoryImpl;
import com.android.tv.testing.dvr.RecordingTestUtils;
import com.android.tv.testing.fakes.FakeClock;
import java.util.concurrent.TimeUnit;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Tests for {@link RecordingTask}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.N, application = TestSingletonApp.class)
public class RecordingTaskTest {
    private static final long DURATION = TimeUnit.MINUTES.toMillis(30);
    private static final long START_OFFSET_MS = RecordingScheduler.MS_TO_WAKE_BEFORE_START;
    private static final String INPUT_ID = "input_id";
    private static final int CHANNEL_ID = 273;

    private FakeClock mFakeClock;
    private DvrDataManagerInMemoryImpl mDataManager;
    @Mock Handler mMockHandler;
    @Mock DvrManager mDvrManager;
    @Mock InputSessionManager mMockSessionManager;
    @Mock RecordingSession mMockRecordingSession;
    private final TestableFeature mDvrFeature = CommonFeatures.DVR;

    @Before
    public void setUp() {
        mDvrFeature.enableForTest();
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        MockitoAnnotations.initMocks(this);
        mFakeClock = FakeClock.createWithCurrentTime();
        mDataManager = new DvrDataManagerInMemoryImpl(RuntimeEnvironment.application, mFakeClock);
    }

    @After
    public void tearDown() {
        mDvrFeature.resetForTests();
    }

    @Test
    public void testHandle_init() {
        Channel channel = createTestChannel();
        ScheduledRecording r = createRecording(channel);
        RecordingTask task = createRecordingTask(r, channel);
        String inputId = channel.getInputId();
        when(mMockSessionManager.createRecordingSession(
                        eq(inputId), anyString(), eq(task), eq(mMockHandler), anyLong()))
                .thenReturn(mMockRecordingSession);
        when(mMockHandler.sendMessageAtTime(any(), anyLong())).thenReturn(true);
        assertTrue(task.handleMessage(createMessage(RecordingTask.MSG_INITIALIZE)));
        assertEquals(State.CONNECTION_PENDING, task.getState());
        verify(mMockSessionManager)
                .createRecordingSession(
                        eq(inputId), anyString(), eq(task), eq(mMockHandler), anyLong());
        verify(mMockRecordingSession).tune(eq(inputId), eq(channel.getUri()));
        verifyNoMoreInteractions(mMockHandler, mMockRecordingSession, mMockSessionManager);
    }

    private static Channel createTestChannel() {
        return new ChannelImpl.Builder()
                .setInputId(INPUT_ID)
                .setId(CHANNEL_ID)
                .setDisplayName("Test Ch " + CHANNEL_ID)
                .build();
    }

    @Test
    public void testOnConnected() {
        Channel channel = createTestChannel();
        ScheduledRecording r = createRecording(channel);
        mDataManager.addScheduledRecording(r);
        RecordingTask task = createRecordingTask(r, channel);
        String inputId = channel.getInputId();
        when(mMockSessionManager.createRecordingSession(
                        eq(inputId), anyString(), eq(task), eq(mMockHandler), anyLong()))
                .thenReturn(mMockRecordingSession);
        when(mMockHandler.sendEmptyMessageDelayed(anyInt(), anyLong())).thenReturn(true);
        task.handleMessage(createMessage(RecordingTask.MSG_INITIALIZE));
        task.onTuned(channel.getUri());
        assertEquals(State.CONNECTED, task.getState());
    }

    private ScheduledRecording createRecording(Channel c) {
        long startTime = mFakeClock.currentTimeMillis() + START_OFFSET_MS;
        long endTime = startTime + DURATION;
        return RecordingTestUtils.createTestRecordingWithPeriod(
                c.getInputId(), c.getId(), startTime, endTime);
    }

    private RecordingTask createRecordingTask(ScheduledRecording r, Channel channel) {
        RecordingTask recordingTask =
                new RecordingTask(
                        RuntimeEnvironment.application,
                        r,
                        channel,
                        mDvrManager,
                        mMockSessionManager,
                        mDataManager,
                        mFakeClock);
        recordingTask.setHandler(mMockHandler);
        return recordingTask;
    }

    private Message createMessage(int what) {
        Message msg = new Message();
        msg.setTarget(mMockHandler);
        msg.what = what;
        return msg;
    }
}
