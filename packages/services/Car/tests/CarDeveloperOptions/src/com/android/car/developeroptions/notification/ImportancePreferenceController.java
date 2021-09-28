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

package com.android.car.developeroptions.notification;

import static android.app.NotificationChannel.USER_LOCKED_SOUND;
import static android.app.NotificationManager.IMPORTANCE_DEFAULT;

import android.app.NotificationChannel;
import android.content.Context;
import android.media.RingtoneManager;

import com.android.car.developeroptions.core.PreferenceControllerMixin;

import androidx.preference.Preference;

public class ImportancePreferenceController extends NotificationPreferenceController
        implements PreferenceControllerMixin, Preference.OnPreferenceChangeListener  {

    private static final String KEY_IMPORTANCE = "importance";
    private NotificationSettingsBase.ImportanceListener mImportanceListener;

    public ImportancePreferenceController(Context context,
            NotificationSettingsBase.ImportanceListener importanceListener,
            NotificationBackend backend) {
        super(context, backend);
        mImportanceListener = importanceListener;
    }

    @Override
    public String getPreferenceKey() {
        return KEY_IMPORTANCE;
    }

    @Override
    public boolean isAvailable() {
        if (mAppRow == null) {
            return false;
        }
        if (mAppRow.banned) {
            return false;
        }
        if (mChannel == null) {
            return false;
        }
        if (isDefaultChannel()) {
            return false;
        }
        if (mChannelGroup != null && mChannelGroup.isBlocked()) {
            return false;
        }
        return true;
    }

    @Override
    public void updateState(Preference preference) {
        if (mAppRow!= null && mChannel != null) {
            preference.setEnabled(mAdmin == null && isChannelConfigurable());
            ImportancePreference pref = (ImportancePreference) preference;
            pref.setBlockable(isChannelBlockable());
            pref.setConfigurable(isChannelConfigurable());
            pref.setImportance(mChannel.getImportance());
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (mChannel != null) {
            final int importance = (Integer) newValue;

            // If you are moving from an importance level without sound to one with sound,
            // but the sound you had selected was "Silence",
            // then set sound for this channel to your default sound,
            // because you probably intended to cause this channel to actually start making sound.
            if (mChannel.getImportance() < IMPORTANCE_DEFAULT
                    && !SoundPreferenceController.hasValidSound(mChannel)
                    && importance >= IMPORTANCE_DEFAULT) {
                mChannel.setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION),
                        mChannel.getAudioAttributes());
                mChannel.lockFields(USER_LOCKED_SOUND);
            }

            mChannel.setImportance(importance);
            mChannel.lockFields(NotificationChannel.USER_LOCKED_IMPORTANCE);
            saveChannel();
            mImportanceListener.onImportanceChanged();
        }
        return true;
    }
}
