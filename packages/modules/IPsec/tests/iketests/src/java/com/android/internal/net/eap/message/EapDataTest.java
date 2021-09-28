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

package com.android.internal.net.eap.message;

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.fail;

import org.junit.Test;

import java.nio.ByteBuffer;

public class EapDataTest {
    private static final byte[] EXPECTED_EAP_DATA_BYTES = new byte[] {
            EAP_TYPE_SIM,
            (byte) 1,
            (byte) 2,
            (byte) 3
    };
    private static final byte[] EAP_TYPE_DATA = new byte[] {(byte) 1, (byte) 2, (byte) 3};
    private static final int UNSUPPORTED_EAP_TYPE = -1;

    @Test
    public void testEapDataConstructor() {
        new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);
    }

    @Test
    public void testEapDataConstructorNullEapData() {
        try {
            new EapData(EAP_TYPE_SIM, null);
            fail("IllegalArgumentException expected for null eapTypeData");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testEapDataConstructorUnsupportedType() {
        try {
            new EapData(UNSUPPORTED_EAP_TYPE, EAP_TYPE_DATA);
            fail("Expected IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testGetLength() {
        EapData eapData = new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);
        assertEquals(EAP_TYPE_DATA.length + 1, eapData.getLength());
    }

    @Test
    public void testEquals() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);
        EapData eapDataCopy = new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);
        assertEquals(eapData, eapDataCopy);

        EapData eapDataDifferent = new EapData(EAP_TYPE_SIM, new byte[0]);
        assertNotEquals(eapData, eapDataDifferent);
    }

    @Test
    public void testHashCode() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);
        EapData eapDataCopy = new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);
        assertNotEquals(0, eapData.hashCode());
        assertEquals(eapData.hashCode(), eapDataCopy.hashCode());
    }

    @Test
    public void testEncodeToByteBuffer() {
        EapData eapData = new EapData(EAP_TYPE_SIM, EAP_TYPE_DATA);

        ByteBuffer b = ByteBuffer.allocate(eapData.getLength());
        eapData.encodeToByteBuffer(b);
        assertArrayEquals(EXPECTED_EAP_DATA_BYTES, b.array());
    }
}
