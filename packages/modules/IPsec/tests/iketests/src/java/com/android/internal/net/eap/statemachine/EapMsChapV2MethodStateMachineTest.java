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

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_MSCHAP_V2;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_AUTHENTICATOR_CHALLENGE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_AUTHENTICATOR_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_CHALLENGE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_MASTER_KEY;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_MSK;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_NT_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PASSWORD;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PASSWORD_HASH;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PASSWORD_HASH_HASH;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PASSWORD_UTF_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PEER_CHALLENGE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_RECEIVE_START_KEY;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_SEND_START_KEY;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_USERNAME;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_USERNAME_ASCII_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.eap.EapSessionConfig.EapMsChapV2Config;

import com.android.internal.net.eap.statemachine.EapMsChapV2MethodStateMachine.CreatedState;
import com.android.internal.net.utils.Log;

import org.junit.Before;
import org.junit.Test;

import java.security.SecureRandom;

public class EapMsChapV2MethodStateMachineTest {
    private EapMsChapV2Config mEapMsChapV2Config;
    private EapMsChapV2MethodStateMachine mStateMachine;

    @Before
    public void setUp() {
        mEapMsChapV2Config = new EapMsChapV2Config(MSCHAP_V2_USERNAME, MSCHAP_V2_PASSWORD);
        mStateMachine = new EapMsChapV2MethodStateMachine(mEapMsChapV2Config, new SecureRandom());
    }

    @Test
    public void testGetEapMethod() {
        assertEquals(EAP_TYPE_MSCHAP_V2, mStateMachine.getEapMethod());
    }

    @Test
    public void testStartsOnCreatedState() {
        assertTrue(mStateMachine.getState() instanceof CreatedState);
    }

    // Tests for MS CHAPv2 authentication utils. Test vectors from RFC 2759#9.2.

    @Test
    public void testUsernameToBytes() {
        assertArrayEquals(
                MSCHAP_V2_USERNAME_ASCII_BYTES,
                EapMsChapV2MethodStateMachine.usernameToBytes(MSCHAP_V2_USERNAME));
    }

    @Test
    public void testPasswordToBytes() {
        assertArrayEquals(
                MSCHAP_V2_PASSWORD_UTF_BYTES,
                EapMsChapV2MethodStateMachine.passwordToBytes(MSCHAP_V2_PASSWORD));
    }

    @Test
    public void testGenerateNtResponse() throws Exception {
        byte[] ntResponse =
                EapMsChapV2MethodStateMachine.generateNtResponse(
                        MSCHAP_V2_AUTHENTICATOR_CHALLENGE,
                        MSCHAP_V2_PEER_CHALLENGE,
                        MSCHAP_V2_USERNAME,
                        MSCHAP_V2_PASSWORD);
        assertArrayEquals(MSCHAP_V2_NT_RESPONSE, ntResponse);
    }

    @Test
    public void testChallengeHash() throws Exception {
        byte[] challenge =
                EapMsChapV2MethodStateMachine.challengeHash(
                        MSCHAP_V2_PEER_CHALLENGE,
                        MSCHAP_V2_AUTHENTICATOR_CHALLENGE,
                        MSCHAP_V2_USERNAME);
        assertArrayEquals(MSCHAP_V2_CHALLENGE, challenge);
    }

    @Test
    public void testNtPasswordHash() {
        byte[] passwordHash = EapMsChapV2MethodStateMachine.ntPasswordHash(MSCHAP_V2_PASSWORD);
        assertArrayEquals(MSCHAP_V2_PASSWORD_HASH, passwordHash);
    }

    @Test
    public void testHashNtPasswordHash() {
        byte[] passwordHashHash =
                EapMsChapV2MethodStateMachine.hashNtPasswordHash(MSCHAP_V2_PASSWORD_HASH);
        assertArrayEquals(MSCHAP_V2_PASSWORD_HASH_HASH, passwordHashHash);
    }

    @Test
    public void testChallengeResponse() throws Exception {
        byte[] challengeResponse =
                EapMsChapV2MethodStateMachine.challengeResponse(
                        MSCHAP_V2_CHALLENGE, MSCHAP_V2_PASSWORD_HASH);
        assertArrayEquals(MSCHAP_V2_NT_RESPONSE, challengeResponse);
    }

    @Test
    public void testGenerateAuthenticatorResponse() throws Exception {
        byte[] authenticatorResponse =
                EapMsChapV2MethodStateMachine.generateAuthenticatorResponse(
                        MSCHAP_V2_PASSWORD,
                        MSCHAP_V2_NT_RESPONSE,
                        MSCHAP_V2_PEER_CHALLENGE,
                        MSCHAP_V2_AUTHENTICATOR_CHALLENGE,
                        MSCHAP_V2_USERNAME);
        assertArrayEquals(MSCHAP_V2_AUTHENTICATOR_RESPONSE, authenticatorResponse);
    }

    @Test
    public void testCheckAuthenticatorResponse() throws Exception {
        assertTrue(
                "AuthenticatorResponse didn't match computed response",
                EapMsChapV2MethodStateMachine.checkAuthenticatorResponse(
                        MSCHAP_V2_PASSWORD,
                        MSCHAP_V2_NT_RESPONSE,
                        MSCHAP_V2_PEER_CHALLENGE,
                        MSCHAP_V2_AUTHENTICATOR_CHALLENGE,
                        MSCHAP_V2_USERNAME,
                        MSCHAP_V2_AUTHENTICATOR_RESPONSE));
    }

    @Test
    public void testGetMasterKey() throws Exception {
        byte[] masterKey =
                EapMsChapV2MethodStateMachine.getMasterKey(
                        MSCHAP_V2_PASSWORD_HASH_HASH, MSCHAP_V2_NT_RESPONSE);
        assertArrayEquals(MSCHAP_V2_MASTER_KEY, masterKey);
    }

    @Test
    public void testGetAsymmetricStartKeySendKey() throws Exception {
        byte[] startKey =
                EapMsChapV2MethodStateMachine.getAsymmetricStartKey(MSCHAP_V2_MASTER_KEY, true);
        assertArrayEquals(Log.byteArrayToHexString(startKey), MSCHAP_V2_SEND_START_KEY, startKey);
    }

    @Test
    public void testGetAsymmetricStartKeyReceiveKey() throws Exception {
        byte[] receiveKey =
                EapMsChapV2MethodStateMachine.getAsymmetricStartKey(MSCHAP_V2_MASTER_KEY, false);
        assertArrayEquals(MSCHAP_V2_RECEIVE_START_KEY, receiveKey);
    }

    @Test
    public void testGenerateMsk() throws Exception {
        byte[] msk =
                EapMsChapV2MethodStateMachine.generateMsk(
                        MSCHAP_V2_PASSWORD, MSCHAP_V2_NT_RESPONSE);
        assertArrayEquals(MSCHAP_V2_MSK, msk);
    }
}
