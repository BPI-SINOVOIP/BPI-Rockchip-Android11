/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tv.settings.accessibility;

import static com.android.tv.settings.util.InstrumentationUtils.logToggleInteracted;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.tvsettings.TvSettingsEnums;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.UserHandle;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.Keep;
import androidx.preference.Preference;
import androidx.preference.TwoStatePreference;

import com.android.internal.logging.nano.MetricsProto;
import com.android.settingslib.accessibility.AccessibilityUtils;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;

import java.util.List;

/**
 * Fragment for configuring the accessibility shortcut
 */
@Keep
public class AccessibilityShortcutFragment extends SettingsPreferenceFragment {
    private static final String KEY_ENABLE = "enable";
    private static final String KEY_SERVICE = "service";

    /**
     * Setting specifying if the accessibility shortcut is enabled.
     *
     * @deprecated this setting is no longer in use.
     */
    @Deprecated
    private static final String ACCESSIBILITY_SHORTCUT_ENABLED =
            "accessibility_shortcut_enabled";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.accessibility_shortcut, null);

        final TwoStatePreference enablePref = (TwoStatePreference) findPreference(KEY_ENABLE);
        enablePref.setOnPreferenceChangeListener((preference, newValue) -> {
            logToggleInteracted(TvSettingsEnums.SYSTEM_A11Y_SHORTCUT_ON_OFF, (Boolean) newValue);
            setAccessibilityShortcutEnabled((Boolean) newValue);
            return true;
        });

        boolean shortcutEnabled = Settings.Secure.getInt(getContext().getContentResolver(),
                ACCESSIBILITY_SHORTCUT_ENABLED, 1) == 1;

        enablePref.setChecked(shortcutEnabled);
        setAccessibilityShortcutEnabled(shortcutEnabled);
    }

    @Override
    public void onResume() {
        super.onResume();
        final Preference servicePref = findPreference(KEY_SERVICE);
        final List<AccessibilityServiceInfo> installedServices = getContext()
                .getSystemService(AccessibilityManager.class)
                .getInstalledAccessibilityServiceList();
        final PackageManager packageManager = getContext().getPackageManager();
        final String currentService = getCurrentService(getContext());
        for (AccessibilityServiceInfo service : installedServices) {
            final String serviceString = service.getComponentName().flattenToString();
            if (TextUtils.equals(currentService, serviceString)) {
                servicePref.setSummary(service.getResolveInfo().loadLabel(packageManager));
            }
        }
    }

    private void setAccessibilityShortcutEnabled(boolean enabled) {
        Settings.Secure.putInt(getContext().getContentResolver(),
                ACCESSIBILITY_SHORTCUT_ENABLED, enabled ? 1 : 0);
        final Preference servicePref = findPreference(KEY_SERVICE);
        servicePref.setEnabled(enabled);
    }

    static String getCurrentService(Context context) {
        String shortcutServiceString = AccessibilityUtils
                .getShortcutTargetServiceComponentNameString(context, UserHandle.myUserId());
        if (shortcutServiceString != null) {
            ComponentName shortcutName = ComponentName.unflattenFromString(shortcutServiceString);
            if (shortcutName != null) {
                return shortcutName.flattenToString();
            }
        }
        return null;
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.ACCESSIBILITY_TOGGLE_GLOBAL_GESTURE;
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.SYSTEM_A11Y_SHORTCUT;
    }
}
