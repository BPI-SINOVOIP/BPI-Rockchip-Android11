/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.dialer.ui.activecall;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertArrayEquals;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import android.app.Application;
import android.content.Context;
import android.net.Uri;
import android.telecom.Call;
import android.telecom.CallAudioState;
import android.telecom.DisconnectCause;
import android.telecom.GatewayInfo;

import androidx.core.util.Pair;

import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.TestDialerApplication;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.telecom.InCallServiceImpl;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.telephony.common.CallDetail;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(CarDialerRobolectricTestRunner.class)
public class InCallViewModelTest {
    private static final String NUMBER = "6505551234";
    private static final long CONNECT_TIME_MILLIS = 500000000;
    private static final CharSequence LABEL = "DisconnectCause";
    private static final Uri GATEWAY_ADDRESS = Uri.fromParts("tel", NUMBER, null);

    private InCallViewModel mInCallViewModel;
    private List<Call> mListForMockCalls;

    @Mock
    private InCallServiceImpl mInCallService;

    @Mock
    private UiCallManager mMockUiCallManager;
    @Mock
    private Call mMockActiveCall;
    @Mock
    private Call mMockDialingCall;
    @Mock
    private Call mMockHoldingCall;
    @Mock
    private Call mMockRingingCall;
    @Mock
    private Call.Details mMockDetails;
    @Captor
    private ArgumentCaptor<Call.Callback> mCallbackCaptor;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        Context context = RuntimeEnvironment.application;

        ((TestDialerApplication) context).setupInCallServiceImpl(mInCallService);
        UiBluetoothMonitor.init(context);

        when(mMockActiveCall.getState()).thenReturn(Call.STATE_ACTIVE);
        when(mMockDialingCall.getState()).thenReturn(Call.STATE_DIALING);
        when(mMockHoldingCall.getState()).thenReturn(Call.STATE_HOLDING);
        when(mMockRingingCall.getState()).thenReturn(Call.STATE_RINGING);
        doNothing().when(mMockActiveCall).registerCallback(mCallbackCaptor.capture());

        // Set up call details
        GatewayInfo gatewayInfo = new GatewayInfo("", GATEWAY_ADDRESS, GATEWAY_ADDRESS);
        DisconnectCause disconnectCause = new DisconnectCause(1, LABEL, null, "");
        when(mMockDetails.getHandle()).thenReturn(GATEWAY_ADDRESS);
        when(mMockDetails.getDisconnectCause()).thenReturn(disconnectCause);
        when(mMockDetails.getGatewayInfo()).thenReturn(gatewayInfo);
        when(mMockDetails.getConnectTimeMillis()).thenReturn(CONNECT_TIME_MILLIS);

        when(mMockDialingCall.getDetails()).thenReturn(mMockDetails);

        mListForMockCalls = new ArrayList<>();
        mListForMockCalls.add(mMockActiveCall);
        mListForMockCalls.add(mMockDialingCall);
        mListForMockCalls.add(mMockHoldingCall);
        mListForMockCalls.add(mMockRingingCall);
        when(mInCallService.getCalls()).thenReturn(mListForMockCalls);
        UiCallManager.set(mMockUiCallManager);
        when(mMockUiCallManager.getAudioRoute()).thenReturn(CallAudioState.ROUTE_BLUETOOTH);

        mInCallViewModel = new InCallViewModel((Application) context);
        mInCallViewModel.getIncomingCall().observeForever(s -> { });
        mInCallViewModel.getOngoingCallList().observeForever(s -> { });
        mInCallViewModel.getPrimaryCall().observeForever(s -> { });
        mInCallViewModel.getPrimaryCallState().observeForever(s -> { });
        mInCallViewModel.getPrimaryCallDetail().observeForever(s -> { });
        mInCallViewModel.getCallStateAndConnectTime().observeForever(s -> { });
        mInCallViewModel.getAudioRoute().observeForever(s -> { });
    }

    @After
    public void tearDown() {
        UiBluetoothMonitor.get().tearDown();
        UiCallManager.set(null);
    }

    @Test
    public void testGetCallList() {
        List<Call> callListInOrder =
                Arrays.asList(mMockDialingCall, mMockActiveCall, mMockHoldingCall);
        List<Call> viewModelCallList = mInCallViewModel.getOngoingCallList().getValue();
        assertArrayEquals(callListInOrder.toArray(), viewModelCallList.toArray());
    }

    @Test
    public void testStateChange_triggerCallListUpdate() {
        Call.Callback callback = mCallbackCaptor.getValue();
        assertThat(callback).isNotNull();

        when(mMockActiveCall.getState()).thenReturn(Call.STATE_HOLDING);
        when(mMockHoldingCall.getState()).thenReturn(Call.STATE_ACTIVE);
        callback.onStateChanged(mMockActiveCall, Call.STATE_HOLDING);

        List<Call> callListInOrder =
                Arrays.asList(mMockDialingCall, mMockHoldingCall, mMockActiveCall);
        List<Call> viewModelCallList = mInCallViewModel.getOngoingCallList().getValue();
        assertArrayEquals(callListInOrder.toArray(), viewModelCallList.toArray());
    }

    @Test
    public void testGetIncomingCall() {
        Call incomingCall = mInCallViewModel.getIncomingCall().getValue();
        assertThat(incomingCall).isEqualTo(mMockRingingCall);
    }

    @Test
    public void testGetPrimaryCall() {
        assertThat(mInCallViewModel.getPrimaryCall().getValue()).isEqualTo(mMockDialingCall);
    }

    @Test
    public void testGetPrimaryCallState() {
        assertThat(mInCallViewModel.getPrimaryCallState().getValue()).isEqualTo(Call.STATE_DIALING);
    }

    @Test
    public void testGetPrimaryCallDetail() {
        CallDetail callDetail = mInCallViewModel.getPrimaryCallDetail().getValue();
        assertThat(callDetail.getNumber()).isEqualTo(NUMBER);
        assertThat(callDetail.getConnectTimeMillis()).isEqualTo(CONNECT_TIME_MILLIS);
        assertThat(callDetail.getDisconnectCause()).isEqualTo(LABEL);
        assertThat(callDetail.getGatewayInfoOriginalAddress()).isEqualTo(GATEWAY_ADDRESS);
    }

    @Test
    public void testGetCallStateAndConnectTime() {
        Pair<Integer, Long> pair = mInCallViewModel.getCallStateAndConnectTime().getValue();
        assertThat(pair.first).isEqualTo(Call.STATE_DIALING);
        assertThat(pair.second).isEqualTo(CONNECT_TIME_MILLIS);
    }

    @Test
    public void testGetAudioRoute() {
        assertThat(mInCallViewModel.getAudioRoute().getValue())
                .isEqualTo(CallAudioState.ROUTE_BLUETOOTH);
    }
}
