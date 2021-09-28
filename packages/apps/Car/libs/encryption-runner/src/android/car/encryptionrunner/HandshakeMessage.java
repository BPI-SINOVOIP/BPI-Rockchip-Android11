/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.car.encryptionrunner;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * During an {@link EncryptionRunner} handshake process, these are the messages returned as part
 * of each step.
 */
public class HandshakeMessage {

    /**
     * States for handshake progress.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({HandshakeState.UNKNOWN, HandshakeState.IN_PROGRESS, HandshakeState.VERIFICATION_NEEDED,
            HandshakeState.FINISHED, HandshakeState.INVALID, HandshakeState.RESUMING_SESSION,
            HandshakeState.OOB_VERIFICATION_NEEDED})
    public @interface HandshakeState {
        /**
         * The initial state, this value is not expected to be returned.
         */
        int UNKNOWN = 0;
        /**
         * The handshake is in progress.
         */
        int IN_PROGRESS = 1;
        /**
         * The handshake is complete, but verification of the code is needed.
         */
        int VERIFICATION_NEEDED = 2;
        /**
         * The handshake is complete.
         */
        int FINISHED = 3;
        /**
         * The handshake is complete and not successful.
         */
        int INVALID = 4;
        /**
         * The handshake is complete, but extra verification is needed.
         */
        int RESUMING_SESSION = 5;
        /**
         * The handshake is complete, but out of band verification of the code is needed.
         */
        int OOB_VERIFICATION_NEEDED = 6;
    }

    @HandshakeState
    private final int mHandshakeState;
    private final Key mKey;
    private final byte[] mNextMessage;
    private final String mVerificationCode;
    private final byte[] mOobVerificationCode;

    /**
     * @return Returns a builder for {@link HandshakeMessage}.
     */
    public static Builder newBuilder() {
        return new Builder();
    }

    /**
     * Use the builder;
     */
    private HandshakeMessage(
            @HandshakeState int handshakeState,
            @Nullable Key key,
            @Nullable byte[] nextMessage,
            @Nullable String verificationCode,
            @Nullable byte[] oobVerificationCode) {
        mHandshakeState = handshakeState;
        mKey = key;
        mNextMessage = nextMessage;
        mVerificationCode = verificationCode;
        mOobVerificationCode = oobVerificationCode;
    }

    /**
     * Returns the next message to send in a handshake.
     */
    @Nullable
    public byte[] getNextMessage() {
        return mNextMessage == null ? null : mNextMessage.clone();
    }

    /**
     * Returns the state of the handshake.
     */
    @HandshakeState
    public int getHandshakeState() {
        return mHandshakeState;
    }

    /**
     * Returns the encryption key that can be used to encrypt data.
     */
    @Nullable
    public Key getKey() {
        return mKey;
    }

    /**
     * Returns a verification code to show to the user.
     */
    @Nullable
    public String getVerificationCode() {
        return mVerificationCode;
    }

    /**
     * Returns a verification code to be encrypted using an out-of-band key and sent to the remote
     * device.
     */
    @Nullable
    public byte[] getOobVerificationCode() {
        return mOobVerificationCode;
    }

    static class Builder {
        @HandshakeState
        int mHandshakeState;
        Key mKey;
        byte[] mNextMessage;
        String mVerificationCode;
        byte[] mOobVerificationCode;

        Builder setHandshakeState(@HandshakeState int handshakeState) {
            mHandshakeState = handshakeState;
            return this;
        }

        Builder setKey(@Nullable Key key) {
            mKey = key;
            return this;
        }

        Builder setNextMessage(@Nullable byte[] nextMessage) {
            mNextMessage = nextMessage == null ? null : nextMessage.clone();
            return this;
        }

        Builder setVerificationCode(@Nullable String verificationCode) {
            mVerificationCode = verificationCode;
            return this;
        }

        Builder setOobVerificationCode(@NonNull byte[] oobVerificationCode) {
            mOobVerificationCode = oobVerificationCode;
            return this;
        }

        HandshakeMessage build() {
            if (mHandshakeState == HandshakeState.UNKNOWN) {
                throw new IllegalStateException("must set handshake state before calling build");
            }
            if (mHandshakeState == HandshakeState.VERIFICATION_NEEDED
                    && TextUtils.isEmpty(mVerificationCode)) {
                throw new IllegalStateException(
                        "State is verification needed, but verification code null.");
            }
            if (mHandshakeState == HandshakeState.OOB_VERIFICATION_NEEDED
                    && (mOobVerificationCode == null || mOobVerificationCode.length == 0)) {
                throw new IllegalStateException(
                        "State is OOB verification needed, but OOB verification code null.");
            }
            return new HandshakeMessage(mHandshakeState, mKey, mNextMessage, mVerificationCode,
                    mOobVerificationCode);
        }

    }
}
