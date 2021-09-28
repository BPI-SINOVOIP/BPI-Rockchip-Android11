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

package android.telecom.cts;

import android.content.Context;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.telecom.CallAudioState;
import android.telecom.Connection;
import android.telecom.TelecomManager;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * Verifies the behavior of Telecom during various outgoing call flows.
 */
public class OutgoingCallTest extends BaseTelecomTestWithMockServices {
    private static final long STATE_CHANGE_DELAY = 1000;

    private static final String TEST_EMERGENCY_NUMBER = "9998887776655443210";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        NewOutgoingCallBroadcastReceiver.reset();
        if (mShouldTestTelecom) {
            setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        TestUtils.clearSystemDialerOverride(getInstrumentation());
        TestUtils.removeTestEmergencyNumber(getInstrumentation(), TEST_EMERGENCY_NUMBER);
    }

    /* TODO: Need to send some commands to the UserManager via adb to do setup
    public void testDisallowOutgoingCallsForSecondaryUser() {
    } */

    /* TODO: Need to figure out a way to mock emergency calls without adb root
    public void testOutgoingCallBroadcast_isSentForAllCalls() {
    } */

    /**
     * Verifies that providing the EXTRA_START_CALL_WITH_SPEAKERPHONE extra starts the call with
     * speakerphone automatically enabled.
     *
     * @see {@link TelecomManager#EXTRA_START_CALL_WITH_SPEAKERPHONE}
     */
    public void testStartCallWithSpeakerphoneTrue_SpeakerphoneOnInCall() {
        if (!mShouldTestTelecom) {
            return;
        }

        final Bundle extras = new Bundle();
        extras.putBoolean(TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE, true);
        placeAndVerifyCall(extras);
        verifyConnectionForOutgoingCall();
        assertAudioRoute(mInCallCallbacks.getService(), CallAudioState.ROUTE_SPEAKER);
    }

    public void testStartCallWithSpeakerphoneFalse_SpeakerphoneOffInCall() {
        if (!mShouldTestTelecom) {
            return;
        }

        final Bundle extras = new Bundle();
        extras.putBoolean(TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE, false);
        placeAndVerifyCall(extras);
        verifyConnectionForOutgoingCall();
        if (mInCallCallbacks.getService().getCallAudioState().getSupportedRouteMask() ==
                CallAudioState.ROUTE_SPEAKER) {
            return; // Do not verify if the only available route is speaker.
        }
        assertNotAudioRoute(mInCallCallbacks.getService(), CallAudioState.ROUTE_SPEAKER);
    }

    public void testStartCallWithSpeakerphoneNotProvided_SpeakerphoneOffByDefault() {
        if (!mShouldTestTelecom) {
            return;
        }

        AudioManager am = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();
        if (mInCallCallbacks.getService().getCallAudioState().getSupportedRouteMask() ==
                CallAudioState.ROUTE_SPEAKER) {
            return; // Do not verify if the only available route is speaker.
        }
        assertNotAudioRoute(mInCallCallbacks.getService(), CallAudioState.ROUTE_SPEAKER);
    }

    public void testPhoneStateListenerInvokedOnOutgoingEmergencyCall() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        TestUtils.setSystemDialerOverride(getInstrumentation());
        TestUtils.addTestEmergencyNumber(getInstrumentation(), TEST_EMERGENCY_NUMBER);
        mTelecomManager.placeCall(Uri.fromParts("tel", TEST_EMERGENCY_NUMBER, null), null);
        verifyPhoneStateListenerCallbacksForEmergencyCall(TEST_EMERGENCY_NUMBER);
    }

    public void testPhoneStateListenerInvokedOnOutgoingCall() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();
        String expectedNumber = connectionService.outgoingConnections.get(0)
                .getAddress().getSchemeSpecificPart();
        verifyPhoneStateListenerCallbacksForCall(TelephonyManager.CALL_STATE_OFFHOOK,
                expectedNumber);
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
        final Bundle extras = new Bundle();
        extras.putBoolean(TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE, true);
        CountDownLatch count = new CountDownLatch(1);
        Executor executor = (Runnable command)->count.countDown();
        PhoneStateListener listener = new PhoneStateListener(executor);
        mTelephonyManager.listen(listener, PhoneStateListener.LISTEN_CALL_STATE);


        placeAndVerifyCall(extras);
        verifyConnectionForOutgoingCall();
        count.await(TestUtils.WAIT_FOR_PHONE_STATE_LISTENER_REGISTERED_TIMEOUT_S,
                TimeUnit.SECONDS);
        Thread.sleep(STATE_CHANGE_DELAY);
        assertEquals(TelephonyManager.CALL_STATE_OFFHOOK, mTelephonyManager.getCallState());
    }

    /**
     * Ensure that {@link TelecomManager#EXTRA_PHONE_ACCOUNT_HANDLE} is taken account when placing
     * an outgoing call.
     * @throws Exception
     */
    public void testExtraPhoneAccountHandleAvailable() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        final Bundle extras1 = new Bundle();
        final Bundle extras2 = new Bundle();
        extras1.putParcelable(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE,
                TestUtils.TEST_PHONE_ACCOUNT_HANDLE);
        extras2.putParcelable(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE,
                TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2);

        mTelecomManager.registerPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT);
        TestUtils.enablePhoneAccount(
                getInstrumentation(), TestUtils.TEST_PHONE_ACCOUNT_HANDLE);
        placeAndVerifyCall(extras1);
        Connection conn = verifyConnectionForOutgoingCall();
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, conn.getPhoneAccountHandle());

        cleanupCalls();
        assertCtsConnectionServiceUnbound();
        CtsConnectionService.tearDown();
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);

        mTelecomManager.registerPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT_2);
        TestUtils.enablePhoneAccount(
                getInstrumentation(), TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2);
        placeAndVerifyCall(extras2);
        conn = verifyConnectionForOutgoingCall();
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2, conn.getPhoneAccountHandle());
        conn.onDisconnect();
    }
}
