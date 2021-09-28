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

package com.android.internal.net.eap.message.simaka.attributes;

import static com.android.internal.net.TestUtils.hexStringToInt;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_CLIENT_ERROR_CODE;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_CLIENT_ERROR_CODE;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_CLIENT_ERROR_CODE_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.ERROR_CODE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtClientErrorCode;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttributeFactory;

import org.junit.Before;
import org.junit.Test;

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;

public class AtClientErrorCodeTest {
    private static final int EXPECTED_LENGTH = 4;

    private EapSimAkaAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = new EapSimAkaAttributeFactory() {};
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_CLIENT_ERROR_CODE);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        assertTrue(result instanceof AtClientErrorCode);
        AtClientErrorCode atClientErrorCode = (AtClientErrorCode) result;
        assertEquals(EAP_AT_CLIENT_ERROR_CODE, atClientErrorCode.attributeType);
        assertEquals(EXPECTED_LENGTH, atClientErrorCode.lengthInBytes);
        assertEquals(hexStringToInt(ERROR_CODE), atClientErrorCode.errorCode);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_CLIENT_ERROR_CODE_INVALID_LENGTH);
        try {
            mAttributeFactory.getAttribute(input);
            fail("Expected BufferUnderflowException for invalid attribute length");
        } catch (BufferUnderflowException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtClientErrorCode atNotification = new AtClientErrorCode(
                EXPECTED_LENGTH, hexStringToInt(ERROR_CODE));
        ByteBuffer result = ByteBuffer.allocate(EXPECTED_LENGTH);

        atNotification.encode(result);
        assertArrayEquals(AT_CLIENT_ERROR_CODE, result.array());
    }
}
