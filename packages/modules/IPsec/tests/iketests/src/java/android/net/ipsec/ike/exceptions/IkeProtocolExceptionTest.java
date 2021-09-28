/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.net.ipsec.ike.exceptions;

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_NO_PROPOSAL_CHOSEN;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import com.android.internal.net.ipsec.ike.exceptions.NoValidProposalChosenException;
import com.android.internal.net.ipsec.ike.exceptions.UnsupportedCriticalPayloadException;
import com.android.internal.net.ipsec.ike.message.IkeNotifyPayload;

import org.junit.Test;

import java.util.LinkedList;
import java.util.List;

public final class IkeProtocolExceptionTest {
    @Test
    public void buildNotifyPayloadWithData() throws Exception {
        List<Integer> unsupportedTypes = new LinkedList<>();
        unsupportedTypes.add(55); // 0x37 in hex
        unsupportedTypes.add(56);
        unsupportedTypes.add(57);
        UnsupportedCriticalPayloadException exception =
                new UnsupportedCriticalPayloadException(unsupportedTypes);

        IkeNotifyPayload payload = exception.buildNotifyPayload();
        assertEquals(ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD, payload.notifyType);
        assertArrayEquals(new byte[] {(byte) 0x37}, payload.notifyData);
    }

    @Test
    public void buildNotifyPayloadWithoutData() throws Exception {
        NoValidProposalChosenException exception =
                new NoValidProposalChosenException("IkeProtocolExceptionTest");

        IkeNotifyPayload payload = exception.buildNotifyPayload();
        assertEquals(ERROR_TYPE_NO_PROPOSAL_CHOSEN, payload.notifyType);
        assertArrayEquals(new byte[0], payload.notifyData);
    }
}
