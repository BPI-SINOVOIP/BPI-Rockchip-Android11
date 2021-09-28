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

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.android.internal.net.eap.EapTestUtils.getDummyEapSimSessionConfig;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_MD5_CHALLENGE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_NAK_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NAK_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.REQUEST_UNSUPPORTED_TYPE_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SHORT_PACKET;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.net.eap.EapSessionConfig;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.EapInvalidPacketLengthException;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.statemachine.EapStateMachine.EapState;

import org.junit.Before;
import org.junit.Test;

import java.security.SecureRandom;

/**
 * EapStateTest is a test template for testing EapState implementations.
 *
 * <p>Specifically, testing for CreatedState, IdentityState, and MethodState should subclass
 * EapStateTest
 */
public class EapStateTest {
    protected Context mContext;
    protected EapSessionConfig mEapSessionConfig;
    protected EapStateMachine mEapStateMachine;
    protected EapState mEapState;

    @Before
    public void setUp() {
        mContext = getInstrumentation().getContext();
        mEapSessionConfig = getDummyEapSimSessionConfig();
        mEapStateMachine = new EapStateMachine(mContext, mEapSessionConfig, new SecureRandom());

        // this EapState definition is used to make sure all non-Success/Failure EAP states
        // produce the same results for error cases.
        mEapState = mEapStateMachine.new EapState() {
            @Override
            public EapResult process(byte[] msg) {
                return decode(msg).eapResult;
            }
        };
    }

    @Test
    public void testProcessNullPacket() {
        EapResult result = mEapState.process(null);
        assertTrue(result instanceof EapError);

        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof IllegalArgumentException);
    }

    @Test
    public void testProcessUnsupportedEapDataType() {
        EapResult result = mEapState.process(REQUEST_UNSUPPORTED_TYPE_PACKET);
        assertTrue(result instanceof EapResponse);

        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NAK_PACKET, eapResponse.packet);
    }

    @Test
    public void testProcessDecodeFailure() {
        EapResult result = mEapState.process(SHORT_PACKET);
        assertTrue(result instanceof EapError);

        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidPacketLengthException);
    }

    @Test
    public void testProcessResponse() {
        EapResult result = mEapState.process(EAP_RESPONSE_NOTIFICATION_PACKET);
        assertTrue(result instanceof EapError);

        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }

    @Test
    public void testProcessNakRequest() {
        EapResult result = mEapState.process(EAP_REQUEST_NAK_PACKET);
        assertTrue(result instanceof EapError);

        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }

    @Test
    public void testProcessMd5Challenge() {
        EapResult result = mEapState.process(EAP_REQUEST_MD5_CHALLENGE);
        assertTrue(result instanceof EapResponse);

        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NAK_PACKET, eapResponse.packet);
    }
}
