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

package com.android.server.telecom.tests;

import android.media.AudioManager;
import android.os.HandlerThread;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.server.telecom.CallAudioManager;
import com.android.server.telecom.CallAudioModeStateMachine;
import com.android.server.telecom.CallAudioRouteStateMachine;
import com.android.server.telecom.CallAudioModeStateMachine.MessageArgs.Builder;
import com.android.server.telecom.SystemStateHelper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(JUnit4.class)
public class CallAudioModeStateMachineTest extends TelecomTestCase {
    private static final int TEST_TIMEOUT = 1000;

    @Mock private SystemStateHelper mSystemStateHelper;
    @Mock private AudioManager mAudioManager;
    @Mock private CallAudioManager mCallAudioManager;

    private HandlerThread mTestThread;

    @Override
    @Before
    public void setUp() throws Exception {
        mTestThread = new HandlerThread("CallAudioModeStateMachineTest");
        mTestThread.start();
        super.setUp();
    }

    @Override
    @After
    public void tearDown() throws Exception {
        mTestThread.quit();
        mTestThread.join();
        super.tearDown();
    }

    @SmallTest
    @Test
    public void testNoFocusWhenRingerSilenced() throws Throwable {
        CallAudioModeStateMachine sm = new CallAudioModeStateMachine(mSystemStateHelper,
                mAudioManager, mTestThread.getLooper());
        sm.setCallAudioManager(mCallAudioManager);
        sm.sendMessage(CallAudioModeStateMachine.ABANDON_FOCUS_FOR_TESTING);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        resetMocks();
        when(mCallAudioManager.startRinging()).thenReturn(false);

        sm.sendMessage(CallAudioModeStateMachine.NEW_RINGING_CALL, new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(true)
                .setHasHoldingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build());
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        assertEquals(CallAudioModeStateMachine.RING_STATE_NAME, sm.getCurrentStateName());

        verify(mAudioManager, never()).requestAudioFocusForCall(anyInt(), anyInt());
        verify(mAudioManager, never()).setMode(anyInt());

        verify(mCallAudioManager, never()).stopRinging();

        verify(mCallAudioManager).stopCallWaiting();
    }

    @SmallTest
    @Test
    public void testNoRingWhenDeviceIsAtEar() {
        CallAudioModeStateMachine sm = new CallAudioModeStateMachine(mSystemStateHelper,
                mAudioManager, mTestThread.getLooper());
        sm.setCallAudioManager(mCallAudioManager);
        sm.sendMessage(CallAudioModeStateMachine.ABANDON_FOCUS_FOR_TESTING);
        sm.sendMessage(CallAudioModeStateMachine.NEW_HOLDING_CALL, new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(true)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build());
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);
        assertEquals(CallAudioModeStateMachine.TONE_HOLD_STATE_NAME, sm.getCurrentStateName());
        when(mSystemStateHelper.isDeviceAtEar()).thenReturn(true);

