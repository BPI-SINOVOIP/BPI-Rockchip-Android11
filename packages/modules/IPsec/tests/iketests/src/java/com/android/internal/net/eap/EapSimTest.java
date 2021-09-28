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
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_AKA_IDENTITY_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NAK_PACKET;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
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
 * This test verifies that EAP-SIM is functional for an end-to-end implementation
 */
public class EapSimTest extends EapMethodEndToEndTest {
    private static final long AUTHENTICATOR_TIMEOUT_MILLIS = 250L;

    private static final byte[] NONCE = hexStringToByteArray("37f3ddd3954c4831a5ee08c574844398");
    private static final String UNFORMATTED_IDENTITY = "123456789ABCDEF"; // IMSI

    // EAP_IDENTITY = hex("test@android.net")
    private static final byte[] EAP_IDENTITY =
            hexStringToByteArray("7465737440616E64726F69642E6E6574");

    private static final int SUB_ID = 1;

    // Base 64 of: RAND
    private static final String BASE64_RAND_1 = "EAEjRWeJq83vESNFZ4mrze8=";
    private static final String BASE64_RAND_2 = "EBEjRWeJq83vESNFZ4mrze8=";
    private static final String BASE64_RAND_3 = "ECEjRWeJq83vESNFZ4mrze8=";

    // BASE 64 of: "04" + SRES + "08" + KC
    // SRES 1: 0ABCDEF0      KC 1: FEDCBA9876543210
    // SRES 2: 1ABCDEF1      KC 2: FEDCBA9876543211
    // SRES 3: 2ABCDEF2      KC 3: FEDCBA9876543212
    private static final String BASE64_RESP_1 = "BAq83vAI/ty6mHZUMhA=";
    private static final String BASE64_RESP_2 = "BBq83vEI/ty6mHZUMhE=";
    private static final String BASE64_RESP_3 = "BCq83vII/ty6mHZUMhI=";

    // MK: 202FC68A3335E8A939A33BC0A0EA8C435DC10060
    // K_encr: F63E152461391FF655C2632E35D076ED
    // K_aut: 48E001C8DBA37120FD0465153A56F712
    private static final byte[] MSK =
            hexStringToByteArray(
                    "9B1E2B6892BC113F6B6D0B5789DD8ADD"
                            + "B83BE2A84AA50FCAECD0003F92D8DA16"
                            + "4BF983C923695C309F1D7D68DB6992B0"
                            + "76EA8CE7129647A6F198F3A6AA8ADED9");
    private static final byte[] EMSK = hexStringToByteArray(
            "88210b6724400313539c740f417076b0"
                    + "41da7e64658ec365bd2901a7cd7c2763"
                    + "dad1a0508b92a42fdf85ac53c6f7e756"
                    + "7f99b62bcaf467441b567f19b58d86ae");

