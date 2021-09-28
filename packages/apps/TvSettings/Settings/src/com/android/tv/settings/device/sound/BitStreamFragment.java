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
 * limitations under the License
 */

package com.android.tv.settings.device.sound;

import android.content.ContentResolver;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.os.audio.RkAudioSettingManager;
import android.media.AudioSystem;
import android.os.Bundle;
import android.provider.Settings;
import androidx.preference.SwitchPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceViewHolder;
import androidx.preference.TwoStatePreference;
import android.text.TextUtils;

import com.android.internal.logging.nano.MetricsProto;
import com.android.settingslib.core.AbstractPreferenceController;
import com.android.tv.settings.PreferenceControllerFragment;
import com.android.tv.settings.R;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import android.util.Log;
import android.os.SystemProperties;

/**
 * The "BitStreamFragment" screen in Sound Settings.
 */
public class BitStreamFragment extends PreferenceControllerFragment implements Preference.OnPreferenceChangeListener {
    private static final String TAG = "BitStreamFragment";

    static final String KEY_SURROUND_PASSTHROUGH = "bit_stream_surround_passthrough";
    static final String KEY_SURROUND_SOUND_CATEGORY = "bit_stream_surround_sound_category";
    static final String KEY_SURROUND_SOUND_FORMAT_PREFIX = "surround_sound_format_";
    static final String KEY_AUDIO_DEVICE = "audio_device";

    static final String VAL_SURROUND_SOUND_AUTO = "Auto";
    static final String VAL_SURROUND_SOUND_NEVER = "never";
    static final String VAL_SURROUND_SOUND_ALWAYS = "always";
    static final String VAL_SURROUND_SOUND_MANUAL = "Manual";

    public static final String DECODE = "decode";
    public static final String HDMI = "hdmi";
    public static final String SPDIF = "spdif";

    public static final int MODE_DECODE = 0;
    public static final int MODE_HDMI = 1;
    public static final int MODE_SPDIF = 2;

    public static final int DEVICE_DECODE = 0;
    public static final int DEVICE_BITSTREAM = 1;

    public static final int MODE_NOT_SET = 0;
    public static final int MODE_SUCCESS = 1;

    public static final int MODE_AUTO = 0;
    public static final int MDOE_MANUAL = 1;

    static public RkAudioSettingManager mAudioSetting;
    private Map<String, Boolean> mFormats;
    private List<AbstractPreferenceController> mPreferenceControllers;
    private PreferenceCategory mSurroundSoundCategoryPref;
    private ListPreference audioDevicePref;
    private ListPreference audioDeviceHdmiPref;
    private ListPreference surroundPref;

    private String[] decodeFormats;
    private String[] hdmiFormats;
    private String[] spdifFormats;

    public static BitStreamFragment newInstance() {
        return new BitStreamFragment();
    }

    @Override
    public void onAttach(Context context) {
        mAudioSetting = new RkAudioSettingManager();
        // decodeFormats = context.getResources().getStringArray(R.array.decode_formats);
        hdmiFormats = context.getResources().getStringArray(R.array.hdmi_formats);
        spdifFormats = context.getResources().getStringArray(R.array.spdif_formats);
        mFormats = new LinkedHashMap<String, Boolean>();
        getFormats();
        super.onAttach(context);
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.bitstream;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.bitstream, null);

        audioDevicePref = (ListPreference) findPreference(KEY_AUDIO_DEVICE);

        audioDevicePref.setOnPreferenceChangeListener(this);

        surroundPref = (ListPreference) findPreference(KEY_SURROUND_PASSTHROUGH);
        surroundPref.setOnPreferenceChangeListener(this);

        mSurroundSoundCategoryPref = (PreferenceCategory) findPreference(KEY_SURROUND_SOUND_CATEGORY);
        buildPref(getDevice());

