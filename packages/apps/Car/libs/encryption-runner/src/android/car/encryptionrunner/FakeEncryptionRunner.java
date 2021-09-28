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

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An encryption runner that doesn't actually do encryption. Useful for debugging. Do not use in
 * production environments.
 */
@VisibleForTesting
public class FakeEncryptionRunner implements EncryptionRunner {

    private static final String KEY = "key";
    private static final byte[] PLACEHOLDER_MESSAGE = "Placeholder Message".getBytes();
    @VisibleForTesting
    public static final String INIT = "init";
    @VisibleForTesting
    public static final String INIT_RESPONSE = "initResponse";
    @VisibleForTesting
    public static final String CLIENT_RESPONSE = "clientResponse";
    public static final String VERIFICATION_CODE = "1234";

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Mode.UNKNOWN, Mode.CLIENT, Mode.SERVER})
    @interface Mode {

        int UNKNOWN = 0;
        int CLIENT = 1;
        int SERVER = 2;
    }

    private boolean mIsReconnect;
    private boolean mInitReconnectVerification;
    private Key mCurrentFakeKey;
    @Mode
    private int mMode;
    @HandshakeMessage.HandshakeState
    private int mState;

    @Override
    public HandshakeMessage initHandshake() {
        checkRunnerIsNew();
        mMode = Mode.CLIENT;
        mState = HandshakeMessage.HandshakeState.IN_PROGRESS;
        return HandshakeMessage.newBuilder()
                .setHandshakeState(mState)
                .setNextMessage(INIT.getBytes())
                .build();
    }

    @Override
    public HandshakeMessage respondToInitRequest(byte[] initializationRequest)
            throws HandshakeException {
        checkRunnerIsNew();
        mMode = Mode.SERVER;
        if (!new String(initializationRequest).equals(INIT)) {
            throw new HandshakeException("Unexpected initialization request");
        }
        mState = HandshakeMessage.HandshakeState.IN_PROGRESS;
        return HandshakeMessage.newBuilder()
                .setHandshakeState(HandshakeMessage.HandshakeState.IN_PROGRESS)
                .setNextMessage(INIT_RESPONSE.getBytes())
                .build();
    }

    @Mode
    protected int getMode() {
        return mMode;
    }

    @HandshakeMessage.HandshakeState
    protected int getState() {
        return mState;
    }

    protected void setState(@HandshakeMessage.HandshakeState int state) {
        mState = state;
    }

    private void checkRunnerIsNew() {
        if (mState != HandshakeMessage.HandshakeState.UNKNOWN) {
            throw new IllegalStateException("runner already initialized.");
        }
    }

    @Override
    public HandshakeMessage continueHandshake(byte[] response) throws HandshakeException {
        if (mState != HandshakeMessage.HandshakeState.IN_PROGRESS) {
            throw new HandshakeException("not waiting for response but got one");
        }
        switch (mMode) {
            case Mode.SERVER:
                if (!CLIENT_RESPONSE.equals(new String(response))) {
                    throw new HandshakeException("unexpected response: " + new String(response));
                }
                mState = HandshakeMessage.HandshakeState.VERIFICATION_NEEDED;
                if (mIsReconnect) {
                    verifyPin();
                    mState = HandshakeMessage.HandshakeState.RESUMING_SESSION;
                }
                return HandshakeMessage.newBuilder()
                        .setVerificationCode(VERIFICATION_CODE)
                        .setHandshakeState(mState)
                        .build();
            case Mode.CLIENT:
                if (!INIT_RESPONSE.equals(new String(response))) {
                    throw new HandshakeException("unexpected response: " + new String(response));
                }
                mState = HandshakeMessage.HandshakeState.VERIFICATION_NEEDED;
                if (mIsReconnect) {
                    verifyPin();
                    mState = HandshakeMessage.HandshakeState.RESUMING_SESSION;
                }
                return HandshakeMessage.newBuilder()
                        .setHandshakeState(mState)
                        .setNextMessage(CLIENT_RESPONSE.getBytes())
                        .setVerificationCode(VERIFICATION_CODE)
                        .build();
            default:
                throw new IllegalStateException("unexpected role: " + mMode);
        }
    }

    @Override
    public HandshakeMessage authenticateReconnection(byte[] message, byte[] previousKey)
            throws HandshakeException {
        mCurrentFakeKey = new FakeKey();
        // Blindly verify the reconnection because this is a fake encryption runner.
        return HandshakeMessage.newBuilder()
                .setHandshakeState(HandshakeMessage.HandshakeState.FINISHED)
                .setKey(mCurrentFakeKey)
                .setNextMessage(mInitReconnectVerification ? null : PLACEHOLDER_MESSAGE)
                .build();
    }

    @Override
    public HandshakeMessage initReconnectAuthentication(byte[] previousKey)
            throws HandshakeException {
        mInitReconnectVerification = true;
        mState = HandshakeMessage.HandshakeState.RESUMING_SESSION;
        return HandshakeMessage.newBuilder()
                .setHandshakeState(mState)
                .setNextMessage(PLACEHOLDER_MESSAGE)
                .build();
    }

    @Override
    public Key keyOf(byte[] serialized) {
        return new FakeKey();
    }

    @Override
    public HandshakeMessage verifyPin() throws HandshakeException {
        if (mState != HandshakeMessage.HandshakeState.VERIFICATION_NEEDED) {
            throw new IllegalStateException("asking to verify pin, state = " + mState);
        }
        mState = HandshakeMessage.HandshakeState.FINISHED;
        return HandshakeMessage.newBuilder().setKey(new FakeKey()).setHandshakeState(
                mState).build();
    }

    @Override
    public void invalidPin() {
        mState = HandshakeMessage.HandshakeState.INVALID;
    }

    @Override
    public void setIsReconnect(boolean isReconnect) {
        mIsReconnect = isReconnect;
    }

    /** Method to throw a HandshakeException for testing. */
    public static void throwHandshakeException(String message) throws HandshakeException {
        throw new HandshakeException(message);
    }

    class FakeKey implements Key {
        @Override
        public byte[] asBytes() {
            return KEY.getBytes();
        }

        @Override
        public byte[] encryptData(byte[] data) {
            return data;
        }

        @Override
        public byte[] decryptData(byte[] encryptedData) {
            return encryptedData;
        }

        @Override
        public byte[] getUniqueSession() {
            return KEY.getBytes();
        }
    }
}
