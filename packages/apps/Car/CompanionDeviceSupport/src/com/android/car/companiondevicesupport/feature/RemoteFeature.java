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

package com.android.car.companiondevicesupport.feature;

import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.connecteddevice.util.SafeLog.logw;

import android.annotation.CallSuper;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.UserHandle;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IConnectedDeviceManager;
import com.android.car.companiondevicesupport.api.external.IConnectionCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceCallback;
import com.android.car.companiondevicesupport.service.CompanionDeviceSupportService;

import java.time.Duration;
import java.util.List;

/**
 * Base class for a feature that must bind to {@link CompanionDeviceSupportService}. Callbacks
 * are registered automatically and events are forwarded to internal methods. Override these to
 * add custom logic for callback triggers.
 */
public abstract class RemoteFeature {

    private static final String TAG = "RemoteFeature";

    private static final String SERVICE_PACKAGE_NAME = "com.android.car.companiondevicesupport";

    private static final String FULLY_QUALIFIED_SERVICE_NAME = SERVICE_PACKAGE_NAME
            + ".service.CompanionDeviceSupportService";

    private static final Duration BIND_RETRY_DURATION = Duration.ofSeconds(1);

    private static final int MAX_BIND_ATTEMPTS = 3;

    private final Context mContext;

    private final ParcelUuid mFeatureId;

    private IConnectedDeviceManager mConnectedDeviceManager;

    private int mBindAttempts;

    public RemoteFeature(@NonNull Context context, @NonNull ParcelUuid featureId) {
        mContext = context;
        mFeatureId = featureId;
    }

    /** Start setup process and begin binding to {@link CompanionDeviceSupportService}. */
    @CallSuper
    public void start() {
        mBindAttempts = 0;
        bindToService();
    }

    /** Called when the hosting service is being destroyed. Cleans up internal feature logic. */
    @CallSuper
    public void stop() {
        try {
            mConnectedDeviceManager.unregisterConnectionCallback(mConnectionCallback);
            for (CompanionDevice device : mConnectedDeviceManager.getActiveUserConnectedDevices()) {
                mConnectedDeviceManager.unregisterDeviceCallback(device, mFeatureId,
                        mDeviceCallback);
            }
            mConnectedDeviceManager.unregisterDeviceAssociationCallback(mDeviceAssociationCallback);
        } catch (RemoteException e) {
            loge(TAG, "Error while stopping remote feature.", e);
        }
        mContext.unbindService(mServiceConnection);
    }

    /** Return the {@link Context} registered with the feature. */
    @NonNull
    public Context getContext() {
        return  mContext;
    }

    /**
     *  Return the {@link IConnectedDeviceManager} bound with the feature. Returns {@code null} if
     *  binding has not completed yet.
     */
    @Nullable
    public IConnectedDeviceManager getConnectedDeviceManager() {
        return mConnectedDeviceManager;
    }

    /** Return the {@link ParcelUuid} feature id registered for the feature. */
    @NonNull
    public ParcelUuid getFeatureId() {
        return mFeatureId;
    }

    /** Securely send message to a device. */
    public void sendMessageSecurely(@NonNull String deviceId, @NonNull byte[] message) {
        CompanionDevice device = getConnectedDeviceById(deviceId);
        if (device == null) {
            loge(TAG, "No matching device found with id " + deviceId + " when trying to send "
                    + "secure message.");
            onMessageFailedToSend(deviceId, message, false);
            return;
        }

        sendMessageSecurely(device, message);
    }

    /** Securely send message to a device. */
    public void sendMessageSecurely(@NonNull CompanionDevice device, @NonNull byte[] message) {
        try {
            getConnectedDeviceManager().sendMessageSecurely(device, getFeatureId(), message);
        } catch (RemoteException e) {
            loge(TAG, "Error while sending secure message.", e);
            onMessageFailedToSend(device.getDeviceId(), message, true);
        }
    }

    /** Send a message to a device without encryption. */
    public void sendMessageUnsecurely(@NonNull String deviceId, @NonNull byte[] message) {
        CompanionDevice device = getConnectedDeviceById(deviceId);
        if (device == null) {
            loge(TAG, "No matching device found with id " + deviceId + " when trying to send "
                    + "unsecure message.");
            onMessageFailedToSend(deviceId, message, false);
            return;
        }
    }

    /** Send a message to a device without encryption. */
    public void sendMessageUnsecurely(@NonNull CompanionDevice device, @NonNull byte[] message) {
        try {
            getConnectedDeviceManager().sendMessageUnsecurely(device, getFeatureId(), message);
        } catch (RemoteException e) {
            loge(TAG, "Error while sending unsecure message.", e);
            onMessageFailedToSend(device.getDeviceId(), message, true);
        }
    }

    /**
     * Return the {@link CompanionDevice} with a matching device id for the currently active user.
     * Returns {@code null} if no match found.
     */
    @Nullable
    public CompanionDevice getConnectedDeviceById(@NonNull String deviceId) {
        List<CompanionDevice> connectedDevices;
        try {
            connectedDevices = getConnectedDeviceManager().getActiveUserConnectedDevices();
        } catch (RemoteException e) {
            loge(TAG, "Exception while retrieving connected devices.", e);
            return null;
        }

        for (CompanionDevice device : connectedDevices) {
            if (device.getDeviceId().equals(deviceId)) {
                return device;
            }
        }

        return null;
    }

