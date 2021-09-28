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
package com.android.internal.net.ipsec.ike.exceptions;

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD;

import android.net.ipsec.ike.exceptions.IkeProtocolException;

import java.util.ArrayList;
import java.util.List;

/**
 * This exception is thrown when payload type is not supported and critical bit is set
 *
 * <p>Include UNSUPPORTED_CRITICAL_PAYLOAD Notify payloads in a response message. Each payload
 * contains only one payload type.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-2.5">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class UnsupportedCriticalPayloadException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 1;

    public final List<Integer> payloadTypeList;

    /**
     * Construct an instance of UnsupportedCriticalPayloadException.
     *
     * <p>To keep IkeProtocolException simpler, we only pass the first payload type to the
     * superclass which can be retrieved by users.
     *
     * @param payloadList the list of all unsupported critical payload types.
     */
    public UnsupportedCriticalPayloadException(List<Integer> payloadList) {
        super(
                ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD,
                integerToByteArray(payloadList.get(0), EXPECTED_ERROR_DATA_LEN));
        payloadTypeList = payloadList;
    }

    /**
     * Construct a instance of UnsupportedCriticalPayloadException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public UnsupportedCriticalPayloadException(byte[] notifyData) {
        super(ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD, notifyData);
        payloadTypeList = new ArrayList<>(1);
        payloadTypeList.add(byteArrayToInteger(notifyData));
    }

    /**
     * Return the all the unsupported critical payloads included in this exception.
     *
     * @return the unsupported critical payload list.
     */
    public List<Integer> getUnsupportedCriticalPayloadList() {
        return payloadTypeList;
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
