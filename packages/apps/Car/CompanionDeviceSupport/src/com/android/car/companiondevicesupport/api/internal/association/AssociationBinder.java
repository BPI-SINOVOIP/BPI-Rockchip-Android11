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

package com.android.car.companiondevicesupport.api.internal.association;

import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;

import android.os.RemoteException;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IConnectionCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.connecteddevice.ConnectedDeviceManager;
import com.android.car.connecteddevice.ConnectedDeviceManager.DeviceAssociationCallback;
import com.android.car.connecteddevice.AssociationCallback;
import com.android.car.connecteddevice.model.ConnectedDevice;
import com.android.car.connecteddevice.util.RemoteCallbackBinder;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/** Binder for exposing connected device association actions to internal features. */
public class AssociationBinder extends IAssociatedDeviceManager.Stub {

    private static final String TAG = "AssociationBinder";

    private final ConnectedDeviceManager mConnectedDeviceManager;
    /**
     * {@link #mRemoteAssociationCallbackBinder} and {@link #mIAssociationCallback} can only be
     * modified together through {@link #setAssociationCallback(IAssociationCallback)} or
     * {@link #clearAssociationCallback()} from the association thread.
     */
    private RemoteCallbackBinder mRemoteAssociationCallbackBinder;
    private IAssociationCallback mIAssociationCallback;
    /**
     * {@link #mRemoteDeviceAssociationCallbackBinder} and {@link #mDeviceAssociationCallback} can
     * only be modified together through
     * {@link #setDeviceAssociationCallback(IDeviceAssociationCallback)} or
     * {@link #clearDeviceAssociationCallback()} from the association thread.
     */
    private RemoteCallbackBinder mRemoteDeviceAssociationCallbackBinder;
    private DeviceAssociationCallback mDeviceAssociationCallback;

    /**
     * {@link #mRemoteConnectionCallbackBinder} and {@link #mIConnectionCallback} can only be
     * modified together through {@link #setConnectionCallback(IConnectionCallback)} or
     * {@link #clearConnectionCallback()} from the association thread.
     */
    private RemoteCallbackBinder mRemoteConnectionCallbackBinder;
    private ConnectedDeviceManager.ConnectionCallback mConnectionCallback;

    private final Executor mCallbackExecutor = Executors.newSingleThreadExecutor();

    public AssociationBinder(ConnectedDeviceManager connectedDeviceManager) {
        mConnectedDeviceManager = connectedDeviceManager;
    }

    @Override
    public void setAssociationCallback(IAssociationCallback callback) {
        mRemoteAssociationCallbackBinder = new RemoteCallbackBinder(callback.asBinder(),
                iBinder -> stopAssociation());
        mIAssociationCallback = callback;
        if (mIAssociationCallback == null) {
            logd(TAG, "mIAssociationCallback is null");
        }
    }

    @Override
    public void clearAssociationCallback() {
        mIAssociationCallback = null;
        if (mRemoteAssociationCallbackBinder == null) {
            return;
        }
        mRemoteAssociationCallbackBinder.cleanUp();
        mRemoteAssociationCallbackBinder = null;
    }

    @Override
    public void startAssociation() {
        mConnectedDeviceManager.startAssociation(mAssociationCallback);
    }

    @Override
    public void stopAssociation() {
        mConnectedDeviceManager.stopAssociation(mAssociationCallback);
    }

    @Override
    public List<AssociatedDevice> getActiveUserAssociatedDevices() {
        List<com.android.car.connecteddevice.model.AssociatedDevice> associatedDevices =
                mConnectedDeviceManager.getActiveUserAssociatedDevices();
        List<AssociatedDevice> convertedDevices = new ArrayList<>();
        for (com.android.car.connecteddevice.model.AssociatedDevice device : associatedDevices) {
            convertedDevices.add(new AssociatedDevice(device));
        }
        return convertedDevices;
    }

    @Override
    public void acceptVerification() {
        mConnectedDeviceManager.notifyOutOfBandAccepted();
    }

    @Override
    public void removeAssociatedDevice(String deviceId) {
        mConnectedDeviceManager.removeActiveUserAssociatedDevice(deviceId);
    }

    @Override
    public void setDeviceAssociationCallback(IDeviceAssociationCallback callback) {
        mDeviceAssociationCallback = new DeviceAssociationCallback() {
            @Override
            public void onAssociatedDeviceAdded(
                    com.android.car.connecteddevice.model.AssociatedDevice device) {
                try {
                    callback.onAssociatedDeviceAdded(new AssociatedDevice(device));
                } catch (RemoteException exception) {
                    loge(TAG, "onAssociatedDeviceAdded failed.", exception);
                }
            }

            @Override
            public void onAssociatedDeviceRemoved(
                    com.android.car.connecteddevice.model.AssociatedDevice device) {
                try {
                    callback.onAssociatedDeviceRemoved(new AssociatedDevice(device));
                } catch (RemoteException exception) {
                    loge(TAG, "onAssociatedDeviceRemoved failed.", exception);
                }
            }

            @Override
            public void onAssociatedDeviceUpdated(
                    com.android.car.connecteddevice.model.AssociatedDevice device) {
                try {
                    callback.onAssociatedDeviceUpdated(new AssociatedDevice(device));
                } catch (RemoteException exception) {
                    loge(TAG, "onAssociatedDeviceUpdated failed.", exception);
                }
            }
        };
        mRemoteDeviceAssociationCallbackBinder = new RemoteCallbackBinder(callback.asBinder(),
                iBinder -> clearDeviceAssociationCallback());
        mConnectedDeviceManager.registerDeviceAssociationCallback(mDeviceAssociationCallback,
                mCallbackExecutor);
    }

