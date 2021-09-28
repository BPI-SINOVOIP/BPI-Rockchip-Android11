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

package com.android.server.telecom.tests;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.ComponentName;
import android.net.Uri;
import android.telecom.Connection;
import android.telecom.DisconnectCause;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.test.suitebuilder.annotation.SmallTest;
import android.widget.Toast;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallState;
import com.android.server.telecom.CallerInfoLookupHelper;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.ClockProxy;
import com.android.server.telecom.ConnectionServiceWrapper;
import com.android.server.telecom.PhoneAccountRegistrar;
import com.android.server.telecom.PhoneNumberUtilsAdapter;
import com.android.server.telecom.TelecomSystem;
import com.android.server.telecom.ui.ToastFactory;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

@RunWith(AndroidJUnit4.class)
public class CallTest extends TelecomTestCase {
    private static final Uri TEST_ADDRESS = Uri.parse("tel:555-1212");
    private static final PhoneAccountHandle SIM_1_HANDLE = new PhoneAccountHandle(
            ComponentName.unflattenFromString("com.foo/.Blah"), "Sim1");
    private static final PhoneAccount SIM_1_ACCOUNT = new PhoneAccount.Builder(SIM_1_HANDLE, "Sim1")
            .setCapabilities(PhoneAccount.CAPABILITY_SIM_SUBSCRIPTION
                    | PhoneAccount.CAPABILITY_CALL_PROVIDER)
            .setIsEnabled(true)
            .build();

    @Mock private CallsManager mMockCallsManager;
    @Mock private CallerInfoLookupHelper mMockCallerInfoLookupHelper;
    @Mock private PhoneAccountRegistrar mMockPhoneAccountRegistrar;
    @Mock private ClockProxy mMockClockProxy;
    @Mock private ToastFactory mMockToastProxy;
    @Mock private Toast mMockToast;
    @Mock private PhoneNumberUtilsAdapter mMockPhoneNumberUtilsAdapter;
    @Mock private ConnectionServiceWrapper mMockConnectionService;

    private final TelecomSystem.SyncRoot mLock = new TelecomSystem.SyncRoot() { };

    @Before
    public void setUp() throws Exception {
        super.setUp();
        doReturn(mMockCallerInfoLookupHelper).when(mMockCallsManager).getCallerInfoLookupHelper();
        doReturn(mMockPhoneAccountRegistrar).when(mMockCallsManager).getPhoneAccountRegistrar();
        doReturn(SIM_1_ACCOUNT).when(mMockPhoneAccountRegistrar).getPhoneAccountUnchecked(
                eq(SIM_1_HANDLE));
        doReturn(new ComponentName(mContext, CallTest.class))
                .when(mMockConnectionService).getComponentName();
        doReturn(mMockToast).when(mMockToastProxy).makeText(any(), anyInt(), anyInt());
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    @SmallTest
    public void testSetHasGoneActive() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);

