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

import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NAK_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SUCCESS_PACKET;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.InvalidEapResponseException;
import com.android.internal.net.eap.message.EapMessage;

import org.junit.Before;
import org.junit.Test;

public class EapResponseTest {
    private EapMessage mEapResponse;
    private EapMessage mEapSuccess;

    @Before
    public void setUp() throws Exception {
        mEapResponse = EapMessage.decode(EAP_RESPONSE_NAK_PACKET);
        mEapSuccess = EapMessage.decode(EAP_SUCCESS_PACKET);
    }

    @Test
    public void testGetEapResponse() {
        EapResult eapResult = EapResponse.getEapResponse(mEapResponse);
        assertTrue(eapResult instanceof EapResponse);

        EapResponse eapResponse = (EapResponse) eapResult;
        assertArrayEquals(EAP_RESPONSE_NAK_PACKET, eapResponse.packet);
    }

    @Test
    public void testGetEapResponseNullMessage() {
        try {
            EapResponse.getEapResponse(null);
            fail("Expected IllegalArgumentException for null EapMessage");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testGetEapResponseNonRequestMessage() {
        EapResult eapResult = EapResponse.getEapResponse(mEapSuccess);
        assertTrue(eapResult instanceof EapError);

        EapError eapError = (EapError) eapResult;
        assertTrue(eapError.cause instanceof InvalidEapResponseException);
    }
}
