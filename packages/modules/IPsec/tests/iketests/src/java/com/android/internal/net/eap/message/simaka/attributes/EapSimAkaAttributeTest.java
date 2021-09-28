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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;

import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;

import org.junit.Test;

import java.nio.ByteBuffer;

public class EapSimAkaAttributeTest {
    private static final int EXPECTED_ATTRIBUTE_TYPE = 1;
    private static final int EXPECTED_LENGTH_IN_BYTES = 4;
    private static final int BUFFER_LENGTH = 2;
    private static final int EXPECTED_LENGTH_ENCODED = 1;
    private static final byte[] EXPECTED_ENCODING = {
            (byte) EXPECTED_ATTRIBUTE_TYPE,
            (byte) EXPECTED_LENGTH_ENCODED
    };

    @Test
    public void testEncode() throws Exception {
        EapSimAkaAttribute eapSimAkaAttribute = new EapSimAkaAttribute(
                EXPECTED_ATTRIBUTE_TYPE,
                EXPECTED_LENGTH_IN_BYTES) {
            public void encode(ByteBuffer byteBuffer) {
                encodeAttributeHeader(byteBuffer);
            }
        };

        ByteBuffer result = ByteBuffer.allocate(BUFFER_LENGTH);
        eapSimAkaAttribute.encode(result);
        assertArrayEquals(EXPECTED_ENCODING, result.array());
        assertFalse(result.hasRemaining());
    }
}
