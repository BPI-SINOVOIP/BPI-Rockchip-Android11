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
package com.android.car.audio;

import android.annotation.UserIdInt;
import android.car.settings.CarSettings;
import android.content.ContentResolver;
import android.provider.Settings;

import androidx.annotation.NonNull;

import java.util.Objects;

/**
 * Use to save/load car volume settings
 */
public class CarAudioSettings {

    // The trailing slash forms a directory-liked hierarchy and
    // allows listening for both GROUP/MEDIA and GROUP/NAVIGATION.
    private static final String VOLUME_SETTINGS_KEY_FOR_GROUP_PREFIX = "android.car.VOLUME_GROUP/";

    // Key to persist master mute state in system settings
    private static final String VOLUME_SETTINGS_KEY_MASTER_MUTE = "android.car.MASTER_MUTE";

    /**
     * Gets the key to persist volume for a volume group in settings
     *
     * @param zoneId The audio zone id
     * @param groupId The volume group id
     * @return Key to persist volume index for volume group in system settings
     */
    private static String getVolumeSettingsKeyForGroup(int zoneId, int groupId) {
        final int maskedGroupId = (zoneId << 8) + groupId;
        return VOLUME_SETTINGS_KEY_FOR_GROUP_PREFIX + maskedGroupId;
    }

    private final ContentResolver mContentResolver;

    CarAudioSettings(@NonNull ContentResolver contentResolver) {
        mContentResolver = Objects.requireNonNull(contentResolver);
    }

    int getStoredVolumeGainIndexForUser(int userId, int zoneId, int id) {
        return Settings.System.getIntForUser(mContentResolver,
                getVolumeSettingsKeyForGroup(zoneId, id), -1, userId);
    }

    void storeVolumeGainIndexForUser(int userId, int zoneId, int id, int gainIndex) {
        Settings.System.putIntForUser(mContentResolver,
                getVolumeSettingsKeyForGroup(zoneId, id),
                gainIndex, userId);
    }

    void storeMasterMute(Boolean masterMuteValue) {
        Settings.Global.putInt(mContentResolver,
                VOLUME_SETTINGS_KEY_MASTER_MUTE,
                masterMuteValue ? 1 : 0);
    }

    boolean getMasterMute() {
        return Settings.Global.getInt(mContentResolver,
                VOLUME_SETTINGS_KEY_MASTER_MUTE, 0) != 0;
    }

    /**
     * Determines if for a given userId the reject navigation on call setting is enabled
     */
    public boolean isRejectNavigationOnCallEnabledInSettings(@UserIdInt int userId) {
        return Settings.Secure.getIntForUser(mContentResolver,
                CarSettings.Secure.KEY_AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL,
                /*disabled by default*/ 0, userId) == 1;
    }

    public ContentResolver getContentResolver() {
        return mContentResolver;
    }
}
