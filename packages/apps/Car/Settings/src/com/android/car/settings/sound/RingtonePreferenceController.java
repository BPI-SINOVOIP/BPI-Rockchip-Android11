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

package com.android.car.settings.sound;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;

import com.android.car.settings.common.CarSettingActivities;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;

/** Business logic for changing the default ringtone. */
public class RingtonePreferenceController extends PreferenceController<RingtonePreference> {

    private RingtoneManager mRingtoneManager;

    public RingtonePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController,
            CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mRingtoneManager = new RingtoneManager(context);
    }

    @Override
    protected void onCreateInternal() {
        mRingtoneManager.setType(getPreference().getRingtoneType());
    }

    @Override
    protected Class<RingtonePreference> getPreferenceType() {
        return RingtonePreference.class;
    }

    @Override
    protected void updateState(RingtonePreference preference) {
        Uri ringtoneUri = RingtoneManager.getActualDefaultRingtoneUri(getContext(),
                getPreference().getRingtoneType());
        // If this URI cannot be found by the ringtone manager, set the URI to be null.
        if (mRingtoneManager.getRingtonePosition(ringtoneUri) < 0) {
            ringtoneUri = null;
        }
        preference.setSummary(Ringtone.getTitle(getContext(), ringtoneUri,
                /* followSettingsUri= */ false, /* allowRemote= */ true));
    }

    @Override
    protected boolean handlePreferenceClicked(RingtonePreference preference) {
        onPrepareRingtonePickerIntent(preference, preference.getIntent());
        getContext().startActivity(preference.getIntent());
        return true;
    }

    /**
     * Prepares the intent to launch the ringtone picker. This can be modified
     * to adjust the parameters of the ringtone picker.
     */
    private void onPrepareRingtonePickerIntent(RingtonePreference ringtonePreference,
            Intent ringtonePickerIntent) {
        ringtonePickerIntent.setClass(getContext(),
                CarSettingActivities.RingtonePickerActivity.class);

        ringtonePickerIntent.putExtra(RingtoneManager.EXTRA_RINGTONE_TITLE,
                ringtonePreference.getTitle());
        ringtonePickerIntent.putExtra(RingtoneManager.EXTRA_RINGTONE_TYPE,
                ringtonePreference.getRingtoneType());
        ringtonePickerIntent.putExtra(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT,
                ringtonePreference.getShowSilent());

        // Allow playback in external activity.
        ringtonePickerIntent.putExtra(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);
    }
}
