/**
 * Copyright (c) 2019 The Android Open Source Project
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

package com.android.car.secondaryhome.launcher;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;

import java.util.Comparator;

/** An entry that represents a single activity that can be launched. */
public final class AppEntry {
    @NonNull
    private final Intent mLaunchIntent;
    @NonNull
    private final String mLabel;
    @Nullable
    private final Drawable mIcon;

    AppEntry(@NonNull ResolveInfo info, @NonNull PackageManager packageManager) {
        mLabel = info.loadLabel(packageManager).toString();
        mIcon = info.loadIcon(packageManager);
        mLaunchIntent = new Intent().setComponent(new ComponentName(
                info.activityInfo.packageName, info.activityInfo.name));
    }

    @NonNull String getLabel() {
        return mLabel;
    }

    @Nullable Drawable getIcon() {
        return mIcon;
    }

    @NonNull Intent getLaunchIntent() {
        return mLaunchIntent;
    }

    @NonNull ComponentName getComponentName() {
        return mLaunchIntent.getComponent();
    }

    @Override
    public String toString() {
        return mLabel;
    }

    public static final Comparator<AppEntry> AppNameComparator =
            Comparator.comparing(AppEntry::getLabel, String::compareToIgnoreCase);
}
