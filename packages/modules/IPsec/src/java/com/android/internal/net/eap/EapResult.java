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

package com.android.internal.net.eap;

import android.annotation.NonNull;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.eap.exceptions.InvalidEapResponseException;
import com.android.internal.net.eap.message.EapMessage;

/**
 * EapResult represents the return type R for a process operation within the EapStateMachine.
 */
public abstract class EapResult {

    /**
     * EapSuccess represents a success response from the EapStateMachine.
     *
     * @see <a href="https://tools.ietf.org/html/rfc3748">RFC 3748, Extensible Authentication
     * Protocol (EAP)</a>
     */
    public static class EapSuccess extends EapResult {
        public final byte[] msk;
        public final byte[] emsk;

        public EapSuccess(@NonNull byte[] msk, @NonNull byte[] emsk) {
            if (msk == null || emsk == null) {
                throw new IllegalArgumentException("msk and emsk must not be null");
            }
            this.msk = msk;
            this.emsk = emsk;
        }
    }

    /**
     * EapFailure represents a failure response from the EapStateMachine.
     *
     * @see <a href="https://tools.ietf.org/html/rfc3748">RFC 3748, Extensible Authentication
     * Protocol (EAP)</a>
     */
    public static class EapFailure extends EapResult {}

    /**
     * EapResponse represents an outgoing message from the EapStateMachine.
     *
     * @see <a href="https://tools.ietf.org/html/rfc3748">RFC 3748, Extensible Authentication
     * Protocol (EAP)</a>
     */
    public static class EapResponse extends EapResult {
        public final byte[] packet;

        @VisibleForTesting
        protected EapResponse(byte[] packet) {
            this.packet = packet;
        }

        /**
         * Constructs and returns an EapResult for the given EapMessage.
         *
         * <p>If the given EapMessage is not of type EAP-Response, an EapError object will be
         * returned.
         *
         * @param message the EapMessage to be encoded in the EapResponse instance.
         * @return an EapResponse instance for the given message. If message.eapCode != {@link
         * EapMessage#EAP_CODE_RESPONSE}, an EapError instance is returned.
         */
        public static EapResult getEapResponse(@NonNull EapMessage message) {
            if (message == null) {
                throw new IllegalArgumentException("EapMessage should not be null");
            } else if (message.eapCode != EapMessage.EAP_CODE_RESPONSE) {
                return new EapError(new InvalidEapResponseException(
                        "Cannot construct an EapResult from a non-EAP-Response message"));
            }

            return new EapResponse(message.encode());
        }
    }

    /**
     * EapError represents an error that occurred in the EapStateMachine.
     *
     * @see <a href="https://tools.ietf.org/html/rfc3748">RFC 3748, Extensible Authentication
     * Protocol (EAP)</a>
     */
    public static class EapError extends EapResult {
        public final Exception cause;

        /**
         * Constructs an EapError instance for the given cause.
         *
         * @param cause the Exception that caused the EapError to be returned from the
         *              EapStateMachine
         */
        public EapError(Exception cause) {
            this.cause = cause;
        }
    }
}
