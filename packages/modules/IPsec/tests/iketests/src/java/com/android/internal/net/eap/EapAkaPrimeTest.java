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

import java.nio.charset.StandardCharsets;

/**
 * This test verifies that EAP-AKA' is functional for an end-to-end implementation.
 *
 * <p>This test uses externally generated test vectors (RFC 5448 Appendix C).
 *
 * @see <a href="https://tools.ietf.org/html/rfc5448#appendix-C">RFC 5448, EAP-AKA' Test Vectors</a>
 */
public class EapAkaPrimeTest extends EapMethodEndToEndTest {
    private static final long AUTHENTICATOR_TIMEOUT_MILLIS = 250L;

    private static final int SUB_ID = 1;
    private static final String UNFORMATTED_IDENTITY = "123456789ABCDEF"; // IMSI

    // EAP_IDENTITY = hex("test@android.net")
    private static final byte[] EAP_IDENTITY =
            hexStringToByteArray("7465737440616E64726F69642E6E6574");
    private static final boolean ALLOW_MISMATCHED_NETWORK_NAMES = false;
    private static final String PEER_NETWORK_NAME_1 = "foo:bar";
    private static final String PEER_NETWORK_NAME_2 = "bar";

    // hex("foo:bar:buzz")
    private static final String SERVER_NETWORK_NAME = "666F6F3A6261723A62757A7A";

    // IK: 7320EE404E055EF2B5AB0F86E96C48BE
    // CK: E9D1707652E13BF3E05975F601678E5C
    // Server Network Name: 666F6F3A6261723A62757A7A
    // SQN ^ AK: 35A9143ED9E1
    // IK': 79DC30692F3D2303D148549E5D50D0AA
    // CK': BBD0A7AD3F14757BA604C4CBE70F9090
    // K_encr: CB8396FFC7A0B0894D1153F876349201
    // K_aut: B9AEC10CF18A9838871427CE5448126B77BCF9273C14C95B8ABD96A196B688B2
    // K_re: 579714654806CA889DEF8D9F8A8A2240542DD9433260C0A651D28B5000C28810
    private static final String RAND_1 = "D6A296F030A305601B311D38A004505C";
    private static final String RAND_2 = "000102030405060708090A0B0C0D0E0F";
    private static final String AUTN = "35A9143ED9E100011795E785DAFAAD9B";
    private static final String RES = "E5167A255FDCDE9248AF6B50ADA0D944";
    private static final String AUTS = "0102030405060708090A0B0C0D0E";
    private static final byte[] MSK =
            hexStringToByteArray(
                    "0CEC2A8CBA4E03D01D7506D3B739C97E"
                            + "3F945A86C2CB049F5E3525D1EC5AE756"
                            + "2B53EC957C3D02DEEC424403973FE32E"
                            + "FC1299FD48505BA2E4B43EB83C312E5C");
    private static final byte[] EMSK =
            hexStringToByteArray(
                    "1E8D3F94848418010C5163AEC00357B3"
                            + "02DB7D65003CE5B7D06B89F097BA5508"
                            + "1F26323CEFEEA0B6D9329E533F3D9FB4"
                            + "AC10C0C02FB1664995ED5AC794E8D9FC");

