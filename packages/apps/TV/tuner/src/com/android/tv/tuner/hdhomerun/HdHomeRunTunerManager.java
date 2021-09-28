/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.tuner.hdhomerun;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.annotation.WorkerThread;
import android.util.Log;
import java.util.HashSet;
import java.util.Set;

/**
 * A class to manage tuner resources of HDHomeRun devices. It handles tuner resource acquisition and
 * release.
 */
class HdHomeRunTunerManager {
    private static final String TAG = "HdHomeRunTunerManager";
    private static final boolean DEBUG = false;

    private static final String PREF_KEY_SCANNED_DEVICE_ID = "scanned_device_id";

    private static HdHomeRunTunerManager sInstance;

    private final Set<HdHomeRunDevice> mHdHomeRunDevices = new HashSet<>();
    private final Set<HdHomeRunDevice> mUsedDevices = new HashSet<>();

    private HdHomeRunTunerManager() {}

    /** Returns the instance of this manager. */
    public static synchronized HdHomeRunTunerManager getInstance() {
        if (sInstance == null) {
            sInstance = new HdHomeRunTunerManager();
        }
        return sInstance;
    }

    /** Returns number of tuners. */
    @WorkerThread
    synchronized int getTunerCount() {
        updateDevicesLocked(null);
        if (DEBUG) Log.d(TAG, "getTunerCount: " + mHdHomeRunDevices.size());
        return mHdHomeRunDevices.size();
    }

    /** Creates an HDHomeRun device. If there is no available one, returns {@code null}. */
    @WorkerThread
    synchronized HdHomeRunDevice acquireDevice(Context context) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        int scannedDeviceId = sp.getInt(PREF_KEY_SCANNED_DEVICE_ID, 0);
        updateDevicesLocked(scannedDeviceId == 0 ? null : scannedDeviceId);
        if (DEBUG) Log.d(TAG, "createDevice: device count = " + mHdHomeRunDevices.size());
        HdHomeRunDevice availableDevice = null;
        // Use the device used for scanning first since other devices might have different line-up.
        if (scannedDeviceId != 0) {
            for (HdHomeRunDevice device : mHdHomeRunDevices) {
                if (!mUsedDevices.contains(device) && scannedDeviceId == device.getDeviceId()) {
                    if (!HdHomeRunInterface.isDeviceAvailable(
                            device.getDeviceId(), device.getIpAddress(), device.getTunerIndex())) {
                        if (DEBUG) Log.d(TAG, "Device not available: " + device);
                        continue;
                    }
                    availableDevice = device;
                    break;
                }
            }
        }
        if (availableDevice == null) {
            for (HdHomeRunDevice device : mHdHomeRunDevices) {
                if (!mUsedDevices.contains(device)) {
                    if (!HdHomeRunInterface.isDeviceAvailable(
                            device.getDeviceId(), device.getIpAddress(), device.getTunerIndex())) {
                        if (DEBUG) Log.d(TAG, "Device not available: " + device);
                        continue;
                    }
                    availableDevice = device;
                    break;
                }
            }
        }
        if (availableDevice != null) {
            if (DEBUG) Log.d(TAG, "created device " + availableDevice);
            mUsedDevices.add(availableDevice);
            return availableDevice;
        }
        return null;
    }

    /** Releases a created device by {@link #acquireDevice(Context)}. */
    synchronized void releaseDevice(HdHomeRunDevice device) {
        if (DEBUG) Log.d(TAG, "releaseDevice: " + device);
        mUsedDevices.remove(device);
    }

    /**
     * Marks the device associated to this instance as a scanned device. Scanned device has higher
     * priority among multiple HDHomeRun devices.
     */
    static void markAsScannedDevice(Context context, HdHomeRunDevice device) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        sp.edit().putInt(PREF_KEY_SCANNED_DEVICE_ID, device.getDeviceId()).apply();
    }

    private void updateDevicesLocked(Integer deviceId) {
        mHdHomeRunDevices.clear();
        mHdHomeRunDevices.addAll(HdHomeRunInterface.scanDevices(deviceId));
    }
}
