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

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_TS_UNACCEPTABLE;

import android.net.ipsec.ike.exceptions.IkeProtocolException;

/**
 * This exception is thrown if the remote sever proposed unacceptable TS.
 *
 * <p>If remote server is the exchange initiator, IKE library should respond with a TS_UNACCEPTABLE
 * Notify message. If the remote server is the exchange responder, IKE library should initiate a
 * Delete IKE exchange and close the IKE Session.
 */
public final class TsUnacceptableException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 0;

    /** Construct an instance of TsUnacceptableException. */
    public TsUnacceptableException() {
        super(ERROR_TYPE_TS_UNACCEPTABLE);
    }

    /**
     * Construct a instance of TsUnacceptableException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public TsUnacceptableException(byte[] notifyData) {
        super(ERROR_TYPE_TS_UNACCEPTABLE, notifyData);
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
