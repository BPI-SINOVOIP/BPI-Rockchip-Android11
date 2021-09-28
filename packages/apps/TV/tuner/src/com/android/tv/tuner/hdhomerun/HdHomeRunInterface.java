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

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.util.Log;
import com.android.tv.tuner.hdhomerun.HdHomeRunDiscover.HdHomeRunDiscoverDevice;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** An interface class provides methods to access physical HDHomeRun devices. */
@WorkerThread
public class HdHomeRunInterface {
    private static final String TAG = "HdHomeRunInterface";
    private static final boolean DEBUG = false;

    private static final int FETCH_DEVICE_NAME_TRY_NUM = 2;
    private static final int MAX_DEVICES = 1;
    private static final boolean DISABLE_CABLE = false;

    /**
     * Scans for HDHomeRun devices on the network.
     *
     * @param deviceId The target device ID we want to find, scans for all available devices if
     *     {@code null} or the given ID cannot be found.
     * @return A set of HDHomeRun devices
     */
    @NonNull
    public static Set<HdHomeRunDevice> scanDevices(Integer deviceId) {
        List<HdHomeRunDiscoverDevice> discoveredDevices = null;
        if (deviceId != null) {
            discoveredDevices =
                    HdHomeRunUtils.findHdHomeRunDevices(
                            0, HdHomeRunUtils.HDHOMERUN_DEVICE_TYPE_TUNER, deviceId, 1);
            if (discoveredDevices.isEmpty()) {
                Log.i(TAG, "Can't find device with ID: " + deviceId);
            }
        }
        if (discoveredDevices == null || discoveredDevices.isEmpty()) {
            discoveredDevices =
                    HdHomeRunUtils.findHdHomeRunDevices(
                            0,
                            HdHomeRunUtils.HDHOMERUN_DEVICE_TYPE_TUNER,
                            HdHomeRunUtils.HDHOMERUN_DEVICE_ID_WILDCARD,
                            MAX_DEVICES);
            if (DEBUG) Log.d(TAG, "Found " + discoveredDevices.size() + " devices");
        }
        Set<HdHomeRunDevice> result = new HashSet<>();
        for (HdHomeRunDiscoverDevice discoveredDevice : discoveredDevices) {
            String model =
                    fetchDeviceModel(discoveredDevice.mDeviceId, discoveredDevice.mIpAddress);
            if (model == null) {
                Log.e(TAG, "Fetching device model failed: " + discoveredDevice.mDeviceId);
                continue;
            } else if (DEBUG) {
                Log.d(TAG, "Fetch Device Model: " + model);
            }
            if (DISABLE_CABLE) {
                if (model != null && model.contains("cablecard")) {
                    // filter out CableCARD devices
                    continue;
                }
            }
            for (int i = 0; i < discoveredDevice.mTunerCount; i++) {
                result.add(
                        new HdHomeRunDevice(
                                discoveredDevice.mIpAddress,
                                discoveredDevice.mDeviceType,
                                discoveredDevice.mDeviceId,
                                i,
                                model));
            }
        }
        return result;
    }

    /**
     * Returns {@code true} if the given device IP, ID and tuner index is available for use.
     *
     * @param deviceId The target device ID, 0 denotes a wildcard match.
     * @param deviceIp The target device IP.
     * @param tunerIndex The target tuner index of the target device. This parameter is only
     *     meaningful when the target device has multiple tuners.
     */
    public static boolean isDeviceAvailable(int deviceId, int deviceIp, int tunerIndex) {
        // TODO: check the lock state for the given tuner.
        if ((deviceId == 0) && (deviceIp == 0)) {
            return false;
        }
        if (HdHomeRunUtils.isIpMulticast(deviceIp)) {
            return false;
        }
        if ((deviceId == 0) || (deviceId == HdHomeRunUtils.HDHOMERUN_DEVICE_ID_WILDCARD)) {
            try (HdHomeRunControlSocket controlSock =
                    new HdHomeRunControlSocket(deviceId, deviceIp)) {
                deviceId = controlSock.getDeviceId();
            }
        }
        return deviceId != 0;
    }

    @Nullable
    private static String fetchDeviceModel(int deviceId, int deviceIp) {
        for (int i = 0; i < FETCH_DEVICE_NAME_TRY_NUM; i++) {
            try (HdHomeRunControlSocket controlSock =
                    new HdHomeRunControlSocket(deviceId, deviceIp)) {
                String model = controlSock.get("/sys/model");
                if (model != null) {
                    return model;
                }
            }
        }
        return null;
    }

    private HdHomeRunInterface() {}
}
