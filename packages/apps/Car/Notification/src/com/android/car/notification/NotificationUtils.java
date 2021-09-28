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

package com.android.car.notification;

import android.annotation.ColorInt;
import android.app.ActivityManager;
import android.app.Notification;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.os.Bundle;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import com.android.internal.graphics.ColorUtils;

public class NotificationUtils {
    private static final String TAG = "NotificationUtils";
    private static final int MAX_FIND_COLOR_STEPS = 15;
    private static final double MIN_COLOR_CONTRAST = 0.00001;
    private static final double MIN_CONTRAST_RATIO = 4.5;
    private static final double MIN_LIGHTNESS = 0;
    private static final float MAX_LIGHTNESS = 1;
    private static final float LIGHT_COLOR_LUMINANCE_THRESHOLD = 0.5f;

    private NotificationUtils() {
    }

    /**
     * Returns the color assigned to the given attribute.
     */
    public static int getAttrColor(Context context, int attr) {
        TypedArray ta = context.obtainStyledAttributes(new int[]{attr});
        int colorAccent = ta.getColor(0, 0);
        ta.recycle();
        return colorAccent;
    }

    /**
     * Validates if the notification posted by the application meets at least one of the below
     * conditions.
     *
     * <ul>
     * <li>application is signed with platform key.
     * <li>application is a system and privileged app.
     * </ul>
     */
    public static boolean isSystemPrivilegedOrPlatformKey(Context context, AlertEntry alertEntry) {
        return isSystemPrivilegedOrPlatformKeyInner(context, alertEntry,
                /* checkForPrivilegedApp= */ true);
    }

    /**
     * Validates if the notification posted by the application meets at least one of the below
     * conditions.
     *
     * <ul>
     * <li>application is signed with platform key.
     * <li>application is a system app.
     * </ul>
     */
    public static boolean isSystemOrPlatformKey(Context context, AlertEntry alertEntry) {
        return isSystemPrivilegedOrPlatformKeyInner(context, alertEntry,
                /* checkForPrivilegedApp= */ false);
    }

    /**
     * Validates if the notification posted by the application is a system application.
     */
    public static boolean isSystemApp(Context context,
            StatusBarNotification statusBarNotification) {
        PackageInfo packageInfo = getPackageInfo(context, statusBarNotification);
        if (packageInfo == null) return false;

        return packageInfo.applicationInfo.isSystemApp();
    }

    /**
     * Validates if the notification posted by the application is signed with platform key.
     */
    public static boolean isSignedWithPlatformKey(Context context,
            StatusBarNotification statusBarNotification) {
        PackageInfo packageInfo = getPackageInfo(context, statusBarNotification);
        if (packageInfo == null) return false;

        return packageInfo.applicationInfo.isSignedWithPlatformKey();
    }

    /**
     * Choose a correct notification layout for this heads-up notification.
     * Note that the layout chosen can be different for the same notification
     * in the notification center.
     */
    public static CarNotificationTypeItem getNotificationViewType(AlertEntry alertEntry) {
        String category = alertEntry.getNotification().category;
        if (category != null) {
            switch (category) {
                case Notification.CATEGORY_CAR_EMERGENCY:
                    return CarNotificationTypeItem.EMERGENCY;
                case Notification.CATEGORY_NAVIGATION:
                    return CarNotificationTypeItem.NAVIGATION;
                case Notification.CATEGORY_CALL:
                    return CarNotificationTypeItem.CALL;
                case Notification.CATEGORY_CAR_WARNING:
                    return CarNotificationTypeItem.WARNING;
                case Notification.CATEGORY_CAR_INFORMATION:
                    return CarNotificationTypeItem.INFORMATION;
                case Notification.CATEGORY_MESSAGE:
                    return CarNotificationTypeItem.MESSAGE;
                default:
                    break;
            }
        }
        Bundle extras = alertEntry.getNotification().extras;
        if (extras.containsKey(Notification.EXTRA_BIG_TEXT)
                && extras.containsKey(Notification.EXTRA_SUMMARY_TEXT)) {
            return CarNotificationTypeItem.INBOX;
        }
        // progress, media, big text, big picture, and basic templates
        return CarNotificationTypeItem.BASIC;
    }

    /**
     * Resolves a Notification's color such that it has enough contrast to be used as the
     * color for the Notification's action and header text.
     *
     * @param backgroundColor the background color to ensure the contrast against.
     * @return a color of the same hue as {@code notificationColor} with enough contrast against
     * the backgrounds.
     */
    public static int resolveContrastColor(
            @ColorInt int notificationColor, @ColorInt int backgroundColor) {
        return getContrastedForegroundColor(notificationColor, backgroundColor, MIN_CONTRAST_RATIO);
    }

    /**
     * Returns true if a color is considered a light color.
     */
    public static boolean isColorLight(int backgroundColor) {
        return Color.luminance(backgroundColor) > LIGHT_COLOR_LUMINANCE_THRESHOLD;
    }