        resetMocks();
        sm.sendMessage(CallAudioModeStateMachine.NEW_RINGING_CALL, new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(true)
                .setHasHoldingCalls(true)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build());
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        verify(mAudioManager, never()).requestAudioFocusForCall(anyInt(), anyInt());
        verify(mAudioManager, never()).setMode(anyInt());
        verify(mCallAudioManager, never()).startRinging();
        verify(mCallAudioManager).startCallWaiting(nullable(String.class));
    }

    @SmallTest
    @Test
    public void testRegainFocusWhenHfpIsConnectedSilenced() throws Throwable {
        CallAudioModeStateMachine sm = new CallAudioModeStateMachine(mSystemStateHelper,
                mAudioManager, mTestThread.getLooper());
        sm.setCallAudioManager(mCallAudioManager);
        sm.sendMessage(CallAudioModeStateMachine.ABANDON_FOCUS_FOR_TESTING);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        resetMocks();
        when(mCallAudioManager.startRinging()).thenReturn(false);

        sm.sendMessage(CallAudioModeStateMachine.NEW_RINGING_CALL, new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(true)
                .setHasHoldingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build());
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        assertEquals(CallAudioModeStateMachine.RING_STATE_NAME, sm.getCurrentStateName());

        verify(mAudioManager, never()).requestAudioFocusForCall(anyInt(), anyInt());
        verify(mAudioManager, never()).setMode(anyInt());

        verify(mCallAudioManager, never()).stopRinging();

        verify(mCallAudioManager).stopCallWaiting();

        when(mCallAudioManager.startRinging()).thenReturn(true);

        sm.sendMessage(CallAudioModeStateMachine.RINGER_MODE_CHANGE);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        verify(mCallAudioManager, times(2)).startRinging();
        verify(mAudioManager).requestAudioFocusForCall(AudioManager.STREAM_RING,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        verify(mAudioManager).setMode(AudioManager.MODE_RINGTONE);
        verify(mCallAudioManager).setCallAudioRouteFocusState(
                CallAudioRouteStateMachine.RINGING_FOCUS);
    }

    @SmallTest
    @Test
    public void testDoNotRingTwiceWhenHfpConnected() {
        CallAudioModeStateMachine sm = new CallAudioModeStateMachine(mSystemStateHelper,
                mAudioManager, mTestThread.getLooper());
        sm.setCallAudioManager(mCallAudioManager);
        sm.sendMessage(CallAudioModeStateMachine.ABANDON_FOCUS_FOR_TESTING);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        resetMocks();
        when(mCallAudioManager.startRinging()).thenReturn(true);

        sm.sendMessage(CallAudioModeStateMachine.NEW_RINGING_CALL, new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(true)
                .setHasHoldingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build());
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        assertEquals(CallAudioModeStateMachine.RING_STATE_NAME, sm.getCurrentStateName());

        verify(mAudioManager).requestAudioFocusForCall(AudioManager.STREAM_RING,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        verify(mAudioManager).setMode(AudioManager.MODE_RINGTONE);
        verify(mCallAudioManager).setCallAudioRouteFocusState(
                CallAudioRouteStateMachine.RINGING_FOCUS);

        when(mCallAudioManager.isRingtonePlaying()).thenReturn(true);
        sm.sendMessage(CallAudioModeStateMachine.RINGER_MODE_CHANGE);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        // Make sure we don't try and start ringing again.
        verify(mCallAudioManager, times(1)).startRinging();
    }

    @SmallTest
    @Test
    public void testStartRingingAfterHfpConnectedIfNotAlreadyPlaying() {
        CallAudioModeStateMachine sm = new CallAudioModeStateMachine(mSystemStateHelper,
                mAudioManager, mTestThread.getLooper());
        sm.setCallAudioManager(mCallAudioManager);
        sm.sendMessage(CallAudioModeStateMachine.ABANDON_FOCUS_FOR_TESTING);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        resetMocks();
        when(mCallAudioManager.startRinging()).thenReturn(true);

        sm.sendMessage(CallAudioModeStateMachine.NEW_RINGING_CALL, new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(true)
                .setHasHoldingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build());
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        assertEquals(CallAudioModeStateMachine.RING_STATE_NAME, sm.getCurrentStateName());

        verify(mAudioManager).requestAudioFocusForCall(AudioManager.STREAM_RING,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        verify(mAudioManager).setMode(AudioManager.MODE_RINGTONE);
        verify(mCallAudioManager).setCallAudioRouteFocusState(
                CallAudioRouteStateMachine.RINGING_FOCUS);

        when(mCallAudioManager.isRingtonePlaying()).thenReturn(false);
        sm.sendMessage(CallAudioModeStateMachine.RINGER_MODE_CHANGE);
        waitForHandlerAction(sm.getHandler(), TEST_TIMEOUT);

        // Make sure we do try and start ringing again, since the ringtone wasn't already playing.
        verify(mCallAudioManager, times(2)).startRinging();
    }

    private void resetMocks() {
        clearInvocations(mCallAudioManager, mAudioManager);
    }
}
