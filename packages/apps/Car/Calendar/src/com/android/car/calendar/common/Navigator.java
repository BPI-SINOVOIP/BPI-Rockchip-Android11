/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.calendar.common;

import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;
import android.view.Display;
import android.widget.Toast;

/** Launches a navigation activity. */
public class Navigator {
    private static final String TAG = "CarCalendarNavigator";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private final Context mContext;

    public Navigator(Context context) {
        this.mContext = context;
    }

    /** Launches a navigation activity to the given address or place name. */
    public void navigate(String locationText) {
        Uri navigateUri = Uri.parse("google.navigation:q=" + locationText);
        Intent intent = new Intent(Intent.ACTION_VIEW, navigateUri);
        if (intent.resolveActivity(mContext.getPackageManager()) != null) {
            if (DEBUG) Log.d(TAG, "Starting navigation");

            // Workaround to bring GMM to the front. CarLauncher contains an ActivityView that
            // opens GMM in a virtual display which causes it not to move to the front.
            // This workaround is not required for other launchers.
            // TODO(b/153046584): Remove workaround for GMM not moving to front
            ActivityOptions activityOptions =
                    ActivityOptions.makeBasic().setLaunchDisplayId(Display.DEFAULT_DISPLAY);
            mContext.startActivity(intent, activityOptions.toBundle());
        } else {
            Toast.makeText(mContext, "Navigation app not found", Toast.LENGTH_LONG).show();
        }
    }
}
