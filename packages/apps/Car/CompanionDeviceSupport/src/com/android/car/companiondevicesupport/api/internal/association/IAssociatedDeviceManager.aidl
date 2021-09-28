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

package com.android.car.companiondevicesupport.api.internal.association;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IConnectionCallback;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.internal.association.IAssociationCallback;

/** Manager of devices associated with the car. */
interface IAssociatedDeviceManager {

    /**
     * Set a callback for association.
     * @param callback {@link IAssociationCallback} to set.
     */
    void setAssociationCallback(in IAssociationCallback callback);

    /** Clear the association callback from manager. */
    void clearAssociationCallback();

    /** Starts the association with a new device. */
    void startAssociation();

    /** Stops the association with current device. */
    void stopAssociation();

    /** Returns {@link List<AssociatedDevice>} of devices associated with the given user. */
    List<AssociatedDevice> getActiveUserAssociatedDevices();

    /** Confirms the paring code. */
    void acceptVerification();

    /** Remove the associated device of the given identifier for the active user. */
    void removeAssociatedDevice(in String deviceId);

    /**
     * Set a callback for associated device related events.
     *
     * @param callback {@link IDeviceAssociationCallback} to set.
     */
    void setDeviceAssociationCallback(in IDeviceAssociationCallback callback);

    /** Clear the device association callback from manager. */
    void clearDeviceAssociationCallback();

    /** Returns {@link List<CompanionDevice>} of devices currently connected. */
    List<CompanionDevice> getActiveUserConnectedDevices();

    /**
     * Set a callback for connection events for only the currently active user's devices.
     *
     * @param callback {@link IConnectionCallback} to set.
     */
    void setConnectionCallback(in IConnectionCallback callback);

    /** Clear the connection callback from manager. */
    void clearConnectionCallback();

    /** Enable connection on the associated device with the given identifier. */
    void enableAssociatedDeviceConnection(in String deviceId);

    /** Disable connection on the associated device with the given identifier. */
    void disableAssociatedDeviceConnection(in String deviceId);
}
