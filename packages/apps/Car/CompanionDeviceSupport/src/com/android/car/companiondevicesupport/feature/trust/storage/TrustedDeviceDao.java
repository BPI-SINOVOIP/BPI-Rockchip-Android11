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

import androidx.room.Dao;
import androidx.room.Delete;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import java.util.List;

/** Queries for trusted device table. */
@Dao
public interface TrustedDeviceDao {

    /** Get a {@link TrustedDeviceEntity} based on device id. */
    @Query("SELECT * FROM trusted_devices WHERE id LIKE :deviceId LIMIT 1")
    TrustedDeviceEntity getTrustedDevice(String deviceId);

    /** Get all {@link TrustedDeviceEntity}s associated with a user. */
    @Query("SELECT * FROM trusted_devices WHERE userId LIKE :userId")
    List<TrustedDeviceEntity> getTrustedDevicesForUser(int userId);

    /**
     * Add a {@link TrustedDeviceEntity}. Replace if a device already exists with the same
     * device id.
     */
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void addOrReplaceTrustedDevice(TrustedDeviceEntity trustedDevice);

    /** Remove a {@link TrustedDeviceEntity}. */
    @Delete
    void removeTrustedDevice(TrustedDeviceEntity trustedDevice);
}
