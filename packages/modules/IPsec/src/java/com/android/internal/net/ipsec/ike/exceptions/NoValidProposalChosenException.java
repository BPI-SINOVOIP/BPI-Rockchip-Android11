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

import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_NO_PROPOSAL_CHOSEN;

import android.net.ipsec.ike.exceptions.IkeProtocolException;

/**
 * This exception is thrown if either none of SA proposals from SA initiator is acceptable or the
 * negotiated SA proposal from SA responder is invalid.
 *
 * <p>Include the NO_PROPOSAL_CHOSEN Notify payload in an encrypted response message if received
 * message is an encrypted request from SA initiator.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-2.7">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class NoValidProposalChosenException extends IkeProtocolException {
    private static final int EXPECTED_ERROR_DATA_LEN = 0;

    /**
     * Construct an instance of NoValidProposalChosenException.
     *
     * @param message the descriptive message.
     */
    public NoValidProposalChosenException(String message) {
        super(ERROR_TYPE_NO_PROPOSAL_CHOSEN, message);
    }

    /**
     * Construct an instance of NoValidProposalChosenException.
     *
     * @param message the descriptive message.
     * @param cause the reason of exception.
     */
    public NoValidProposalChosenException(String message, Throwable cause) {
        super(ERROR_TYPE_NO_PROPOSAL_CHOSEN, cause);
    }

    /**
     * Construct a instance of NoValidProposalChosenException from a notify payload.
     *
     * @param notifyData the notify data included in the payload.
     */
    public NoValidProposalChosenException(byte[] notifyData) {
        super(ERROR_TYPE_NO_PROPOSAL_CHOSEN, notifyData);
    }

    @Override
    protected boolean isValidDataLength(int dataLen) {
        return EXPECTED_ERROR_DATA_LEN == dataLen;
    }
}