    // MK: ED275A588A4C1AEC15C55261DCCD851189E5C5FD
    // K_encr: FED573CFA6FC81267C08E264F50A0BB9
    // K_aut: 277B5D6A68FE5156A387996510AC5D61
    private static final byte[] MSK_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "8023A49840433464DA1A4F2457FAB3D6"
                            + "B1A3CA6E5E1DB212FA1AEA17F0A5C933"
                            + "5541DE7448FE448AC3F09DC25BBAE1EE"
                            + "17DCE3D32099519CC75840F0E3FB612B");
    private static final byte[] EMSK_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "F7E213F0E8F14A21C87F9B5DFADA9A75"
                            + "A8EAF4AD718BF8C3ED6557BDB60E4671"
                            + "E6AE109448B2F32F9B984667AE6C2B3F"
                            + "2FDFE67F97AF4D4727A2EA37F06B7785");

    private static final byte[] EAP_SIM_START_REQUEST = hexStringToByteArray(
            "01850014120a0000" // EAP header
                    + "0f02000200010000" // AT_VERSION_LIST attribute
                    + "0d010000"); // AT_ANY_ID_REQ attribute
    private static final byte[] EAP_SIM_START_RESPONSE = hexStringToByteArray(
            "02850034120a0000" // EAP header
                    + "0705000037f3ddd3954c4831a5ee08c574844398" // AT_NONCE_MT attribute
                    + "10010001" // AT_SELECTED_VERSION attribute
                    + "0e05001031313233343536373839414243444546"); // AT_IDENTITY attribute
    private static final byte[] EAP_SIM_CHALLENGE_REQUEST = hexStringToByteArray(
            "01860050120b0000" // EAP header
                    + "010d0000" // AT_RAND attribute
                    + "0123456789abcdef1123456789abcdef" // Rand 1
                    + "1123456789abcdef1123456789abcdef" // Rand 2
                    + "2123456789abcdef1123456789abcdef" // Rand 3
                    + "0b050000e4675b17fa7ba4d93db48d1af9ecbb01"); // AT_MAC attribute
    private static final byte[] EAP_SIM_CHALLENGE_RESPONSE =
            hexStringToByteArray(
                    "0286001c120b0000" // EAP header
                            + "0b050000e5df9cb1d935ea5f54d449a038bed061"); // AT_MAC attribute

    private static final byte[] EAP_SIM_START_REQUEST_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "01850010" // EAP-Request | ID | length in bytes
                            + "120a0000" // EAP-SIM | Start| 2B padding
                            + "0f02000200010000"); // AT_VERSION_LIST attribute
    private static final byte[] EAP_SIM_START_RESPONSE_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "02850020" // EAP-Response | ID | length in bytes
                            + "120a0000" // EAP-SIM | Start | 2B padding
                            + "0705000037f3ddd3954c4831a5ee08c574844398" // AT_NONCE_MT attribute
                            + "10010001"); // AT_SELECTED_VERSION attribute
    private static final byte[] EAP_SIM_CHALLENGE_REQUEST_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "01860050" // EAP-Request | ID | length in bytes
                            + "120b0000" // EAP-SIM | Challenge | 2B padding
                            + "010d0000" // AT_RAND attribute
                            + "0123456789abcdef1123456789abcdef" // Rand 1
                            + "1123456789abcdef1123456789abcdef" // Rand 2
                            + "2123456789abcdef1123456789abcdef" // Rand 3
                            + "0b050000F2F8C10FCA946AAFE9555E2BD3693DF6"); // AT_MAC attribute
    private static final byte[] EAP_SIM_CHALLENGE_RESPONSE_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "0286001c" // EAP-Response | ID | length in bytes
                            + "120b0000" // EAP-SIM | Challenge | 2B padding
                            + "0b050000DAC3C1B7D9DBFBC923464A94F186E410"); // AT_MAC attribute

    private static final byte[] EAP_SIM_CHALLENGE_REQUEST_INVALID_MAC =
            hexStringToByteArray(
                    "01860050" // EAP-Request | ID | length in bytes
                            + "120b0000" // EAP-SIM | Challenge | 2B padding
                            + "010d0000" // AT_RAND attribute
                            + "0123456789abcdef1123456789abcdef" // Rand 1
                            + "1123456789abcdef1123456789abcdef" // Rand 2
                            + "2123456789abcdef1123456789abcdef" // Rand 3
                            + "0b05000000112233445566778899AABBCCDDEEFF"); // AT_MAC attribute
    private static final byte[] EAP_SIM_CLIENT_ERROR_RESPONSE =
            hexStringToByteArray(
                    "0286000C" // EAP-Response | ID | length in bytes
                            + "120E0000" // EAP-SIM | Client-Error | 2B padding
                            + "16010000"); // AT_CLIENT_ERROR_CODE attribute

    private TelephonyManager mMockTelephonyManager;

    @Before
    @Override
    public void setUp() {
        super.setUp();

        mMockTelephonyManager = mock(TelephonyManager.class);

        mEapSessionConfig =
                new EapSessionConfig.Builder()
                        .setEapIdentity(EAP_IDENTITY)
                        .setEapSimConfig(SUB_ID, APPTYPE_USIM)
                        .build();
        mEapAuthenticator =
                new EapAuthenticator(
                        mTestLooper.getLooper(),
                        mMockCallback,
                        new EapStateMachine(mMockContext, mEapSessionConfig, mMockSecureRandom),
                        (runnable) -> runnable.run(),
                        AUTHENTICATOR_TIMEOUT_MILLIS);
    }

    @Test
    public void testEapSimEndToEnd() {
        verifyEapSimStart(EAP_SIM_START_REQUEST, EAP_SIM_START_RESPONSE, true);
        verifyEapSimChallenge(EAP_SIM_CHALLENGE_REQUEST, EAP_SIM_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapSimEndToEndWithoutIdentityRequest() {
        verifyEapSimStart(
                EAP_SIM_START_REQUEST_WITHOUT_IDENTITY_REQ,
                EAP_SIM_START_RESPONSE_WITHOUT_IDENTITY_REQ,
                false);
        verifyEapSimChallenge(
                EAP_SIM_CHALLENGE_REQUEST_WITHOUT_IDENTITY_REQ,
                EAP_SIM_CHALLENGE_RESPONSE_WITHOUT_IDENTITY_REQ);
        verifyEapSuccess(MSK_WITHOUT_IDENTITY_REQ, EMSK_WITHOUT_IDENTITY_REQ);
    }

    @Test
    public void testEapSimUnsupportedType() {
        verifyUnsupportedType(EAP_REQUEST_AKA_IDENTITY_PACKET, EAP_RESPONSE_NAK_PACKET);

        verifyEapSimStart(EAP_SIM_START_REQUEST, EAP_SIM_START_RESPONSE, true);
        verifyEapSimChallenge(EAP_SIM_CHALLENGE_REQUEST, EAP_SIM_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void verifyEapSimWithEapNotifications() {
        verifyEapNotification(1);
        verifyEapSimStart(EAP_SIM_START_REQUEST, EAP_SIM_START_RESPONSE, true);

        verifyEapNotification(2);
        verifyEapSimChallenge(EAP_SIM_CHALLENGE_REQUEST, EAP_SIM_CHALLENGE_RESPONSE);
        verifyEapNotification(3);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapSimInvalidMac() {
        verifyEapSimStart(EAP_SIM_START_REQUEST, EAP_SIM_START_RESPONSE, true);
        verifyEapSimChallenge(EAP_SIM_CHALLENGE_REQUEST_INVALID_MAC, EAP_SIM_CLIENT_ERROR_RESPONSE);
        verifyExpectsEapFailure(EAP_SIM_CHALLENGE_REQUEST);
        verifyEapFailure();
    }

    private void verifyEapSimStart(
            byte[] incomingEapPacket, byte[] outgoingEapPacket, boolean expectIdentityRequest) {
        TelephonyManager mockTelephonyManagerFromContext = mock(TelephonyManager.class);
        doReturn(mockTelephonyManagerFromContext)
                .when(mMockContext)
                .getSystemService(Context.TELEPHONY_SERVICE);
        doReturn(mMockTelephonyManager)
                .when(mockTelephonyManagerFromContext)
                .createForSubscriptionId(SUB_ID);

        // EAP-SIM/Start request
        doReturn(UNFORMATTED_IDENTITY).when(mMockTelephonyManager).getSubscriberId();
        doAnswer(invocation -> {
            byte[] dst = invocation.getArgument(0);
            System.arraycopy(NONCE, 0, dst, 0, NONCE.length);
            return null;
        }).when(mMockSecureRandom).nextBytes(eq(new byte[NONCE.length]));

        mEapAuthenticator.processEapMessage(incomingEapPacket);
        mTestLooper.dispatchAll();
        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));

        if (expectIdentityRequest) {
            verify(mMockTelephonyManager).getSubscriberId();
        }

        verify(mMockSecureRandom).nextBytes(any(byte[].class));

        // verify EAP-SIM/Start response
        verify(mMockCallback).onResponse(eq(outgoingEapPacket));
        verifyNoMoreInteractions(
                mMockContext,
                mMockTelephonyManager,
                mMockSecureRandom,
                mMockCallback);
    }

    private void verifyEapSimChallenge(byte[] incomingEapPacket, byte[] outgoingEapPacket) {
        // EAP-SIM/Challenge request
        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE64_RAND_1))
                .thenReturn(BASE64_RESP_1);
        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE64_RAND_2))
                .thenReturn(BASE64_RESP_2);
        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE64_RAND_3))
                .thenReturn(BASE64_RESP_3);

        mEapAuthenticator.processEapMessage(incomingEapPacket);
        mTestLooper.dispatchAll();

        // verify EAP-SIM/Challenge response
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        eq(TelephonyManager.APPTYPE_USIM),
                        eq(TelephonyManager.AUTHTYPE_EAP_SIM),
                        eq(BASE64_RAND_1));
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        eq(TelephonyManager.APPTYPE_USIM),
                        eq(TelephonyManager.AUTHTYPE_EAP_SIM),
                        eq(BASE64_RAND_2));
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        eq(TelephonyManager.APPTYPE_USIM),
                        eq(TelephonyManager.AUTHTYPE_EAP_SIM),
                        eq(BASE64_RAND_3));
        verify(mMockCallback).onResponse(eq(outgoingEapPacket));
        verifyNoMoreInteractions(
                mMockContext,
                mMockTelephonyManager,
                mMockSecureRandom,
                mMockCallback);
    }

    @Override
    protected void verifyEapSuccess(byte[] msk, byte[] emsk) {
        super.verifyEapSuccess(msk, emsk);

        verifyNoMoreInteractions(mMockTelephonyManager);
    }
}
