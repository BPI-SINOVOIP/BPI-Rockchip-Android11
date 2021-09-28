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
package com.android.internal.net.ipsec.ike.exceptions;

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_INVALID_KE_PAYLOAD;

import android.net.ipsec.ike.exceptions.IkeProtocolException;

/**
 * This exception is thrown when the received KE payload in the request is different from accepted
 * Diffie-Hellman group.
 *
 * <p>Responder should include an INVALID_KE_PAYLOAD Notify payload in a response message for both
 * IKE INI exchange and other SA negotiation exchanges after IKE is setup..
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-1.3">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class InvalidKeException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 2;

    /**
     * Construct an instance of InvalidKeException
     *
     * @param dhGroup the expected DH group
     */
    public InvalidKeException(int dhGroup) {
        super(ERROR_TYPE_INVALID_KE_PAYLOAD, integerToByteArray(dhGroup, EXPECTED_ERROR_DATA_LEN));
    }

    /**
     * Construct a instance of InvalidKeException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public InvalidKeException(byte[] notifyData) {
        super(ERROR_TYPE_INVALID_KE_PAYLOAD, notifyData);
    }

    /**
     * Return the expected DH Group included in this exception.
     *
     * @return the expected DH Group.
     */
    public int getDhGroup() {
        return byteArrayToInteger(getErrorData());
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
