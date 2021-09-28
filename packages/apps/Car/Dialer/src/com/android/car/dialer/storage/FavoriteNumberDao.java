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

package com.android.car.dialer.storage;

import androidx.lifecycle.LiveData;
import androidx.room.Dao;
import androidx.room.Delete;
import androidx.room.Insert;
import androidx.room.Query;
import androidx.room.Update;

import java.util.List;

/** Data access object for the {@link FavoriteNumberEntity} that interacts with the database. */
@Dao
public interface FavoriteNumberDao {
    /** Insert a new entry to database. */
    @Insert
    void insert(FavoriteNumberEntity favoriteNumber);

    /** Insert multiple favorite number entries. */
    @Insert
    void insertAll(List<FavoriteNumberEntity> favoriteNumbers);

    /** Get all the favorite number entries. */
    @Query("SELECT * FROM favorite_number_entity")
    LiveData<List<FavoriteNumberEntity>> loadAll();

    /**
     * Update the given favorite number entry. Does nothing if the entry does not exist in database.
     */
    @Update
    void update(FavoriteNumberEntity favoriteNumber);

    /** Update multiple favorite number entries. */
    @Update
    void updateAll(List<FavoriteNumberEntity> favoriteNumbers);

    /** Delete the favorite number entry from database. */
    @Delete
    void delete(FavoriteNumberEntity favoriteNumbers);

    /** Delete all the favorite numbers whose account name do not match any of the devices. */
    @Query("DELETE FROM favorite_number_entity WHERE mAccountName IS NOT NULL"
            + " AND mAccountName NOT IN (:pairedDeviceAddresses)")
    void cleanup(List<String> pairedDeviceAddresses);
}
