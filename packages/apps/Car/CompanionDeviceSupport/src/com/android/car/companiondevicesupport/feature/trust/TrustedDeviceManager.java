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

package com.android.car.companiondevicesupport.feature.trust;

import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.connecteddevice.util.SafeLog.logw;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.UserHandle;

import androidx.core.content.ContextCompat;
import androidx.room.Room;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IConnectedDeviceManager;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceEnrollmentCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceAgentDelegate;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceManager;
import com.android.car.companiondevicesupport.api.internal.trust.TrustedDevice;
import com.android.car.companiondevicesupport.feature.trust.storage.TrustedDeviceDao;
import com.android.car.companiondevicesupport.feature.trust.storage.TrustedDeviceDatabase;
import com.android.car.companiondevicesupport.feature.trust.storage.TrustedDeviceEntity;
import com.android.car.companiondevicesupport.feature.trust.ui.TrustedDeviceActivity;
import com.android.car.companiondevicesupport.protos.PhoneAuthProto.PhoneCredentials;
import com.android.car.companiondevicesupport.protos.TrustedDeviceMessageProto.TrustedDeviceError;
import com.android.car.companiondevicesupport.protos.TrustedDeviceMessageProto.TrustedDeviceError.ErrorType;
import com.android.car.companiondevicesupport.protos.TrustedDeviceMessageProto.TrustedDeviceMessage.MessageType;
import com.android.car.companiondevicesupport.protos.TrustedDeviceMessageProto.TrustedDeviceMessage;
import com.android.car.connecteddevice.util.ByteUtils;
import com.android.car.connecteddevice.util.RemoteCallbackBinder;
import com.android.car.protobuf.ByteString;
import com.android.car.protobuf.InvalidProtocolBufferException;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArraySet;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;


/** Manager for the feature of unlocking the head unit with a user's trusted device. */
public class TrustedDeviceManager extends ITrustedDeviceManager.Stub {
    private static final String TAG = "TrustedDeviceManager";

    private static final String CHANNEL_ID = "trusteddevice_notification_channel";

    private static final int ENROLLMENT_NOTIFICATION_ID = 0;

    private static final int TRUSTED_DEVICE_MESSAGE_VERSION = 2;

    /** Length of token generated on a trusted device. */
    private static final int ESCROW_TOKEN_LENGTH = 8;

    private final Map<IBinder, ITrustedDeviceCallback> mTrustedDeviceCallbacks =
            new ConcurrentHashMap<>();

    private final Map<IBinder, ITrustedDeviceEnrollmentCallback> mEnrollmentCallbacks
            = new ConcurrentHashMap<>();

    private final Map<IBinder, IDeviceAssociationCallback> mAssociatedDeviceCallbacks =
            new ConcurrentHashMap<>();

    private final Set<RemoteCallbackBinder> mCallbackBinders = new CopyOnWriteArraySet<>();

    private final Context mContext;

    private final TrustedDeviceFeature mTrustedDeviceFeature;

    private final Executor mExecutor = Executors.newSingleThreadExecutor();

    private final AtomicBoolean mIsWaitingForCredentials = new AtomicBoolean(false);

    private final NotificationManager mNotificationManager;

    private TrustedDeviceDao mDatabase;

    private ITrustedDeviceAgentDelegate mTrustAgentDelegate;

    private CompanionDevice mPendingDevice;

    private byte[] mPendingToken;

    private PendingCredentials mPendingCredentials;

