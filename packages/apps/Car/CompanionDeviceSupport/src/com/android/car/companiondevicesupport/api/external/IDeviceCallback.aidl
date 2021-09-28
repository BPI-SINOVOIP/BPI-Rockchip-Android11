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

import com.android.car.companiondevicesupport.api.external.CompanionDevice;

/** Triggered companionDevice events for a connected companionDevice. */
oneway interface IDeviceCallback {
    /**
     * Triggered when secure channel has been established on a companionDevice. Encrypted messaging
     * now available.
     */
    void onSecureChannelEstablished(in CompanionDevice companionDevice);

    /** Triggered when a new message is received from a companionDevice. */
    void onMessageReceived(in CompanionDevice companionDevice, in byte[] message);

    /** Triggered when an error has occurred for a companionDevice. */
    void onDeviceError(in CompanionDevice companionDevice, int error);
}