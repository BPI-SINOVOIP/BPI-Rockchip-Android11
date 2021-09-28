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

package android.platform.test.rule;

import android.platform.helpers.HelperAccessor;
import android.platform.helpers.INotificationHelper;

import androidx.annotation.VisibleForTesting;

import org.junit.runner.Description;

/**
 * This rule adds notifications before a test class and removes them after.
 */
public class NotificationPressureRule extends TestWatcher {
    private static final int DEFAULT_NOTIFICATION_COUNT = 50;
    private static final int MAX_NOTIFICATION_COUNT = 50;

    private INotificationHelper mNotificationHelper = initNotificationHelper();

    private final int mNotificationCount;
    private final String mPackage;

    public NotificationPressureRule() {
        this(DEFAULT_NOTIFICATION_COUNT);
    }

    public NotificationPressureRule(int notificationCount) throws IllegalArgumentException {
        if (notificationCount > MAX_NOTIFICATION_COUNT) {
            throw new IllegalArgumentException(String.format(
                    "Notifications are limited to %d per package.", MAX_NOTIFICATION_COUNT));
        }
        mNotificationCount = notificationCount;
        mPackage = null;
    }

    public NotificationPressureRule(int notificationCount, String pkg)
            throws IllegalArgumentException {
        if (notificationCount > MAX_NOTIFICATION_COUNT) {
            throw new IllegalArgumentException(
                    String.format(
                            "Notifications are limited to %d per package.",
                            MAX_NOTIFICATION_COUNT));
        }
        mNotificationCount = notificationCount;
        mPackage = pkg;
    }


    @VisibleForTesting
    INotificationHelper initNotificationHelper() {
        HelperAccessor<INotificationHelper> helperAccessor =
                new HelperAccessor<>(INotificationHelper.class);
        return helperAccessor.get();
    }

    @Override
    protected void starting(Description description) {
        mNotificationHelper.postNotifications(mNotificationCount, mPackage);
    }

    @Override
    protected void finished(Description description) {
        mNotificationHelper.cancelNotifications();
    }
}
