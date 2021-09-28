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

package com.android.car.companiondevicesupport.feature.trust.ui;

import android.annotation.NonNull;

import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.internal.trust.TrustedDevice;

import java.util.List;

/**
 * ViewModel that shares trusted device data between {@link TrustedDeviceActivity} and
 * {@link TrustedDeviceDetailFragment}.
 */
public class TrustedDeviceViewModel extends ViewModel {
    private final MutableLiveData<List<TrustedDevice>> mTrustedDevices = new MutableLiveData<>();
    private final MutableLiveData<AssociatedDevice> mAssociatedDevice = new MutableLiveData<>(null);
    private final MutableLiveData<TrustedDevice> mDeviceToDisable =
            new MutableLiveData<>(null);
    private final MutableLiveData<AssociatedDevice> mDeviceToEnable =
            new MutableLiveData<>(null);
    private final MutableLiveData<TrustedDevice> mDeviceDisabled = new MutableLiveData<>(null);
    private final MutableLiveData<TrustedDevice> mDeviceEnabled = new MutableLiveData<>(null);

    /**
     * Set trusted devices.
     *
     * @param devices Trusted devices.
     */
    void setTrustedDevices(@NonNull List<TrustedDevice> devices) {
        mTrustedDevices.postValue(devices);
    }

    /**
     * Set current associated device.
     *
     * @param device Associated device.
     */
    void setAssociatedDevice(@NonNull AssociatedDevice device) {
        mAssociatedDevice.setValue(device);
    }

    /**
     * Set the trusted device to disable.
     *
     * @param device The trusted device to disable.
     */
    void setDeviceToDisable(TrustedDevice device) {
        mDeviceToDisable.setValue(device);
    }

    /**
     * Set the associated device to enroll.
     *
     * @param device The associated device to enroll.
     */
    void setDeviceToEnable(AssociatedDevice device) {
        mDeviceToEnable.setValue(device);
    }

    /** Set the disabled trusted device. */
    void setDisabledDevice(TrustedDevice device) {
        mDeviceDisabled.postValue(device);
    }

    /** Set the enabled trusted device. */
    void setEnabledDevice(TrustedDevice device) {
        mDeviceEnabled.postValue(device);
    }

    /** Get trusted device list. It will return an empty list if there's no trusted device. */
    MutableLiveData<List<TrustedDevice>> getTrustedDevices() {
        return mTrustedDevices;
    }

    /** Get current associated device. */
    MutableLiveData<AssociatedDevice> getAssociatedDevice() {
        return mAssociatedDevice;
    }

    /** Get the trusted device to disable. */
    MutableLiveData<TrustedDevice> getDeviceToDisable() {
        return mDeviceToDisable;
    }

    /** Get the associated device to enroll. */
    MutableLiveData<AssociatedDevice> getDeviceToEnable() {
        return mDeviceToEnable;
    }

    /** Get the disabled trusted device. */
    MutableLiveData<TrustedDevice> getDisabledDevice() {
        return mDeviceDisabled;
    }

    /** Get the enabled trusted device. */
    MutableLiveData<TrustedDevice> getEnabledDevice() {
        return mDeviceEnabled;
    }
}
