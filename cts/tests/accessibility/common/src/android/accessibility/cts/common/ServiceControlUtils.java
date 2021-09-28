/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.accessibility.cts.common;

import static com.android.compatibility.common.util.TestUtils.waitOn;

import android.content.ContentResolver;
import android.content.Context;
import android.provider.Settings;
import android.view.accessibility.AccessibilityManager;

import java.util.function.BooleanSupplier;

/**
 * Utility methods for enabling and disabling the services used in this package
 */
public class ServiceControlUtils {

    public static String getEnabledServices(ContentResolver cr) {
        return Settings.Secure.getString(cr, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
    }

    /**
     * Wait for a specified condition that will change with a services state change
     *
     * @param context A valid context
     * @param condition The condition to check
     * @param timeoutMs The timeout in millis
     * @param conditionName The name to include in the assertion. If null, will be given a default.
     */
    public static void waitForConditionWithServiceStateChange(Context context,
            BooleanSupplier condition, long timeoutMs, String conditionName) {
        AccessibilityManager manager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        Object lock = new Object();
        AccessibilityManager.AccessibilityServicesStateChangeListener listener = (m) -> {
            synchronized (lock) {
                lock.notifyAll();
            }
        };
        manager.addAccessibilityServicesStateChangeListener(listener, null);
        try {
            waitOn(lock, condition, timeoutMs, conditionName);
        } finally {
            manager.removeAccessibilityServicesStateChangeListener(listener);
        }
    }
}
