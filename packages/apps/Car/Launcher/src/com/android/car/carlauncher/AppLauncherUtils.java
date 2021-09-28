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

package com.android.car.carlauncher;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.annotation.Nullable;
import android.app.Activity;
import android.app.ActivityOptions;
import android.car.Car;
import android.car.CarNotConnectedException;
import android.car.content.pm.CarPackageManager;
import android.car.media.CarMediaManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.LauncherActivityInfo;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Process;
import android.service.media.MediaBrowserService;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import java.lang.annotation.Retention;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Util class that contains helper method used by app launcher classes.
 */
class AppLauncherUtils {
    private static final String TAG = "AppLauncherUtils";

    @Retention(SOURCE)
    @IntDef({APP_TYPE_LAUNCHABLES, APP_TYPE_MEDIA_SERVICES})
    @interface AppTypes {}
    static final int APP_TYPE_LAUNCHABLES = 1;
    static final int APP_TYPE_MEDIA_SERVICES = 2;

    private AppLauncherUtils() {
    }

    /**
     * Comparator for {@link AppMetaData} that sorts the list
     * by the "displayName" property in ascending order.
     */
    static final Comparator<AppMetaData> ALPHABETICAL_COMPARATOR = Comparator
            .comparing(AppMetaData::getDisplayName, String::compareToIgnoreCase);

    /**
     * Helper method that launches the app given the app's AppMetaData.
     *
     * @param app the requesting app's AppMetaData
     */
    static void launchApp(Context context, Intent intent) {
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(context.getDisplayId());
        context.startActivity(intent, options.toBundle());
    }

    /** Bundles application and services info. */
    static class LauncherAppsInfo {
        /*
         * Map of all car launcher components' (including launcher activities and media services)
         * metadata keyed by ComponentName.
         */
        private final Map<ComponentName, AppMetaData> mLaunchables;

        /** Map of all the media services keyed by ComponentName. */
        private final Map<ComponentName, ResolveInfo> mMediaServices;

        LauncherAppsInfo(@NonNull Map<ComponentName, AppMetaData> launchablesMap,
                @NonNull Map<ComponentName, ResolveInfo> mediaServices) {
            mLaunchables = launchablesMap;
            mMediaServices = mediaServices;
        }

        /** Returns true if all maps are empty. */
        boolean isEmpty() {
            return mLaunchables.isEmpty() && mMediaServices.isEmpty();
        }

        /**
         * Returns whether the given componentName is a media service.
         */
        boolean isMediaService(ComponentName componentName) {
            return mMediaServices.containsKey(componentName);
        }

        /** Returns the {@link AppMetaData} for the given componentName. */
        @Nullable
        AppMetaData getAppMetaData(ComponentName componentName) {
            return mLaunchables.get(componentName);
        }

        /** Returns a new list of all launchable components' {@link AppMetaData}. */
        @NonNull
        List<AppMetaData> getLaunchableComponentsList() {
            return new ArrayList<>(mLaunchables.values());
        }
    }

    private final static LauncherAppsInfo EMPTY_APPS_INFO = new LauncherAppsInfo(
            Collections.emptyMap(), Collections.emptyMap());

    /*
     * Gets the media source in a given package. If there are multiple sources in the package,
     * returns the first one.
     */
    static ComponentName getMediaSource(@NonNull PackageManager packageManager,
            @NonNull String packageName) {
        Intent mediaIntent = new Intent();
        mediaIntent.setPackage(packageName);
        mediaIntent.setAction(MediaBrowserService.SERVICE_INTERFACE);

        List<ResolveInfo> mediaServices = packageManager.queryIntentServices(mediaIntent,
                PackageManager.GET_RESOLVED_FILTER);

        if (mediaServices == null || mediaServices.isEmpty()) {
            return null;
        }
        String defaultService = mediaServices.get(0).serviceInfo.name;
        if (!TextUtils.isEmpty(defaultService)) {
            return new ComponentName(packageName, defaultService);
        }
        return null;
    }

