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

import android.media.ToneGenerator;
import android.telecom.DisconnectCause;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.SparseArray;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallAudioModeStateMachine;
import com.android.server.telecom.CallAudioModeStateMachine.MessageArgs;
import com.android.server.telecom.CallAudioRouteStateMachine;
import com.android.server.telecom.CallState;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.CallAudioManager;
import com.android.server.telecom.DtmfLocalTonePlayer;
import com.android.server.telecom.InCallTonePlayer;
import com.android.server.telecom.CallAudioModeStateMachine.MessageArgs.Builder;
import com.android.server.telecom.RingbackPlayer;
import com.android.server.telecom.Ringer;
import com.android.server.telecom.TelecomSystem;
import com.android.server.telecom.bluetooth.BluetoothStateReceiver;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

import java.util.LinkedHashSet;
import java.util.List;
import java.util.stream.Collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(JUnit4.class)
public class CallAudioManagerTest extends TelecomTestCase {
    @Mock private CallAudioRouteStateMachine mCallAudioRouteStateMachine;
    @Mock private CallsManager mCallsManager;
    @Mock private CallAudioModeStateMachine mCallAudioModeStateMachine;
    @Mock private InCallTonePlayer.Factory mPlayerFactory;
    @Mock private Ringer mRinger;
    @Mock private RingbackPlayer mRingbackPlayer;
    @Mock private DtmfLocalTonePlayer mDtmfLocalTonePlayer;
    @Mock private BluetoothStateReceiver mBluetoothStateReceiver;
    @Mock private TelecomSystem.SyncRoot mLock;

