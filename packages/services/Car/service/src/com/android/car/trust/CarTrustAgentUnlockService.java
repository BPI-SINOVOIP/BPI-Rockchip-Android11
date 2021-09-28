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

package com.android.car.trust;

import static com.android.car.trust.EventLog.CLIENT_AUTHENTICATED;
import static com.android.car.trust.EventLog.RECEIVED_DEVICE_ID;
import static com.android.car.trust.EventLog.REMOTE_DEVICE_CONNECTED;
import static com.android.car.trust.EventLog.START_UNLOCK_ADVERTISING;
import static com.android.car.trust.EventLog.STOP_UNLOCK_ADVERTISING;
import static com.android.car.trust.EventLog.UNLOCK_CREDENTIALS_RECEIVED;
import static com.android.car.trust.EventLog.UNLOCK_ENCRYPTION_STATE;
import static com.android.car.trust.EventLog.UNLOCK_SERVICE_INIT;
import static com.android.car.trust.EventLog.WAITING_FOR_CLIENT_AUTH;
import static com.android.car.trust.EventLog.logUnlockEvent;

import android.annotation.IntDef;
import android.annotation.Nullable;
import android.bluetooth.BluetoothDevice;
import android.car.encryptionrunner.EncryptionRunner;
import android.car.encryptionrunner.EncryptionRunnerFactory;
import android.car.encryptionrunner.HandshakeException;
import android.car.encryptionrunner.HandshakeMessage;
import android.car.encryptionrunner.Key;
import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.android.car.BLEStreamProtos.BLEOperationProto.OperationType;
import com.android.car.PhoneAuthProtos.PhoneAuthProto.PhoneCredentials;
import com.android.car.R;
import com.android.car.Utils;
import com.android.car.protobuf.InvalidProtocolBufferException;
import com.android.car.trust.CarTrustAgentBleManager.SendMessageCallback;
import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.SignatureException;
import java.util.LinkedList;
import java.util.Queue;
import java.util.UUID;

/**
 * A service that interacts with the Trust Agent {@link CarBleTrustAgent} and a comms (BLE) service
 * {@link CarTrustAgentBleManager} to receive the necessary credentials to authenticate
 * an Android user.
 *
 * <p>
 * The unlock flow is as follows:
 * <ol>
 * <li>IHU advertises via BLE when it is in a locked state.  The advertisement includes its
 * identifier.
 * <li>Phone (Trusted device) scans, finds and connects to the IHU.
 * <li>Protocol versions are exchanged and verified.
 * <li>Phone sends its identifier in plain text.
 * <li>IHU verifies that the phone is enrolled as a trusted device from its identifier.
 * <li>IHU, then sends an ACK back to the phone.
 * <li>Phone & IHU go over the key exchange (using UKEY2) for encrypting this new session.
 * <li>Key exchange is completed without any numeric comparison.
 * <li>Phone sends its MAC (digest) that is computed from the context from this new session and the
 * previous session.
 * <li>IHU computes Phone's MAC and validates against what the phone sent.  On validation failure,
 * the stored encryption keys for the phone are deleted.  This would require the phone to re-enroll
 * again.
 * <li>IHU sends its MAC that is computed similarly from the new session and previous session
 * contexts.
 * <li>Phone computes IHU's MAC internally and validates it against what it received.
 * <li>At this point, the devices have mutually authenticated each other and also have keys to
 * encrypt
 * current session.
 * <li>IHU saves the current session keys.  This would serve for authenticating the next session.
 * <li>Phone sends the encrypted escrow token and handle to the IHU.
 * <li>IHU retrieves the user id and authenticates the user.
 * </ol>
 *
 * @deprecated Unlocking of a trusted device is no longer a supported feature of car service and
 * these APIs will be removed in the next Android release.
 */
@Deprecated
public class CarTrustAgentUnlockService {
    private static final String TAG = "CarTrustAgentUnlock";
    private static final String TRUSTED_DEVICE_UNLOCK_ENABLED_KEY = "trusted_device_unlock_enabled";

    // Arbitrary log size
    private static final int MAX_LOG_SIZE = 20;

