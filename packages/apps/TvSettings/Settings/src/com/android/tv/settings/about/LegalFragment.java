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

package com.android.tv.settings.about;

import static com.android.tv.settings.util.InstrumentationUtils.logEntrySelected;

import android.app.tvsettings.TvSettingsEnums;
import android.content.Context;
import android.os.Bundle;

import androidx.annotation.Keep;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.android.internal.logging.nano.MetricsProto;
import com.android.tv.settings.PreferenceUtils;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.overlay.FeatureFactory;

@Keep
public class LegalFragment extends SettingsPreferenceFragment {

    private static final String KEY_TERMS = "terms";
    private static final String KEY_LICENSE = "license";
    private static final String KEY_COPYRIGHT = "copyright";
    private static final String KEY_WEBVIEW_LICENSE = "webview_license";
    private static final String KEY_ADS = "ads";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.about_legal, null);
        final PreferenceScreen screen = getPreferenceScreen();

        final Context context = getActivity();
        PreferenceUtils.resolveSystemActivityOrRemove(context, screen,
                findPreference(KEY_TERMS), PreferenceUtils.FLAG_SET_TITLE);
        PreferenceUtils.resolveSystemActivityOrRemove(context, screen,
                findPreference(KEY_LICENSE), PreferenceUtils.FLAG_SET_TITLE);
        PreferenceUtils.resolveSystemActivityOrRemove(context, screen,
                findPreference(KEY_COPYRIGHT), PreferenceUtils.FLAG_SET_TITLE);
        PreferenceUtils.resolveSystemActivityOrRemove(context, screen,
                findPreference(KEY_WEBVIEW_LICENSE), PreferenceUtils.FLAG_SET_TITLE);
        if (FeatureFactory.getFactory(getContext()).isTwoPanelLayout()) {
            Preference adsPref = findPreference(KEY_ADS);
            if (adsPref != null) {
                adsPref.setVisible(false);
            }
        } else {
            PreferenceUtils.resolveSystemActivityOrRemove(context, screen,
                    findPreference(KEY_ADS), PreferenceUtils.FLAG_SET_TITLE);
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        switch (preference.getKey()) {
            case KEY_LICENSE:
                logEntrySelected(TvSettingsEnums.SYSTEM_ABOUT_LEGAL_INFO_OPEN_SOURCE);
                break;
            case KEY_TERMS:
                logEntrySelected(TvSettingsEnums.SYSTEM_ABOUT_LEGAL_INFO_GOOGLE_LEGAL);
                break;
            case KEY_WEBVIEW_LICENSE:
                logEntrySelected(TvSettingsEnums.SYSTEM_ABOUT_LEGAL_INFO_SYSTEM_WEBVIEW);
                break;
        }
        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.ABOUT_LEGAL_SETTINGS;
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.SYSTEM_ABOUT_LEGAL_INFO;
    }
}
