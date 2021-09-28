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

package com.android.car.companiondevicesupport.feature.trust.storage;

import androidx.annotation.NonNull;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

import com.android.car.companiondevicesupport.api.internal.trust.TrustedDevice;

/** Table entity representing a trusted device. */
@Entity(tableName = "trusted_devices")
public class TrustedDeviceEntity {
    /** Device id of trusted device. */
    @PrimaryKey
    @NonNull
    public String id;

    /** Id of user associated with trusted device. */
    public int userId;

    /** Handle assigned to this device. */
    public long handle;

    public TrustedDeviceEntity() { }

    public TrustedDeviceEntity(TrustedDevice trustedDevice) {
        id = trustedDevice.getDeviceId();
        userId = trustedDevice.getUserId();
        handle = trustedDevice.getHandle();
    }

    /** Return a new {@link TrustedDevice} of this entity. */
    public TrustedDevice toTrustedDevice() {
        return new TrustedDevice(id, userId, handle);
    }
}
