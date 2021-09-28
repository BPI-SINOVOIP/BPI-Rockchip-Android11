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

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapData.EAP_IDENTITY;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_FAILURE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.CHALLENGE_RESPONSE_INVALID_KC;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.CHALLENGE_RESPONSE_INVALID_SRES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SIM_IDENTITY_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EMSK;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.KC_1_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.KC_2_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSK;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SRES_1_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SRES_2_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.VALID_CHALLENGE_RESPONSE;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_MAC;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RAND;
import static com.android.internal.net.eap.message.simaka.EapSimTypeData.EAP_SIM_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.NONCE_MT;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_1;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_1_BYTES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_2;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_2_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.telephony.TelephonyManager;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapFailure;
import com.android.internal.net.eap.EapResult.EapSuccess;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.exceptions.simaka.EapSimAkaAuthenticationFailureException;
import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidLengthException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNonceMt;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRandSim;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.message.simaka.EapSimTypeData;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;
import com.android.internal.net.eap.statemachine.EapSimMethodStateMachine.ChallengeState;
import com.android.internal.net.eap.statemachine.EapSimMethodStateMachine.ChallengeState.RandChallengeResult;

import org.junit.Test;

import java.nio.BufferUnderflowException;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;

public class EapSimChallengeStateTest extends EapSimStateTest {
    private static final int VALID_SRES_LENGTH = 4;
    private static final int INVALID_SRES_LENGTH = 5;
    private static final int VALID_KC_LENGTH = 8;
    private static final int INVALID_KC_LENGTH = 9;
    private static final int AT_RAND_LENGTH = 36;
    private static final List<Integer> VERSIONS = Arrays.asList(1);

    // Base64 of {@link EapTestAttributeDefinitions#RAND_1}
    private static final String BASE_64_RAND_1 = "EAARIjNEVWZ3iJmqu8zd7v8=";

    // Base64 of {@link EapTestAttributeDefinitions#RAND_2}
    private static final String BASE_64_RAND_2 = "EP/u3cy7qpmId2ZVRDMiEQA=";

    // Base64 of "04" + SRES_1 + "08" + KC_1
    private static final String BASE_64_RESP_1 = "BBEiM0QIAQIDBAUGBwg=";

    // Base64 of "04" + SRES_2 + "08" + KC_2
    private static final String BASE_64_RESP_2 = "BEQzIhEICAcGBQQDAgE=";

    // Base64 of "04" + SRES_1 + '081122"
    private static final String BASE_64_INVALID_RESP = "BBEiM0QIESI=";

    private AtNonceMt mAtNonceMt;
    private ChallengeState mChallengeState;

    @Override
    public void setUp() {
        super.setUp();

        try {
            mAtNonceMt = new AtNonceMt(NONCE_MT);
        } catch (EapSimAkaInvalidAttributeException ex) {
            // this will never happen
        }
        mChallengeState = mEapSimMethodStateMachine
                .new ChallengeState(VERSIONS, mAtNonceMt, EAP_SIM_IDENTITY_BYTES);
        mEapSimMethodStateMachine.transitionTo(mChallengeState);
    }