    // These can be overridden to perform custom actions.

    /** Called when a new {@link CompanionDevice} is connected. */
    protected void onDeviceConnected(@NonNull CompanionDevice device) { }

    /** Called when a {@link CompanionDevice} disconnects. */
    protected void onDeviceDisconnected(@NonNull CompanionDevice device) { }

    /** Called when a secure channel has been established with a {@link CompanionDevice}. */
    protected void onSecureChannelEstablished(@NonNull CompanionDevice device) { }

    /**
     * Called when a message fails to send to a device.
     *
     * @param deviceId Id of the device the message failed to send to.
     * @param message Message to send.
     * @param isTransient {@code true} if cause of failure is transient and can be retried.
     *     {@code false} if failure is permanent.
     */
    protected void onMessageFailedToSend(@NonNull String deviceId, @NonNull byte[] message,
            boolean isTransient) { }

    /** Called when a new {@link byte[]} message is received for this feature. */
    protected void onMessageReceived(@NonNull CompanionDevice device, @NonNull byte[] message) { }

    /** Called when an error has occurred with the connection. */
    protected void onDeviceError(@NonNull CompanionDevice device, int error) { }

    /** Called when a new {@link AssociatedDevice} is added for the given user. */
    protected void onAssociatedDeviceAdded(@NonNull AssociatedDevice device) { }

    /** Called when an {@link AssociatedDevice} is removed for the given user. */
    protected void onAssociatedDeviceRemoved(@NonNull AssociatedDevice device) { }

    /** Called when an {@link AssociatedDevice} is updated for the given user. */
    protected void onAssociatedDeviceUpdated(@NonNull AssociatedDevice device) { }

    private void bindToService() {
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(SERVICE_PACKAGE_NAME, FULLY_QUALIFIED_SERVICE_NAME));
        intent.setAction(CompanionDeviceSupportService.ACTION_BIND_CONNECTED_DEVICE_MANAGER);
        boolean success = mContext.bindServiceAsUser(intent, mServiceConnection,
                Context.BIND_AUTO_CREATE, UserHandle.SYSTEM);
        if (!success) {
            mBindAttempts++;
            if (mBindAttempts > MAX_BIND_ATTEMPTS) {
                loge(TAG, "Failed to bind to CompanionDeviceSupportService after " + mBindAttempts
                        + " attempts. Aborting.");
                return;
            }
            logw(TAG, "Unable to bind to CompanionDeviceSupportService. Trying again.");
            new Handler(Looper.getMainLooper()).postDelayed(this::bindToService,
                    BIND_RETRY_DURATION.toMillis());
        }
    }

    private final ServiceConnection mServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mConnectedDeviceManager = IConnectedDeviceManager.Stub.asInterface(service);
            try {
                mConnectedDeviceManager.registerActiveUserConnectionCallback(mConnectionCallback);
                mConnectedDeviceManager
                        .registerDeviceAssociationCallback(mDeviceAssociationCallback);
                logd(TAG, "Successfully bound to ConnectedDeviceManager.");
                List<CompanionDevice> activeUserConnectedDevices =
                        mConnectedDeviceManager.getActiveUserConnectedDevices();
                for (CompanionDevice device : activeUserConnectedDevices) {
                    mConnectedDeviceManager.registerDeviceCallback(device, mFeatureId,
                            mDeviceCallback);
                }
            } catch (RemoteException e) {
                loge(TAG, "Error while inspecting connected devices.", e);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            stop();
        }
    };

    private final IConnectionCallback mConnectionCallback = new IConnectionCallback.Stub() {
        @Override
        public void onDeviceConnected(CompanionDevice companionDevice) throws RemoteException {
            mConnectedDeviceManager.registerDeviceCallback(companionDevice, mFeatureId,
                    mDeviceCallback);
            RemoteFeature.this.onDeviceConnected(companionDevice);
        }

        @Override
        public void onDeviceDisconnected(CompanionDevice companionDevice) throws RemoteException {
            mConnectedDeviceManager.unregisterDeviceCallback(companionDevice, mFeatureId,
                    mDeviceCallback);
            RemoteFeature.this.onDeviceDisconnected(companionDevice);
        }
    };

    private final IDeviceCallback mDeviceCallback = new IDeviceCallback.Stub() {
        @Override
        public void onSecureChannelEstablished(CompanionDevice companionDevice) {
            RemoteFeature.this.onSecureChannelEstablished(companionDevice);
        }

        @Override
        public void onMessageReceived(CompanionDevice companionDevice, byte[] message) {
            RemoteFeature.this.onMessageReceived(companionDevice, message);
        }

        @Override
        public void onDeviceError(CompanionDevice companionDevice, int error) {
            RemoteFeature.this.onDeviceError(companionDevice, error);
        }
    };

    private final IDeviceAssociationCallback mDeviceAssociationCallback =
            new IDeviceAssociationCallback.Stub() {
        @Override
        public void onAssociatedDeviceAdded(AssociatedDevice device) {
            RemoteFeature.this.onAssociatedDeviceAdded(device);
        }

        @Override
        public void onAssociatedDeviceRemoved(AssociatedDevice device) {
            RemoteFeature.this.onAssociatedDeviceRemoved(device);
        }

        @Override
        public void onAssociatedDeviceUpdated(AssociatedDevice device) {
            RemoteFeature.this.onAssociatedDeviceUpdated(device);
        }
    };
}
