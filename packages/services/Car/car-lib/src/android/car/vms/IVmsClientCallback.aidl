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

package android.car.vms;

import android.car.vms.VmsAvailableLayers;
import android.car.vms.VmsLayer;
import android.car.vms.VmsSubscriptionState;
import android.os.SharedMemory;

/**
 * Hidden API for sending notifications to Vehicle Map Service clients.
 *
 * @hide
 */
oneway interface IVmsClientCallback {
    void onLayerAvailabilityChanged(
        in VmsAvailableLayers availableLayers) = 0;

    void onSubscriptionStateChanged(
        in VmsSubscriptionState subscriptionState) = 1;

    void onPacketReceived(
        int providerId,
        in VmsLayer layer,
        in byte[] packet) = 2;

    void onLargePacketReceived(
        int providerId,
        in VmsLayer layer,
        in SharedMemory packet) = 3;
}