    private CallAudioManager mCallAudioManager;

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        doAnswer((invocation) -> {
            InCallTonePlayer mockInCallTonePlayer = mock(InCallTonePlayer.class);
            doAnswer((invocation2) -> {
                mCallAudioManager.setIsTonePlaying(true);
                return true;
            }).when(mockInCallTonePlayer).startTone();
            return mockInCallTonePlayer;
        }).when(mPlayerFactory).createPlayer(anyInt());
        when(mCallsManager.getLock()).thenReturn(mLock);
        mCallAudioManager = new CallAudioManager(
                mCallAudioRouteStateMachine,
                mCallsManager,
                mCallAudioModeStateMachine,
                mPlayerFactory,
                mRinger,
                mRingbackPlayer,
                mBluetoothStateReceiver,
                mDtmfLocalTonePlayer);
    }

    @Override
    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @MediumTest
    @Test
    public void testUnmuteOfSecondIncomingCall() {
        // Start with a single incoming call.
        Call call = createIncomingCall();
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.can(android.telecom.Call.Details.CAPABILITY_SPEED_UP_MT_AUDIO))
                .thenReturn(false);
        when(call.getId()).thenReturn("1");

        // Answer the incoming call
        mCallAudioManager.onIncomingCallAnswered(call);
        when(call.getState()).thenReturn(CallState.ACTIVE);
        mCallAudioManager.onCallStateChanged(call, CallState.RINGING, CallState.ACTIVE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_RINGING_CALLS), captor.capture());
        CallAudioModeStateMachine.MessageArgs correctArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(true)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setHasAudioProcessingCalls(false)
                        .setIsTonePlaying(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(correctArgs, captor.getValue());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        assertMessageArgEquality(correctArgs, captor.getValue());

        // Mute the current ongoing call.
        mCallAudioManager.mute(true);

        // Create a second incoming call.
        Call call2 = mock(Call.class);
        when(call2.getState()).thenReturn(CallState.RINGING);
        when(call2.can(android.telecom.Call.Details.CAPABILITY_SPEED_UP_MT_AUDIO))
                .thenReturn(false);
        when(call2.getId()).thenReturn("2");
        mCallAudioManager.onCallAdded(call2);

        // Answer the incoming call
        mCallAudioManager.onIncomingCallAnswered(call);

        // Capture the calls to sendMessageWithSessionInfo; we want to look for mute on and off
        // messages and make sure that there was a mute on before the mute off.
        ArgumentCaptor<Integer> muteCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mCallAudioRouteStateMachine, atLeastOnce())
                .sendMessageWithSessionInfo(muteCaptor.capture());
        List<Integer> values = muteCaptor.getAllValues();
        values = values.stream()
                .filter(value -> value == CallAudioRouteStateMachine.MUTE_ON ||
                        value == CallAudioRouteStateMachine.MUTE_OFF)
                .collect(Collectors.toList());

        // Make sure we got a mute on and a mute off.
        assertTrue(values.contains(CallAudioRouteStateMachine.MUTE_ON));
        assertTrue(values.contains(CallAudioRouteStateMachine.MUTE_OFF));
        // And that the mute on happened before the off.
        assertTrue(values.indexOf(CallAudioRouteStateMachine.MUTE_ON) < values
                .lastIndexOf(CallAudioRouteStateMachine.MUTE_OFF));
    }

    @MediumTest
    @Test
    public void testSingleIncomingCallFlowWithoutMTSpeedUp() {
        Call call = createIncomingCall();
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.can(android.telecom.Call.Details.CAPABILITY_SPEED_UP_MT_AUDIO))
                .thenReturn(false);

        // Answer the incoming call
        mCallAudioManager.onIncomingCallAnswered(call);
        when(call.getState()).thenReturn(CallState.ACTIVE);
        mCallAudioManager.onCallStateChanged(call, CallState.RINGING, CallState.ACTIVE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_RINGING_CALLS), captor.capture());
        CallAudioModeStateMachine.MessageArgs correctArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(true)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setHasAudioProcessingCalls(false)
                        .setIsTonePlaying(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(correctArgs, captor.getValue());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        assertMessageArgEquality(correctArgs, captor.getValue());

        disconnectCall(call);
        stopTone();

        mCallAudioManager.onCallRemoved(call);
        verifyProperCleanup();
    }

    @MediumTest
    @Test
    public void testSingleIncomingCallFlowWithMTSpeedUp() {
        Call call = createIncomingCall();
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.can(android.telecom.Call.Details.CAPABILITY_SPEED_UP_MT_AUDIO))
                .thenReturn(true);
        when(call.getState()).thenReturn(CallState.ANSWERED);

        // Answer the incoming call
        mCallAudioManager.onCallStateChanged(call, CallState.RINGING, CallState.ANSWERED);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_RINGING_CALLS), captor.capture());
        CallAudioModeStateMachine.MessageArgs correctArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(true)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setHasAudioProcessingCalls(false)
                        .setIsTonePlaying(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(correctArgs, captor.getValue());
        assertMessageArgEquality(correctArgs, captor.getValue());
        when(call.getState()).thenReturn(CallState.ACTIVE);
        mCallAudioManager.onCallStateChanged(call, CallState.ANSWERED, CallState.ACTIVE);

        disconnectCall(call);
        stopTone();

        mCallAudioManager.onCallRemoved(call);
        verifyProperCleanup();
    }

    @MediumTest
    @Test
    public void testSingleOutgoingCall() {
        Call call = mock(Call.class);
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.getState()).thenReturn(CallState.CONNECTING);

        mCallAudioManager.onCallAdded(call);
        assertEquals(call, mCallAudioManager.getForegroundCall());
        verify(mCallAudioRouteStateMachine).sendMessageWithSessionInfo(
                CallAudioRouteStateMachine.UPDATE_SYSTEM_AUDIO_ROUTE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        CallAudioModeStateMachine.MessageArgs expectedArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(true)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs, captor.getValue());

        when(call.getState()).thenReturn(CallState.DIALING);
        mCallAudioManager.onCallStateChanged(call, CallState.CONNECTING, CallState.DIALING);
        verify(mCallAudioModeStateMachine, times(2)).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
        verify(mCallAudioModeStateMachine, times(2)).sendMessageWithArgs(
                anyInt(), any(CallAudioModeStateMachine.MessageArgs.class));


        when(call.getState()).thenReturn(CallState.ACTIVE);
        mCallAudioManager.onCallStateChanged(call, CallState.DIALING, CallState.ACTIVE);
        verify(mCallAudioModeStateMachine, times(3)).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
        verify(mCallAudioModeStateMachine, times(3)).sendMessageWithArgs(
                anyInt(), any(CallAudioModeStateMachine.MessageArgs.class));

        disconnectCall(call);
        stopTone();

        mCallAudioManager.onCallRemoved(call);
        verifyProperCleanup();
    }

    @SmallTest
    @Test
    public void testNewCallGoesToAudioProcessing() {
        Call call = mock(Call.class);
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.getState()).thenReturn(CallState.NEW);

        // Make sure nothing happens when we add the NEW call
        mCallAudioManager.onCallAdded(call);

        verify(mCallAudioRouteStateMachine, never()).sendMessageWithSessionInfo(anyInt());
        verify(mCallAudioModeStateMachine, never()).sendMessageWithArgs(
                anyInt(), nullable(MessageArgs.class));

        // Transition the call to AUDIO_PROCESSING and see what happens
        when(call.getState()).thenReturn(CallState.AUDIO_PROCESSING);
        mCallAudioManager.onCallStateChanged(call, CallState.NEW, CallState.AUDIO_PROCESSING);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_AUDIO_PROCESSING_CALL), captor.capture());

        CallAudioModeStateMachine.MessageArgs expectedArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(true)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs, captor.getValue());
    }

    @SmallTest
    @Test
    public void testRingingCallGoesToAudioProcessing() {
        Call call = mock(Call.class);
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.getState()).thenReturn(CallState.RINGING);

        // Make sure appropriate messages are sent when we add a RINGING call
        mCallAudioManager.onCallAdded(call);

        assertEquals(call, mCallAudioManager.getForegroundCall());
        verify(mCallAudioRouteStateMachine).sendMessageWithSessionInfo(
                CallAudioRouteStateMachine.UPDATE_SYSTEM_AUDIO_ROUTE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_RINGING_CALL), captor.capture());
        CallAudioModeStateMachine.MessageArgs expectedArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(true)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs, captor.getValue());

        // Transition the call to AUDIO_PROCESSING and see what happens
        when(call.getState()).thenReturn(CallState.AUDIO_PROCESSING);
        mCallAudioManager.onCallStateChanged(call, CallState.RINGING, CallState.AUDIO_PROCESSING);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_AUDIO_PROCESSING_CALL), captor.capture());

        CallAudioModeStateMachine.MessageArgs expectedArgs2 =
                new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(true)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs2, captor.getValue());

        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_RINGING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs2, captor.getValue());
    }

    @SmallTest
    @Test
    public void testActiveCallGoesToAudioProcessing() {
        Call call = mock(Call.class);
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();
        when(call.getState()).thenReturn(CallState.ACTIVE);

        // Make sure appropriate messages are sent when we add an active call
        mCallAudioManager.onCallAdded(call);

        assertEquals(call, mCallAudioManager.getForegroundCall());
        verify(mCallAudioRouteStateMachine).sendMessageWithSessionInfo(
                CallAudioRouteStateMachine.UPDATE_SYSTEM_AUDIO_ROUTE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        CallAudioModeStateMachine.MessageArgs expectedArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(true)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs, captor.getValue());

        // Transition the call to AUDIO_PROCESSING and see what happens
        when(call.getState()).thenReturn(CallState.AUDIO_PROCESSING);
        mCallAudioManager.onCallStateChanged(call, CallState.ACTIVE, CallState.AUDIO_PROCESSING);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_AUDIO_PROCESSING_CALL), captor.capture());

        CallAudioModeStateMachine.MessageArgs expectedArgs2 =
                new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(true)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs2, captor.getValue());

        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_ACTIVE_OR_DIALING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs2, captor.getValue());
    }

    @SmallTest
    @Test
    public void testAudioProcessingCallDisconnects() {
        Call call = createAudioProcessingCall();
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        when(call.getState()).thenReturn(CallState.DISCONNECTED);
        when(call.getDisconnectCause()).thenReturn(new DisconnectCause(DisconnectCause.LOCAL,
                "", "", "", ToneGenerator.TONE_UNKNOWN));

        mCallAudioManager.onCallStateChanged(call, CallState.AUDIO_PROCESSING,
                CallState.DISCONNECTED);
        verify(mPlayerFactory, never()).createPlayer(anyInt());
        CallAudioModeStateMachine.MessageArgs expectedArgs2 = new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(false)
                .setHasAudioProcessingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_AUDIO_PROCESSING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs2, captor.getValue());
        verify(mCallAudioModeStateMachine, never()).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.TONE_STARTED_PLAYING), nullable(MessageArgs.class));

        mCallAudioManager.onCallRemoved(call);
        verifyProperCleanup();
    }

    @SmallTest
    @Test
    public void testAudioProcessingCallDoesSimulatedRing() {
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        Call call = createAudioProcessingCall();

        when(call.getState()).thenReturn(CallState.SIMULATED_RINGING);

        mCallAudioManager.onCallStateChanged(call, CallState.AUDIO_PROCESSING,
                CallState.SIMULATED_RINGING);
        verify(mPlayerFactory, never()).createPlayer(anyInt());
        CallAudioModeStateMachine.MessageArgs expectedArgs = new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(true)
                .setHasHoldingCalls(false)
                .setHasAudioProcessingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_AUDIO_PROCESSING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_RINGING_CALL), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
    }

    @SmallTest
    @Test
    public void testAudioProcessingCallGoesActive() {
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        Call call = createAudioProcessingCall();

        when(call.getState()).thenReturn(CallState.ACTIVE);

        mCallAudioManager.onCallStateChanged(call, CallState.AUDIO_PROCESSING,
                CallState.ACTIVE);
        verify(mPlayerFactory, never()).createPlayer(anyInt());
        CallAudioModeStateMachine.MessageArgs expectedArgs = new Builder()
                .setHasActiveOrDialingCalls(true)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(false)
                .setHasAudioProcessingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_AUDIO_PROCESSING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
    }

    @SmallTest
    @Test
    public void testSimulatedRingingCallGoesActive() {
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        Call call = createSimulatedRingingCall();

        when(call.getState()).thenReturn(CallState.ACTIVE);

        mCallAudioManager.onCallStateChanged(call, CallState.SIMULATED_RINGING,
                CallState.ACTIVE);
        verify(mPlayerFactory, never()).createPlayer(anyInt());
        CallAudioModeStateMachine.MessageArgs expectedArgs = new Builder()
                .setHasActiveOrDialingCalls(true)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(false)
                .setHasAudioProcessingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_RINGING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_ACTIVE_OR_DIALING_CALL), captor.capture());
        assertMessageArgEquality(expectedArgs, captor.getValue());
    }

    private Call createAudioProcessingCall() {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.AUDIO_PROCESSING);
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        // Set up an AUDIO_PROCESSING call
        mCallAudioManager.onCallAdded(call);

        assertNull(mCallAudioManager.getForegroundCall());

        verify(mCallAudioRouteStateMachine, never()).sendMessageWithSessionInfo(
                CallAudioRouteStateMachine.UPDATE_SYSTEM_AUDIO_ROUTE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_AUDIO_PROCESSING_CALL), captor.capture());
        CallAudioModeStateMachine.MessageArgs expectedArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(true)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs, captor.getValue());

        return call;
    }

    @SmallTest
    @Test
    public void testSimulatedRingingCallDisconnects() {
        Call call = createSimulatedRingingCall();
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        when(call.getState()).thenReturn(CallState.DISCONNECTED);
        when(call.getDisconnectCause()).thenReturn(new DisconnectCause(DisconnectCause.LOCAL,
                "", "", "", ToneGenerator.TONE_UNKNOWN));

        mCallAudioManager.onCallStateChanged(call, CallState.SIMULATED_RINGING,
                CallState.DISCONNECTED);
        verify(mPlayerFactory, never()).createPlayer(anyInt());
        CallAudioModeStateMachine.MessageArgs expectedArgs2 = new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(false)
                .setHasAudioProcessingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_RINGING_CALLS), captor.capture());
        assertMessageArgEquality(expectedArgs2, captor.getValue());
        verify(mCallAudioModeStateMachine, never()).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.TONE_STARTED_PLAYING), nullable(MessageArgs.class));

        mCallAudioManager.onCallRemoved(call);
        verifyProperCleanup();
    }

    private Call createSimulatedRingingCall() {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.SIMULATED_RINGING);
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor = makeNewCaptor();

        mCallAudioManager.onCallAdded(call);

        assertEquals(call, mCallAudioManager.getForegroundCall());

        verify(mCallAudioRouteStateMachine).sendMessageWithSessionInfo(
                CallAudioRouteStateMachine.UPDATE_SYSTEM_AUDIO_ROUTE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_RINGING_CALL), captor.capture());
        CallAudioModeStateMachine.MessageArgs expectedArgs =
                new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(true)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setHasAudioProcessingCalls(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        assertMessageArgEquality(expectedArgs, captor.getValue());

        return call;
    }

    private Call createIncomingCall() {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.RINGING);

        mCallAudioManager.onCallAdded(call);
        assertEquals(call, mCallAudioManager.getForegroundCall());
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor =
                ArgumentCaptor.forClass(CallAudioModeStateMachine.MessageArgs.class);
        verify(mCallAudioRouteStateMachine).sendMessageWithSessionInfo(
                CallAudioRouteStateMachine.UPDATE_SYSTEM_AUDIO_ROUTE);
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NEW_RINGING_CALL), captor.capture());
        assertMessageArgEquality(new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(true)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build(),
                captor.getValue());

        return call;
    }

    private void disconnectCall(Call call) {
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor =
                ArgumentCaptor.forClass(CallAudioModeStateMachine.MessageArgs.class);
        CallAudioModeStateMachine.MessageArgs correctArgs;

        when(call.getState()).thenReturn(CallState.DISCONNECTED);
        when(call.getDisconnectCause()).thenReturn(new DisconnectCause(DisconnectCause.LOCAL,
                "", "", "", ToneGenerator.TONE_PROP_PROMPT));

        mCallAudioManager.onCallStateChanged(call, CallState.ACTIVE, CallState.DISCONNECTED);
        verify(mPlayerFactory).createPlayer(InCallTonePlayer.TONE_CALL_ENDED);
        correctArgs = new Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(false)
                .setIsTonePlaying(true)
                .setForegroundCallIsVoip(false)
                .setSession(null)
                .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.NO_MORE_ACTIVE_OR_DIALING_CALLS), captor.capture());
        assertMessageArgEquality(correctArgs, captor.getValue());
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.TONE_STARTED_PLAYING), captor.capture());
        assertMessageArgEquality(correctArgs, captor.getValue());
    }

    private void stopTone() {
        ArgumentCaptor<CallAudioModeStateMachine.MessageArgs> captor =
                ArgumentCaptor.forClass(CallAudioModeStateMachine.MessageArgs.class);
        mCallAudioManager.setIsTonePlaying(false);
        CallAudioModeStateMachine.MessageArgs correctArgs = new Builder()
                        .setHasActiveOrDialingCalls(false)
                        .setHasRingingCalls(false)
                        .setHasHoldingCalls(false)
                        .setIsTonePlaying(false)
                        .setForegroundCallIsVoip(false)
                        .setSession(null)
                        .build();
        verify(mCallAudioModeStateMachine).sendMessageWithArgs(
                eq(CallAudioModeStateMachine.TONE_STOPPED_PLAYING), captor.capture());
        assertMessageArgEquality(correctArgs, captor.getValue());
    }

    private void verifyProperCleanup() {
        assertEquals(0, mCallAudioManager.getTrackedCalls().size());
        SparseArray<LinkedHashSet<Call>> callStateToCalls = mCallAudioManager.getCallStateToCalls();
        for (int i = 0; i < callStateToCalls.size(); i++) {
            assertEquals(0, callStateToCalls.valueAt(i).size());
        }
    }

    private ArgumentCaptor<MessageArgs> makeNewCaptor() {
        return ArgumentCaptor.forClass(CallAudioModeStateMachine.MessageArgs.class);
    }

    private void assertMessageArgEquality(CallAudioModeStateMachine.MessageArgs expected,
            CallAudioModeStateMachine.MessageArgs actual) {
        assertEquals(expected.hasActiveOrDialingCalls, actual.hasActiveOrDialingCalls);
        assertEquals(expected.hasHoldingCalls, actual.hasHoldingCalls);
        assertEquals(expected.hasRingingCalls, actual.hasRingingCalls);
        assertEquals(expected.isTonePlaying, actual.isTonePlaying);
        assertEquals(expected.foregroundCallIsVoip, actual.foregroundCallIsVoip);
    }
}
