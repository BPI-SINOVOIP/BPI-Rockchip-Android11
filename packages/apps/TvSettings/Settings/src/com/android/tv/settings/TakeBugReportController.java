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

package com.android.tv.settings;

import android.app.Activity;
import android.content.Context;

import androidx.preference.Preference;

import com.android.settingslib.core.AbstractPreferenceController;
import com.android.tv.settings.system.development.DevelopmentFragment;

/**
 * Add "take a bug report" preference for quick settings in dog-food device.
 */
public class TakeBugReportController extends AbstractPreferenceController {
    static final String KEY_TAKE_BUG_REPORT = "take_bug_report";

    public TakeBugReportController(Context context) {
        super(context);
    }

    @Override
    public boolean isAvailable() {
        return android.os.Build.TYPE.equals("userdebug")
                && mContext != null
                && mContext.getResources() != null
                && mContext.getResources().getBoolean(
                        R.bool.config_quick_settings_show_take_bugreport);
    }

    @Override
    public String getPreferenceKey() {
        return KEY_TAKE_BUG_REPORT;
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        if (KEY_TAKE_BUG_REPORT.equals(preference.getKey())) {
            DevelopmentFragment.captureBugReport((Activity) mContext);
            return true;
        }
        return super.handlePreferenceTreeClick(preference);
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);
        if (KEY_TAKE_BUG_REPORT.equals(preference.getKey())) {
            preference.setTitle(com.android.internal.R.string.bugreport_title);
            preference.setIcon(R.drawable.ic_bug_report);
        }
    }
}
