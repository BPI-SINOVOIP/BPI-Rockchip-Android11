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

package com.android.car.developeroptions.privacy;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.provider.DeviceConfig;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.NonNull;
import androidx.preference.Preference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.core.BasePreferenceController;

import java.util.List;


public class AccessibilityUsagePreferenceController extends BasePreferenceController  {

    private final @NonNull List<AccessibilityServiceInfo> mEnabledServiceInfos;

    public AccessibilityUsagePreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);

        final AccessibilityManager accessibilityManager = context.getSystemService(
                AccessibilityManager.class);
        mEnabledServiceInfos = accessibilityManager.getEnabledAccessibilityServiceList(
                AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
    }

    @Override
    public int getAvailabilityStatus() {
        return (mEnabledServiceInfos.isEmpty() || !Boolean.parseBoolean(
                DeviceConfig.getProperty(DeviceConfig.NAMESPACE_PRIVACY,
                        Utils.PROPERTY_PERMISSIONS_HUB_ENABLED)))
                ? UNSUPPORTED_ON_DEVICE : AVAILABLE;
    }

    @Override
    public CharSequence getSummary() {
        return mContext.getResources().getQuantityString(R.plurals.accessibility_usage_summary,
                mEnabledServiceInfos.size(), mEnabledServiceInfos.size());
    }
}
