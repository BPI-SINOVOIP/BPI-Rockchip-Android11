/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.settings.device.displaysound;

import android.content.ContentResolver;
import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.preference.Preference;
import androidx.preference.TwoStatePreference;

import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;

/**
 * The "Display & sound" screen in TV Settings.
 */
@Keep
public class DisplaySoundFragment extends SettingsPreferenceFragment {

    static final String KEY_SOUND_EFFECTS = "sound_effects";

    private AudioManager mAudioManager;

    public static DisplaySoundFragment newInstance() {
        return new DisplaySoundFragment();
    }

    public static boolean getSoundEffectsEnabled(ContentResolver contentResolver) {
        return Settings.System.getInt(contentResolver, Settings.System.SOUND_EFFECTS_ENABLED, 1)
                != 0;
    }

    @Override
    public void onAttach(Context context) {
        mAudioManager = context.getSystemService(AudioManager.class);
        super.onAttach(context);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.display_sound, null);

        final TwoStatePreference soundPref = findPreference(KEY_SOUND_EFFECTS);
        soundPref.setChecked(getSoundEffectsEnabled());
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (TextUtils.equals(preference.getKey(), KEY_SOUND_EFFECTS)) {
            final TwoStatePreference soundPref = (TwoStatePreference) preference;
            setSoundEffectsEnabled(soundPref.isChecked());
        }
        return super.onPreferenceTreeClick(preference);
    }

    private boolean getSoundEffectsEnabled() {
        return getSoundEffectsEnabled(getActivity().getContentResolver());
    }

    private void setSoundEffectsEnabled(boolean enabled) {
        if (enabled) {
            mAudioManager.loadSoundEffects();
        } else {
            mAudioManager.unloadSoundEffects();
        }
        Settings.System.putInt(getActivity().getContentResolver(),
                Settings.System.SOUND_EFFECTS_ENABLED, enabled ? 1 : 0);
    }

    @Override
    public int getMetricsCategory() {
        return 0;
    }
}
