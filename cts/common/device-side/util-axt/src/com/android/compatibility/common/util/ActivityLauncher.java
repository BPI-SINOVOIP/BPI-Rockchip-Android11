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
package com.android.compatibility.common.util;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher.ActivityWatcher;

/**
 * Helper used to launch an activity and watch for its lifecycle events.
 *
 * @param <A> activity type
 */
public final class ActivityLauncher<A extends Activity> {

    private final ActivityWatcher mWatcher;
    private final ActivityTestRule<A> mActivityTestRule;
    private final Intent mLaunchIntent;

    public ActivityLauncher(@NonNull Context context, @NonNull ActivitiesWatcher watcher,
            @NonNull Class<A> activityClass) {
        mWatcher = watcher.watch(activityClass);
        mActivityTestRule = new ActivityTestRule<>(activityClass);
        mLaunchIntent = new Intent(context, activityClass);
    }

    /**
     * Gets a watcher for the activity lifecycle events.
     */
    @NonNull
    public ActivityWatcher getWatcher() {
        return mWatcher;
    }

    /**
     * Launches the activity.
     */
    @NonNull
    public A launchActivity() {
        return mActivityTestRule.launchActivity(mLaunchIntent);
    }
}