    TrustedDeviceManager(@NonNull Context context) {
        mContext = context;
        mTrustedDeviceFeature = new TrustedDeviceFeature(context);
        mTrustedDeviceFeature.setCallback(mFeatureCallback);
        mTrustedDeviceFeature.setAssociatedDeviceCallback(mAssociatedDeviceCallback);
        mTrustedDeviceFeature.start();
        mDatabase = Room.databaseBuilder(context, TrustedDeviceDatabase.class,
                TrustedDeviceDatabase.DATABASE_NAME).build().trustedDeviceDao();
        mNotificationManager = (NotificationManager) mContext.
                getSystemService(Context.NOTIFICATION_SERVICE);
        String channelName = mContext.getString(R.string.trusted_device_notification_channel_name);
        NotificationChannel channel = new NotificationChannel(CHANNEL_ID, channelName,
                NotificationManager.IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(channel);
        logd(TAG, "TrustedDeviceManager created successfully.");
    }

    void cleanup() {
        mPendingToken = null;
        mPendingDevice = null;
        mPendingCredentials = null;
        mIsWaitingForCredentials.set(false);
        mTrustedDeviceCallbacks.clear();
        mEnrollmentCallbacks.clear();
        mAssociatedDeviceCallbacks.clear();
        mTrustedDeviceFeature.stop();
    }

    private void startEnrollment(@NonNull CompanionDevice device, @NonNull byte[] token) {
        logd(TAG, "Starting trusted device enrollment process.");
        mPendingDevice = device;
        showEnrollmentNotification();

        mPendingToken = token;
        if (mTrustAgentDelegate == null) {
            logd(TAG, "No trust agent delegate has been set yet. No further enrollment action "
                    + "can be taken at this time.");
            return;
        }

        try {
            mTrustAgentDelegate.addEscrowToken(token, ActivityManager.getCurrentUser());
        } catch (RemoteException e) {
            loge(TAG, "Error while adding token through delegate.", e);
        }
    }

    private void showEnrollmentNotification() {
        UserHandle currentUser = UserHandle.of(ActivityManager.getCurrentUser());
        Intent enrollmentIntent = new Intent(mContext, TrustedDeviceActivity.class)
                // Setting this action ensures that the TrustedDeviceActivity is resumed if it is
                // already running.
                .setAction("com.android.settings.action.EXTRA_SETTINGS")
                .putExtra(TrustedDeviceConstants.INTENT_EXTRA_ENROLL_NEW_TOKEN, true)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        PendingIntent pendingIntent = PendingIntent
                .getActivityAsUser(mContext, /* requestCode = */ 0, enrollmentIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT, null, currentUser);
        Notification notification = new Notification.Builder(mContext, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_directions_car_filled)
                .setColor(ContextCompat.getColor(mContext, R.color.car_red_300))
                .setContentTitle(mContext.getString(R.string.trusted_device_notification_title))
                .setContentText(mContext.getString(R.string.trusted_device_notification_content))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .build();
        mNotificationManager.notifyAsUser(/* tag = */ null, ENROLLMENT_NOTIFICATION_ID,
                notification, currentUser);
    }

    private void unlockUser(@NonNull String deviceId, @NonNull PhoneCredentials credentials) {
        logd(TAG, "Unlocking with credentials.");
        try {
            mTrustAgentDelegate.unlockUserWithToken(credentials.getEscrowToken().toByteArray(),
                    ByteUtils.bytesToLong(credentials.getHandle().toByteArray()),
                    ActivityManager.getCurrentUser());
            mTrustedDeviceFeature.sendMessageSecurely(deviceId, createAcknowledgmentMessage());
        } catch (RemoteException e) {
            loge(TAG, "Error while unlocking user through delegate.", e);
        }
    }

    @Override
    public void onEscrowTokenAdded(int userId, long handle) {
        logd(TAG, "Escrow token has been successfully added.");
        mPendingToken = null;

        if (mEnrollmentCallbacks.size() == 0) {
            mIsWaitingForCredentials.set(true);
            return;
        }

        mIsWaitingForCredentials.set(false);
        notifyEnrollmentCallbacks(callback -> {
            try {
                callback.onValidateCredentialsRequest();
            } catch (RemoteException e) {
                loge(TAG, "Error while requesting credential validation.", e);
            }
        });
    }

    @Override
    public void onEscrowTokenActivated(int userId, long handle) {
        if (mPendingDevice == null) {
            loge(TAG, "Unable to complete device enrollment. Pending device was null.");
            return;
        }

        logd(TAG, "Enrollment completed successfully! Sending handle to connected device and "
                + "persisting trusted device record.");

        mTrustedDeviceFeature.sendMessageSecurely(mPendingDevice, createHandleMessage(handle));

        String deviceId = mPendingDevice.getDeviceId();

        TrustedDeviceEntity entity = new TrustedDeviceEntity();
        entity.id = deviceId;
        entity.userId = userId;
        entity.handle = handle;
        mDatabase.addOrReplaceTrustedDevice(entity);

        mPendingDevice = null;

        TrustedDevice trustedDevice = new TrustedDevice(deviceId, userId, handle);
        notifyTrustedDeviceCallbacks(callback -> {
            try {
                callback.onTrustedDeviceAdded(trustedDevice);
            } catch (RemoteException e) {
                loge(TAG, "Failed to notify that enrollment completed successfully.", e);
            }
        });
    }

    @Override
    public List<TrustedDevice> getTrustedDevicesForActiveUser() {
        List<TrustedDeviceEntity> foundEntities =
                mDatabase.getTrustedDevicesForUser(ActivityManager.getCurrentUser());

        List<TrustedDevice> trustedDevices = new ArrayList<>();
        if (foundEntities == null) {
            return trustedDevices;
        }

        for (TrustedDeviceEntity entity : foundEntities) {
            trustedDevices.add(entity.toTrustedDevice());
        }

        return trustedDevices;
    }

    @Override
    public void removeTrustedDevice(TrustedDevice trustedDevice) {
        if (mTrustAgentDelegate == null) {
            loge(TAG, "No TrustAgent delegate has been set. Unable to remove trusted device.");
            return;
        }

        try {
            mTrustAgentDelegate.removeEscrowToken(trustedDevice.getHandle(),
                    trustedDevice.getUserId());
            mDatabase.removeTrustedDevice(new TrustedDeviceEntity(trustedDevice));
        } catch (RemoteException e) {
            loge(TAG, "Error while removing token through delegate.", e);
            return;
        }
        notifyTrustedDeviceCallbacks(callback -> {
            try {
                callback.onTrustedDeviceRemoved(trustedDevice);
            } catch (RemoteException e) {
                loge(TAG, "Failed to notify that a trusted device has been removed.", e);
            }
        });
    }

    @Override
    public List<CompanionDevice> getActiveUserConnectedDevices() {
        List<CompanionDevice> devices = new ArrayList<>();
        IConnectedDeviceManager manager = mTrustedDeviceFeature.getConnectedDeviceManager();
        if (manager == null) {
            loge(TAG, "Unable to get connected devices. Service not connected. ");
            return devices;
        }
        try {
            devices = manager.getActiveUserConnectedDevices();
        } catch (RemoteException e) {
            loge(TAG, "Failed to get connected devices. ", e);
        }
        return devices;
    }

    @Override
    public void initiateEnrollment(String deviceId) {
        mTrustedDeviceFeature.sendMessageSecurely(deviceId, createStartEnrollmentMessage());
    }

    @Override
    public void registerTrustedDeviceCallback(ITrustedDeviceCallback callback)  {
        mTrustedDeviceCallbacks.put(callback.asBinder(), callback);
        RemoteCallbackBinder remoteBinder = new RemoteCallbackBinder(callback.asBinder(), iBinder ->
                unregisterTrustedDeviceCallback(callback));
        mCallbackBinders.add(remoteBinder);
    }

    @Override
    public void unregisterTrustedDeviceCallback(ITrustedDeviceCallback callback) {
        IBinder binder = callback.asBinder();
        mTrustedDeviceCallbacks.remove(binder);
        removeRemoteBinder(binder);
    }

    @Override
    public void registerAssociatedDeviceCallback(IDeviceAssociationCallback callback) {
        mAssociatedDeviceCallbacks.put(callback.asBinder(), callback);
        RemoteCallbackBinder remoteBinder = new RemoteCallbackBinder(callback.asBinder(), iBinder ->
                unregisterAssociatedDeviceCallback(callback));
        mCallbackBinders.add(remoteBinder);
    }

    @Override
    public void unregisterAssociatedDeviceCallback(IDeviceAssociationCallback callback) {
        IBinder binder = callback.asBinder();
        mAssociatedDeviceCallbacks.remove(binder);
        removeRemoteBinder(binder);
    }

    @Override
    public void registerTrustedDeviceEnrollmentCallback(
            ITrustedDeviceEnrollmentCallback callback) {
        mEnrollmentCallbacks.put(callback.asBinder(), callback);
        RemoteCallbackBinder remoteBinder = new RemoteCallbackBinder(callback.asBinder(),
                iBinder -> unregisterTrustedDeviceEnrollmentCallback(callback));
        mCallbackBinders.add(remoteBinder);
        // A token has been added and is waiting on user credential validation.
        if (mIsWaitingForCredentials.getAndSet(false)) {
            mExecutor.execute(() -> {
                try {
                    callback.onValidateCredentialsRequest();
                } catch (RemoteException e) {
                    loge(TAG, "Error while notifying enrollment listener.", e);
                }
            });
        }
    }

    @Override
    public void unregisterTrustedDeviceEnrollmentCallback(
            ITrustedDeviceEnrollmentCallback callback) {
        IBinder binder = callback.asBinder();
        mEnrollmentCallbacks.remove(binder);
        removeRemoteBinder(binder);
    }

    @Override
    public void setTrustedDeviceAgentDelegate(ITrustedDeviceAgentDelegate trustAgentDelegate) {
        logd(TAG, "Set trusted device agent delegate: " + trustAgentDelegate + ".");
        mTrustAgentDelegate = trustAgentDelegate;

        // Add pending token if present.
        if (mPendingToken != null) {
            try {
                trustAgentDelegate.addEscrowToken(mPendingToken, ActivityManager.getCurrentUser());
            } catch (RemoteException e) {
                loge(TAG, "Error while adding token through delegate.", e);
            }
            return;
        }

        // Unlock with pending credentials if present.
        if (mPendingCredentials != null) {
            unlockUser(mPendingCredentials.mDeviceId, mPendingCredentials.mPhoneCredentials);
            mPendingCredentials = null;
        }
    }

    @Override
    public void clearTrustedDeviceAgentDelegate(ITrustedDeviceAgentDelegate trustAgentDelegate) {
        if (trustAgentDelegate.asBinder() != mTrustAgentDelegate.asBinder()) {
            logd(TAG, "TrustedDeviceAgentDelegate " + trustAgentDelegate + " doesn't match the " +
                    "current TrustedDeviceAgentDelegate: " + mTrustAgentDelegate +
                    ". Ignoring call to clear.");
            return;
        }
        logd(TAG, "Clear current TrustedDeviceAgentDelegate: " + trustAgentDelegate + ".");
        mTrustAgentDelegate = null;
    }

    private boolean areCredentialsValid(@Nullable PhoneCredentials credentials) {
        return credentials != null && credentials.getEscrowToken() != null
                && credentials.getHandle() != null;
    }

    private void processEnrollmentMessage(@NonNull CompanionDevice device,
            @Nullable ByteString payload) {
        if (payload == null) {
            logw(TAG, "Received enrollment message with null payload. Ignoring.");
            return;
        }

        byte[] message = payload.toByteArray();
        if (message.length != ESCROW_TOKEN_LENGTH) {
            logw(TAG, "Received invalid escrow token of length " + message.length + ". Ignoring.");
            return;
        }

        startEnrollment(device, message);
    }

    private void processUnlockMessage(@NonNull CompanionDevice device,
            @Nullable ByteString payload) {
        if (payload == null) {
            logw(TAG, "Received unlock message with null payload. Ignoring.");
            return;
        }
        byte[] message = payload.toByteArray();

        PhoneCredentials credentials = null;
        try {
            credentials = PhoneCredentials.parseFrom(message);
        } catch (InvalidProtocolBufferException e) {
            loge(TAG, "Unable to parse credentials from device. Not unlocking head unit.");
            return;
        }

        if (!areCredentialsValid(credentials)) {
            loge(TAG, "Received invalid credentials from device. Not unlocking head unit.");
            return;
        }

        TrustedDeviceEntity entity = mDatabase.getTrustedDevice(device.getDeviceId());

        if (entity == null) {
            logw(TAG, "Received unlock request from an untrusted device.");
            // TODO(b/145618412) Notify device that it is no longer trusted.
            return;
        }

        if (entity.userId != ActivityManager.getCurrentUser()) {
            logw(TAG, "Received credentials from background user " + entity.userId
                    + ". Ignoring.");
            return;
        }

        TrustedDeviceEventLog.onCredentialsReceived();

        if (mTrustAgentDelegate == null) {
            logd(TAG, "No trust agent delegate set yet. Credentials will be delivered once "
                    + "set.");
            mPendingCredentials = new PendingCredentials(device.getDeviceId(), credentials);
            return;
        }

        logd(TAG, "Received unlock credentials from trusted device " + device.getDeviceId()
                + ". Attempting unlock.");

        unlockUser(device.getDeviceId(), credentials);
    }

    private void processErrorMessage(@Nullable ByteString payload) {
        if (payload == null) {
            logw(TAG, "Received error message with null payload. Ignoring.");
            return;
        }
        TrustedDeviceError trustedDeviceError = null;
        try {
            trustedDeviceError = TrustedDeviceError.parseFrom(payload.toByteArray());
        } catch (InvalidProtocolBufferException e) {
            loge(TAG, "Received error message from client, but cannot parse.", e);
            notifyEnrollmentError(TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_UNKNOWN);
            return;
        }
        int errorType = trustedDeviceError.getTypeValue();
        int error;
        switch (errorType) {
            case ErrorType.MESSAGE_TYPE_UNKNOWN_VALUE:
                error = TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_MESSAGE_TYPE_UNKNOWN;
                break;
            case ErrorType.DEVICE_NOT_SECURED_VALUE:
                error = TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_DEVICE_NOT_SECURED;
                break;
            default:
                loge(TAG, "Encountered unexpected error type: " + errorType + ".");
                error = TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_UNKNOWN;
        }
        notifyEnrollmentError(error);
    }

    private void notifyEnrollmentError(@TrustedDeviceConstants.TrustedDeviceError int error) {
        notifyEnrollmentCallbacks(callback -> {
            try {
                callback.onTrustedDeviceEnrollmentError(error);
            } catch (RemoteException e) {
                loge(TAG, "Error while notifying enrollment error.", e);
            }
        });
    }

    private void notifyTrustedDeviceCallbacks(Consumer<ITrustedDeviceCallback> notification) {
        mTrustedDeviceCallbacks.forEach((iBinder, callback) -> mExecutor.execute(() ->
                notification.accept(callback)));
    }

    private void notifyEnrollmentCallbacks(
            Consumer<ITrustedDeviceEnrollmentCallback> notification ) {
        mEnrollmentCallbacks.forEach((iBinder, callback) -> mExecutor.execute(() ->
                notification.accept(callback)));
    }

    private void notifyAssociatedDeviceCallbacks(
            Consumer<IDeviceAssociationCallback> notification ) {
        mAssociatedDeviceCallbacks.forEach((iBinder, callback) -> mExecutor.execute(() ->
                notification.accept(callback)));
    }

    private void removeRemoteBinder(IBinder binder) {
        RemoteCallbackBinder remoteBinderToRemove = null;
        for (RemoteCallbackBinder remoteBinder : mCallbackBinders) {
            if (remoteBinder.getCallbackBinder().equals(binder)) {
                remoteBinderToRemove = remoteBinder;
                break;
            }
        }
        if (remoteBinderToRemove != null) {
            remoteBinderToRemove.cleanUp();
            mCallbackBinders.remove(remoteBinderToRemove);
        }
    }

    private byte[] createAcknowledgmentMessage() {
        return TrustedDeviceMessage.newBuilder()
                .setVersion(TRUSTED_DEVICE_MESSAGE_VERSION)
                .setType(MessageType.ACK)
                .build()
                .toByteArray();
    }

    private byte[] createHandleMessage(long handle) {
        return TrustedDeviceMessage.newBuilder()
                .setVersion(TRUSTED_DEVICE_MESSAGE_VERSION)
                .setType(MessageType.HANDLE)
                .setPayload(ByteString.copyFrom(ByteUtils.longToBytes(handle)))
                .build()
                .toByteArray();
    }

    private byte[] createStartEnrollmentMessage() {
        return TrustedDeviceMessage.newBuilder()
                .setVersion(TRUSTED_DEVICE_MESSAGE_VERSION)
                .setType(MessageType.START_ENROLLMENT)
                .build()
                .toByteArray();
    }

    private final TrustedDeviceFeature.Callback mFeatureCallback =
            new TrustedDeviceFeature.Callback() {
        @Override
        public void onMessageReceived(CompanionDevice device, byte[] message) {
            TrustedDeviceMessage trustedDeviceMessage = null;
            try {
                trustedDeviceMessage = TrustedDeviceMessage.parseFrom(message);
            } catch (InvalidProtocolBufferException e) {
                loge(TAG, "Received message from client, but cannot parse.", e);
                return;
            }

            switch (trustedDeviceMessage.getType()) {
                case ESCROW_TOKEN:
                    processEnrollmentMessage(device, trustedDeviceMessage.getPayload());
                    break;
                case UNLOCK_CREDENTIALS:
                    processUnlockMessage(device, trustedDeviceMessage.getPayload());
                    break;
                case ACK:
                    // The client sends an acknowledgment when the handle has been received, but
                    // nothing needs to be on the IHU side. So simply log this message.
                    logd(TAG, "Received acknowledgment message from client.");
                    break;
                case ERROR:
                    processErrorMessage(trustedDeviceMessage.getPayload());
                    break;
                default:
                    // The client should only be sending requests to either enroll or unlock.
                    loge(TAG, "Received a message from the client with an invalid MessageType ( "
                            + trustedDeviceMessage.getType() + "). Ignoring.");
            }
        }

        @Override
        public void onDeviceError(CompanionDevice device, int error) {
        }
    };

    private final TrustedDeviceFeature.AssociatedDeviceCallback mAssociatedDeviceCallback =
            new TrustedDeviceFeature.AssociatedDeviceCallback() {
        @Override
        public void onAssociatedDeviceAdded(AssociatedDevice device) {
            notifyAssociatedDeviceCallbacks(callback -> {
                try {
                    callback.onAssociatedDeviceAdded(device);
                } catch (RemoteException e) {
                    loge(TAG, "Failed to notify that an associated device has been added.", e);
                }
            });
        }

        @Override
        public void onAssociatedDeviceRemoved(AssociatedDevice device) {
            List<TrustedDevice> devices = getTrustedDevicesForActiveUser();
            if (devices == null || devices.isEmpty()) {
                return;
            }
            TrustedDevice deviceToRemove = null;
            for (TrustedDevice trustedDevice : devices) {
                if (trustedDevice.getDeviceId().equals(device.getDeviceId())) {
                    deviceToRemove = trustedDevice;
                    break;
                }
            }
            if (deviceToRemove != null) {
                removeTrustedDevice(deviceToRemove);
            }
            notifyAssociatedDeviceCallbacks(callback -> {
                try {
                    callback.onAssociatedDeviceRemoved(device);
                } catch (RemoteException e) {
                    loge(TAG, "Failed to notify that an associated device has been " +
                            "removed.", e);
                }
            });
        }

        @Override
        public void onAssociatedDeviceUpdated(AssociatedDevice device) {
            notifyAssociatedDeviceCallbacks(callback -> {
                try {
                    callback.onAssociatedDeviceUpdated(device);
                } catch (RemoteException e) {
                    loge(TAG, "Failed to notify that an associated device has been " +
                            "updated.", e);
                }
            });
        }
    };

    private static class PendingCredentials {
        final String mDeviceId;
        final PhoneCredentials mPhoneCredentials;

        PendingCredentials(@NonNull String deviceId, @NonNull PhoneCredentials credentials) {
            mDeviceId = deviceId;
            mPhoneCredentials = credentials;
        }
    }
}
