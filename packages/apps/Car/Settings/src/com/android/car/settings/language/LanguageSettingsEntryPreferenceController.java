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

package com.android.car.settings.language;

import android.app.ActivityManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.RemoteException;

import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.internal.app.LocaleHelper;

import java.util.Locale;

/** Updates the language settings entry summary with the currently configured locale. */
public class LanguageSettingsEntryPreferenceController extends PreferenceController<Preference> {

    public LanguageSettingsEntryPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected void updateState(Preference preference) {
        Locale locale = getConfiguredLocale();
        preference.setSummary(
                LocaleHelper.getDisplayName(locale, locale, /* sentenceCase= */ true));
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    /**
     * Returns the locale from current system configuration, or the default locale if no system
     * locale is available.
     */
    private static Locale getConfiguredLocale() {
        try {
            Locale configLocale =
                    ActivityManager.getService().getConfiguration().getLocales().get(0);
            return configLocale != null ? configLocale : Locale.getDefault();
        } catch (RemoteException e) {
            return Locale.getDefault();
        }
    }
}
