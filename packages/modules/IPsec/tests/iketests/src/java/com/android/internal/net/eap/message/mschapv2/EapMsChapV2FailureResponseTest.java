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

package com.android.internal.net.eap.message.mschapv2;

import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_FAILURE_RESPONSE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_FAILURE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2FailureResponse;

import org.junit.Test;

public class EapMsChapV2FailureResponseTest {
    @Test
    public void testGetEapMsChapV2FailureResponse() {
        EapMsChapV2FailureResponse failureResponse =
                EapMsChapV2FailureResponse.getEapMsChapV2FailureResponse();
        assertEquals(EAP_MSCHAP_V2_FAILURE, failureResponse.opCode);
    }

    @Test
    public void testEncode() {
        EapMsChapV2FailureResponse failureResponse =
                EapMsChapV2FailureResponse.getEapMsChapV2FailureResponse();
        assertArrayEquals(EAP_MSCHAP_V2_FAILURE_RESPONSE, failureResponse.encode());
    }
}
