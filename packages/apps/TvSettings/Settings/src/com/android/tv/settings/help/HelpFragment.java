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

package com.android.tv.settings.help;

import android.content.pm.ResolveInfo;
import android.os.Bundle;

import androidx.annotation.Keep;
import androidx.preference.Preference;

import com.android.internal.logging.nano.MetricsProto;
import com.android.tv.settings.MainFragment;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.overlay.FeatureFactory;

/**
 * The "Help & feedback" screen in TV settings.
 */
@Keep
public class HelpFragment extends SettingsPreferenceFragment {

    private static final String KEY_HELP = "help_center";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.help_and_feedback, null);

        final Preference helpPref = findPreference(KEY_HELP);
        if (helpPref != null) {
            final ResolveInfo info = MainFragment.systemIntentIsHandled(getActivity(),
                    helpPref.getIntent());
            helpPref.setVisible(info != null);
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        switch (preference.getKey()) {
            case KEY_HELP:
                FeatureFactory.getFactory(getActivity()).getSupportFeatureProvider().startSupport(
                        getActivity());
                return true;
            default:
                return super.onPreferenceTreeClick(preference);
        }
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.ACTION_SETTING_HELP_AND_FEEDBACK;
    }
}
