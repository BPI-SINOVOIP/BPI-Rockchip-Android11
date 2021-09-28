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

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_INVALID_MAJOR_VERSION;

import android.net.ipsec.ike.exceptions.IkeProtocolException;

/**
 * This exception is thrown when major version is higher than 2.
 *
 * <p>Include INVALID_MAJOR_VERSION Notify payload in an unencrypted response message containing
 * version number 2.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-2.5">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class InvalidMajorVersionException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 1;

    /**
     * Construct a instance of InvalidMajorVersionException
     *
     * @param version the major version in received packet
     */
    public InvalidMajorVersionException(byte version) {
        super(ERROR_TYPE_INVALID_MAJOR_VERSION, new byte[] {version});
    }

    /**
     * Construct a instance of InvalidMajorVersionException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public InvalidMajorVersionException(byte[] notifyData) {
        super(ERROR_TYPE_INVALID_MAJOR_VERSION, notifyData);
    }

    /**
     * Return the major verion included in this exception.
     *
     * @return the major verion
     */
    public int getMajorVerion() {
        return byteArrayToInteger(getErrorData());
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
