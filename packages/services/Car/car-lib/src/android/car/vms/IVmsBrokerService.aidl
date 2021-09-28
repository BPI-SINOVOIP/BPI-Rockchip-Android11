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

import android.car.vms.IVmsClientCallback;
import android.car.vms.VmsAssociatedLayer;
import android.car.vms.VmsLayer;
import android.car.vms.VmsLayerDependency;
import android.car.vms.VmsProviderInfo;
import android.car.vms.VmsRegistrationInfo;
import android.os.SharedMemory;

/**
 * Hidden API for communicating with the Vehicle Map Service message broker.
 *
 * @hide
 */
interface IVmsBrokerService {
    // Client operations
    // Restricted to callers with android.car.permission.VMS_SUBSCRIBER
    // or android.car.permission.VMS_PUBLISHER

    VmsRegistrationInfo registerClient(
    in IBinder token,
    in IVmsClientCallback callback,
    boolean legacyClient) = 0;

    void unregisterClient(in IBinder token) = 1;

    VmsProviderInfo getProviderInfo(in IBinder token, int providerId) = 2;

    // Subscriber operations
    // Restricted to callers with android.car.permission.VMS_SUBSCRIBER

    void setSubscriptions(
        in IBinder token,
        in List<VmsAssociatedLayer> layers) = 3;

    void setMonitoringEnabled(in IBinder token, boolean enabled) = 4;

    // Publisher operations
    // Restricted to callers with android.car.permission.VMS_PUBLISHER

    int registerProvider(
        in IBinder token,
        in VmsProviderInfo providerInfo) = 5;

    void setProviderOfferings(
        in IBinder token,
        int providerId,
        in List<VmsLayerDependency> offerings) = 6;

    void publishPacket(
        in IBinder token,
        int providerId,
        in VmsLayer layer,
        in byte[] packet) = 7;

    void publishLargePacket(
        in IBinder token,
        int providerId,
        in VmsLayer layer,
        in SharedMemory packet) = 8;
}
