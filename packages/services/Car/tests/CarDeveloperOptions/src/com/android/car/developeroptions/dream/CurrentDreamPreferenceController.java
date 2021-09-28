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

package com.android.car.developeroptions.dream;

import android.content.Context;

import androidx.preference.Preference;

import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.car.developeroptions.widget.GearPreference;
import com.android.settingslib.dream.DreamBackend;
import com.android.settingslib.dream.DreamBackend.DreamInfo;

import java.util.Optional;

public class CurrentDreamPreferenceController extends BasePreferenceController {

    private final DreamBackend mBackend;

    public CurrentDreamPreferenceController(Context context, String key) {
        super(context, key);
        mBackend = DreamBackend.getInstance(context);
    }

    @Override
    public int getAvailabilityStatus() {
        return mBackend.getDreamInfos().size() > 0 ? AVAILABLE : CONDITIONALLY_UNAVAILABLE;
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);
        setGearClickListenerForPreference(preference);
    }

    @Override
    public CharSequence getSummary() {
        return mBackend.getActiveDreamName();
    }

    private void setGearClickListenerForPreference(Preference preference) {
        if (!(preference instanceof GearPreference)) {
            return;
        }

        final GearPreference gearPreference = (GearPreference) preference;
        final Optional<DreamInfo> info = getActiveDreamInfo();
        if (!info.isPresent() || info.get().settingsComponentName == null) {
            gearPreference.setOnGearClickListener(null);
            return;
        }
        gearPreference.setOnGearClickListener(gearPref -> launchScreenSaverSettings());
    }

    private void launchScreenSaverSettings() {
        final Optional<DreamInfo> info = getActiveDreamInfo();
        if (!info.isPresent()) return;
        mBackend.launchSettings(mContext, info.get());
    }

    private Optional<DreamInfo> getActiveDreamInfo() {
        return mBackend.getDreamInfos()
                .stream()
                .filter((info) -> info.isActive)
                .findFirst();
    }
}