    // IK: 7320EE404E055EF2B5AB0F86E96C48BE
    // CK: E9D1707652E13BF3E05975F601678E5C
    // Server Network Name: 666F6F3A6261723A62757A7A
    // SQN ^ AK: 35A9143ED9E1
    // IK': 79DC30692F3D2303D148549E5D50D0AA
    // CK': BBD0A7AD3F14757BA604C4CBE70F9090
    // K_encr: A12359DF8B2DBBAF42FA92345D84B270
    // K_aut: 3194EE30607AFDBF04AF8457340988AD195852601ADBA7B3816AA8A476A647DA
    // K_re: 4152DDC0F9DB44629D237ED8B18AF597572D2BAF251CDE9A1F1BD7A8500E4F13
    private static final byte[] MSK_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "6C56239E95246E5A19CC81EFE221976A"
                            + "7602378D53E05501E50AB4B974390B1B"
                            + "3876C0B5C794B685EEBF96000815DD20"
                            + "15EE7DF5AA50856F96414695C474DDA0");
    private static final byte[] EMSK_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "60E2C25C18AD451784BEBEE77DAC4D9A"
                            + "0CF7E4DEA2E2E6F254C0CB40DDF8F5C0"
                            + "058577AE640901417D8B11A99C9983D1"
                            + "219C30D1CBD56881E1D43E387355E64A");

    // Base 64 of: [Length][RAND_1][Length][AUTN]
    private static final String BASE64_CHALLENGE_1 =
            "ENailvAwowVgGzEdOKAEUFwQNakUPtnhAAEXleeF2vqtmw==";

    // Base 64 of: ['DB'][Length][RES][Length][CK][Length][IK]
    private static final String BASE_64_RESPONSE_SUCCESS =
            "2xDlFnolX9zekkiva1CtoNlEEOnRcHZS4Tvz4Fl19gFnjlwQcyDuQE4FXvK1qw+G6WxIvg==";

    // Base 64 of: [Length][RAND_2][Length][AUTN]
    private static final String BASE64_CHALLENGE_2 =
            "EAABAgMEBQYHCAkKCwwNDg8QNakUPtnhAAEXleeF2vqtmw==";

    // Base 64 of: ['DC'][Length][AUTS]
    private static final String BASE_64_RESPONSE_SYNC_FAIL = "3A4BAgMEBQYHCAkKCwwNDg==";

    private static final String REQUEST_MAC = "0A73E5323CFAB1EB833E7B5C7C317D25";
    private static final String RESPONSE_MAC = "762E5A8EA05E4FEB72AF1C72205B670B";
    private static final String REQUEST_MAC_WITHOUT_IDENTITY_REQ =
            "34287531A4B339E87735A6509941ABBF";
    private static final String RESPONSE_MAC_WITHOUT_IDENTITY_REQ =
            "D1967EC074E691DE419498A8127F6CF0";

    private static final byte[] EAP_AKA_PRIME_IDENTITY_REQUEST =
            hexStringToByteArray(
                    "01CD000C" // EAP-Request | ID | length in bytes
                            + "32050000" // EAP-AKA' | Identity | 2B padding
                            + "0D010000"); // AT_ANY_ID_REQ attribute
    private static final byte[] EAP_AKA_PRIME_IDENTITY_RESPONSE =
            hexStringToByteArray(
                    "02CD001C" // EAP-Response | ID | length in bytes
                            + "32050000" // EAP-AKA' | Identity | 2B padding
                            + "0E05001036313233343536373839414243444546"); // AT_IDENTITY attribute

    private static final byte[] EAP_AKA_PRIME_CHALLENGE_REQUEST =
            hexStringToByteArray(
                    "01CE0058" // EAP-Request | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "01050000" + RAND_1 // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "1704000C" + SERVER_NETWORK_NAME // AT_KDF_INPUT attribute
                            + "18010001" // AT_KDF attribute
                            + "0B050000" + REQUEST_MAC); // AT_MAC attribute
    private static final byte[] EAP_AKA_PRIME_CHALLENGE_RESPONSE =
            hexStringToByteArray(
                    "02CE0030" // EAP-Response | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "03050080" + RES // AT_RES attribute
                            + "0B050000" + RESPONSE_MAC); // AT_MAC attribute

    private static final byte[] EAP_AKA_PRIME_CHALLENGE_REQUEST_WITHOUT_IDENTITY_REQ =
            hexStringToByteArray(
                    "01CE0058" // EAP-Request | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "01050000" + RAND_1 // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "1704000C" + SERVER_NETWORK_NAME // AT_KDF_INPUT attribute
                            + "18010001" // AT_KDF attribute
                            + "0B050000" + REQUEST_MAC_WITHOUT_IDENTITY_REQ); // AT_MAC attribute
    private static final byte[] EAP_AKA_PRIME_CHALLENGE_RESPONSE_WITHOUT_IDENTITY_REQUEST =
            hexStringToByteArray(
                    "02CE0030" // EAP-Response | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "03050080" + RES // AT_RES attribute
                            + "0B050000" + RESPONSE_MAC_WITHOUT_IDENTITY_REQ); // AT_MAC attribute

    private static final byte[] EAP_AKA_PRIME_CHALLENGE_REQUEST_SYNC_FAIL =
            hexStringToByteArray(
                    "01CE0058" // EAP-Request | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "01050000" + RAND_2 // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "1704000C" + SERVER_NETWORK_NAME // AT_KDF_INPUT attribute
                            + "18010001" // AT_KDF attribute
                            + "0B050000" + REQUEST_MAC); // AT_MAC attribute
    private static final byte[] EAP_AKA_PRIME_SYNC_FAIL_RESPONSE =
            hexStringToByteArray(
                    "02CE0018" // EAP-Response | ID | length in bytes
                            + "32040000" // EAP-AKA' | Synchronization-Failure | 2B padding
                            + "0404" + AUTS);  // AT_AUTS attribute

    private static final byte[] EAP_AKA_PRIME_AUTHENTICATION_REJECT =
            hexStringToByteArray(
                    "02CE0008" // EAP-Response | ID | length in bytes
                            + "32020000"); // EAP-AKA' | Authentication-Reject | 2B padding

    private static final byte[] EAP_RESPONSE_NAK_PACKET =
            hexStringToByteArray("021000060332"); // NAK with EAP-AKA' listed

    ////////////////////////////////////
    // RFC 5448 Appendix C: Test Vectors
    ////////////////////////////////////
    private static final String EXTERNAL_UNFORMATTED_IDENTITY = "0555444333222111"; // IMSI
    private static final byte[] EXTERNAL_UNFORMATTED_IDENTITY_BYTES =
            EXTERNAL_UNFORMATTED_IDENTITY.getBytes(StandardCharsets.US_ASCII);

    private static final String EXTERNAL_SERVER_NETWORK = "WLAN";
    private static final String EXTERNAL_SERVER_NETWORK_HEX = "574C414E";

    // Base 64 of: [Length][EXTERNAL_RAND][Length][EXTERNAL_AUTN]
    private static final String EXTERNAL_BASE_64_CHALLENGE =
            "EIHpK2wO4OEuvOuo2SqZ36UQu1LpHHR6w6sqXCPRXuNR1Q==";

    // Base 64 of: ['DB'][Length][RES][Length][CK][Length][IK]
    private static final String EXTERNAL_BASE_64_RESPONSE_SUCCESS =
            "2wgo17Dyouw95RBTSfvgmGSflI9dLpc6gcAPEJdEhxrTK/m70d1c5U4+Llo=";

    // IK: 9744871AD32BF9BBD1DD5CE54E3E2E5A
    // CK: 5349FBE098649F948F5D2E973A81C00F
    // Server Network Name: 574C414E
    // SQN ^ AK: BB52E91C747A
    // IK': CCFC230CA74FCC96C0A5D61164F5A76C
    // CK': 0093962D0DD84AA5684B045C9EDFFA04
    // K_encr: 766FA0A6C317174B812D52FBCD11A179
    // K_aut: 0842EA722FF6835BFA2032499FC3EC23C2F0E388B4F07543FFC677F1696D71EA
    // K_re: CF83AA8BC7E0ACED892ACC98E76A9B2095B558C7795C7094715CB3393AA7D17A
    private static final String EXTERNAL_RAND = "81E92B6C0EE0E12EBCEBA8D92A99DFA5";
    private static final String EXTERNAL_AUTN = "BB52E91C747AC3AB2A5C23D15EE351D5";
    private static final String EXTERNAL_RES = "28D7B0F2A2EC3DE5";
    private static final byte[] EXTERNAL_MSK =
            hexStringToByteArray(
                    "67C42D9AA56C1B79E295E3459FC3D187"
                            + "D42BE0BF818D3070E362C5E967A4D544"
                            + "E8ECFE19358AB3039AFF03B7C930588C"
                            + "055BABEE58A02650B067EC4E9347C75A");
    private static final byte[] EXTERNAL_EMSK =
            hexStringToByteArray(
                    "F861703CD775590E16C7679EA3874ADA"
                            + "866311DE290764D760CF76DF647EA01C"
                            + "313F69924BDD7650CA9BAC141EA075C4"
                            + "EF9E8029C0E290CDBAD5638B63BC23FB");

    private static final byte[] EXTERNAL_EAP_AKA_PRIME_IDENTITY_RESPONSE =
            hexStringToByteArray(
                    "02CD0020" // EAP-Response | ID | length in bytes
                            + "32050000" // EAP-AKA' | Identity | 2B padding
                            + "0E0600113630353535343434333333323232313131000000"); // AT_IDENTITY

    private static final String EXTERNAL_REQUEST_MAC = "621879e3e2da6da77e5184edb7385b13";
    private static final String EXTERNAL_RESPONSE_MAC = "702DF07378567EF9C8E73A58578B7700";
    private static final byte[] EXTERNAL_EAP_AKA_PRIME_CHALLENGE_REQUEST =
            hexStringToByteArray(
                    "01CE0050" // EAP-Request | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "01050000" + EXTERNAL_RAND // AT_RAND attribute
                            + "02050000" + EXTERNAL_AUTN // AT_AUTN attribute
                            + "17020004" + EXTERNAL_SERVER_NETWORK_HEX // AT_KDF_INPUT attribute
                            + "18010001" // AT_KDF attribute
                            + "0B050000" + EXTERNAL_REQUEST_MAC); // AT_MAC attribute
    private static final byte[] EXTERNAL_EAP_AKA_PRIME_CHALLENGE_RESPONSE =
            hexStringToByteArray(
                    "02CE0028" // EAP-Response | ID | length in bytes
                            + "32010000" // EAP-AKA' | Challenge | 2B padding
                            + "03030040" + EXTERNAL_RES // AT_RES attribute
                            + "0B050000" + EXTERNAL_RESPONSE_MAC); // AT_MAC attribute

    private TelephonyManager mMockTelephonyManager;

    @Before
    @Override
    public void setUp() {
        super.setUp();

        setUp(ALLOW_MISMATCHED_NETWORK_NAMES, PEER_NETWORK_NAME_1);
    }

    private void setUp(boolean allowMismatchedNetworkNames, String peerNetworkName) {
        setUp(EAP_IDENTITY, allowMismatchedNetworkNames, peerNetworkName);
    }

    private void setUp(
            byte[] eapIdentity, boolean allowMismatchedNetworkNames, String peerNetworkName) {
        mMockTelephonyManager = mock(TelephonyManager.class);

        mEapSessionConfig =
                new EapSessionConfig.Builder()
                        .setEapIdentity(eapIdentity)
                        .setEapAkaPrimeConfig(
                                SUB_ID, APPTYPE_USIM, peerNetworkName, allowMismatchedNetworkNames)
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
    public void testEapAkaPrimeEndToEnd() {
        verifyEapAkaPrimeIdentity();
        verifyEapAkaPrimeChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_PRIME_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaPrimeEndToEndExternalTestVectors() {
        setUp(
                EXTERNAL_UNFORMATTED_IDENTITY_BYTES,
                ALLOW_MISMATCHED_NETWORK_NAMES,
                EXTERNAL_SERVER_NETWORK);

        verifyEapAkaPrimeChallenge(
                EXTERNAL_BASE_64_CHALLENGE,
                EXTERNAL_BASE_64_RESPONSE_SUCCESS,
                EXTERNAL_EAP_AKA_PRIME_CHALLENGE_REQUEST,
                EXTERNAL_EAP_AKA_PRIME_CHALLENGE_RESPONSE);
        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);

        verifyEapSuccess(EXTERNAL_MSK, EXTERNAL_EMSK);
    }

    @Test
    public void testEapAkaPrimeEndToEndWithoutIdentityRequest() {
        verifyEapAkaPrimeChallengeWithoutIdentityReq();
        verifyEapSuccess(MSK_WITHOUT_IDENTITY_REQ, EMSK_WITHOUT_IDENTITY_REQ);
    }

    @Test
    public void testEapAkaPrimeWithEapNotifications() {
        verifyEapNotification(1);
        verifyEapAkaPrimeIdentity();

        verifyEapNotification(2);
        verifyEapAkaPrimeChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_PRIME_CHALLENGE_RESPONSE);

        verifyEapNotification(3);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaPrimeUnsupportedType() {
        verifyUnsupportedType(EAP_REQUEST_SIM_START_PACKET, EAP_RESPONSE_NAK_PACKET);

        verifyEapAkaPrimeIdentity();
        verifyEapAkaPrimeChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_PRIME_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaPrimeSynchronizationFailure() {
        verifyEapAkaPrimeIdentity();
        verifyEapAkaPrimeSynchronizationFailure();
        verifyEapAkaPrimeChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_PRIME_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    @Test
    public void testEapAkaPrimeAuthenticationReject() {
        verifyEapAkaPrimeIdentity();

        // return null from TelephonyManager to simluate rejection of AUTN
        verifyEapAkaPrimeChallenge(null, EAP_AKA_PRIME_AUTHENTICATION_REJECT);
        verifyExpectsEapFailure(EAP_AKA_PRIME_CHALLENGE_REQUEST);
        verifyEapFailure();
    }

    @Test
    public void testEapAkaPrimeMismatchedNetworkNamesNotAllowed() {
        // use mismatched peer network name
        setUp(false, PEER_NETWORK_NAME_2);
        verifyEapAkaPrimeIdentity();
        verifyEapAkaPrimeChallengeMismatchedNetworkNames();
        verifyEapFailure();
    }

    @Test
    public void testEapAkaPrimeMismatchedNetworkNamesAllowed() {
        setUp(true, PEER_NETWORK_NAME_2);
        verifyEapAkaPrimeIdentity();
        verifyEapAkaPrimeChallenge(BASE_64_RESPONSE_SUCCESS, EAP_AKA_PRIME_CHALLENGE_RESPONSE);
        verifyEapSuccess(MSK, EMSK);
    }

    private void verifyEapAkaPrimeIdentity() {
        verifyEapAkaPrimeIdentity(UNFORMATTED_IDENTITY, EAP_AKA_PRIME_IDENTITY_RESPONSE);
    }

    private void verifyEapAkaPrimeIdentity(String unformattedIdentity, byte[] responseMessage) {
        // EAP-AKA'/Identity request
        doReturn(unformattedIdentity).when(mMockTelephonyManager).getSubscriberId();

        mEapAuthenticator.processEapMessage(EAP_AKA_PRIME_IDENTITY_REQUEST);
        mTestLooper.dispatchAll();

        // verify EAP-AKA'/Identity response
        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));
        verify(mMockTelephonyManager).getSubscriberId();
        verify(mMockCallback).onResponse(eq(responseMessage));
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaPrimeChallenge(
            String challengeBase64,
            String responseBase64,
            byte[] incomingEapPacket,
            byte[] outgoingEapPacket) {
        // EAP-AKA'/Challenge request
        when(mMockTelephonyManager.getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA,
                        challengeBase64))
                .thenReturn(responseBase64);

        mEapAuthenticator.processEapMessage(incomingEapPacket);
        mTestLooper.dispatchAll();

        // verify EAP-AKA'/Challenge response
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA,
                        challengeBase64);
        verify(mMockCallback).onResponse(eq(outgoingEapPacket));
    }

    private void verifyEapAkaPrimeChallenge(String responseBase64, byte[] outgoingPacket) {
        verifyEapAkaPrimeChallenge(
                BASE64_CHALLENGE_1,
                responseBase64,
                EAP_AKA_PRIME_CHALLENGE_REQUEST,
                outgoingPacket);
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaPrimeChallengeWithoutIdentityReq() {
        verifyEapAkaPrimeChallenge(
                BASE64_CHALLENGE_1,
                BASE_64_RESPONSE_SUCCESS,
                EAP_AKA_PRIME_CHALLENGE_REQUEST_WITHOUT_IDENTITY_REQ,
                EAP_AKA_PRIME_CHALLENGE_RESPONSE_WITHOUT_IDENTITY_REQUEST);

        // also need to verify interactions with Context and TelephonyManager
        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaPrimeSynchronizationFailure() {
        verifyEapAkaPrimeChallenge(
                BASE64_CHALLENGE_2,
                BASE_64_RESPONSE_SYNC_FAIL,
                EAP_AKA_PRIME_CHALLENGE_REQUEST_SYNC_FAIL,
                EAP_AKA_PRIME_SYNC_FAIL_RESPONSE);
        verifyNoMoreInteractions(
                mMockContext, mMockTelephonyManager, mMockSecureRandom, mMockCallback);
    }

    private void verifyEapAkaPrimeChallengeMismatchedNetworkNames() {
        // EAP-AKA'/Challenge request
        mEapAuthenticator.processEapMessage(EAP_AKA_PRIME_CHALLENGE_REQUEST);
        mTestLooper.dispatchAll();
        verify(mMockCallback).onResponse(eq(EAP_AKA_PRIME_AUTHENTICATION_REJECT));
    }

    @Override
    protected void verifyEapSuccess(byte[] msk, byte[] emsk) {
        super.verifyEapSuccess(msk, emsk);

        verifyNoMoreInteractions(mMockTelephonyManager);
    }
}