    /**
     * Gets all the components that we want to see in the launcher in unsorted order, including
     * launcher activities and media services.
     *
     * @param blackList             A (possibly empty) list of apps (package names) to hide
     * @param customMediaComponents A (possibly empty) list of media components (component names)
     *                              that shouldn't be shown in Launcher because their applications'
     *                              launcher activities will be shown
     * @param appTypes              Types of apps to show (e.g.: all, or media sources only)
     * @param openMediaCenter       Whether launcher should navigate to media center when the
     *                              user selects a media source.
     * @param launcherApps          The {@link LauncherApps} system service
     * @param carPackageManager     The {@link CarPackageManager} system service
     * @param packageManager        The {@link PackageManager} system service
     * @return a new {@link LauncherAppsInfo}
     */
    @NonNull
    static LauncherAppsInfo getLauncherApps(
            @NonNull Set<String> blackList,
            @NonNull Set<String> customMediaComponents,
            @AppTypes int appTypes,
            boolean openMediaCenter,
            LauncherApps launcherApps,
            CarPackageManager carPackageManager,
            PackageManager packageManager,
            CarMediaManager carMediaManager) {

        if (launcherApps == null || carPackageManager == null || packageManager == null
                || carMediaManager == null) {
            return EMPTY_APPS_INFO;
        }

        List<ResolveInfo> mediaServices = packageManager.queryIntentServices(
                new Intent(MediaBrowserService.SERVICE_INTERFACE),
                PackageManager.GET_RESOLVED_FILTER);
        List<LauncherActivityInfo> availableActivities =
                launcherApps.getActivityList(null, Process.myUserHandle());

        Map<ComponentName, AppMetaData> launchablesMap = new HashMap<>(
                mediaServices.size() + availableActivities.size());
        Map<ComponentName, ResolveInfo> mediaServicesMap = new HashMap<>(mediaServices.size());

        // Process media services
        if ((appTypes & APP_TYPE_MEDIA_SERVICES) != 0) {
            for (ResolveInfo info : mediaServices) {
                String packageName = info.serviceInfo.packageName;
                String className = info.serviceInfo.name;
                ComponentName componentName = new ComponentName(packageName, className);
                mediaServicesMap.put(componentName, info);
                if (shouldAddToLaunchables(componentName, blackList, customMediaComponents,
                        appTypes, APP_TYPE_MEDIA_SERVICES)) {
                    final boolean isDistractionOptimized = true;

                    Intent intent = new Intent(Car.CAR_INTENT_ACTION_MEDIA_TEMPLATE);
                    intent.putExtra(Car.CAR_EXTRA_MEDIA_COMPONENT, componentName.flattenToString());

                    AppMetaData appMetaData = new AppMetaData(
                        info.serviceInfo.loadLabel(packageManager),
                        componentName,
                        info.serviceInfo.loadIcon(packageManager),
                        isDistractionOptimized,
                        context -> {
                            if (openMediaCenter) {
                                AppLauncherUtils.launchApp(context, intent);
                            } else {
                                selectMediaSourceAndFinish(context, componentName, carMediaManager);
                            }
                        },
                        context -> {
                            // getLaunchIntentForPackage looks for a main activity in the category
                            // Intent.CATEGORY_INFO, then Intent.CATEGORY_LAUNCHER, and returns null
                            // if neither are found
                            Intent packageLaunchIntent =
                                    packageManager.getLaunchIntentForPackage(packageName);
                            AppLauncherUtils.launchApp(context,
                                    packageLaunchIntent != null ? packageLaunchIntent : intent);
                        });
                    launchablesMap.put(componentName, appMetaData);
                }
            }
        }

        // Process activities
        if ((appTypes & APP_TYPE_LAUNCHABLES) != 0) {
            for (LauncherActivityInfo info : availableActivities) {
                ComponentName componentName = info.getComponentName();
                String packageName = componentName.getPackageName();
                if (shouldAddToLaunchables(componentName, blackList, customMediaComponents,
                        appTypes, APP_TYPE_LAUNCHABLES)) {
                    boolean isDistractionOptimized =
                        isActivityDistractionOptimized(carPackageManager, packageName,
                            info.getName());

                    Intent intent = new Intent(Intent.ACTION_MAIN)
                        .setComponent(componentName)
                        .addCategory(Intent.CATEGORY_LAUNCHER)
                        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                    AppMetaData appMetaData = new AppMetaData(
                        info.getLabel(),
                        componentName,
                        info.getBadgedIcon(0),
                        isDistractionOptimized,
                        context -> AppLauncherUtils.launchApp(context, intent),
                        null);
                    launchablesMap.put(componentName, appMetaData);
                }
            }
        }

        return new LauncherAppsInfo(launchablesMap, mediaServicesMap);
    }

    private static boolean shouldAddToLaunchables(@NonNull ComponentName componentName,
            @NonNull Set<String> blackList,
            @NonNull Set<String> customMediaComponents,
            @AppTypes int appTypesToShow,
            @AppTypes int componentAppType) {
        if (blackList.contains(componentName.getPackageName())) {
            return false;
        }
        switch (componentAppType) {
            // Process media services
            case APP_TYPE_MEDIA_SERVICES:
                // For a media service in customMediaComponents, if its application's launcher
                // activity will be shown in the Launcher, don't show the service's icon in the
                // Launcher.
                if (customMediaComponents.contains(componentName.flattenToString())
                        && (appTypesToShow & APP_TYPE_LAUNCHABLES) != 0) {
                    return false;
                }
                return true;
            // Process activities
            case APP_TYPE_LAUNCHABLES:
                return true;
            default:
                Log.e(TAG, "Invalid componentAppType : " + componentAppType);
                return false;
        }
    }

    private static void selectMediaSourceAndFinish(Context context, ComponentName componentName,
            CarMediaManager carMediaManager) {
        try {
            carMediaManager.setMediaSource(componentName, CarMediaManager.MEDIA_SOURCE_MODE_BROWSE);
            if (context instanceof Activity) {
                ((Activity) context).finish();
            }
        } catch (CarNotConnectedException e) {
            Log.e(TAG, "Car not connected", e);
        }
    }

    /**
     * Gets if an activity is distraction optimized.
     *
     * @param carPackageManager The {@link CarPackageManager} system service
     * @param packageName       The package name of the app
     * @param activityName      The requested activity name
     * @return true if the supplied activity is distraction optimized
     */
    static boolean isActivityDistractionOptimized(
            CarPackageManager carPackageManager, String packageName, String activityName) {
        boolean isDistractionOptimized = false;
        // try getting distraction optimization info
        try {
            if (carPackageManager != null) {
                isDistractionOptimized =
                        carPackageManager.isActivityDistractionOptimized(packageName, activityName);
            }
        } catch (CarNotConnectedException e) {
            Log.e(TAG, "Car not connected when getting DO info", e);
        }
        return isDistractionOptimized;
    }
}
