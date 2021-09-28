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
package com.android.cts.profileowner;

import android.app.PendingIntent;
import android.app.usage.UsageStatsManager;
import android.content.Intent;

import androidx.test.InstrumentationRegistry;

import java.time.Duration;
import java.util.concurrent.TimeUnit;

public class AppUsageObserverTest extends BaseProfileOwnerTest {

    static final int OBSERVER_LIMIT = 1000;

    public void testAppUsageObserver_MinTimeLimit() throws Exception {
        final String[] packages = {"not.real.package.name"};
        final int obsId = 0;
        UsageStatsManager usm = mContext.getSystemService(UsageStatsManager.class);

        Intent intent = new Intent(Intent.ACTION_MAIN);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                InstrumentationRegistry.getContext(),
                1, intent, 0);

        usm.registerAppUsageObserver(obsId, packages, 60, TimeUnit.SECONDS, pendingIntent);
        usm.unregisterAppUsageObserver(obsId);
        try {
            usm.registerAppUsageObserver(obsId, packages, 59, TimeUnit.SECONDS, pendingIntent);
            fail("Should have thrown an IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
            // Do nothing. Exception is expected.
        }
    }

    public void testUsageSessionObserver_MinTimeLimit() throws Exception {
        final String[] packages = {"not.real.package.name"};
        final int obsId = 0;
        UsageStatsManager usm = mContext.getSystemService(UsageStatsManager.class);

        Intent intent = new Intent(Intent.ACTION_MAIN);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                InstrumentationRegistry.getContext(),
                1, intent, 0);

        usm.registerUsageSessionObserver(obsId, packages, Duration.ofSeconds(60),
                Duration.ofSeconds(10), pendingIntent, null);
        usm.unregisterUsageSessionObserver(obsId);
        try {
            usm.registerUsageSessionObserver(obsId, packages, Duration.ofSeconds(59),
                    Duration.ofSeconds(10), pendingIntent, null);
            fail("Should have thrown an IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
            // Do nothing. Exception is expected.
        }
    }

    public void testObserverLimit() throws Exception {
        final String[] packages = {"not.real.package.name"};
        UsageStatsManager usm = mContext.getSystemService(UsageStatsManager.class);

        Intent intent = new Intent(Intent.ACTION_MAIN);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                InstrumentationRegistry.getContext(),
                1, intent, 0);

        // Register too many AppUsageObservers
        for (int obsId = 0; obsId < OBSERVER_LIMIT; obsId++) {
            usm.registerAppUsageObserver(obsId, packages, 60, TimeUnit.MINUTES, pendingIntent);
        }
        try {
            usm.registerAppUsageObserver(OBSERVER_LIMIT, packages, 60, TimeUnit.MINUTES,
                    pendingIntent);
            fail("Should have thrown an IllegalStateException");
        } catch (IllegalStateException expected) {
            // Do nothing. Exception is expected.
        }

        // Register too many UsageSessionObservers.
        for (int obsId = 0; obsId < OBSERVER_LIMIT; obsId++) {
            usm.registerUsageSessionObserver(obsId, packages, Duration.ofSeconds(60),
                    Duration.ofSeconds(10), pendingIntent, null);
        }
        try {
            usm.registerUsageSessionObserver(OBSERVER_LIMIT, packages, Duration.ofSeconds(60),
                    Duration.ofSeconds(10), pendingIntent, null);
            fail("Should have thrown an IllegalStateException");
        } catch (IllegalStateException expected) {
            // Do nothing. Exception is expected.
        }

        for (int obsId = 0; obsId < OBSERVER_LIMIT; obsId++) {
            usm.unregisterAppUsageObserver(obsId);
        }

        for (int obsId = 0; obsId < OBSERVER_LIMIT; obsId++) {
            usm.unregisterUsageSessionObserver(obsId);
        }
    }
}
