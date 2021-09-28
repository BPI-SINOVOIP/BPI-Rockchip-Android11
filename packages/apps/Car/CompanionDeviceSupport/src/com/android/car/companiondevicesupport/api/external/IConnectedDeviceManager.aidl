/**
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

package com.android.car.companiondevicesupport.api.external;

import android.os.ParcelUuid;

import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IConnectionCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceCallback;

/** Manager of devices connected to the car. */
interface IConnectedDeviceManager {

    /** Returns {@link List<CompanionDevice>} of devices currently connected. */
    List<CompanionDevice> getActiveUserConnectedDevices();

    /**
     * Register a callback for manager triggered connection events for only the currently active
     * user's devices.
     *
     * @param callback {@link IConnectionCallback} to register.
     */
    void registerActiveUserConnectionCallback(in IConnectionCallback callback);

    /**
     * Unregister a connection callback from manager.
     *
     * @param callback {@link IConnectionCallback} to unregister.
     */
    void unregisterConnectionCallback(in IConnectionCallback callback);

    /**
     * Register a callback for a specific companionDevice and recipient.
     *
     * @param companionDevice {@link CompanionDevice} to register triggers on.
     * @param recipientId {@link ParcelUuid} to register as recipient of.
     * @param callback {@link IDeviceCallback} to register.
     */
    void registerDeviceCallback(in CompanionDevice companionDevice, in ParcelUuid recipientId,
            in IDeviceCallback callback);

    /**
     * Unregister callback from companionDevice events.
     *
     * @param companionDevice {@link CompanionDevice} callback was registered on.
     * @param recipientId {@link ParcelUuid} callback was registered under.
     * @param callback {@link IDeviceCallback} to unregister.
     */
    void unregisterDeviceCallback(in CompanionDevice companionDevice, in ParcelUuid recipientId,
            in IDeviceCallback callback);

    /**
     * Securely send message to a companionDevice.
     *
     * @param companionDevice {@link CompanionDevice} to send the message to.
     * @param recipientId Recipient {@link ParcelUuid}.
     * @param message Message to send.
     * @return `true` if message was able to initiate, `false` if secure channel was not available.
     */
    boolean sendMessageSecurely(in CompanionDevice companionDevice, in ParcelUuid recipientId,
            in byte[] message);

    /**
     * Send an unencrypted message to a companionDevice.
     *
     * @param companionDevice {@link CompanionDevice} to send the message to.
     * @param recipientId Recipient {@link ParcelUuid}.
     * @param message Message to send.
     */
    void sendMessageUnsecurely(in CompanionDevice companionDevice, in ParcelUuid recipientId,
            in byte[] message);

    /**
     * Register a callback for associated devic erelated events.
     *
     * @param callback {@link IDeviceAssociationCallback} to register.
     */
    void registerDeviceAssociationCallback(in IDeviceAssociationCallback callback);

    /**
     * Unregister a device association callback from manager.
     *
     * @param callback {@link IDeviceAssociationCallback} to unregister.
     */
    void unregisterDeviceAssociationCallback(in IDeviceAssociationCallback callback);
}