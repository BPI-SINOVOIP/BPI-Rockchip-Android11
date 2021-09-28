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

import android.net.ipsec.ike.exceptions.IkeProtocolException;

/**
 * This exception is thrown if any IKE message field is invalid.
 *
 * <p>Include INVALID_SYNTAX Notify payload in an encrypted response message if current message is
 * an encrypted request and cryptographic checksum is valid. Fatal error.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-3.10.1">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class InvalidSyntaxException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 0;

    /**
     * Construct an instance of InvalidSyntaxException.
     *
     * @param message the descriptive message.
     */
    public InvalidSyntaxException(String message) {
        super(ERROR_TYPE_INVALID_SYNTAX, message);
    }

    /**
     * Construct a instance of InvalidSyntaxException.
     *
     * @param cause the reason of exception.
     */
    public InvalidSyntaxException(Throwable cause) {
        super(ERROR_TYPE_INVALID_SYNTAX, cause);
    }

    /**
     * Construct a instance of InvalidSyntaxException.
     *
     * @param message the descriptive message.
     * @param cause the reason of exception.
     */
    public InvalidSyntaxException(String message, Throwable cause) {
        super(ERROR_TYPE_INVALID_SYNTAX, message, cause);
    }

    /**
     * Construct a instance of InvalidSyntaxException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public InvalidSyntaxException(byte[] notifyData) {
        super(ERROR_TYPE_INVALID_SYNTAX, notifyData);
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
