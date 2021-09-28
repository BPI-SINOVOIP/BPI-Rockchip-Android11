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

package com.android.car.companiondevicesupport.api.internal.trust;

import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceAgentDelegate;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceEnrollmentCallback;
import com.android.car.companiondevicesupport.api.internal.trust.TrustedDevice;

/**
 * Manager of trusted devices with the car to be used by any service/activity that needs to interact
 * with trusted devices.
 */
interface ITrustedDeviceManager {

    /** Indicate the escrow token has been added for a user and corresponding handle. */
    void onEscrowTokenAdded(in int userId, in long handle);

    /** Indicate the escrow token has been activated for a user and corresponding handle. */
    void onEscrowTokenActivated(in int userId, in long handle);

    /** Register a new callback for trusted device events. */
    void registerTrustedDeviceCallback(in ITrustedDeviceCallback callback);

    /** Remove a previously registered callback. */
    void unregisterTrustedDeviceCallback(in ITrustedDeviceCallback callback);

    /** Register a new callback for enrollment triggered events. */
    void registerTrustedDeviceEnrollmentCallback(in ITrustedDeviceEnrollmentCallback callback);

    /** Remove a previously registered callback. */
    void unregisterTrustedDeviceEnrollmentCallback(in ITrustedDeviceEnrollmentCallback callback);

    /** Set a delegate for TrustAgent operation calls. */
    void setTrustedDeviceAgentDelegate(in ITrustedDeviceAgentDelegate trustAgentDelegate);

    /** Remove a prevoiusly set delegate. */
    void clearTrustedDeviceAgentDelegate(in ITrustedDeviceAgentDelegate trustAgentDelegate);

    /** Returns a list of trusted devices for user. */
    List<TrustedDevice> getTrustedDevicesForActiveUser();

    /** Remove a trusted device and invalidate any credentials associated with it. */
    void removeTrustedDevice(in TrustedDevice trustedDevice);

    /** Returns {@link List<CompanionDevice>} of devices currently connected. */
    List<CompanionDevice> getActiveUserConnectedDevices();

    /** Register a new callback for associated device events. */
    void registerAssociatedDeviceCallback(in IDeviceAssociationCallback callback);

    /** Remove a previously registered callback. */
    void unregisterAssociatedDeviceCallback(IDeviceAssociationCallback callback);

    /** Attempts to initiate trusted device enrollment on the phone with the given device id. */
    void initiateEnrollment(in String deviceId);
}
