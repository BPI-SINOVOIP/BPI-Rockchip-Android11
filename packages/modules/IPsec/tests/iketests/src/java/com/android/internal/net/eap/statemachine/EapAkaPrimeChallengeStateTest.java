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

package com.android.internal.net.eap.statemachine;

import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA_PRIME;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.CK_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_PRIME_AUTHENTICATION_REJECT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_PRIME_IDENTITY_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_PRIME_IDENTITY_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.IK_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.IMSI;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_IDENTITY;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AUTN_BYTES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.MAC_BYTES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_1_BYTES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RES_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.net.eap.EapSessionConfig;

import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaPrimeTypeData;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAnyIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAutn;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtKdf;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtKdfInput;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRandAka;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.ChallengeState.RandChallengeResult;
import com.android.internal.net.eap.statemachine.EapAkaPrimeMethodStateMachine.ChallengeState;

import org.junit.Before;
import org.junit.Test;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;

public class EapAkaPrimeChallengeStateTest extends EapAkaPrimeStateTest {
    private static final String SERVER_NETWORK_NAME_STRING = "foo:bar:buzz";
    private static final byte[] SERVER_NETWORK_NAME =
            SERVER_NETWORK_NAME_STRING.getBytes(StandardCharsets.UTF_8);
    private static final String INCORRECT_NETWORK_NAME = "foo:buzz";
    private static final byte[] INCORRECT_SERVER_NETWORK_NAME =
            INCORRECT_NETWORK_NAME.getBytes(StandardCharsets.UTF_8);
    private static final int VALID_KDF = 1;
    private static final int INVALID_KDF = 10;

    private static final byte[] EXPECTED_CK_IK_PRIME =
            hexStringToByteArray(
                    "3F7B2687E2000F35F4AD0796E579CC90CCE648C28E26FAD3FC2749B790249F1E");
    private static final byte[] K_ENCR = hexStringToByteArray("9CD8BDFF999E547B73D127172ADB7881");
    private static final byte[] K_AUT =
            hexStringToByteArray(
                    "CC3CD1AD2B63D01931D1E48567966ECBA60092C4554C6A60EBD180D0957282BE");
    private static final byte[] K_RE =
            hexStringToByteArray(
                    "333A148B074FC01B2D0F17ABB741B2EA1968899A152CC9F2B6CBE728A9A31BC1");
    private static final byte[] MSK =
            hexStringToByteArray(
                    "A29268E164BB89E741FBEC0301006E34D277758145AD8404EAF2C5414F866E2A"
                            + "D635B0752C3B3D3F14A4BA9A388D338BF5F72161199E5D136CA1E4F7FE95E7FB");
    private static final byte[] EMSK =
            hexStringToByteArray(
                    "81B2C0EFB382B7E7749E9FAB6A678605D65F18E440FD997854CCB3FB9FF7CDB1"
                            + "D05D73736650F1A5470FA87F39B3BF584DBB3C27422E536C9C2CB1F985CEBB0B");

    private ChallengeState mState;

    @Before
    public void setUp() {
        super.setUp();

        mState = mStateMachine.new ChallengeState();
        mStateMachine.transitionTo(mState);
    }

    @Test
    public void testTransitionWithEapIdentity() throws Exception {
        mStateMachine.transitionTo(mStateMachine.new CreatedState());

        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(new EapAkaPrimeTypeData(EAP_AKA_CHALLENGE, new ArrayList<>()));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mStateMachine.process(eapMessage);

        ChallengeState challengeState = (ChallengeState) mStateMachine.getState();
        assertArrayEquals(EAP_IDENTITY_BYTES, challengeState.mIdentity);

        // decode() is called in CreatedState and ChallengeState
        verify(mMockTypeDataDecoder, times(2)).decode(eq(DUMMY_EAP_TYPE_DATA));
    }

    @Test
    public void testTransitionWithEapAkaPrimeIdentity() throws Exception {
        mStateMachine.transitionTo(mStateMachine.new CreatedState());

        // Process AKA' Identity Request
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaPrimeTypeData(EAP_AKA_IDENTITY, Arrays.asList(new AtAnyIdReq())));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        doReturn(IMSI).when(mMockTelephonyManager).getSubscriberId();

