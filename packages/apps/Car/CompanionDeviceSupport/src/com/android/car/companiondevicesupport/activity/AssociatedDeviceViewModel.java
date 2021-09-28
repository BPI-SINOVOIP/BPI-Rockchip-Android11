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

package com.android.car.companiondevicesupport.activity;

import static com.android.car.companiondevicesupport.activity.AssociationActivity.ASSOCIATED_DEVICE_DATA_NAME_EXTRA;
import static com.android.car.companiondevicesupport.service.CompanionDeviceSupportService.ACTION_BIND_ASSOCIATION;
import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.Application;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.UserHandle;

import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IConnectionCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.internal.association.IAssociatedDeviceManager;
import com.android.car.companiondevicesupport.api.internal.association.IAssociationCallback;
import com.android.car.companiondevicesupport.service.CompanionDeviceSupportService;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation {@link ViewModel} for sharing associated devices data between
 * {@link AssociatedDeviceDetailFragment} and {@link AssociationActivity}
 */
public class AssociatedDeviceViewModel extends AndroidViewModel {

    private static final String TAG = "AssociatedDeviceViewModel";

    public enum AssociationState { NONE, PENDING, STARTING, STARTED, COMPLETED, ERROR }

    private IAssociatedDeviceManager mAssociatedDeviceManager;
    private List<AssociatedDevice> mAssociatedDevices = new ArrayList<>();
    private List<CompanionDevice> mConnectedDevices = new ArrayList<>();

    private final MutableLiveData<AssociatedDeviceDetails> mDeviceDetails =
            new MutableLiveData<>(null);
    private final MutableLiveData<String> mAdvertisedCarName = new MutableLiveData<>(null);
    private final MutableLiveData<String> mPairingCode = new MutableLiveData<>(null);
    private final MutableLiveData<AssociatedDevice> mDeviceToRemove = new MutableLiveData<>(null);
    private final MutableLiveData<Integer> mBluetoothState =
            new MutableLiveData<>(BluetoothAdapter.STATE_OFF);
    private final MutableLiveData<AssociationState> mAssociationState =
            new MutableLiveData<>(AssociationState.NONE);
    private final MutableLiveData<AssociatedDevice> mRemovedDevice = new MutableLiveData<>(null);
    private final MutableLiveData<Boolean> mIsFinished = new MutableLiveData<>(false);

