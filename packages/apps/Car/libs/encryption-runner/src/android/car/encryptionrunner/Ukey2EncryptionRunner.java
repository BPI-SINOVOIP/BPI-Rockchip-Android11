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

import android.util.Log;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.security.cryptauth.lib.securegcm.D2DConnectionContext;
import com.google.security.cryptauth.lib.securegcm.Ukey2Handshake;
import com.google.security.cryptauth.lib.securemessage.CryptoOps;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SignatureException;

import javax.crypto.spec.SecretKeySpec;

/**
 * An {@link EncryptionRunner} that uses Ukey2 as the underlying implementation.
 */
public class Ukey2EncryptionRunner implements EncryptionRunner {

    private static final Ukey2Handshake.HandshakeCipher CIPHER =
            Ukey2Handshake.HandshakeCipher.P256_SHA512;
    private static final int RESUME_HMAC_LENGTH = 32;
    private static final byte[] RESUME = "RESUME".getBytes();
    private static final byte[] SERVER = "SERVER".getBytes();
    private static final byte[] CLIENT = "CLIENT".getBytes();
    private static final int AUTH_STRING_LENGTH = 6;

    @IntDef({Mode.UNKNOWN, Mode.CLIENT, Mode.SERVER})
    private @interface Mode {
        int UNKNOWN = 0;
        int CLIENT = 1;
        int SERVER = 2;
    }

    private Ukey2Handshake mUkey2client;
    private boolean mRunnerIsInvalid;
    private Key mCurrentKey;
    private byte[] mCurrentUniqueSesion;
    private byte[] mPrevUniqueSesion;
    private boolean mIsReconnect;
    private boolean mInitReconnectionVerification;
    @Mode
    private int mMode = Mode.UNKNOWN;

    @Override
    public HandshakeMessage initHandshake() {
        checkRunnerIsNew();
        mMode = Mode.CLIENT;
        try {
            mUkey2client = Ukey2Handshake.forInitiator(CIPHER);
            return HandshakeMessage.newBuilder()
                    .setHandshakeState(getHandshakeState())
                    .setNextMessage(mUkey2client.getNextHandshakeMessage())
                    .build();
        } catch (com.google.security.cryptauth.lib.securegcm.HandshakeException e) {
            Log.e(TAG, "unexpected exception", e);
            throw new RuntimeException(e);
        }

    }

    @Override
    public void setIsReconnect(boolean isReconnect) {
        mIsReconnect = isReconnect;
    }

    @Override
    public HandshakeMessage respondToInitRequest(byte[] initializationRequest)
            throws HandshakeException {
        checkRunnerIsNew();
        mMode = Mode.SERVER;
        try {
            if (mUkey2client != null) {
                throw new IllegalStateException("Cannot reuse encryption runners, "
                        + "this one is already initialized");
            }
            mUkey2client = Ukey2Handshake.forResponder(CIPHER);
            mUkey2client.parseHandshakeMessage(initializationRequest);
            return HandshakeMessage.newBuilder()
                    .setHandshakeState(getHandshakeState())
                    .setNextMessage(mUkey2client.getNextHandshakeMessage())
                    .build();

        } catch (com.google.security.cryptauth.lib.securegcm.HandshakeException
                | Ukey2Handshake.AlertException e) {
            throw new HandshakeException(e);
        }
    }

    private void checkRunnerIsNew() {
        if (mUkey2client != null) {
            throw new IllegalStateException("This runner is already initialized.");
        }
    }


    @Override
    public HandshakeMessage continueHandshake(byte[] response) throws HandshakeException {
        checkInitialized();
        try {
            if (mUkey2client.getHandshakeState() != Ukey2Handshake.State.IN_PROGRESS) {
                throw new IllegalStateException("handshake is not in progress, state ="
                        + mUkey2client.getHandshakeState());
            }
            mUkey2client.parseHandshakeMessage(response);

            // Not obvious from ukey2 api, but getting the next message can change the state.
            // calling getNext message might go from in progress to verification needed, on
            // the assumption that we already send this message to the peer.
            byte[] nextMessage = null;
            if (mUkey2client.getHandshakeState() == Ukey2Handshake.State.IN_PROGRESS) {
                nextMessage = mUkey2client.getNextHandshakeMessage();
            }
            String verificationCode = null;
            if (mUkey2client.getHandshakeState() == Ukey2Handshake.State.VERIFICATION_NEEDED) {
                // getVerificationString() needs to be called before verifyPin().
                verificationCode = generateReadablePairingCode(
                        mUkey2client.getVerificationString(AUTH_STRING_LENGTH));
                if (mIsReconnect) {
                    HandshakeMessage handshakeMessage = verifyPin();
                    return HandshakeMessage.newBuilder()
                            .setHandshakeState(handshakeMessage.getHandshakeState())
                            .setNextMessage(nextMessage)
                            .build();
                }
            }
            return HandshakeMessage.newBuilder()
                    .setHandshakeState(getHandshakeState())
                    .setNextMessage(nextMessage)
                    .setVerificationCode(verificationCode)
                    .build();
        } catch (com.google.security.cryptauth.lib.securegcm.HandshakeException
                | Ukey2Handshake.AlertException e) {
            throw new HandshakeException(e);
        }
    }

