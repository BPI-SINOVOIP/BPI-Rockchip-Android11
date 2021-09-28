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
package com.android.apppredictionservice;

import static android.os.Process.myUserHandle;
import static android.text.TextUtils.isEmpty;

import static java.util.Collections.emptyList;

import android.app.prediction.AppPredictionContext;
import android.app.prediction.AppPredictionSessionId;
import android.app.prediction.AppTarget;
import android.app.prediction.AppTargetEvent;
import android.app.prediction.AppTargetId;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.LauncherActivityInfo;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.CancellationSignal;
import android.service.appprediction.AppPredictionService;
import android.util.Log;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Consumer;

/*
 * New plugin that replaces prediction driven Aiai APK in P
 * PredictionService simply populates the top row of the app
 * drawer with the 5 most recently used apps. Each time a new
 * app is launched, it is added to the left of the top row.
 * Duplicates are not added.
 */
public class PredictionService extends AppPredictionService {

    private static final String TAG = PredictionService.class.getSimpleName();

    private final Set<AppPredictionSessionId> activeLauncherSessions = new HashSet<>();

    private boolean mAppSuggestionsEnabled = true;

    public static final String MY_PREF = "mypref";

    private final List<AppTarget> predictionList = new ArrayList<>(5);

    private final List<String> appNames = new ArrayList<>(5);
    private final String[] appNameKeys = new String[] {
            "first", "second", "third", "fourth", "fifth" };

    SharedPreferences sharedPreferences;
    SharedPreferences.Editor editor;

    @Override
    public void onCreate() {
        super.onCreate();

        Intent calendarIntent = new Intent(Intent.ACTION_MAIN);
        calendarIntent.addCategory(Intent.CATEGORY_APP_CALENDAR);

        Intent galleryIntent = new Intent(Intent.ACTION_MAIN);
        galleryIntent.addCategory(Intent.CATEGORY_APP_GALLERY);

        Intent mapsIntent = new Intent(Intent.ACTION_MAIN);
        mapsIntent.addCategory(Intent.CATEGORY_APP_MAPS);

        Intent emailIntent = new Intent(Intent.ACTION_MAIN);
        emailIntent.addCategory(Intent.CATEGORY_APP_EMAIL);

        Intent browserIntent = new Intent(Intent.ACTION_MAIN);
        browserIntent.addCategory(Intent.CATEGORY_APP_BROWSER);

        String[] DEFAULT_PACKAGES = new String[] {
              getDefaultSystemHandlerActivityPackageName(calendarIntent),
              getDefaultSystemHandlerActivityPackageName(galleryIntent),
              getDefaultSystemHandlerActivityPackageName(mapsIntent),
              getDefaultSystemHandlerActivityPackageName(emailIntent),
              getDefaultSystemHandlerActivityPackageName(browserIntent),
        };

        Log.d(TAG, "AppPredictionService onCreate");
        this.sharedPreferences = getSharedPreferences(MY_PREF, Context.MODE_PRIVATE);
        this.editor = sharedPreferences.edit();

        if (sharedPreferences.getString(appNameKeys[0], "").isEmpty()) {
            // fill the list with defaults if first one is null when devices powers up for the first time
            for (int i = 0; i < appNameKeys.length; i++) {
                editor.putString(appNameKeys[i],
                        getLauncherComponent(DEFAULT_PACKAGES[i]).flattenToShortString());
            }
            this.editor.apply();
        }

        for (int i = 0; i < appNameKeys.length; i++) {
            String appName = sharedPreferences.getString(appNameKeys[i], "");
            ComponentName cn = ComponentName.unflattenFromString(appName);
            AppTarget target = new AppTarget.Builder(
                    new AppTargetId(Integer.toString(i + 1)), cn.getPackageName(), myUserHandle())
                    .setClassName(cn.getClassName())
                    .build();
            appNames.add(appName);
            predictionList.add(target);
        }
        postPredictionUpdateToAllClients();
    }

    private ComponentName getLauncherComponent(String packageName) {
        List<LauncherActivityInfo> infos = getSystemService(LauncherApps.class)
                .getActivityList(packageName, myUserHandle());
        if (infos.isEmpty()) {
            return new ComponentName(packageName, "#");
        } else {
            return infos.get(0).getComponentName();
        }
    }

