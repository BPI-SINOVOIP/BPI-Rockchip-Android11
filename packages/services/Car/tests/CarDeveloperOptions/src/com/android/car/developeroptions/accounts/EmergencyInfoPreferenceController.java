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
package com.android.car.developeroptions.accounts;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.os.UserHandle;
import android.os.UserManager;
import android.text.TextUtils;

import androidx.preference.Preference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.settingslib.search.SearchIndexableRaw;

import java.util.List;

public class EmergencyInfoPreferenceController extends BasePreferenceController {
    public static String getIntentAction(Context context) {
        return context.getResources().getString(R.string.config_emergency_intent_action);
    }

    public EmergencyInfoPreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
    }

    @Override
    public void updateRawDataToIndex(List<SearchIndexableRaw> rawData) {
        if (isAvailable()) {
            SearchIndexableRaw data = new SearchIndexableRaw(mContext);
            final Resources res = mContext.getResources();
            data.title = res.getString(com.android.car.developeroptions.R.string.emergency_info_title);
            data.screenTitle = res.getString(com.android.car.developeroptions.R.string.emergency_info_title);
            rawData.add(data);
        }
    }

    @Override
    public void updateState(Preference preference) {
        UserInfo info = mContext.getSystemService(UserManager.class).getUserInfo(
                UserHandle.myUserId());
        preference.setSummary(mContext.getString(R.string.emergency_info_summary, info.name));
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        if (TextUtils.equals(getPreferenceKey(), preference.getKey())) {
            Intent intent = new Intent(getIntentAction(mContext));
            intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            mContext.startActivity(intent);
            return true;
        }
        return false;
    }

    @Override
    public int getAvailabilityStatus() {
        if (!mContext.getResources().getBoolean(R.bool.config_show_emergency_info_in_device_info)) {
            return UNSUPPORTED_ON_DEVICE;
        }
        final Intent intent = new Intent(getIntentAction(mContext)).setPackage(
                getPackageName(mContext));
        final List<ResolveInfo> infos = mContext.getPackageManager().queryIntentActivities(intent,
                0);
        return infos != null && !infos.isEmpty()
                ? AVAILABLE : UNSUPPORTED_ON_DEVICE;
    }

    private static String getPackageName(Context context) {
        return context.getResources().getString(R.string.config_emergency_package_name);
    }
}