    /**
     * Returns a human-readable pairing code string generated from the verification bytes. Converts
     * each byte into a digit with a simple modulo.
     *
     * <p>This should match the implementation in the iOS and Android client libraries.
     */
    @VisibleForTesting
    String generateReadablePairingCode(byte[] verificationCode) {
        StringBuilder outString = new StringBuilder();
        for (byte b : verificationCode) {
            int unsignedInt = Byte.toUnsignedInt(b);
            int digit = unsignedInt % 10;
            outString.append(digit);
        }

        return outString.toString();
    }

    private static class UKey2Key implements Key {

        private final D2DConnectionContext mConnectionContext;

        UKey2Key(@NonNull D2DConnectionContext connectionContext) {
            this.mConnectionContext = connectionContext;
        }

        @Override
        public byte[] asBytes() {
            return mConnectionContext.saveSession();
        }

        @Override
        public byte[] encryptData(byte[] data) {
            return mConnectionContext.encodeMessageToPeer(data);
        }

        @Override
        public byte[] decryptData(byte[] encryptedData) throws SignatureException {
            return mConnectionContext.decodeMessageFromPeer(encryptedData);
        }

        @Override
        public byte[] getUniqueSession() throws NoSuchAlgorithmException {
            return mConnectionContext.getSessionUnique();
        }
    }

    @Override
    public HandshakeMessage verifyPin() throws HandshakeException {
        checkInitialized();
        mUkey2client.verifyHandshake();
        int state = getHandshakeState();
        try {
            mCurrentKey = new UKey2Key(mUkey2client.toConnectionContext());
        } catch (com.google.security.cryptauth.lib.securegcm.HandshakeException e) {
            throw new HandshakeException(e);
        }
        return HandshakeMessage.newBuilder()
                .setHandshakeState(state)
                .setKey(mCurrentKey)
                .build();
    }

    /**
     * <p>After getting message from the other device, authenticate the message with the previous
     * stored key.
     *
     * If current device inits the reconnection authentication by calling {@code
     * initReconnectAuthentication} and sends the message to the other device, the other device
     * will call {@code authenticateReconnection()} with the received message and send its own
     * message back to the init device. The init device will call {@code
     * authenticateReconnection()} on the received message, but do not need to set the next
     * message.
     */
    @Override
    public HandshakeMessage authenticateReconnection(byte[] message, byte[] previousKey)
            throws HandshakeException {
        if (!mIsReconnect) {
            throw new HandshakeException(
                    "Reconnection authentication requires setIsReconnect(true)");
        }
        if (mCurrentKey == null) {
            throw new HandshakeException("Current key is null, make sure verifyPin() is called.");
        }
        if (message.length != RESUME_HMAC_LENGTH) {
            mRunnerIsInvalid = true;
            throw new HandshakeException("Failing because (message.length =" + message.length
                    + ") is not equal to " + RESUME_HMAC_LENGTH);
        }
        try {
            mCurrentUniqueSesion = mCurrentKey.getUniqueSession();
            mPrevUniqueSesion = keyOf(previousKey).getUniqueSession();
        } catch (NoSuchAlgorithmException e) {
            throw new HandshakeException(e);
        }
        switch (mMode) {
            case Mode.SERVER:
                if (!MessageDigest.isEqual(
                        message, computeMAC(mPrevUniqueSesion, mCurrentUniqueSesion, CLIENT))) {
                    mRunnerIsInvalid = true;
                    throw new HandshakeException("Reconnection authentication failed.");
                }
                return HandshakeMessage.newBuilder()
                        .setHandshakeState(HandshakeMessage.HandshakeState.FINISHED)
                        .setKey(mCurrentKey)
                        .setNextMessage(mInitReconnectionVerification ? null
                                : computeMAC(mPrevUniqueSesion, mCurrentUniqueSesion, SERVER))
                        .build();
            case Mode.CLIENT:
                if (!MessageDigest.isEqual(
                        message, computeMAC(mPrevUniqueSesion, mCurrentUniqueSesion, SERVER))) {
                    mRunnerIsInvalid = true;
                    throw new HandshakeException("Reconnection authentication failed.");
                }
                return HandshakeMessage.newBuilder()
                        .setHandshakeState(HandshakeMessage.HandshakeState.FINISHED)
                        .setKey(mCurrentKey)
                        .setNextMessage(mInitReconnectionVerification ? null
                                : computeMAC(mPrevUniqueSesion, mCurrentUniqueSesion, CLIENT))
                        .build();
            default:
                throw new IllegalStateException(
                        "Encountered unexpected role during authenticateReconnection: " + mMode);
        }
    }

