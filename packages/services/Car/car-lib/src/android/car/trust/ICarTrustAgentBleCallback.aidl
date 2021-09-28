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

package android.car.trust;

import android.bluetooth.BluetoothDevice;

/**
 * Callback interface for BLE connection state changes during trusted device enrollment.
 *
 * @hide
 * @deprecated Adding a trust agent is no longer a supported feature and these APIs will be removed
 * in the next Android release.
 */
oneway interface ICarTrustAgentBleCallback {
    /**
     * Called when the GATT server is started and BLE is successfully advertising for enrollment.
     */
    void onEnrollmentAdvertisingStarted();

    /**
     * Called when the BLE enrollment advertisement fails to start.
     */
    void onEnrollmentAdvertisingFailed();

    /**
     * Called when a remote device is connected on BLE.
     */
    void onBleEnrollmentDeviceConnected(in BluetoothDevice device);

    /**
     * Called when a remote device is disconnected on BLE.
     */
    void onBleEnrollmentDeviceDisconnected(in BluetoothDevice device);
}
