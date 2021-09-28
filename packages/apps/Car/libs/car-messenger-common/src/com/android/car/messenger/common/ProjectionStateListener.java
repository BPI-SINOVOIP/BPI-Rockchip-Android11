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

package com.android.car.messenger.common;

import static com.android.car.apps.common.util.SafeLog.logd;
import static com.android.car.apps.common.util.SafeLog.loge;
import static com.android.car.apps.common.util.SafeLog.logi;

import android.bluetooth.BluetoothDevice;
import android.car.Car;
import android.car.CarProjectionManager;
import android.car.projection.ProjectionStatus;
import android.content.Context;
import android.os.Bundle;
import android.os.Parcelable;

import androidx.annotation.Nullable;

import java.util.Collections;
import java.util.List;

/**
 * {@link ProjectionStatus} listener that exposes APIs to detect whether a projection application
 * is active.
 */
public class ProjectionStateListener {
    private static final String TAG = "CMC.ProjectionStateHandler";
    static final String PROJECTION_STATUS_EXTRA_DEVICE_STATE =
            "android.car.projection.DEVICE_STATE";

    private CarProjectionManager mCarProjectionManager = null;
    private final CarProjectionManager.ProjectionStatusListener mListener =
            (state, packageName, details) -> {
                mProjectionState = state;
                mProjectionDetails = details;
            };
    private Car mCar;

    private int mProjectionState = ProjectionStatus.PROJECTION_STATE_INACTIVE;
    private List<ProjectionStatus> mProjectionDetails = Collections.emptyList();

    public ProjectionStateListener(Context context) {
        Car.createCar(context, /* handler= */ null, Car.CAR_WAIT_TIMEOUT_DO_NOT_WAIT,
                (car, ready) -> {
                    mCar = car;
                    mCarProjectionManager = (CarProjectionManager) mCar.getCarManager(
                            Car.PROJECTION_SERVICE);
                    if (mCarProjectionManager != null) {
                        mCarProjectionManager.registerProjectionStatusListener(mListener);
                    }
                });
    }

    /** Unregisters the listener. Should be called when the caller's lifecycle is ending. **/
    public void destroy() {
        if (mCarProjectionManager != null) {
            mCarProjectionManager.unregisterProjectionStatusListener(mListener);
        }
        if (mCar != null) {
            mCar.disconnect();
            mCar = null;
        }
        mProjectionState = ProjectionStatus.PROJECTION_STATE_INACTIVE;
        mProjectionDetails = Collections.emptyList();
    }

    /**
     * Returns {@code true} if the input device currently has a projection app running in the
     * foreground.
     * @param bluetoothAddress of the device that should be checked. If null, return whether any
     *                         device is currently running a projection app in the foreground.
     */
    public boolean isProjectionInActiveForeground(@Nullable String bluetoothAddress) {
        if (bluetoothAddress == null) {
            logi(TAG, "returning non-device-specific projection status");
            return isProjectionInActiveForeground();
        }

        if (!isProjectionInActiveForeground()) {
            return false;
        }

        for (ProjectionStatus status : mProjectionDetails) {
            if (!status.isActive()) {
                // Don't suppress UI for packages that aren't actively projecting.
                logd(TAG, "skip non-projecting package " + status.getPackageName());
                continue;
            }

            for (ProjectionStatus.MobileDevice device : status.getConnectedMobileDevices()) {
                if (!device.isProjecting()) {
                    // Don't suppress UI for devices that aren't foreground.
                    logd(TAG, "skip non-projecting device " + device.getName());
                    continue;
                }

                Bundle extras = device.getExtras();
                if (extras.getInt(PROJECTION_STATUS_EXTRA_DEVICE_STATE,
                        ProjectionStatus.PROJECTION_STATE_ACTIVE_FOREGROUND)
                        != ProjectionStatus.PROJECTION_STATE_ACTIVE_FOREGROUND) {
                    logd(TAG, "skip device " + device.getName() + " - not foreground");
                    continue;
                }

                Parcelable projectingBluetoothDevice =
                        extras.getParcelable(BluetoothDevice.EXTRA_DEVICE);
                logd(TAG, "Device " + device.getName() + " has BT device "
                        + projectingBluetoothDevice);

                if (projectingBluetoothDevice == null) {
                    logi(TAG, "Suppressing message notification - device " + device
                            + " is projection, and does not specify a Bluetooth address");
                    return true;
                } else if (!(projectingBluetoothDevice instanceof BluetoothDevice)) {
                    loge(TAG, "Device " + device + " has bad EXTRA_DEVICE value "
                            + projectingBluetoothDevice + " - treating as unspecified");
                    return true;
                } else if (bluetoothAddress.equals(
                        ((BluetoothDevice) projectingBluetoothDevice).getAddress())) {
                    logi(TAG, "Suppressing message notification - device " + device
                            + "is projecting, and message is coming from device's Bluetooth address"
                            + bluetoothAddress);
                    return true;
                }
            }
        }

        // No projecting apps want to suppress this device, so let it through.
        return false;
    }

    /** Returns {@code true} if a projection app is active in the foreground. **/
    private boolean isProjectionInActiveForeground() {
        return mProjectionState == ProjectionStatus.PROJECTION_STATE_ACTIVE_FOREGROUND;
    }
}
