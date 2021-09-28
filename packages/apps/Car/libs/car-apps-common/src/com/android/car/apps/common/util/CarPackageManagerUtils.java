/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.apps.common.util;

import android.app.PendingIntent;
import android.car.content.pm.CarPackageManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.annotation.NonNull;

/**
 * Utility class to access CarPackageManager
 */
public class CarPackageManagerUtils {
    private static final String TAG = "CarPackageManagerUtils";

    /**
     * Returns whether the given {@link PendingIntent} represents an activity that is distraction
     * optimized.
     */
    public static boolean isDistractionOptimized(CarPackageManager carPackageManager,
            @NonNull PendingIntent pendingIntent) {
        if (carPackageManager != null) {
            return carPackageManager.isPendingIntentDistractionOptimized(pendingIntent);
        }
        return false;
    }

    /**
     * Returns true if the provided Activity is distraction optimized
     */
    public static boolean isDistractionOptimized(CarPackageManager carPackageManager,
            @NonNull ActivityInfo activityInfo) {
        if (carPackageManager != null) {
            return carPackageManager.isActivityDistractionOptimized(
                    activityInfo.packageName, activityInfo.name);
        }
        return false;
    }

    /**
     * Attempts to resolve the provided intent into an activity, and returns true if the
     * resolved activity is distraction optimized
     */
    public static boolean isDistractionOptimized(CarPackageManager carPackageManager,
            PackageManager packageManager, Intent intent) {
        ResolveInfo info = packageManager.resolveActivity(
                intent, PackageManager.MATCH_DEFAULT_ONLY);
        return (info != null) ? isDistractionOptimized(carPackageManager, info.activityInfo)
                : false;
    }
}