        EapResponse eapResponse = (EapResponse) mStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_PRIME_IDENTITY_RESPONSE, eapResponse.packet);

        // decode() is called in CreatedState and IdentityState
        verify(mMockTypeDataDecoder, times(2)).decode(eq(DUMMY_EAP_TYPE_DATA));
        verify(mMockTelephonyManager).getSubscriberId();

        // Process AKA' Challenge Request
        decodeResult =
                new DecodeResult<>(new EapAkaPrimeTypeData(EAP_AKA_CHALLENGE, new ArrayList<>()));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mStateMachine.process(eapMessage);

        ChallengeState challengeState = (ChallengeState) mStateMachine.getState();
        assertArrayEquals(EAP_AKA_PRIME_IDENTITY_BYTES, challengeState.mIdentity);

        // decode() called again in IdentityState and ChallengeState
        verify(mMockTypeDataDecoder, times(4)).decode(eq(DUMMY_EAP_TYPE_DATA));
    }

    @Test
    public void testProcessMissingAtKdf() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);
        AtAutn atAutn = new AtAutn(AUTN_BYTES);
        AtMac atMac = new AtMac(MAC_BYTES);
        AtKdfInput atKdfInput = new AtKdfInput(0, SERVER_NETWORK_NAME);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaPrimeTypeData(
                                EAP_AKA_CHALLENGE,
                                Arrays.asList(atRandAka, atAutn, atMac, atKdfInput)));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_PRIME_AUTHENTICATION_REJECT, eapResponse.packet);
        verify(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
    }

    @Test
    public void testProcessMissingAtKdfInput() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);
        AtAutn atAutn = new AtAutn(AUTN_BYTES);
        AtMac atMac = new AtMac(MAC_BYTES);
        AtKdf atKdf = new AtKdf(VALID_KDF);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaPrimeTypeData(
                                EAP_AKA_CHALLENGE, Arrays.asList(atRandAka, atAutn, atMac, atKdf)));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_PRIME_AUTHENTICATION_REJECT, eapResponse.packet);
        verify(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
    }

    @Test
    public void testProcessUnsupportedKdf() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);
        AtAutn atAutn = new AtAutn(AUTN_BYTES);
        AtMac atMac = new AtMac(MAC_BYTES);
        AtKdfInput atKdfInput = new AtKdfInput(0, SERVER_NETWORK_NAME);
        AtKdf atKdf = new AtKdf(INVALID_KDF);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaPrimeTypeData(
                                EAP_AKA_CHALLENGE,
                                Arrays.asList(atRandAka, atAutn, atMac, atKdfInput, atKdf)));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_PRIME_AUTHENTICATION_REJECT, eapResponse.packet);
        verify(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
    }

    @Test
    public void testProcessIncorrectNetworkName() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);
        AtAutn atAutn = new AtAutn(AUTN_BYTES);
        AtMac atMac = new AtMac(MAC_BYTES);
        AtKdfInput atKdfInput = new AtKdfInput(0, INCORRECT_SERVER_NETWORK_NAME);
        AtKdf atKdf = new AtKdf(VALID_KDF);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaPrimeTypeData(
                                EAP_AKA_CHALLENGE,
                                Arrays.asList(atRandAka, atAutn, atMac, atKdfInput, atKdf)));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_PRIME_AUTHENTICATION_REJECT, eapResponse.packet);
        verify(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
    }

    @Test
    public void testProcessIncorrectNetworkNameIsIgnored() throws Exception {
        // Create state machine with configs allowing invalid network name to be ignored
        mStateMachine =
                new EapAkaPrimeMethodStateMachine(
                        mMockContext,
                        EAP_IDENTITY_BYTES,
                        new EapSessionConfig.EapAkaPrimeConfig(
                                SUB_ID, APPTYPE_USIM, PEER_NETWORK_NAME, true),
                        mMockTypeDataDecoder);
        mState = mStateMachine.new ChallengeState();
        mStateMachine.transitionTo(mState);

        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);
        AtAutn atAutn = new AtAutn(AUTN_BYTES);
        AtMac atMac = new AtMac(MAC_BYTES);
        AtKdfInput atKdfInput = new AtKdfInput(0, INCORRECT_SERVER_NETWORK_NAME);
        AtKdf atKdf = new AtKdf(VALID_KDF);

        EapAkaPrimeTypeData eapAkaPrimeTypeData =
                new EapAkaPrimeTypeData(
                        EAP_AKA_CHALLENGE,
                        Arrays.asList(atRandAka, atAutn, atMac, atKdfInput, atKdf));
        assertTrue(
                "Incorrect network names should be ignored",
                mState.isValidChallengeAttributes(eapAkaPrimeTypeData));
    }

    @Test
    public void testHasMatchingNetworkNames() {
        // "" should match anything
        assertTrue(mState.hasMatchingNetworkNames("", SERVER_NETWORK_NAME_STRING));
        assertTrue(mState.hasMatchingNetworkNames(SERVER_NETWORK_NAME_STRING, ""));

        // "foo:bar" should match "foo:bar:buzz"
        assertTrue(mState.hasMatchingNetworkNames(PEER_NETWORK_NAME, SERVER_NETWORK_NAME_STRING));
        assertTrue(mState.hasMatchingNetworkNames(SERVER_NETWORK_NAME_STRING, PEER_NETWORK_NAME));

        // "foo:buzz" shouldn't match "foo:bar:buzz"
        assertFalse(
                mState.hasMatchingNetworkNames(SERVER_NETWORK_NAME_STRING, INCORRECT_NETWORK_NAME));
        assertFalse(
                mState.hasMatchingNetworkNames(INCORRECT_NETWORK_NAME, SERVER_NETWORK_NAME_STRING));
    }

    @Test
    public void testDeriveCkIkPrime() throws Exception {
        RandChallengeResult randChallengeResult =
                mState.new RandChallengeResult(RES_BYTES, IK_BYTES, CK_BYTES);
        AtKdfInput atKdfInput =
                new AtKdfInput(0, PEER_NETWORK_NAME.getBytes(StandardCharsets.UTF_8));
        AtAutn atAutn = new AtAutn(AUTN_BYTES);

        // S = FC | Network Name | len(Network Name) | SQN ^ AK | len(SQN ^ AK)
        //   = 20666F6F3A62617200070123456789AB0006
        // K = CK | IK
        //   = 00112233445566778899AABBCCDDEEFFFFEEDDCCBBAA99887766554433221100
        // CK' | IK' = HMAC-SHA256(K, S)
        //           = 3F7B2687E2000F35F4AD0796E579CC90CCE648C28E26FAD3FC2749B790249F1E
        byte[] result = mState.deriveCkIkPrime(randChallengeResult, atKdfInput, atAutn);
        assertArrayEquals(EXPECTED_CK_IK_PRIME, result);
    }

    @Test
    public void testGenerateAndPersistEapAkaKeys() throws Exception {
        RandChallengeResult randChallengeResult =
                mState.new RandChallengeResult(RES_BYTES, IK_BYTES, CK_BYTES);

        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);
        AtAutn atAutn = new AtAutn(AUTN_BYTES);
        AtMac atMac = new AtMac(MAC_BYTES);
        AtKdfInput atKdfInput =
                new AtKdfInput(0, PEER_NETWORK_NAME.getBytes(StandardCharsets.UTF_8));
        AtKdf atKdf = new AtKdf(VALID_KDF);

        EapAkaPrimeTypeData eapAkaPrimeTypeData =
                new EapAkaPrimeTypeData(
                        EAP_AKA_CHALLENGE,
                        Arrays.asList(atRandAka, atAutn, atMac, atKdfInput, atKdf));

        // IK' | CK' = CCE648C28E26FAD3FC2749B790249F1E3F7B2687E2000F35F4AD0796E579CC90
        // data = "EAP-AKA'" | Identity
        //      = 4541502D414B41277465737440616E64726F69642E6E6574
        // prf+(CK' | IK', data) = T1 | T2 | T3 | T4 | T5 | T6 | T7
        // K_encr | K_aut | K_re | MSK | EMSK = prf+(IK' | CK', data)
        assertNull(
                mState.generateAndPersistEapAkaKeys(randChallengeResult, 0, eapAkaPrimeTypeData));
        assertArrayEquals(K_ENCR, mStateMachine.mKEncr);
        assertArrayEquals(K_AUT, mStateMachine.mKAut);
        assertArrayEquals(K_RE, mStateMachine.mKRe);
        assertArrayEquals(MSK, mStateMachine.mMsk);
        assertArrayEquals(EMSK, mStateMachine.mEmsk);
    }
}
