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

package com.android.internal.net.eap;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_AKA_IDENTITY_PACKET;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.net.eap.EapSessionConfig;

import com.android.internal.net.eap.statemachine.EapStateMachine;

import org.junit.Before;
import org.junit.Test;

public class EapMsChapV2Test extends EapMethodEndToEndTest {
    private static final long AUTHENTICATOR_TIMEOUT_MILLIS = 250L;

    private static final String USERNAME = "User";
    private static final String PASSWORD = "clientPass";

    private static final byte[] PEER_CHALLENGE =
            hexStringToByteArray("21402324255E262A28295F2B3A337C7E");
    private static final byte[] MSK =
            hexStringToByteArray(
                    "D5F0E9521E3EA9589645E86051C822268B7CDC149B993A1BA118CB153F56DCCB");

    // Server-Name = hex("authenticator@android.net")
    private static final byte[] EAP_MSCHAP_V2_CHALLENGE_REQUEST =
            hexStringToByteArray("01110033" // EAP-Request | ID | length in bytes
                    + "1A0142" // EAP-MSCHAPv2 | Request | MSCHAPv2 ID
                    + "002E10" // MS length | Value Size (0x10)
                    + "5B5D7C7D7B3F2F3E3C2C602132262628" // Authenticator-Challenge
                    + "61757468656E74696361746F7240616E64726F69642E6E6574"); // Server-Name
    private static final byte[] EAP_MSCHAP_V2_CHALLENGE_RESPONSE =
            hexStringToByteArray("0211003F" // EAP-Response | ID | length in bytes
                    + "1A0242" // EAP-MSCHAPv2 | Response | MSCHAPv2 ID
                    + "003A31" // MS length | Value Size (0x31)
                    + "21402324255E262A28295F2B3A337C7E" // Peer-Challenge
                    + "0000000000000000" // 8B (reserved)
                    + "82309ECD8D708B5EA08FAA3981CD83544233114A3D85D6DF" // NT-Response
                    + "00" // Flags
                    + "55736572"); // hex(USERNAME)
    private static final byte[] EAP_MSCHAP_V2_SUCCESS_REQUEST =
            hexStringToByteArray("01120047" // EAP-Request | ID | length in bytes
                    + "1A03420042" // EAP-MSCHAPv2 | Success | MSCHAPv2 ID | MS length
                    + "533D" // hex("S=")
                    + "3430374135353839313135464430443632303946"
                            + "3531304645394330343536363933324344413536" // hex("<auth_string>")
                    + "204D3D" // hex(" M=")
                            + "7465737420416E64726F69642031323334"); // hex("test Android 1234")
    private static final byte[] EAP_MSCHAP_V2_SUCCESS_RESPONSE =
            hexStringToByteArray("02120006" // EAP-Response | ID | length in bytes
                    + "1A03"); // EAP-MSCHAPv2 | Success
    private static final byte[] EAP_MSCHAP_V2_FAILURE_REQUEST =
            hexStringToByteArray("01130049" // EAP-Request | ID | length in bytes
                    + "1A04420044" // EAP-MSCHAPv2 | Failure | MSCHAPv2 ID | MS length
                    + "453D363437" // hex("E=647")
                    + "20523D31" // hex(" R=1")
                    + "20433D" // hex(" C=")
                            + "30303031303230333034303530363037"
                            + "30383039304130423043304430453046" // hex("<authenticator challenge>")
                    + "20563D33" // hex(" V=3")
                    + "204D3D" // hex(" M=")
                    + "7465737420416E64726F69642031323334"); // hex("test Android 1234")
    private static final byte[] EAP_MSCHAP_V2_FAILURE_RESPONSE =
            hexStringToByteArray("02130006" // EAP-Response | ID | length in bytes
                    + "1A04"); // EAP-MSCHAPv2 | Failure

    private static final byte[] EAP_RESPONSE_NAK_PACKET = hexStringToByteArray("02100006031A");

    @Before
    @Override
    public void setUp() {
        super.setUp();

        mEapSessionConfig =
                new EapSessionConfig.Builder().setEapMsChapV2Config(USERNAME, PASSWORD).build();
        mEapAuthenticator =
                new EapAuthenticator(
                        mTestLooper.getLooper(),
                        mMockCallback,
                        new EapStateMachine(mMockContext, mEapSessionConfig, mMockSecureRandom),
                        (runnable) -> runnable.run(),
                        AUTHENTICATOR_TIMEOUT_MILLIS);
    }

    @Test
    public void testEapMsChapV2EndToEndSuccess() {
        verifyEapMsChapV2Challenge();
        verifyEapMsChapV2SuccessRequest();
        verifyEapSuccess(MSK, new byte[0]);
    }

    @Test
    public void testEapMsChapV2EndToEndFailure() {
        verifyEapMsChapV2Challenge();
        verifyEapMsChapV2FailureRequest();
        verifyEapFailure();
    }

    @Test
    public void testEapMsChapV2UnsupportedType() {
        verifyUnsupportedType(EAP_REQUEST_AKA_IDENTITY_PACKET, EAP_RESPONSE_NAK_PACKET);

        verifyEapMsChapV2Challenge();
        verifyEapMsChapV2SuccessRequest();
        verifyEapSuccess(MSK, new byte[0]);
    }

    @Test
    public void verifyEapMsChapV2WithEapNotifications() {
        verifyEapNotification(1);

        verifyEapMsChapV2Challenge();
        verifyEapNotification(2);

        verifyEapMsChapV2SuccessRequest();
        verifyEapNotification(3);

        verifyEapSuccess(MSK, new byte[0]);
    }

    private void verifyEapMsChapV2Challenge() {
        doAnswer(invocation -> {
            byte[] dst = invocation.getArgument(0);
            System.arraycopy(PEER_CHALLENGE, 0, dst, 0, PEER_CHALLENGE.length);
            return null;
        }).when(mMockSecureRandom).nextBytes(eq(new byte[PEER_CHALLENGE.length]));

        mEapAuthenticator.processEapMessage(EAP_MSCHAP_V2_CHALLENGE_REQUEST);
        mTestLooper.dispatchAll();

        verify(mMockCallback).onResponse(eq(EAP_MSCHAP_V2_CHALLENGE_RESPONSE));
        verify(mMockSecureRandom).nextBytes(any(byte[].class));
        verifyNoMoreInteractions(mMockCallback);
    }

    private void verifyEapMsChapV2SuccessRequest() {
        mEapAuthenticator.processEapMessage(EAP_MSCHAP_V2_SUCCESS_REQUEST);
        mTestLooper.dispatchAll();

        verify(mMockCallback).onResponse(eq(EAP_MSCHAP_V2_SUCCESS_RESPONSE));
        verifyNoMoreInteractions(mMockCallback);
    }

    private void verifyEapMsChapV2FailureRequest() {
        mEapAuthenticator.processEapMessage(EAP_MSCHAP_V2_FAILURE_REQUEST);
        mTestLooper.dispatchAll();

        verify(mMockCallback).onResponse(eq(EAP_MSCHAP_V2_FAILURE_RESPONSE));
        verifyNoMoreInteractions(mMockCallback);
    }
}
