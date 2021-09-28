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

import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_SIM_START_PACKET;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.eap.EapSessionConfig;
import android.telephony.TelephonyManager;

import com.android.internal.net.eap.statemachine.EapStateMachine;

import org.junit.Before;
import org.junit.Test;

/**
 * This test verifies that EAP-AKA is functional for an end-to-end implementation.
 *
 * <p>This test uses externally generated test vectors.
 */
public class EapAkaTest extends EapMethodEndToEndTest {
    private static final long AUTHENTICATOR_TIMEOUT_MILLIS = 250L;

    private static final int SUB_ID = 1;
    private static final String UNFORMATTED_IDENTITY = "123456789012345"; // IMSI

    // EAP_IDENTITY = hex("test@android.net")
    private static final byte[] EAP_IDENTITY =
            hexStringToByteArray("7465737440616E64726F69642E6E6574");

    // TODO(b/140797965): find valid AUTN/RAND values for the CTS test sim
    // CK: 070AC6E26957E00C83A4B577210A8AEC
    // IK: 0B48923D40B48C476B0EE8A43F780356
    // MK: B8F5AC1C7A5F5B8D5579A6F22FD18E4EEB0B55C3
    // K_encr: 1C2B848ADA2B9485C52517D1A92BF4AB
    // K_aut: C9500EC59DC62C7D7F5E9E445FA1A3C4
    private static final String RAND_1 = "A789798E3E75560EA3EA5E41A043753A";
    private static final String RAND_2 = "000102030405060708090A0B0C0D0E0F";
    private static final String AUTN = "236C38DD8772000165A96F168F5C14E2";
    private static final String RES = "2B1C2593F5AF288A3766CADA9CE23FB9";
    private static final String AUTS = "0102030405060708090A0B0C0D0E";
    private static final byte[] MSK =
            hexStringToByteArray(
                    "40B9767F5D333645D3B72E9E57EFFA9D"
                            + "C9D1E7EB598D907948DADB5AD968D520"
                            + "48F0C56A0D68E37E9482F77BC8F68990"
                            + "5EAB7114ECA9FC4AB99D0920D76CF1CF");
    private static final byte[] EMSK =
            hexStringToByteArray(
                    "F3463F6ADD56EE5360C81DEF017FD57D"
                            + "CBABF12EA45D2CC815F4334AB7BE8523"
                            + "4FEAF76FAA02EEF963B25C9DC95308AF"
                            + "72A937FF04FF2C6D1F172FD9111411EE");

