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

package android.telecom.cts;

import android.net.Uri;
import android.os.Bundle;
import android.provider.CallLog;
import android.telecom.Call;
import android.telecom.Connection;

import java.util.concurrent.CountDownLatch;

public class EmergencyCallTests extends BaseTelecomTestWithMockServices {

    @Override
    public void setUp() throws Exception {
        // Sets up this package as default dialer in super.
        super.setUp();
        NewOutgoingCallBroadcastReceiver.reset();
        if (!mShouldTestTelecom) return;
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        setupForEmergencyCalling(TEST_EMERGENCY_NUMBER);
    }

    /**
     * Place an outgoing emergency call and ensure it is started successfully.
     */
    public void testStartEmergencyCall() throws Exception {
        if (!mShouldTestTelecom) return;
        placeAndVerifyEmergencyCall(true /*supportsHold*/);
        Call eCall = getInCallService().getLastCall();
        assertCallState(eCall, Call.STATE_DIALING);

        assertIsInCall(true);
        assertIsInManagedCall(true);
    }

    /**
     * Place an outgoing emergency call and ensure any incoming call is rejected automatically and
     * logged in call log as a new missed call.
     *
     * Note: PSAPs have requirements that active emergency calls can not be put on hold, so if for
     * some reason an incoming emergency callback happens while on another emergency call, that call
     * will automatically be rejected as well.
     */
    public void testOngoingEmergencyCallAndReceiveIncomingCall() throws Exception {
        if (!mShouldTestTelecom) return;

        Connection eConnection = placeAndVerifyEmergencyCall(true /*supportsHold*/);
        assertIsInCall(true);
        assertIsInManagedCall(true);
        Call eCall = getInCallService().getLastCall();
        assertCallState(eCall, Call.STATE_DIALING);
        eConnection.setActive();
        assertCallState(eCall, Call.STATE_ACTIVE);

        Uri normalCallNumber = createRandomTestNumber();
        addAndVerifyNewFailedIncomingCall(normalCallNumber, null);
        assertCallState(eCall, Call.STATE_ACTIVE);

        // Notify as missed instead of rejected, since the user did not explicitly reject.
        verifyCallLogging(normalCallNumber, CallLog.Calls.MISSED_TYPE);
    }

    /**
     * Receive an incoming ringing call and place an emergency call. The ringing call should be
     * rejected and logged as a new missed call.
     */
    public void testIncomingRingingCallAndPlaceEmergencyCall() throws Exception {
        if (!mShouldTestTelecom) return;

        Uri normalCallNumber = createRandomTestNumber();
        addAndVerifyNewIncomingCall(normalCallNumber, null);
        Connection incomingConnection = verifyConnectionForIncomingCall();
        Call incomingCall = getInCallService().getLastCall();
        assertCallState(incomingCall, Call.STATE_RINGING);

        // Do not support holding incoming call for emergency call.
        Connection eConnection = placeAndVerifyEmergencyCall(false /*supportsHold*/);
        Call eCall = getInCallService().getLastCall();
        assertCallState(eCall, Call.STATE_DIALING);

        assertConnectionState(incomingConnection, Connection.STATE_DISCONNECTED);
        assertCallState(incomingCall, Call.STATE_DISCONNECTED);

        eConnection.setActive();
        assertCallState(eCall, Call.STATE_ACTIVE);

        // Notify as missed instead of rejected, since the user did not explicitly reject.
        verifyCallLogging(normalCallNumber, CallLog.Calls.MISSED_TYPE);
    }

    /**
     * While on an outgoing call, receive an incoming ringing call and then place an emergency call.
     * The other two calls should stay active while the ringing call should be rejected and logged
     * as a new missed call.
     */
    public void testActiveCallAndIncomingRingingCallAndPlaceEmergencyCall() throws Exception {
        if (!mShouldTestTelecom) return;

        Uri normalOutgoingCallNumber = createRandomTestNumber();
        Bundle extras = new Bundle();
        extras.putParcelable(TestUtils.EXTRA_PHONE_NUMBER, normalOutgoingCallNumber);
        placeAndVerifyCall(extras);
        Connection outgoingConnection = verifyConnectionForOutgoingCall();
        Call outgoingCall = getInCallService().getLastCall();
        outgoingConnection.setActive();
        assertCallState(outgoingCall, Call.STATE_ACTIVE);

        Uri normalIncomingCallNumber = createRandomTestNumber();
        addAndVerifyNewIncomingCall(normalIncomingCallNumber, null);
        Connection incomingConnection = verifyConnectionForIncomingCall();
        Call incomingCall = getInCallService().getLastCall();
        assertCallState(incomingCall, Call.STATE_RINGING);

        // Do not support holding incoming call for emergency call.
        Connection eConnection = placeAndVerifyEmergencyCall(false /*supportsHold*/);
        Call eCall = getInCallService().getLastCall();
        assertCallState(eCall, Call.STATE_DIALING);

        assertConnectionState(incomingConnection, Connection.STATE_DISCONNECTED);
        assertCallState(incomingCall, Call.STATE_DISCONNECTED);

        eConnection.setActive();
        assertCallState(eCall, Call.STATE_ACTIVE);

        // Notify as missed instead of rejected, since the user did not explicitly reject.
        verifyCallLogging(normalIncomingCallNumber, CallLog.Calls.MISSED_TYPE);
    }
}
