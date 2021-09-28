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

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_TEMPORARY_FAILURE;

import android.net.ipsec.ike.exceptions.IkeProtocolException;

/**
 * This exception is thrown when local node or remote peer receives a request that cannot be
 * completed due to a temporary condition such as a rekeying operation.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-2.7">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class TemporaryFailureException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 0;

    /**
     * Construct an instance of TemporaryFailureException.
     *
     * @param message the descriptive message.
     */
    public TemporaryFailureException(String message) {
        super(ERROR_TYPE_TEMPORARY_FAILURE, message);
    }

    /**
     * Construct a instance of TemporaryFailureException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public TemporaryFailureException(byte[] notifyData) {
        super(ERROR_TYPE_TEMPORARY_FAILURE, notifyData);
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