    // CK: 070AC6E26957E00C83A4B577210A8AEC
    // IK: 0B48923D40B48C476B0EE8A43F780356
    // MK: B8F5AC1C7A5F5B8D5579A6F22FD18E4EEB0B55C3
    // K_encr: 1F6AB828DD3CF1931E6E62038728C47C
    // K_aut: F035D5CA7512389F4F96454EA6737B7A
    private static final byte[] MSK_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "111E1F3118B373F0658230785066F767"
                            + "A238BF780BC77CA77DA3F7A3FE411BE8"
                            + "0FCD763DE7FBCADB7EB9BDEF7F0E7B1D"
                            + "2D5CA87C38AC0D3B8F99B882ECB4E840");
    private static final byte[] EMSK_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "CD1DD12100B99BC029B0C97777C42BD6"
                            + "2FF24A32694D7EA209E3B3DBCC7B9CE9"
                            + "EFD69A3A62CBDBDFA1CECD0F17B0E620"
                            + "D648AA41D55AE0375BAD44C8F1A424BE");

    // Base 64 of: [Length][RAND_1][Length][AUTN]
    private static final String BASE64_CHALLENGE_1 =
            "EKeJeY4+dVYOo+peQaBDdToQI2w43YdyAAFlqW8Wj1wU4g==";

    // Base 64 of: ['DB'][Length][RES][Length][CK][Length][IK]
    private static final String BASE_64_RESPONSE_SUCCESS =
            "2xArHCWT9a8oijdmytqc4j+5EAcKxuJpV+AMg6S1dyEKiuwQC0iSPUC0jEdrDuikP3gDVg==";

    // Base 64 of: [Length][RAND_2][Length][AUTN]
    private static final String BASE64_CHALLENGE_2 =
            "EAABAgMEBQYHCAkKCwwNDg8QI2w43YdyAAFlqW8Wj1wU4g==";

    // Base 64 of: ['DC'][Length][AUTS]
    private static final String BASE_64_RESPONSE_SYNC_FAIL = "3A4BAgMEBQYHCAkKCwwNDg==";

    private static final String REQUEST_MAC = "8E733917F65C54EE820195EFB94246DC";
    private static final String RESPONSE_MAC = "E5A74AE0840C1E75E0C471D5A915AB7F";
    private static final String REQUEST_MAC_WITHOUT_IDENTITY_REQ =
            "B66F674A14D96D358E8682230DF86253";
    private static final String RESPONSE_MAC_WITHOUT_IDENTITY_REQ =
            "631BEABB876F072B49D453B660FAE748";

    private static final byte[] EAP_AKA_IDENTITY_REQUEST =
            hexStringToByteArray(
                    "01CD000C" // EAP-Request | ID | length in bytes
                            + "17050000" // EAP-AKA | Identity | 2B padding
                            + "0D010000"); // AT_ANY_ID_REQ attribute
    private static final byte[] EAP_AKA_IDENTITY_RESPONSE =
            hexStringToByteArray(
                    "02CD001C" // EAP-Response | ID | length in bytes
                            + "17050000" // EAP-AKA | Identity | 2B padding
                            + "0E05001030313233343536373839303132333435"); // AT_IDENTITY attribute

    private static final byte[] EAP_AKA_CHALLENGE_REQUEST =
            hexStringToByteArray(
                    "01CE0044" // EAP-Request | ID | length in bytes
                            + "17010000" // EAP-AKA | Challenge | 2B padding
                            + "01050000" + RAND_1 // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "0B050000" + REQUEST_MAC); // AT_MAC attribute
    private static final byte[] EAP_AKA_CHALLENGE_RESPONSE =
            hexStringToByteArray(
                    "02CE0030" // EAP-Response | ID | length in bytes
                            + "17010000" // EAP-AKA | Challenge | 2B padding
                            + "03050080" + RES // AT_RES attribute
                            + "0B050000" + RESPONSE_MAC); // AT_MAC attribute

    private static final byte[] EAP_AKA_CHALLENGE_REQUEST_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "019B0044" // EAP-Request | ID | length in bytes
                            + "17010000" // EAP-AKA | Challenge | 2B padding
                            + "01050000" + RAND_1 // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "0B050000" + REQUEST_MAC_WITHOUT_IDENTITY_REQ); // AT_MAC attribute
    private static final byte[] EAP_AKA_CHALLENGE_RESPONSE_WITHOUT_IDENTITY_REQUEST =
            hexStringToByteArray(
                    "029B0030" // EAP-Response | ID | length in bytes
                            + "17010000" // EAP-AKA | Challenge | 2B padding
                            + "03050080" + RES // AT_RES attribute
                            + "0B050000" + RESPONSE_MAC_WITHOUT_IDENTITY_REQ); // AT_MAC attribute

    private static final byte[] EAP_AKA_CHALLENGE_REQUEST_SYNC_FAIL =
            hexStringToByteArray(
                    "01CE0044" // EAP-Request | ID | length in bytes
                            + "17010000" // EAP-AKA | Challenge | 2B padding
                            + "01050000" + RAND_2 // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "0B050000" + REQUEST_MAC); // AT_MAC attribute
    private static final byte[] EAP_AKA_SYNC_FAIL_RESPONSE =
            hexStringToByteArray(
                    "02CE0018" // EAP-Response | ID | length in bytes
                            + "17040000" // EAP-AKA | Synchronization-Failure | 2B padding
                            + "0404" + AUTS);  // AT_AUTS attribute

    private static final byte[] EAP_AKA_AUTHENTICATION_REJECT =
            hexStringToByteArray(
                    "02CE0008" // EAP-Response | ID | length in bytes
                            + "17020000"); // EAP-AKA | Authentication-Reject | 2B padding

    private static final byte[] EAP_RESPONSE_NAK_PACKET =
            hexStringToByteArray("021000060317"); // NAK with EAP-AKA listed

    private TelephonyManager mMockTelephonyManager;

    @Before
    @Override
    public void setUp() {
        super.setUp();

        mMockTelephonyManager = mock(TelephonyManager.class);

        mEapSessionConfig =
                new EapSessionConfig.Builder()
                        .setEapIdentity(EAP_IDENTITY)
                        .setEapAkaConfig(SUB_ID, APPTYPE_USIM)
                        .build();
        mEapAuthenticator =
                new EapAuthenticator(
                        mTestLooper.getLooper(),
                        mMockCallback,
                        new EapStateMachine(mMockContext, mEapSessionConfig, mMockSecureRandom),
                        (runnable) -> runnable.run(),
                        AUTHENTICATOR_TIMEOUT_MILLIS);

        TelephonyManager mockTelephonyManagerFromContext = mock(TelephonyManager.class);
        doReturn(mockTelephonyManagerFromContext)
                .when(mMockContext)
                .getSystemService(Context.TELEPHONY_SERVICE);
        doReturn(mMockTelephonyManager)
                .when(mockTelephonyManagerFromContext)
                .createForSubscriptionId(SUB_ID);
    }

    @Test
    public void testEapAkaEndToEnd() {
        verifyEapAkaIdentity();
        verifyEapAkaChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaEndToEndWithoutIdentityRequest() {
        verifyEapAkaChallengeWithoutIdentityReq();
        verifyEapSuccess(MSK_WITHOUT_IDENTITY_REQ, EMSK_WITHOUT_IDENTITY_REQ);
    }

    @Test
    public void testEapAkaWithEapNotifications() {
        verifyEapNotification(1);
        verifyEapAkaIdentity();

        verifyEapNotification(2);
        verifyEapAkaChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_CHALLENGE_RESPONSE);

        verifyEapNotification(3);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaUnsupportedType() {
        verifyUnsupportedType(EAP_REQUEST_SIM_START_PACKET, EAP_RESPONSE_NAK_PACKET);

        verifyEapAkaIdentity();
        verifyEapAkaChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaSynchronizationFailure() {
        verifyEapAkaIdentity();
        verifyEapAkaSynchronizationFailure();
        verifyEapAkaChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaAuthenticationReject() {
        verifyEapAkaIdentity();

        // return null from TelephonyManager to simluate rejection of AUTN
        verifyEapAkaChallenge(null, EAP_AKA_AUTHENTICATION_REJECT);
        verifyExpectsEapFailure(EAP_AKA_CHALLENGE_REQUEST);
        verifyEapFailure();
    }

    private void verifyEapAkaIdentity() {
        // EAP-AKA/Identity request
        doReturn(UNFORMATTED_IDENTITY).when(mMockTelephonyManager).getSubscriberId();

        mEapAuthenticator.processEapMessage(EAP_AKA_IDENTITY_REQUEST);
        mTestLooper.dispatchAll();

        // verify EAP-AKA/Identity response
        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));
        verify(mMockTelephonyManager).getSubscriberId();
        verify(mMockCallback).onResponse(eq(EAP_AKA_IDENTITY_RESPONSE));
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaChallenge(
            String challengeBase64,
            String responseBase64,
            byte[] incomingEapPacket,
            byte[] outgoingEapPacket) {
        // EAP-AKA/Challenge request
        when(mMockTelephonyManager.getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA,
                        challengeBase64))
                .thenReturn(responseBase64);

        mEapAuthenticator.processEapMessage(incomingEapPacket);
        mTestLooper.dispatchAll();

        // verify EAP-AKA/Challenge response
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA,
                        challengeBase64);
        verify(mMockCallback).onResponse(eq(outgoingEapPacket));
    }

    private void verifyEapAkaChallenge(String responseBase64, byte[] outgoingPacket) {
        verifyEapAkaChallenge(
                BASE64_CHALLENGE_1, responseBase64, EAP_AKA_CHALLENGE_REQUEST, outgoingPacket);
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaChallengeWithoutIdentityReq() {
        verifyEapAkaChallenge(
                BASE64_CHALLENGE_1,
                BASE_64_RESPONSE_SUCCESS,
                EAP_AKA_CHALLENGE_REQUEST_WITHOUT_IDENTITY_REQ,
                EAP_AKA_CHALLENGE_RESPONSE_WITHOUT_IDENTITY_REQUEST);

        // also need to verify interactions with Context and TelephonyManager
        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaSynchronizationFailure() {
        verifyEapAkaChallenge(
                BASE64_CHALLENGE_2,
                BASE_64_RESPONSE_SYNC_FAIL,
                EAP_AKA_CHALLENGE_REQUEST_SYNC_FAIL,
                EAP_AKA_SYNC_FAIL_RESPONSE);
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    @Override
    protected void verifyEapSuccess(byte[] msk, byte[] emsk) {
        super.verifyEapSuccess(msk, emsk);

        verifyNoMoreInteractions(mMockTelephonyManager);
    }
}