        assertFalse(call.hasGoneActiveBefore());
        call.setState(CallState.ACTIVE, "");
        assertTrue(call.hasGoneActiveBefore());
        call.setState(CallState.AUDIO_PROCESSING, "");
        assertTrue(call.hasGoneActiveBefore());
    }

    @Test
    @SmallTest
    public void testDisconnectCauseWhenAudioProcessing() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        call.setState(CallState.AUDIO_PROCESSING, "");
        call.disconnect();
        call.setDisconnectCause(new DisconnectCause(DisconnectCause.LOCAL));
        assertEquals(DisconnectCause.REJECTED, call.getDisconnectCause().getCode());
    }

    @Test
    @SmallTest
    public void testDisconnectCauseWhenAudioProcessingAfterActive() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        call.setState(CallState.AUDIO_PROCESSING, "");
        call.setState(CallState.ACTIVE, "");
        call.setState(CallState.AUDIO_PROCESSING, "");
        call.disconnect();
        call.setDisconnectCause(new DisconnectCause(DisconnectCause.LOCAL));
        assertEquals(DisconnectCause.LOCAL, call.getDisconnectCause().getCode());
    }

    @Test
    @SmallTest
    public void testDisconnectCauseWhenSimulatedRingingAndDisconnect() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        call.setState(CallState.SIMULATED_RINGING, "");
        call.disconnect();
        call.setDisconnectCause(new DisconnectCause(DisconnectCause.LOCAL));
        assertEquals(DisconnectCause.MISSED, call.getDisconnectCause().getCode());
    }

    @Test
    @SmallTest
    public void testDisconnectCauseWhenSimulatedRingingAndReject() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        call.setState(CallState.SIMULATED_RINGING, "");
        call.reject(false, "");
        call.setDisconnectCause(new DisconnectCause(DisconnectCause.LOCAL));
        assertEquals(DisconnectCause.REJECTED, call.getDisconnectCause().getCode());
    }

    @Test
    @SmallTest
    public void testCanPullCallRemovedDuringEmergencyCall() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        boolean[] hasCalledConnectionCapabilitiesChanged = new boolean[1];
        call.addListener(new Call.ListenerBase() {
            @Override
            public void onConnectionCapabilitiesChanged(Call call) {
                hasCalledConnectionCapabilitiesChanged[0] = true;
            }
        });
        call.setConnectionService(mMockConnectionService);
        call.setConnectionProperties(Connection.PROPERTY_IS_EXTERNAL_CALL);
        call.setConnectionCapabilities(Connection.CAPABILITY_CAN_PULL_CALL);
        call.setState(CallState.ACTIVE, "");
        assertTrue(hasCalledConnectionCapabilitiesChanged[0]);
        // Capability should be present
        assertTrue((call.getConnectionCapabilities() | Connection.CAPABILITY_CAN_PULL_CALL) > 0);
        hasCalledConnectionCapabilitiesChanged[0] = false;
        // Emergency call in progress
        call.setIsPullExternalCallSupported(false /*isPullCallSupported*/);
        assertTrue(hasCalledConnectionCapabilitiesChanged[0]);
        // Capability should not be present
        assertEquals(0, call.getConnectionCapabilities() & Connection.CAPABILITY_CAN_PULL_CALL);
        hasCalledConnectionCapabilitiesChanged[0] = false;
        // Emergency call complete
        call.setIsPullExternalCallSupported(true /*isPullCallSupported*/);
        assertTrue(hasCalledConnectionCapabilitiesChanged[0]);
        // Capability should be present
        assertEquals(Connection.CAPABILITY_CAN_PULL_CALL,
                call.getConnectionCapabilities() & Connection.CAPABILITY_CAN_PULL_CALL);
    }

    @Test
    @SmallTest
    public void testCanNotPullCallDuringEmergencyCall() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_INCOMING,
                false /* shouldAttachToExistingConnection*/,
                false /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        call.setConnectionService(mMockConnectionService);
        call.setConnectionProperties(Connection.PROPERTY_IS_EXTERNAL_CALL);
        call.setConnectionCapabilities(Connection.CAPABILITY_CAN_PULL_CALL);
        call.setState(CallState.ACTIVE, "");
        // Emergency call in progress, this should show a toast and never call pullExternalCall
        // on the ConnectionService.
        doReturn(true).when(mMockCallsManager).isInEmergencyCall();
        call.pullExternalCall();
        verify(mMockConnectionService, never()).pullExternalCall(any());
        verify(mMockToast).show();
    }

    @Test
    @SmallTest
    public void testCallDirection() {
        Call call = new Call(
                "1", /* callId */
                mContext,
                mMockCallsManager,
                mLock,
                null /* ConnectionServiceRepository */,
                mMockPhoneNumberUtilsAdapter,
                TEST_ADDRESS,
                null /* GatewayInfo */,
                null /* connectionManagerPhoneAccountHandle */,
                SIM_1_HANDLE,
                Call.CALL_DIRECTION_UNDEFINED,
                false /* shouldAttachToExistingConnection*/,
                true /* isConference */,
                mMockClockProxy,
                mMockToastProxy);
        boolean[] hasCallDirectionChanged = new boolean[1];
        call.addListener(new Call.ListenerBase() {
            @Override
            public void onCallDirectionChanged(Call call) {
                hasCallDirectionChanged[0] = true;
            }
        });
        assertFalse(call.isIncoming());
        call.setCallDirection(Call.CALL_DIRECTION_INCOMING);
        assertTrue(hasCallDirectionChanged[0]);
        assertTrue(call.isIncoming());
    }
}
