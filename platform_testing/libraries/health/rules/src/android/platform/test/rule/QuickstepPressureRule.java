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

package android.platform.test.rule;

import android.os.SystemClock;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.Until;
import androidx.annotation.VisibleForTesting;

import org.junit.runner.Description;

/** This rule opens apps then goes to home before a test class. */
public class QuickstepPressureRule extends TestWatcher {
    private static final long MIN_CRASH_WAIT_TIMEOUT = 2500;
    private static final long UI_RESPONSE_TIMEOUT_MSECS = 3000;

    private final String[] mPackages;

    public QuickstepPressureRule(String... packages) {
        if (packages.length == 0) {
            throw new IllegalArgumentException("Must supply an application to open.");
        }
        mPackages = packages;
    }

    @Override
    protected void starting(Description description) {
        // Start each app in sequence.
        for (String pkg : mPackages) {
            startActivity(pkg);
        }

        // Press the home button.
        getUiDevice().pressHome();
    }

    /** Launches the specified activity and keeps it open briefly. */
    @VisibleForTesting
    void startActivity(String pkg) {
        // Open the application and ensure it reaches the foreground.
        getContext().startActivity(getContext().getPackageManager().getLaunchIntentForPackage(pkg));
        if (!getUiDevice().wait(Until.hasObject(By.pkg(pkg).depth(0)), UI_RESPONSE_TIMEOUT_MSECS)) {
            throw new RuntimeException("Application not found in foreground.");
        }
        // Ensure the app doesn't immediately crash in the foreground.
        SystemClock.sleep(MIN_CRASH_WAIT_TIMEOUT);
    }
}