    public AssociatedDeviceViewModel(@NonNull Application application) {
        super(application);
        Intent intent = new Intent(getApplication(), CompanionDeviceSupportService.class);
        intent.setAction(ACTION_BIND_ASSOCIATION);
        getApplication().bindServiceAsUser(intent, mConnection, Context.BIND_AUTO_CREATE,
                UserHandle.SYSTEM);
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter != null) {
            mBluetoothState.postValue(adapter.getState());
        }
    }

    @Override
    protected void onCleared() {
        super.onCleared();
        try {
            unregisterCallbacks();
        } catch (RemoteException e) {
            loge(TAG, "Error clearing registered callbacks. ", e);
        }
        getApplication().unbindService(mConnection);
        getApplication().unregisterReceiver(mReceiver);
        mAssociatedDeviceManager = null;
    }

    /** Confirms that the pairing code matches. */
    public void acceptVerification() {
        mPairingCode.postValue(null);
        try {
            mAssociatedDeviceManager.acceptVerification();
        } catch (RemoteException e) {
            loge(TAG, "Error while accepting verification.", e);
        }
    }

    /** Stops association. */
    public void stopAssociation() {
        AssociationState state = mAssociationState.getValue();
        if (state != AssociationState.STARTING && state != AssociationState.STARTED) {
            return;
        }
        mAdvertisedCarName.postValue(null);
        mPairingCode.postValue(null);
        try {
            mAssociatedDeviceManager.stopAssociation();
        } catch (RemoteException e) {
            loge(TAG, "Error while stopping association process.", e);
        }
        mAssociationState.postValue(AssociationState.NONE);
    }

    /** Retry association. */
    public void retryAssociation() {
        stopAssociation();
        startAssociation();
    }

    /** Select the current associated device as the device to remove. */
    public void selectCurrentDeviceToRemove() {
        mDeviceToRemove.postValue(getAssociatedDevice());
    }

    /** Remove the current associated device.*/
    public void removeCurrentDevice() {
        AssociatedDevice device = getAssociatedDevice();
        if (device == null) {
            return;
        }
        try {
            mAssociatedDeviceManager.removeAssociatedDevice(device.getDeviceId());
        } catch (RemoteException e) {
            loge(TAG, "Failed to remove associated device: " + device, e);
        }
    }

    /** Toggle connection on the current associated device. */
    public void toggleConnectionStatusForCurrentDevice() {
        AssociatedDevice device = getAssociatedDevice();
        if (device == null) {
            return;
        }
        try {
            if (device.isConnectionEnabled()) {
                mAssociatedDeviceManager.disableAssociatedDeviceConnection(device.getDeviceId());
            } else {
                mAssociatedDeviceManager.enableAssociatedDeviceConnection(device.getDeviceId());
            }
        } catch (RemoteException e) {
            loge(TAG, "Failed to toggle connection status for device: " + device + ".", e);
        }
    }

    /** Get the associated device details. */
    public MutableLiveData<AssociatedDeviceDetails> getDeviceDetails() {
        return mDeviceDetails;
    }

    /** Start feature activity for the current associated device. */
    public void startFeatureActivityForCurrentDevice(@NonNull String action) {
        AssociatedDevice device = getAssociatedDevice();
        if (device == null || action == null) {
            return;
        }
        Intent intent = new Intent(action);
        intent.putExtra(ASSOCIATED_DEVICE_DATA_NAME_EXTRA, device);
        getApplication().startActivityAsUser(intent,
                UserHandle.of(ActivityManager.getCurrentUser()));
    }

    /** Reset the value of {@link #mAssociationState} to {@link AssociationState.NONE}. */
    public void resetAssociationState() {
        mAssociationState.postValue(AssociationState.NONE);
    }

    /** Get the associated device to remove. The associated device could be null. */
    public LiveData<AssociatedDevice> getDeviceToRemove() {
        return mDeviceToRemove;
    }

    /** Get the name that is being advertised by the car. */
    public MutableLiveData<String> getAdvertisedCarName() {
        return mAdvertisedCarName;
    }

    /** Get the generated pairing code. */
    public MutableLiveData<String> getPairingCode() {
        return mPairingCode;
    }

    /** Value is {@code true} if the current associated device has been removed. */
    public MutableLiveData<AssociatedDevice> getRemovedDevice() {
        return mRemovedDevice;
    }

    /** Get the current {@link AssociationState}. */
    public MutableLiveData<AssociationState> getAssociationState() {
        return mAssociationState;
    }

    /** Get the current Bluetooth state. */
    public MutableLiveData<Integer> getBluetoothState() {
        return mBluetoothState;
    }

    /** Value is {@code true} if IHU is not in association and has no associated device. */
    public MutableLiveData<Boolean> isFinished() {
        return mIsFinished;
    }

    private void updateDeviceDetails() {
        AssociatedDevice device = getAssociatedDevice();
        if (device == null) {
            return;
        }
        mDeviceDetails.postValue(new AssociatedDeviceDetails(getAssociatedDevice(), isConnected()));
    }

    @Nullable
    private AssociatedDevice getAssociatedDevice() {
        if (mAssociatedDevices.isEmpty()) {
            return null;
        }
        return mAssociatedDevices.get(0);
    }

    private boolean isConnected() {
        if (mAssociatedDevices.isEmpty() || mConnectedDevices.isEmpty()) {
            return false;
        }
        String associatedDeviceId = mAssociatedDevices.get(0).getDeviceId();
        String connectedDeviceId = mConnectedDevices.get(0).getDeviceId();
        return associatedDeviceId.equals(connectedDeviceId);
    }

    private void setAssociatedDevices(@NonNull List<AssociatedDevice> associatedDevices) {
        mAssociatedDevices = associatedDevices;
        updateDeviceDetails();
    }

    private void setConnectedDevices(@NonNull List<CompanionDevice> connectedDevices) {
        mConnectedDevices = connectedDevices;
        updateDeviceDetails();
    }

    private void addOrUpdateAssociatedDevice(@NonNull AssociatedDevice device) {
        mAssociatedDevices.removeIf(d -> d.getDeviceId().equals(device.getDeviceId()));
        mAssociatedDevices.add(device);
        updateDeviceDetails();
    }

    private void removeAssociatedDevice(AssociatedDevice device) {
        if (mAssociatedDevices.removeIf(d -> d.getDeviceId().equals(device.getDeviceId()))) {
            mRemovedDevice.postValue(device);
            mDeviceDetails.postValue(null);
        }
    }

    private void startAssociation() {
        mAssociationState.postValue(AssociationState.PENDING);
        if (!BluetoothAdapter.getDefaultAdapter().isEnabled()) {
            return;
        }
        try {
            mAssociatedDeviceManager.startAssociation();

        } catch (RemoteException e) {
            loge(TAG, "Failed to start association .", e);
            mAssociationState.postValue(AssociationState.ERROR);
        }
        mAssociationState.postValue(AssociationState.STARTING);
    }

    private void registerCallbacks() throws RemoteException {
        mAssociatedDeviceManager.setAssociationCallback(mAssociationCallback);
        mAssociatedDeviceManager.setDeviceAssociationCallback(mDeviceAssociationCallback);
        mAssociatedDeviceManager.setConnectionCallback(mConnectionCallback);
    }

    private void unregisterCallbacks() throws RemoteException {
        mAssociatedDeviceManager.clearDeviceAssociationCallback();
        mAssociatedDeviceManager.clearAssociationCallback();
        mAssociatedDeviceManager.clearConnectionCallback();
    }

    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mAssociatedDeviceManager = IAssociatedDeviceManager.Stub.asInterface(service);
            try {
                registerCallbacks();
                setConnectedDevices(mAssociatedDeviceManager.getActiveUserConnectedDevices());
                setAssociatedDevices(mAssociatedDeviceManager.getActiveUserAssociatedDevices());
            } catch (RemoteException e) {
                loge(TAG, "Initial set failed onServiceConnected", e);
            }
            AssociationState state = mAssociationState.getValue();
            if (mAssociatedDevices.isEmpty() && state != AssociationState.STARTING &&
                    state != AssociationState.STARTED) {
                startAssociation();
            }
            logd(TAG, "Service connected:" + name.getClassName());
            IntentFilter filter = new IntentFilter();
            filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
            getApplication().registerReceiver(mReceiver, filter);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mAssociatedDeviceManager = null;
            logd(TAG, "Service disconnected: " + name.getClassName());
            mIsFinished.postValue(true);
        }
    };

    private final IAssociationCallback mAssociationCallback = new IAssociationCallback.Stub() {
        @Override
        public void onAssociationStartSuccess(String deviceName) {
            mAssociationState.postValue(AssociationState.STARTED);
            mAdvertisedCarName.postValue(deviceName);
        }

        @Override
        public void onAssociationStartFailure() {
            mAssociationState.postValue(AssociationState.ERROR);
            loge(TAG, "Failed to start association.");
        }

        @Override
        public void onAssociationError(int error) throws RemoteException {
            mAssociationState.postValue(AssociationState.ERROR);
            loge(TAG, "Error during association: " + error + ".");
        }

        @Override
        public void onVerificationCodeAvailable(String code) throws RemoteException {
            mAdvertisedCarName.postValue(null);
            mPairingCode.postValue(code);
        }

        @Override
        public void onAssociationCompleted() {
            mAssociationState.postValue(AssociationState.COMPLETED);
        }
    };

    private final IDeviceAssociationCallback mDeviceAssociationCallback =
            new IDeviceAssociationCallback.Stub() {
                @Override
                public void onAssociatedDeviceAdded(AssociatedDevice device) {
                    addOrUpdateAssociatedDevice(device);
                }

                @Override
                public void onAssociatedDeviceRemoved(AssociatedDevice device) {
                    removeAssociatedDevice(device);
                }

                @Override
                public void onAssociatedDeviceUpdated(AssociatedDevice device) {
                    addOrUpdateAssociatedDevice(device);
                }
            };

    private final IConnectionCallback mConnectionCallback = new IConnectionCallback.Stub() {
        @Override
        public void onDeviceConnected(CompanionDevice companionDevice) {
            mConnectedDevices.add(companionDevice);
            updateDeviceDetails();
        }

        @Override
        public void onDeviceDisconnected(CompanionDevice companionDevice) {
            mConnectedDevices.removeIf(d -> d.getDeviceId().equals(companionDevice.getDeviceId()));
            updateDeviceDetails();
        }
    };

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent)  {
            String action = intent.getAction();
            if (!BluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {
                return;
            }
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);
            if (state != BluetoothAdapter.STATE_ON &&
                    state != BluetoothAdapter.STATE_OFF &&
                    state != BluetoothAdapter.ERROR) {
                // No need to convey any other state.
                return;
            }
            mBluetoothState.postValue(state);
            if (state == BluetoothAdapter.STATE_ON &&
                    mAssociationState.getValue() == AssociationState.PENDING &&
                    mAssociatedDeviceManager != null) {
                startAssociation();
            }
        }
    };
}
