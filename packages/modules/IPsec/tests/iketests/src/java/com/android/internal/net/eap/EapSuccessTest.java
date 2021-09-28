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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.EapResult.EapSuccess;

import org.junit.Test;

public class EapSuccessTest {
    public static final byte[] MSK = new byte[] {(byte) 1, (byte) 2, (byte) 3};
    public static final byte[] EMSK = new byte[] {(byte) 4, (byte) 5, (byte) 6};

    @Test
    public void testEapSuccessConstructor() {
        EapSuccess eapSuccess = new EapSuccess(MSK, EMSK);
        assertArrayEquals(MSK, eapSuccess.msk);
        assertArrayEquals(EMSK, eapSuccess.emsk);
    }

    @Test
    public void testEapSuccessConstructorNullMsk() {
        try {
            new EapSuccess(null, EMSK);
            fail("Expected IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testEapSuccessConstructorNullEmsk() {
        try {
            new EapSuccess(MSK, null);
            fail("Expected IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
        }
    }
}