        createFormatPreferences();
        updateFormatPreferencesStates();
    }

    @Override
    protected List<AbstractPreferenceController> onCreatePreferenceControllers(Context context) {
        mPreferenceControllers = new ArrayList<>(mFormats.size());
        for (Map.Entry<String, Boolean> format : mFormats.entrySet()) {
            mPreferenceControllers
                    .add(new SoundFormatPreferenceControllerBitstream(context, format.getKey() /* formatId */));
        }
        return mPreferenceControllers;
    }

    /** Creates and adds switches for each surround sound format. */
    private void createFormatPreferences() {
        Log.i(TAG, "mFormats = " + mFormats.size() + ", = " + mFormats.toString());
        mPreferenceControllers.clear();
        clearPreferenceControllers();
        for (AbstractPreferenceController controller : onCreatePreferenceControllers(getActivity())) {
            addPreferenceController(controller);
        }
        for (Map.Entry<String, Boolean> format : mFormats.entrySet()) {
            String formatId = format.getKey();
            boolean enabled = format.getValue();
            Log.i(TAG, "for formatId = " + formatId + ", enabled = " + enabled);
            SwitchPreference pref = new SwitchPreference(getPreferenceManager().getContext()) {
                @Override
                public void onBindViewHolder(PreferenceViewHolder holder) {
                    super.onBindViewHolder(holder);
                    // Enabling the view will ensure that the preference is focusable even if it
                    // the preference is disabled. This allows the user to scroll down over the
                    // disabled surround sound formats and see them all.
                    holder.itemView.setEnabled(true);
                }
            };
            pref.setTitle(getFormatDisplayName(formatId));
            pref.setKey(formatId);
            pref.setChecked(enabled);
            mSurroundSoundCategoryPref.addPreference(pref);
        }
    }

    /**
     * @return the display name for each surround sound format.
     */
    private String getFormatDisplayName(String formatId) {
        switch (formatId) {
        case "AC3":
            return getContext().getResources().getString(R.string.surround_sound_format_ac3);
        case "EAC3":
            return getContext().getResources().getString(R.string.surround_sound_format_e_ac3);
        case "DTS":
            return getContext().getResources().getString(R.string.surround_sound_format_dts);
        case "DTSHD":
            return getContext().getResources().getString(R.string.surround_sound_format_dts_hd);
        case "TRUEHD":
            return getContext().getResources().getString(R.string.surround_sound_format_true_hd);
        /* case AudioFormat.ENCODING_AC4:
            return "AC4"; */
        default:
            // Fallback in case new formats have been added that we don't know of.
            return formatId;
        }
    }

    private void updateFormatPreferencesStates() {
        for (AbstractPreferenceController controller : mPreferenceControllers) {
            Preference preference = mSurroundSoundCategoryPref.findPreference(controller.getPreferenceKey());
            if (preference != null) {
                controller.updateState(preference);
            }
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (TextUtils.equals(preference.getKey(), KEY_SURROUND_PASSTHROUGH)) {
            final String selection = (String) newValue;
            switch (selection) {
            case VAL_SURROUND_SOUND_AUTO:
                mAudioSetting.setMode(DEVICE_BITSTREAM, MODE_AUTO);
                break;
            case VAL_SURROUND_SOUND_MANUAL:
                mAudioSetting.setMode(DEVICE_BITSTREAM, MDOE_MANUAL);
                break;
            default:
                throw new IllegalArgumentException("Unknown surround sound pref value: " + selection);
            }
            updateFormatPreferencesStates();
            return true;
        } else if (TextUtils.equals(preference.getKey(), KEY_AUDIO_DEVICE)) {
            final String selection = (String) newValue;
            Log.i(TAG, "ROCKCHIP selection = " + selection);
             if (selection.equals(HDMI)) {
                mAudioSetting.setSelect(MODE_HDMI);
                mAudioSetting.setMode(DEVICE_BITSTREAM, MODE_AUTO);
            } else if (selection.equals(SPDIF)) {
                mAudioSetting.setSelect(MODE_SPDIF);
                initSpdifFormats();
                // mAudioSetting.setMode(DEVICE_BITSTREAM, MDOE_MANUAL);
            } else {
                mAudioSetting.setSelect(MODE_DECODE);
                mAudioSetting.setMode(DEVICE_DECODE, MODE_AUTO);
            }
            buildPref(selection);
            getFormats();
            createFormatPreferences();
            updateFormatPreferencesStates();
        }
        return true;
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.SOUND;
    }

    public String getDevice() {
        String device = DECODE;
        if (mAudioSetting.getSelect(MODE_HDMI) == MODE_SUCCESS) {
            device = HDMI;
        } else if (mAudioSetting.getSelect(MODE_SPDIF) == MODE_SUCCESS) {
            device = SPDIF;
        } else {
            device = DECODE;
        }
        return device;
    }

    private void buildPref(String device) {
        audioDevicePref.setValue(device);
        mSurroundSoundCategoryPref.removeAll();
        if (device.equals(HDMI)) {
            getPreferenceScreen().addPreference(mSurroundSoundCategoryPref);
            mSurroundSoundCategoryPref.addPreference(surroundPref);
            int bitstreamMode = mAudioSetting.getMode(MODE_HDMI);
            Log.i(TAG, "buildPref bitstreamMode = " + bitstreamMode);
            if (bitstreamMode == MODE_AUTO) {
                surroundPref.setValue(VAL_SURROUND_SOUND_AUTO);
            }
            if (bitstreamMode == MDOE_MANUAL) {
                surroundPref.setValue(VAL_SURROUND_SOUND_MANUAL);
            }
            surroundPref.setOnPreferenceChangeListener(this);
        } else if (device.equals(SPDIF)) {
            // mSurroundSoundCategoryPref.removePreference(surroundPref);
            getPreferenceScreen().addPreference(mSurroundSoundCategoryPref);
        } else {
            getPreferenceScreen().removePreference(mSurroundSoundCategoryPref);
        }
    }

    private void getFormats() {
        mFormats.clear();
        Log.i(TAG, "getForamts  = " + getDevice());
        if (getDevice().equals(SPDIF)) {
            for (String str : spdifFormats) {
                mFormats.put(str, mAudioSetting.getFormat(MODE_SPDIF, str) == MODE_SUCCESS);
            }
        } else if (getDevice().equals(HDMI)) {
            for (String str : hdmiFormats) {
                mFormats.put(str, mAudioSetting.getFormat(MODE_HDMI, str) == MODE_SUCCESS);
            }
        } else {

        }
    }

    private void initSpdifFormats() {
        for (String format : hdmiFormats) {
            mAudioSetting.setFormat(MODE_HDMI, SoundFormatPreferenceControllerBitstream.MODE_CLOSE, format);
        }
        for (String format : spdifFormats) {
            mAudioSetting.setFormat(MODE_SPDIF, SoundFormatPreferenceControllerBitstream.MODE_OPEN, format);
        }
    }
}