    private static boolean isSystemPrivilegedOrPlatformKeyInner(Context context,
            AlertEntry alertEntry, boolean checkForPrivilegedApp) {
        PackageInfo packageInfo = getPackageInfo(context, alertEntry.getStatusBarNotification());
        if (packageInfo == null) return false;

        // Only include the privilegedApp check if the caller wants this check.
        boolean isPrivilegedApp =
                (!checkForPrivilegedApp) || packageInfo.applicationInfo.isPrivilegedApp();

        return (packageInfo.applicationInfo.isSignedWithPlatformKey() ||
                (packageInfo.applicationInfo.isSystemApp()
                        && isPrivilegedApp));
    }

    private static PackageInfo getPackageInfo(Context context,
            StatusBarNotification statusBarNotification) {
        PackageManager packageManager = context.getPackageManager();
        PackageInfo packageInfo = null;
        try {
            packageInfo = packageManager.getPackageInfoAsUser(
                    statusBarNotification.getPackageName(), /* flags= */ 0,
                    ActivityManager.getCurrentUser());
        } catch (PackageManager.NameNotFoundException ex) {
            Log.e(TAG, "package not found: " + statusBarNotification.getPackageName());
        }
        return packageInfo;
    }

    /**
     * Finds a suitable color such that there's enough contrast.
     *
     * @param foregroundColor  the color to start searching from.
     * @param backgroundColor  the color to ensure contrast against. Assumed to be lighter than
     *                         {@param foregroundColor}
     * @param minContrastRatio the minimum contrast ratio required.
     * @return a color with the same hue as {@param foregroundColor}, potentially darkened to
     * meet the contrast ratio.
     */
    private static int findContrastColorAgainstLightBackground(
            @ColorInt int foregroundColor, @ColorInt int backgroundColor, double minContrastRatio) {
        if (ColorUtils.calculateContrast(foregroundColor, backgroundColor) >= minContrastRatio) {
            return foregroundColor;
        }

        double[] lab = new double[3];
        ColorUtils.colorToLAB(foregroundColor, lab);

        double low = MIN_LIGHTNESS;
        double high = lab[0];
        double a = lab[1];
        double b = lab[2];
        for (int i = 0; i < MAX_FIND_COLOR_STEPS && high - low > MIN_COLOR_CONTRAST; i++) {
            double l = (low + high) / 2;
            foregroundColor = ColorUtils.LABToColor(l, a, b);
            if (ColorUtils.calculateContrast(foregroundColor, backgroundColor) > minContrastRatio) {
                low = l;
            } else {
                high = l;
            }
        }
        return ColorUtils.LABToColor(low, a, b);
    }

    /**
     * Finds a suitable color such that there's enough contrast.
     *
     * @param foregroundColor  the foregroundColor to start searching from.
     * @param backgroundColor  the foregroundColor to ensure contrast against. Assumed to be
     *                         darker than {@param foregroundColor}
     * @param minContrastRatio the minimum contrast ratio required.
     * @return a foregroundColor with the same hue as {@param foregroundColor}, potentially
     * lightened to meet the contrast ratio.
     */
    private static int findContrastColorAgainstDarkBackground(
            @ColorInt int foregroundColor, @ColorInt int backgroundColor, double minContrastRatio) {
        if (ColorUtils.calculateContrast(foregroundColor, backgroundColor) >= minContrastRatio) {
            return foregroundColor;
        }

        float[] hsl = new float[3];
        ColorUtils.colorToHSL(foregroundColor, hsl);

        float low = hsl[2];
        float high = MAX_LIGHTNESS;
        for (int i = 0; i < MAX_FIND_COLOR_STEPS && high - low > MIN_COLOR_CONTRAST; i++) {
            float l = (low + high) / 2;
            hsl[2] = l;
            foregroundColor = ColorUtils.HSLToColor(hsl);
            if (ColorUtils.calculateContrast(foregroundColor, backgroundColor)
                    > minContrastRatio) {
                high = l;
            } else {
                low = l;
            }
        }
        return foregroundColor;
    }

    /**
     * Finds a foregroundColor with sufficient contrast over backgroundColor that has the same or
     * darker hue as the original foregroundColor.
     *
     * @param foregroundColor  the foregroundColor to start searching from
     * @param backgroundColor  the foregroundColor to ensure contrast against
     * @param minContrastRatio the minimum contrast ratio required
     */
    private static int getContrastedForegroundColor(
            @ColorInt int foregroundColor, @ColorInt int backgroundColor, double minContrastRatio) {
        boolean isBackgroundDarker =
                Color.luminance(foregroundColor) > Color.luminance(backgroundColor);
        return isBackgroundDarker
                ? findContrastColorAgainstDarkBackground(
                foregroundColor, backgroundColor, minContrastRatio)
                : findContrastColorAgainstLightBackground(
                        foregroundColor, backgroundColor, minContrastRatio);
    }
}
