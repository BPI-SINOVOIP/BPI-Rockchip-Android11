/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tv.settings.device.sound;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.os.audio.RkAudioSettingManager;
import android.provider.Settings;
import androidx.preference.SwitchPreference;
import androidx.preference.Preference;
import android.text.TextUtils;
import android.util.Log;

import com.android.settingslib.core.AbstractPreferenceController;

import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.HashSet;
import java.util.Map;
import android.media.AudioSystem;
import com.android.tv.settings.R;

/**
 * Controller for the surround sound switch preferences.
 */
public class SoundFormatPreferenceControllerBitstream extends AbstractPreferenceController {

    private static final String TAG = "SoundFormatPreferenceControllerBitstream";

    public static final int MODE_OPEN = 0;
    public static final int MODE_CLOSE = 1;

    private String mFormatId;
    private AudioManager mAudioManager;
    private Map<String, Boolean> mReportedFormats;
    private RkAudioSettingManager mAudioSetting;
    private String[] decodeFormats;
    private String[] hdmiFormats;
    private String[] spdifFormats;

    public SoundFormatPreferenceControllerBitstream(Context context, String formatId) {
        super(context);
        mFormatId = formatId;
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        mAudioSetting = BitStreamFragment.mAudioSetting;
        mReportedFormats = new LinkedHashMap<String, Boolean>();
        // decodeFormats = context.getResources().getStringArray(R.array.decode_formats);
        hdmiFormats = context.getResources().getStringArray(R.array.hdmi_formats);
        spdifFormats = context.getResources().getStringArray(R.array.spdif_formats);
        Log.i(TAG, "SoundFormatPreferenceControllerBitstream isDecodeDevice = " + (mAudioSetting.getSelect(BitStreamFragment.MODE_DECODE) == BitStreamFragment.MODE_SUCCESS));
        if (mAudioSetting.getSelect(BitStreamFragment.MODE_HDMI) == BitStreamFragment.MODE_SUCCESS) {
            Log.i(TAG, "SoundFormatPreferenceControllerBitstream hdmi");
            for (String str : hdmiFormats) {
                mReportedFormats.put(str,
                        mAudioSetting.getFormat(BitStreamFragment.MODE_HDMI, str) == BitStreamFragment.MODE_SUCCESS);
            }
        } else if (mAudioSetting.getSelect(BitStreamFragment.MODE_SPDIF) == BitStreamFragment.MODE_SUCCESS) {
            Log.i(TAG, "SoundFormatPreferenceControllerBitstream spdif");
            for (String str : spdifFormats) {
                mReportedFormats.put(str,
                        mAudioSetting.getFormat(BitStreamFragment.MODE_SPDIF, str) == BitStreamFragment.MODE_SUCCESS);
            }
        } else {
            Log.i(TAG, "SoundFormatPreferenceControllerBitstream decode");
        }
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public String getPreferenceKey() {
        return mFormatId;
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);
        if (preference.getKey().equals(getPreferenceKey())) {
            preference.setEnabled(getFormatPreferencesEnabledState());
            ((SwitchPreference) preference).setChecked(getFormatPreferenceCheckedState());
        }
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        if (preference.getKey().equals(getPreferenceKey())) {
            setSurroundManualFormatsSetting(((SwitchPreference) preference).isChecked());
        }
        return super.handlePreferenceTreeClick(preference);
    }

    /**
     * @return checked state of a surround sound format switch based on passthrough
     *         setting and audio manager state for the format.
     */
    private boolean getFormatPreferenceCheckedState() {
        if (mAudioSetting.getSelect(BitStreamFragment.MODE_HDMI) == BitStreamFragment.MODE_SUCCESS) {
            return mAudioSetting.getFormat(BitStreamFragment.MODE_HDMI, mFormatId) == BitStreamFragment.MODE_SUCCESS;
        } else if (mAudioSetting.getSelect(BitStreamFragment.MODE_SPDIF) == BitStreamFragment.MODE_SUCCESS) {
            return mAudioSetting.getFormat(BitStreamFragment.MODE_SPDIF, mFormatId) == BitStreamFragment.MODE_SUCCESS;
        } else {
            return mAudioSetting.getFormat(BitStreamFragment.MODE_DECODE, mFormatId) == BitStreamFragment.MODE_SUCCESS;
        }
    }

    /**
     * @return true if the format checkboxes should be enabled, i.e. in manual mode.
     */
    private boolean getFormatPreferencesEnabledState() {
        if (mAudioSetting.getSelect(BitStreamFragment.MODE_DECODE) == BitStreamFragment.MODE_SUCCESS) {
            return false;
        } else {
            Log.i(TAG, "getFormatPreferencesEnabledState spdif = " + mAudioSetting.getSelect(BitStreamFragment.MODE_SPDIF));
            Log.i(TAG, "getFormatPreferencesEnabledState hdmi = " + mAudioSetting.getSelect(BitStreamFragment.MODE_HDMI));
            if (mAudioSetting
                    .getSelect(BitStreamFragment.MODE_SPDIF) == BitStreamFragment.MODE_SUCCESS) {
                return true;
            }
            if (mAudioSetting.getSelect(BitStreamFragment.MODE_HDMI) == BitStreamFragment.MODE_SUCCESS && mAudioSetting.getMode(BitStreamFragment.MODE_HDMI) == BitStreamFragment.MDOE_MANUAL) {
                return true;
            } else {
                return false;
            }
        }
    }

    /**
     * Writes enabled/disabled state for a given format to the global settings.
     */
    private void setSurroundManualFormatsSetting(boolean enabled) {
        if (mAudioSetting.getSelect(BitStreamFragment.MODE_HDMI) == BitStreamFragment.MODE_SUCCESS) {
            mAudioSetting.setFormat(BitStreamFragment.MODE_HDMI, enabled ? MODE_OPEN : MODE_CLOSE, mFormatId);
        } else if (mAudioSetting.getSelect(BitStreamFragment.MODE_SPDIF) == BitStreamFragment.MODE_SUCCESS) {
            mAudioSetting.setFormat(BitStreamFragment.MODE_SPDIF, enabled ? MODE_OPEN : MODE_CLOSE, mFormatId);
        } else {
            mAudioSetting.setFormat(BitStreamFragment.MODE_DECODE, enabled ? MODE_OPEN : MODE_CLOSE, mFormatId);
        }
    }

}