    @Test
    public void testProcessIncorrectEapMethodType() throws Exception {
        EapData eapData = new EapData(EAP_IDENTITY, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        EapResult result = mChallengeState.process(eapMessage);
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }

    @Test
    public void testProcessSuccess() throws Exception {
        System.arraycopy(MSK, 0, mEapSimMethodStateMachine.mMsk, 0, MSK.length);
        System.arraycopy(EMSK, 0, mEapSimMethodStateMachine.mEmsk, 0, EMSK.length);
        mChallengeState.mHadSuccessfulChallenge = true;

        EapMessage input = new EapMessage(EAP_CODE_SUCCESS, ID_INT, null);
        EapResult result = mEapSimMethodStateMachine.process(input);
        assertTrue(mEapSimMethodStateMachine.getState() instanceof FinalState);

        EapSuccess eapSuccess = (EapSuccess) result;
        assertArrayEquals(MSK, eapSuccess.msk);
        assertArrayEquals(EMSK, eapSuccess.emsk);
    }

    @Test
    public void testProcessFailure() throws Exception {
        EapMessage input = new EapMessage(EAP_CODE_FAILURE, ID_INT, null);
        EapResult result = mEapSimMethodStateMachine.process(input);
        assertTrue(mEapSimMethodStateMachine.getState() instanceof FinalState);

        assertTrue(result instanceof EapFailure);
    }

    @Test
    public void testIsValidChallengeAttributes() {
        LinkedHashMap<Integer, EapSimAkaAttribute> attributeMap = new LinkedHashMap<>();
        EapSimTypeData eapSimTypeData = new EapSimTypeData(EAP_SIM_CHALLENGE, attributeMap);
        assertFalse(mChallengeState.isValidChallengeAttributes(eapSimTypeData));

        attributeMap.put(EAP_AT_RAND, null); // value doesn't matter, just need key
        eapSimTypeData = new EapSimTypeData(EAP_SIM_CHALLENGE, attributeMap);
        assertFalse(mChallengeState.isValidChallengeAttributes(eapSimTypeData));

        attributeMap.put(EAP_AT_MAC, null); // value doesn't matter, just need key
        eapSimTypeData = new EapSimTypeData(EAP_SIM_CHALLENGE, attributeMap);
        assertTrue(mChallengeState.isValidChallengeAttributes(eapSimTypeData));
    }

    @Test
    public void testRandChallengeResultConstructor() {
        try {
            mChallengeState.new RandChallengeResult(
                    new byte[VALID_SRES_LENGTH], new byte[INVALID_KC_LENGTH]);
            fail("EapSimAkaInvalidLengthException expected for invalid SRES lengths");
        } catch (EapSimAkaInvalidLengthException expected) {
        }

        try {
            mChallengeState.new RandChallengeResult(
                    new byte[INVALID_SRES_LENGTH], new byte[VALID_KC_LENGTH]);
            fail("EapSimAkaInvalidLengthException expected for invalid Kc lengths");
        } catch (EapSimAkaInvalidLengthException expected) {
        }
    }

    @Test
    public void testRandChallengeResultEquals() throws Exception {
        RandChallengeResult resultA =
                mChallengeState.new RandChallengeResult(SRES_1_BYTES, KC_1_BYTES);
        RandChallengeResult resultB =
                mChallengeState.new RandChallengeResult(SRES_1_BYTES, KC_1_BYTES);
        RandChallengeResult resultC =
                mChallengeState.new RandChallengeResult(SRES_2_BYTES, KC_2_BYTES);

        assertEquals(resultA, resultB);
        assertNotEquals(resultA, resultC);
    }

    @Test
    public void testRandChallengeResultHashCode() throws Exception {
        RandChallengeResult resultA =
                mChallengeState.new RandChallengeResult(SRES_1_BYTES, KC_1_BYTES);
        RandChallengeResult resultB =
                mChallengeState.new RandChallengeResult(SRES_1_BYTES, KC_1_BYTES);
        RandChallengeResult resultC =
                mChallengeState.new RandChallengeResult(SRES_2_BYTES, KC_2_BYTES);

        assertEquals(resultA.hashCode(), resultB.hashCode());
        assertNotEquals(resultA.hashCode(), resultC.hashCode());
    }

    @Test
    public void testGetRandChallengeResultFromResponse() throws Exception {
        RandChallengeResult result =
                mChallengeState.getRandChallengeResultFromResponse(VALID_CHALLENGE_RESPONSE);

        assertArrayEquals(SRES_1_BYTES, result.sres);
        assertArrayEquals(KC_1_BYTES, result.kc);
    }

    @Test
    public void testGetRandChallengeResultFromResponseInvalidSres() {
        try {
            mChallengeState.getRandChallengeResultFromResponse(CHALLENGE_RESPONSE_INVALID_SRES);
            fail("EapSimAkaInvalidLengthException expected for invalid SRES_1 length");
        } catch (EapSimAkaInvalidLengthException expected) {
        }
    }

    @Test
    public void testGetRandChallengeResultFromResponseInvalidKc() {
        try {
            mChallengeState.getRandChallengeResultFromResponse(CHALLENGE_RESPONSE_INVALID_KC);
            fail("EapSimAkaInvalidLengthException expected for invalid KC length");
        } catch (EapSimAkaInvalidLengthException expected) {
        }
    }

    @Test
    public void testGetRandChallengeResults() throws Exception {
        EapSimTypeData eapSimTypeData =
                new EapSimTypeData(EAP_SIM_CHALLENGE, Arrays.asList(
                        new AtRandSim(AT_RAND_LENGTH,
                                hexStringToByteArray(RAND_1),
                                hexStringToByteArray(RAND_2))));

        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_1))
                .thenReturn(BASE_64_RESP_1);
        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_2))
                .thenReturn(BASE_64_RESP_2);

        List<RandChallengeResult> actualResult =
                mChallengeState.getRandChallengeResults(eapSimTypeData);

        List<RandChallengeResult> expectedResult = Arrays.asList(
                mChallengeState.new RandChallengeResult(SRES_1_BYTES, KC_1_BYTES),
                mChallengeState.new RandChallengeResult(SRES_2_BYTES, KC_2_BYTES));
        assertEquals(expectedResult, actualResult);

        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_1);
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_2);
        verifyNoMoreInteractions(mMockTelephonyManager);
    }

    @Test
    public void testGetRandChallengeResultsBufferUnderflow() throws Exception {
        EapSimTypeData eapSimTypeData =
                new EapSimTypeData(EAP_SIM_CHALLENGE, Arrays.asList(
                        new AtRandSim(AT_RAND_LENGTH,
                                hexStringToByteArray(RAND_1),
                                hexStringToByteArray(RAND_2))));

        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_1))
                .thenReturn(BASE_64_RESP_1);
        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_2))
                .thenReturn(BASE_64_INVALID_RESP);

        try {
            mChallengeState.getRandChallengeResults(eapSimTypeData);
            fail("BufferUnderflowException expected for short Kc value");
        } catch (BufferUnderflowException ex) {
        }

        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_1);
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_2);
        verifyNoMoreInteractions(mMockTelephonyManager);
    }

    @Test
    public void testProcessUiccAuthenticationNullResponse() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_SIM, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        AtRandSim atRandSim = new AtRandSim(AT_RAND_LENGTH, RAND_1_BYTES, RAND_2_BYTES);

        DecodeResult<EapSimTypeData> decodeResult =
                new DecodeResult<>(
                        new EapSimTypeData(
                                EAP_SIM_CHALLENGE,
                                Arrays.asList(atRandSim, new AtMac())));
        doReturn(decodeResult).when(mMockEapSimTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        when(mMockTelephonyManager
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_1))
                .thenReturn(null);

        EapError eapError = (EapError) mEapSimMethodStateMachine.process(eapMessage);
        assertTrue(eapError.cause instanceof EapSimAkaAuthenticationFailureException);

        verify(mMockEapSimTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        verify(mMockTelephonyManager)
                .getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        BASE_64_RAND_1);
        verifyNoMoreInteractions(mMockEapSimTypeDataDecoder, mMockTelephonyManager);
    }
}