    @Override
    public void clearDeviceAssociationCallback() {
        if (mDeviceAssociationCallback == null) {
            return;
        }
        mConnectedDeviceManager.unregisterDeviceAssociationCallback(mDeviceAssociationCallback);
        mDeviceAssociationCallback = null;
        mRemoteDeviceAssociationCallbackBinder.cleanUp();
        mRemoteDeviceAssociationCallbackBinder = null;
    }

    @Override
    public List<CompanionDevice> getActiveUserConnectedDevices() {
        List<ConnectedDevice> connectedDevices =
                mConnectedDeviceManager.getActiveUserConnectedDevices();
        List<CompanionDevice> convertedDevices = new ArrayList<>();
        for (ConnectedDevice device : connectedDevices) {
            convertedDevices.add(new CompanionDevice(device));
        }
        return convertedDevices;
    }

    @Override
    public void setConnectionCallback(IConnectionCallback callback) {
        mConnectionCallback = new ConnectedDeviceManager.ConnectionCallback() {

            @Override
            public void onDeviceConnected(ConnectedDevice device) {
                if (callback == null) {
                    loge(TAG, "No IConnectionCallback has been set, ignoring " +
                            "onDeviceConnected.");
                    return;
                }
                try {
                    callback.onDeviceConnected(new CompanionDevice(device));
                } catch (RemoteException exception) {
                    loge(TAG, "onDeviceConnected failed.", exception);
                }
            }

            @Override
            public void onDeviceDisconnected(ConnectedDevice device) {
                if (callback == null) {
                    loge(TAG, "No IConnectionCallback has been set, ignoring " +
                            "onDeviceConnected.");
                    return;
                }
                try {
                    callback.onDeviceDisconnected(new CompanionDevice(device));
                } catch (RemoteException exception) {
                    loge(TAG, "onDeviceDisconnected failed.", exception);
                }
            }
        };
        mRemoteConnectionCallbackBinder = new RemoteCallbackBinder(callback.asBinder(),
                iBinder -> clearConnectionCallback());
        mConnectedDeviceManager.registerActiveUserConnectionCallback(mConnectionCallback,
                mCallbackExecutor);
    }

    @Override
    public void clearConnectionCallback() {
        if (mConnectionCallback == null) {
            return;
        }
        mConnectedDeviceManager.unregisterConnectionCallback(mConnectionCallback);
        mRemoteConnectionCallbackBinder.cleanUp();
        mRemoteConnectionCallbackBinder = null;
        mConnectionCallback = null;
    }

    @Override
    public void enableAssociatedDeviceConnection(String deviceId) {
        mConnectedDeviceManager.enableAssociatedDeviceConnection(deviceId);
    }

    @Override
    public void disableAssociatedDeviceConnection(String deviceId) {
        mConnectedDeviceManager.disableAssociatedDeviceConnection(deviceId);
    }

    private AssociationCallback mAssociationCallback = new AssociationCallback() {

        @Override
        public void onAssociationStartSuccess(String deviceName) {
            if (mIAssociationCallback == null) {
                loge(TAG, "No IAssociationCallback has been set, ignoring " +
                        "onAssociationStartSuccess.");
                return;
            }
            try {
                mIAssociationCallback.onAssociationStartSuccess(deviceName);
            } catch (RemoteException exception) {
                loge(TAG, "onAssociationStartSuccess failed.", exception);
            }
        }
        @Override
        public void onAssociationStartFailure() {
            if (mIAssociationCallback == null) {
                loge(TAG, "No IAssociationCallback has been set, ignoring " +
                        "onAssociationStartFailure.");
                return;
            }
            try {
                mIAssociationCallback.onAssociationStartFailure();
            } catch (RemoteException exception) {
                loge(TAG, "onAssociationStartFailure failed.", exception);
            }
        }

        @Override
        public void onAssociationError(int error) {
            if (mIAssociationCallback == null) {
                loge(TAG, "No IAssociationCallback has been set, ignoring " +
                        "onAssociationError: " + error + ".");
                return;
            }
            try {
                mIAssociationCallback.onAssociationError(error);
            } catch (RemoteException exception) {
                loge(TAG, "onAssociationError failed. Error: " + error + "", exception);
            }
        }

        @Override
        public void onVerificationCodeAvailable(String code) {
            if (mIAssociationCallback == null) {
                loge(TAG, "No IAssociationCallback has been set, ignoring " +
                        "onVerificationCodeAvailable, code: " + code);
                return;
            }
            try {
                mIAssociationCallback.onVerificationCodeAvailable(code);
            } catch (RemoteException exception) {
                loge(TAG, "onVerificationCodeAvailable failed. Code: " + code + "", exception);
            }
        }

        @Override
        public void onAssociationCompleted(String deviceId) {
            if (mIAssociationCallback == null) {
                loge(TAG, "No IAssociationCallback has been set, ignoring " +
                        "onAssociationCompleted, deviceId: " + deviceId);
                return;
            }
            try {
                mIAssociationCallback.onAssociationCompleted();
            } catch (RemoteException exception) {
                loge(TAG, "onAssociationCompleted failed.", exception);
            }
        }
    };
}