    private static final byte[] ACKNOWLEDGEMENT_MESSAGE = "ACK".getBytes();

    // State of the unlock process.  Important to maintain the same order in both phone and IHU.
    // State increments to the next state on successful completion.
    private static final int UNLOCK_STATE_WAITING_FOR_UNIQUE_ID = 0;
    private static final int UNLOCK_STATE_KEY_EXCHANGE_IN_PROGRESS = 1;
    private static final int UNLOCK_STATE_MUTUAL_AUTH_ESTABLISHED = 2;
    private static final int UNLOCK_STATE_PHONE_CREDENTIALS_RECEIVED = 3;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = {"UNLOCK_STATE_"},
            value = {UNLOCK_STATE_WAITING_FOR_UNIQUE_ID, UNLOCK_STATE_KEY_EXCHANGE_IN_PROGRESS,
                    UNLOCK_STATE_MUTUAL_AUTH_ESTABLISHED, UNLOCK_STATE_PHONE_CREDENTIALS_RECEIVED})
    @interface UnlockState {
    }

    @UnlockState
    private int mCurrentUnlockState = UNLOCK_STATE_WAITING_FOR_UNIQUE_ID;

    private final CarTrustedDeviceService mTrustedDeviceService;
    private final CarCompanionDeviceStorage mCarCompanionDeviceStorage;
    private final CarTrustAgentBleManager mCarTrustAgentBleManager;
    private CarTrustAgentUnlockDelegate mUnlockDelegate;
    private String mClientDeviceId;
    private final Queue<String> mLogQueue = new LinkedList<>();
    private final UUID mUnlockClientWriteUuid;
    private SendMessageCallback mSendMessageCallback;

    // Locks
    private final Object mDeviceLock = new Object();

    @GuardedBy("mDeviceLock")
    private BluetoothDevice mRemoteUnlockDevice;

    private EncryptionRunner mEncryptionRunner = EncryptionRunnerFactory.newRunner();
    private Key mEncryptionKey;
    @HandshakeMessage.HandshakeState
    private int mEncryptionState = HandshakeMessage.HandshakeState.UNKNOWN;

    CarTrustAgentUnlockService(Context context, CarTrustedDeviceService service,
            CarTrustAgentBleManager bleService) {
        mTrustedDeviceService = service;
        mCarTrustAgentBleManager = bleService;
        mUnlockClientWriteUuid = UUID.fromString(context
                .getString(R.string.unlock_client_write_uuid));
        mEncryptionRunner.setIsReconnect(true);
        mSendMessageCallback = () -> mCarTrustAgentBleManager.disconnectRemoteDevice();
        mCarCompanionDeviceStorage = new CarCompanionDeviceStorage(context);
    }

    /**
     * The interface that an unlock delegate has to implement to get the auth credentials from
     * the unlock service.
     */
    interface CarTrustAgentUnlockDelegate {
        /**
         * Called when the Unlock service has the auth credentials to pass.
         *
         * @param user   user being authorized
         * @param token  escrow token for the user
         * @param handle the handle corresponding to the escrow token
         */
        void onUnlockDataReceived(int user, byte[] token, long handle);
    }

    /**
     * Enable or disable authentication of the head unit with a trusted device.
     *
     * @param isEnabled when set to {@code false}, head unit will not be
     *                  discoverable to unlock the user. Setting it to {@code true} will enable it
     *                  back.
     */
    public void setTrustedDeviceUnlockEnabled(boolean isEnabled) {
        SharedPreferences.Editor editor = mCarCompanionDeviceStorage.getSharedPrefs().edit();
        editor.putBoolean(TRUSTED_DEVICE_UNLOCK_ENABLED_KEY, isEnabled);
        if (!editor.commit()) {
            Log.wtf(TAG, "Unlock Enable Failed. Enable? " + isEnabled);
        }
    }

    /**
     * Set a delegate that implements {@link CarTrustAgentUnlockDelegate}. The delegate will be
     * handed the auth related data (token and handle) when it is received from the remote
     * trusted device. The delegate is expected to use that to authorize the user.
     */
    void setUnlockRequestDelegate(CarTrustAgentUnlockDelegate delegate) {
        mUnlockDelegate = delegate;
    }

    /**
     * Start Unlock Advertising
     */
    void startUnlockAdvertising() {
        if (!mCarCompanionDeviceStorage.getSharedPrefs().getBoolean(
                TRUSTED_DEVICE_UNLOCK_ENABLED_KEY, true)) {
            Log.e(TAG, "Trusted Device Unlock is disabled");
            return;
        }
        mTrustedDeviceService.getCarTrustAgentEnrollmentService().stopEnrollmentAdvertising();
        stopUnlockAdvertising();

        logUnlockEvent(START_UNLOCK_ADVERTISING);
        queueMessageForLog("startUnlockAdvertising");
        mCarTrustAgentBleManager.setUniqueId(mCarCompanionDeviceStorage.getUniqueId());
        mCarTrustAgentBleManager.startUnlockAdvertising();
    }

    /**
     * Stop unlock advertising
     */
    void stopUnlockAdvertising() {
        logUnlockEvent(STOP_UNLOCK_ADVERTISING);
        queueMessageForLog("stopUnlockAdvertising");
        mCarTrustAgentBleManager.stopUnlockAdvertising();
        // Also disconnect from the peer.
        if (mRemoteUnlockDevice != null) {
            mCarTrustAgentBleManager.disconnectRemoteDevice();
            mRemoteUnlockDevice = null;
        }
    }

    void init() {
        logUnlockEvent(UNLOCK_SERVICE_INIT);
        mCarTrustAgentBleManager.setupUnlockBleServer();
        mCarTrustAgentBleManager.addDataReceivedListener(mUnlockClientWriteUuid,
                this::onUnlockDataReceived);
    }

    void release() {
        synchronized (mDeviceLock) {
            mRemoteUnlockDevice = null;
        }
    }

    void onRemoteDeviceConnected(BluetoothDevice device) {
        synchronized (mDeviceLock) {
            if (mRemoteUnlockDevice != null) {
                // TBD, return when this is encountered?
                Log.e(TAG, "Unexpected: Cannot connect to another device when already connected");
            }
            queueMessageForLog("onRemoteDeviceConnected (addr:" + device.getAddress() + ")");
            logUnlockEvent(REMOTE_DEVICE_CONNECTED);
            mRemoteUnlockDevice = device;
        }
        resetEncryptionState();
        mCurrentUnlockState = UNLOCK_STATE_WAITING_FOR_UNIQUE_ID;
    }

    void onRemoteDeviceDisconnected(BluetoothDevice device) {
        // sanity checking
        if (!device.equals(mRemoteUnlockDevice) && device.getAddress() != null) {
            Log.e(TAG, "Disconnected from an unknown device:" + device.getAddress());
        }
        queueMessageForLog("onRemoteDeviceDisconnected (addr:" + device.getAddress() + ")");
        synchronized (mDeviceLock) {
            mRemoteUnlockDevice = null;
        }
        resetEncryptionState();
        mCurrentUnlockState = UNLOCK_STATE_WAITING_FOR_UNIQUE_ID;
    }

    void onUnlockDataReceived(byte[] value) {
        switch (mCurrentUnlockState) {
            case UNLOCK_STATE_WAITING_FOR_UNIQUE_ID:
                if (!CarTrustAgentValidator.isValidUnlockDeviceId(value)) {
                    Log.e(TAG, "Device Id rejected by validator.");
                    resetUnlockStateOnFailure();
                    return;
                }
                mClientDeviceId = convertToDeviceId(value);
                if (mClientDeviceId == null) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "Phone not enrolled as a trusted device");
                    }
                    resetUnlockStateOnFailure();
                    return;
                }
                logUnlockEvent(RECEIVED_DEVICE_ID);
                sendAckToClient(/* isEncrypted = */ false);
                // Next step is to wait for the client to start the encryption handshake.
                mCurrentUnlockState = UNLOCK_STATE_KEY_EXCHANGE_IN_PROGRESS;
                break;
            case UNLOCK_STATE_KEY_EXCHANGE_IN_PROGRESS:
                try {
                    processKeyExchangeHandshakeMessage(value);
                } catch (HandshakeException e) {
                    Log.e(TAG, "Handshake failure", e);
                    resetUnlockStateOnFailure();
                }
                break;
            case UNLOCK_STATE_MUTUAL_AUTH_ESTABLISHED:
                if (mEncryptionKey == null) {
                    Log.e(TAG, "Current session key null. Unexpected at this stage: "
                            + mCurrentUnlockState);
                    // Clear the previous session key.  Need to re-enroll the trusted device.
                    mCarCompanionDeviceStorage.clearEncryptionKey(mClientDeviceId);
                    resetUnlockStateOnFailure();
                    return;
                }

                // Save the current session to be used for authenticating the next session
                mCarCompanionDeviceStorage.saveEncryptionKey(mClientDeviceId,
                        mEncryptionKey.asBytes());

                byte[] decryptedCredentials;
                try {
                    decryptedCredentials = mEncryptionKey.decryptData(value);
                } catch (SignatureException e) {
                    Log.e(TAG, "Could not decrypt phone credentials.", e);
                    resetUnlockStateOnFailure();
                    return;
                }

                processCredentials(decryptedCredentials);
                mCurrentUnlockState = UNLOCK_STATE_PHONE_CREDENTIALS_RECEIVED;
                logUnlockEvent(UNLOCK_CREDENTIALS_RECEIVED);

                // Let the phone know that the token was received.
                sendAckToClient(/* isEncrypted = */ true);
                break;
            case UNLOCK_STATE_PHONE_CREDENTIALS_RECEIVED:
                // Should never get here because the unlock process should be completed now.
                Log.e(TAG, "Landed on unexpected state of credentials received.");
                break;
            default:
                Log.e(TAG, "Encountered unexpected unlock state: " + mCurrentUnlockState);
        }
    }

    private void sendAckToClient(boolean isEncrypted) {
        // Let the phone know that the handle was received.
        byte[] ack = isEncrypted ? mEncryptionKey.encryptData(ACKNOWLEDGEMENT_MESSAGE)
                : ACKNOWLEDGEMENT_MESSAGE;
        mCarTrustAgentBleManager.sendMessage(ack,
                OperationType.CLIENT_MESSAGE, /* isPayloadEncrypted= */ isEncrypted,
                mSendMessageCallback);
    }

    @Nullable
    private String convertToDeviceId(byte[] id) {
        // Validate if the id exists i.e., if the phone is enrolled already
        UUID deviceId = Utils.bytesToUUID(id);
        if (deviceId == null
                || mCarCompanionDeviceStorage.getEncryptionKey(deviceId.toString()) == null) {
            if (deviceId != null) {
                Log.e(TAG, "Unknown phone connected: " + deviceId.toString());
            }
            return null;
        }

        return deviceId.toString();
    }

    private void processKeyExchangeHandshakeMessage(byte[] message) throws HandshakeException {
        HandshakeMessage handshakeMessage;
        switch (mEncryptionState) {
            case HandshakeMessage.HandshakeState.UNKNOWN:
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "Responding to handshake init request.");
                }

                handshakeMessage = mEncryptionRunner.respondToInitRequest(message);
                mEncryptionState = handshakeMessage.getHandshakeState();
                mCarTrustAgentBleManager.sendMessage(
                        handshakeMessage.getNextMessage(),
                        OperationType.ENCRYPTION_HANDSHAKE,
                        /* isPayloadEncrypted= */ false, mSendMessageCallback);
                logUnlockEvent(UNLOCK_ENCRYPTION_STATE, mEncryptionState);
                break;

            case HandshakeMessage.HandshakeState.IN_PROGRESS:
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "Continuing handshake.");
                }

                handshakeMessage = mEncryptionRunner.continueHandshake(message);
                mEncryptionState = handshakeMessage.getHandshakeState();

                logUnlockEvent(UNLOCK_ENCRYPTION_STATE, mEncryptionState);

                if (mEncryptionState != HandshakeMessage.HandshakeState.RESUMING_SESSION) {
                    Log.e(TAG,
                            "Handshake did not went to resume session after calling verify PIN. "
                                    + "Instead got state: " + mEncryptionState);
                    resetUnlockStateOnFailure();
                    return;
                }
                logUnlockEvent(WAITING_FOR_CLIENT_AUTH);
                break;
            case HandshakeMessage.HandshakeState.RESUMING_SESSION:
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "Start reconnection authentication.");
                }
                if (mClientDeviceId == null) {
                    resetUnlockStateOnFailure();
                    return;
                }
                handshakeMessage = mEncryptionRunner.authenticateReconnection(
                        message, mCarCompanionDeviceStorage.getEncryptionKey(mClientDeviceId));
                mEncryptionKey = handshakeMessage.getKey();
                mEncryptionState = handshakeMessage.getHandshakeState();
                logUnlockEvent(UNLOCK_ENCRYPTION_STATE, mEncryptionState);
                if (mEncryptionState != HandshakeMessage.HandshakeState.FINISHED) {
                    resetUnlockStateOnFailure();
                    return;
                }
                mCurrentUnlockState = UNLOCK_STATE_MUTUAL_AUTH_ESTABLISHED;
                sendServerAuthToClient(handshakeMessage.getNextMessage());
                logUnlockEvent(CLIENT_AUTHENTICATED);
                break;
            case HandshakeMessage.HandshakeState.VERIFICATION_NEEDED:
            case HandshakeMessage.HandshakeState.FINISHED:
                // Should never reach this case since this state should occur after a verification
                // code has been accepted. But it should mean handshake is done and the message
                // is one for the escrow token. Waiting Mutual Auth from client, authenticate,
                // compute MACs and send it over
            default:
                Log.w(TAG, "Encountered invalid handshake state: " + mEncryptionState);
                break;
        }
    }

    private void sendServerAuthToClient(byte[] resumeBytes) {
        // send to client
        mCarTrustAgentBleManager.sendMessage(resumeBytes,
                OperationType.CLIENT_MESSAGE, /* isPayloadEncrypted= */false,
                mSendMessageCallback);
    }

    void processCredentials(byte[] credentials) {
        if (mUnlockDelegate == null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "No Unlock delegate to notify of unlock credentials.");
            }
            return;
        }

        queueMessageForLog("processCredentials");

        PhoneCredentials phoneCredentials;
        try {
            phoneCredentials = PhoneCredentials.parseFrom(credentials);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Error parsing credentials protobuf.", e);
            return;
        }

        byte[] handle = phoneCredentials.getHandle().toByteArray();

        mUnlockDelegate.onUnlockDataReceived(
                mCarCompanionDeviceStorage.getUserHandleByTokenHandle(Utils.bytesToLong(handle)),
                phoneCredentials.getEscrowToken().toByteArray(),
                Utils.bytesToLong(handle));
    }

    /**
     * Reset the whole unlock state.  Disconnects from the peer device
     *
     * <p>This method should be called from any stage in the middle of unlock where we
     * encounter a failure.
     */
    private void resetUnlockStateOnFailure() {
        mCarTrustAgentBleManager.disconnectRemoteDevice();
        resetEncryptionState();
    }

    /**
     * Resets the encryption status of this service.
     *
     * <p>This method should be called each time a device connects so that a new handshake can be
     * started and encryption keys exchanged.
     */
    private void resetEncryptionState() {
        mEncryptionRunner = EncryptionRunnerFactory.newRunner();
        // It should always be a reconnection for unlock because only enrolled device can unlock
        // the IHU.
        mEncryptionRunner.setIsReconnect(true);
        mEncryptionKey = null;
        mEncryptionState = HandshakeMessage.HandshakeState.UNKNOWN;
        mCurrentUnlockState = UNLOCK_STATE_WAITING_FOR_UNIQUE_ID;
    }

    void dump(PrintWriter writer) {
        writer.println("*CarTrustAgentUnlockService*");
        writer.println("Unlock Service Logs:");
        for (String log : mLogQueue) {
            writer.println("\t" + log);
        }
    }

    private void queueMessageForLog(String message) {
        if (mLogQueue.size() >= MAX_LOG_SIZE) {
            mLogQueue.remove();
        }
        mLogQueue.add(System.currentTimeMillis() + " : " + message);
    }
}