    /**
     * Both client and server can call this method to send authentication message to the other
     * device.
     */
    @Override
    public HandshakeMessage initReconnectAuthentication(byte[] previousKey)
            throws HandshakeException {
        if (!mIsReconnect) {
            throw new HandshakeException(
                    "Reconnection authentication requires setIsReconnect(true).");
        }
        if (mCurrentKey == null) {
            throw new HandshakeException("Current key is null, make sure verifyPin() is called.");
        }
        mInitReconnectionVerification = true;
        try {
            mCurrentUniqueSesion = mCurrentKey.getUniqueSession();
            mPrevUniqueSesion = keyOf(previousKey).getUniqueSession();
        } catch (NoSuchAlgorithmException e) {
            throw new HandshakeException(e);
        }
        switch (mMode) {
            case Mode.SERVER:
                return HandshakeMessage.newBuilder()
                        .setHandshakeState(HandshakeMessage.HandshakeState.RESUMING_SESSION)
                        .setNextMessage(computeMAC(mPrevUniqueSesion, mCurrentUniqueSesion, SERVER))
                        .build();
            case Mode.CLIENT:
                return HandshakeMessage.newBuilder()
                        .setHandshakeState(HandshakeMessage.HandshakeState.RESUMING_SESSION)
                        .setNextMessage(computeMAC(mPrevUniqueSesion, mCurrentUniqueSesion, CLIENT))
                        .build();
            default:
                throw new IllegalStateException(
                        "Encountered unexpected role during authenticateReconnection: " + mMode);
        }
    }

    protected final Ukey2Handshake getUkey2Client() {
        return mUkey2client;
    }

    protected final boolean isReconnect() {
        return mIsReconnect;
    }

    @HandshakeMessage.HandshakeState
    private int getHandshakeState() {
        checkInitialized();
        switch (mUkey2client.getHandshakeState()) {
            case ALREADY_USED:
            case ERROR:
                throw new IllegalStateException("unexpected error state");
            case FINISHED:
                if (mIsReconnect) {
                    return HandshakeMessage.HandshakeState.RESUMING_SESSION;
                }
                return HandshakeMessage.HandshakeState.FINISHED;
            case IN_PROGRESS:
                return HandshakeMessage.HandshakeState.IN_PROGRESS;
            case VERIFICATION_IN_PROGRESS:
            case VERIFICATION_NEEDED:
                return HandshakeMessage.HandshakeState.VERIFICATION_NEEDED;
            default:
                throw new IllegalStateException("unexpected handshake state");
        }
    }

    @Override
    public Key keyOf(byte[] serialized) {
        return new UKey2Key(D2DConnectionContext.fromSavedSession(serialized));
    }

    @Override
    public void invalidPin() {
        mRunnerIsInvalid = true;
    }

    private UKey2Key checkIsUkey2Key(Key key) {
        if (!(key instanceof UKey2Key)) {
            throw new IllegalArgumentException("wrong key type");
        }
        return (UKey2Key) key;
    }

    protected void checkInitialized() {
        if (mUkey2client == null) {
            throw new IllegalStateException("runner not initialized");
        }
        if (mRunnerIsInvalid) {
            throw new IllegalStateException("runner has been invalidated");
        }
    }

    @Nullable
    private byte[] computeMAC(byte[] previous, byte[] next, byte[] info) {
        try {
            SecretKeySpec inputKeyMaterial = new SecretKeySpec(
                    concatByteArrays(previous, next), "" /* key type is just plain raw bytes */);
            return CryptoOps.hkdf(inputKeyMaterial, RESUME, info);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            // Does not happen in practice
            Log.e(TAG, "Compute MAC failed");
            return null;
        }
    }

    private static byte[] concatByteArrays(@NonNull byte[] a, @NonNull byte[] b) {
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        try {
            outputStream.write(a);
            outputStream.write(b);
        } catch (IOException e) {
            return new byte[0];
        }
        return outputStream.toByteArray();
    }
}
