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

package android.telecom.cts;

import static android.telecom.cts.TestUtils.COMPONENT;
import static android.telecom.cts.TestUtils.PACKAGE;
import static android.telephony.TelephonyManager.CALL_STATE_RINGING;

import android.content.ComponentName;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Telephony;
import android.telecom.Call;
import android.telecom.Connection;
import android.telecom.ConnectionRequest;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.PhoneStateListener;

import java.util.Collection;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * Tests valid/invalid incoming calls that are received from the ConnectionService
 * and registered through TelecomManager
 */
public class IncomingCallTest extends BaseTelecomTestWithMockServices {
    private static final long STATE_CHANGE_DELAY = 1000;

    private static final PhoneAccountHandle TEST_INVALID_HANDLE = new PhoneAccountHandle(
            new ComponentName(PACKAGE, COMPONENT), "WRONG_ID");

    public void testVerstatPassed() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        addAndVerifyNewIncomingCall(MockConnectionService.VERSTAT_PASSED_NUMBER, null);
        verifyConnectionForIncomingCall();

        Call call = mInCallCallbacks.getService().getLastCall();
        assertEquals(Connection.VERIFICATION_STATUS_PASSED,
                call.getDetails().getCallerNumberVerificationStatus());
    }

    public void testVerstatFailed() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        addAndVerifyNewIncomingCall(MockConnectionService.VERSTAT_FAILED_NUMBER, null);
        verifyConnectionForIncomingCall();

        Call call = mInCallCallbacks.getService().getLastCall();
        assertEquals(Connection.VERIFICATION_STATUS_FAILED,
                call.getDetails().getCallerNumberVerificationStatus());
    }

    public void testVerstatNotVerified() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        addAndVerifyNewIncomingCall(MockConnectionService.VERSTAT_NOT_VERIFIED_NUMBER, null);
        verifyConnectionForIncomingCall();

        Call call = mInCallCallbacks.getService().getLastCall();
        assertEquals(Connection.VERIFICATION_STATUS_NOT_VERIFIED,
                call.getDetails().getCallerNumberVerificationStatus());
    }

    public void testAddNewIncomingCall_CorrectPhoneAccountHandle() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        addAndVerifyNewIncomingCall(createTestNumber(), null);
        final Connection connection3 = verifyConnectionForIncomingCall();
        Collection<Connection> connections = CtsConnectionService.getAllConnectionsFromTelecom();
        assertEquals(1, connections.size());
        assertTrue(connections.contains(connection3));
    }

    public void testPhoneStateListenerInvokedOnIncomingCall() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        Uri testNumber = createTestNumber();
        addAndVerifyNewIncomingCall(testNumber, null);
        verifyConnectionForIncomingCall();
        verifyPhoneStateListenerCallbacksForCall(CALL_STATE_RINGING,
                testNumber.getSchemeSpecificPart());
    }

    /**
     * Tests to be sure that new incoming calls can only be added using a valid PhoneAccountHandle
     * (b/26864502). If a PhoneAccount has not been registered for the PhoneAccountHandle, then
     * a SecurityException will be thrown.
     */
    public void testAddNewIncomingCall_IncorrectPhoneAccountHandle() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, createTestNumber());
        try {
            mTelecomManager.addNewIncomingCall(TEST_INVALID_HANDLE, extras);
            fail();
        } catch (SecurityException e) {
            // This should create a security exception!
        }

        assertFalse(CtsConnectionService.isServiceRegisteredToTelecom());
    }

    /**
     * Tests to be sure that new incoming calls can only be added if a PhoneAccount is enabled
     * (b/26864502). If a PhoneAccount is not enabled for the PhoneAccountHandle, then
     * a SecurityException will be thrown.
     */
    public void testAddNewIncomingCall_PhoneAccountNotEnabled() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        // Do not enable PhoneAccount
        setupConnectionService(null, FLAG_REGISTER);
        assertFalse(mTelecomManager.getPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT_HANDLE)
                .isEnabled());
        try {
            addAndVerifyNewIncomingCall(createTestNumber(), null);
            fail();
        } catch (SecurityException e) {
            // This should create a security exception!
        }

        assertFalse(CtsConnectionService.isServiceRegisteredToTelecom());
    }

    /**
     * Ensure {@link Call.Details#PROPERTY_VOIP_AUDIO_MODE} is set for a ringing call which uses
     * voip audio mode.
     * @throws Exception
     */
    public void testAddNewIncomingCallVoipState() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(new MockConnectionService() {
            @Override
            public Connection onCreateIncomingConnection(
                    PhoneAccountHandle connectionManagerPhoneAccount,
                    ConnectionRequest request) {
                Connection connection = super.onCreateIncomingConnection(
                        connectionManagerPhoneAccount,
                        request);
                connection.setAudioModeIsVoip(true);
                lock.release();
                return connection;
            }
        }, FLAG_REGISTER | FLAG_ENABLE);
        addAndVerifyNewIncomingCall(createTestNumber(), null);
        verifyConnectionForIncomingCall();

        assertTrue((mInCallCallbacks.getService().getLastCall().getDetails().getCallProperties()
                & Call.Details.PROPERTY_VOIP_AUDIO_MODE) != 0);
    }

    /**
     * Ensure the {@link android.telephony.PhoneStateListener#onCallStateChanged(int, String)}
     * called in an expected way and phone state is correct.
     * @throws Exception
     */
    public void testPhoneStateChangeAsExpected() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        Uri testNumber = createTestNumber();
        addAndVerifyNewIncomingCall(testNumber, null);

        CountDownLatch count = new CountDownLatch(1);
        Executor executor = (Runnable command)->count.countDown();
        PhoneStateListener listener = new PhoneStateListener(executor);
        mTelephonyManager.listen(listener, PhoneStateListener.LISTEN_CALL_STATE);

        count.await(TestUtils.WAIT_FOR_PHONE_STATE_LISTENER_REGISTERED_TIMEOUT_S,
                TimeUnit.SECONDS);

        Thread.sleep(STATE_CHANGE_DELAY);
        assertEquals(CALL_STATE_RINGING, mTelephonyManager.getCallState());
    }
}