    private void postPredictionUpdate(AppPredictionSessionId sessionId) {
        updatePredictions(sessionId, mAppSuggestionsEnabled ? predictionList : emptyList());
    }

    private void postPredictionUpdateToAllClients() {
        for (AppPredictionSessionId session : activeLauncherSessions) {
            postPredictionUpdate(session);
        }
    }

    @Override
    public void onCreatePredictionSession(
            AppPredictionContext context, AppPredictionSessionId sessionId) {
        Log.d(TAG, "onCreatePredictionSession");

        if (context.getUiSurface().equals("home") || context.getUiSurface().equals("overview")) {
            activeLauncherSessions.add(sessionId);
            postPredictionUpdate(sessionId);
        }
    }

    @Override
    public void onAppTargetEvent(AppPredictionSessionId sessionId, AppTargetEvent event) {

        if (!activeLauncherSessions.contains(sessionId)) {
            return;
        }

        boolean found = false;
        Log.d(TAG, "onAppTargetEvent");

        AppTarget target = event.getTarget();
        if (target == null || isEmpty(target.getPackageName()) || isEmpty(target.getClassName())) {
            return;
        }
        String mostRecentComponent = new ComponentName(
                target.getPackageName(), target.getClassName()).flattenToString();

        // Check if packageName already exists in existing list of appNames
        for (String packageName:appNames) {
            if (packageName.contains(target.getPackageName())) {
                found = true;
                break;
            }
        }

        if (!found) {
            appNames.remove(appNames.size() - 1);
            appNames.add(0, mostRecentComponent);

            for (int i = 0; i < appNameKeys.length; i++) {
                editor.putString(appNameKeys[i], appNames.get(i));
            }
            editor.apply();

            predictionList.remove(predictionList.size() - 1);
            predictionList.add(0, event.getTarget());

            Log.d(TAG, "onAppTargetEvent:: update predictions");
            postPredictionUpdateToAllClients();
        }
    }

    @Override
    public void onLaunchLocationShown(
            AppPredictionSessionId sessionId, String launchLocation, List<AppTargetId> targetIds) {
        Log.d(TAG, "onLaunchLocationShown");
    }

    @Override
    public void onSortAppTargets(
            AppPredictionSessionId sessionId,
            List<AppTarget> targets,
            CancellationSignal cancellationSignal,
            Consumer<List<AppTarget>> callback) {

        Log.d(TAG, "onSortAppTargets");
        if (!activeLauncherSessions.contains(sessionId)) {
            callback.accept(emptyList());
        } else {
            // No-op
            callback.accept(targets);
        }
    }

    @Override
    public void onRequestPredictionUpdate(AppPredictionSessionId sessionId) {
        Log.d(TAG, "onRequestPredictionUpdate");

        if (!activeLauncherSessions.contains(sessionId)) {
            updatePredictions(sessionId, emptyList());
        } else {
            postPredictionUpdate(sessionId);
            Log.d(TAG, "update predictions");
        }
    }

    @Override
    public void onDestroyPredictionSession(AppPredictionSessionId sessionId) {
        Log.d(TAG, "onDestroyPredictionSession");
        activeLauncherSessions.remove(sessionId);
    }

    @Override
    public void onStartPredictionUpdates() {
        Log.d(TAG, "onStartPredictionUpdates");
    }

    @Override
    public void onStopPredictionUpdates() {
        Log.d(TAG, "onStopPredictionUpdates");
    }

    public void setAppSuggestionsEnabled(boolean enabled) {
        mAppSuggestionsEnabled = enabled;
        postPredictionUpdateToAllClients();
    }

    private String getDefaultSystemHandlerActivityPackageName(Intent intent) {
        return getDefaultSystemHandlerActivityPackageName(intent, 0);
    }

    private String getDefaultSystemHandlerActivityPackageName(Intent intent, int flags) {
        ResolveInfo handler = getPackageManager().resolveActivity(intent, flags | PackageManager.MATCH_SYSTEM_ONLY);
        if (handler == null) {
            return null;
        }
        if ((handler.activityInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
            return handler.activityInfo.packageName;
        }
        return null;
    }
}
