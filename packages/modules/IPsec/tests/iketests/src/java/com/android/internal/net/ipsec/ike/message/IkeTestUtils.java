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

package com.android.internal.net.ipsec.ike.message;

import static com.android.internal.net.ipsec.ike.message.IkeMessage.DECODE_STATUS_UNPROTECTED_ERROR;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.net.ipsec.ike.exceptions.IkeProtocolException;
import android.util.Pair;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResult;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultError;

import java.nio.ByteBuffer;

/**
 * IkeTestUtils provides utility methods for testing IKE library.
 *
 * <p>TODO: Consider moving it under ikev2/
 */
public final class IkeTestUtils {
    public static IkePayload hexStringToIkePayload(
            @IkePayload.PayloadType int payloadType, boolean isResp, String payloadHexString)
            throws IkeProtocolException {
        byte[] payloadBytes = TestUtils.hexStringToByteArray(payloadHexString);
        // Returned Pair consists of the IkePayload and the following IkePayload's type.
        Pair<IkePayload, Integer> pair =
                IkePayloadFactory.getIkePayload(payloadType, isResp, ByteBuffer.wrap(payloadBytes));
        return pair.first;
    }

    public static <T extends IkeProtocolException> T decodeAndVerifyUnprotectedErrorMsg(
            byte[] inputPacket, Class<T> expectedException) throws Exception {
        IkeHeader header = new IkeHeader(inputPacket);
        DecodeResult decodeResult = IkeMessage.decode(0, header, inputPacket);

        assertEquals(DECODE_STATUS_UNPROTECTED_ERROR, decodeResult.status);
        DecodeResultError resultError = (DecodeResultError) decodeResult;
        assertNotNull(resultError.ikeException);
        assertTrue(expectedException.isInstance(resultError.ikeException));

        return (T) resultError.ikeException;
    }

    public static IkeSkfPayload makeDummySkfPayload(
            byte[] unencryptedData, int fragNum, int totalFrags) throws Exception {
        IkeEncryptedPayloadBody mockEncryptedBody = mock(IkeEncryptedPayloadBody.class);
        doReturn(unencryptedData).when(mockEncryptedBody).getUnencryptedData();
        return new IkeSkfPayload(mockEncryptedBody, fragNum, totalFrags);
    }
}
