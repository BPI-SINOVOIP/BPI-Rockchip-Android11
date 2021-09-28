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
package android.autofillservice.cts;

import android.util.ArraySet;
import android.util.Log;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.TestNameUtils;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import java.util.Set;

/**
 * Custom {@link TestWatcher} that's the outer rule of all {@link AutoFillServiceTestCase} tests.
 *
 * <p>This class is not thread safe, but should be fine...
 */
public final class AutofillTestWatcher extends TestWatcher {

    /**
     * Cleans up all launched activities between the tests and retries.
     */
    public void cleanAllActivities() {
        try {
            finishActivities();
            waitUntilAllDestroyed();
        } finally {
            resetStaticState();
        }
    }

    private static final String TAG = "AutofillTestWatcher";

    @GuardedBy("sUnfinishedBusiness")
    private static final Set<AbstractAutoFillActivity> sUnfinishedBusiness = new ArraySet<>();

    @GuardedBy("sAllActivities")
    private static final Set<AbstractAutoFillActivity> sAllActivities = new ArraySet<>();

    @Override
    protected void starting(Description description) {
        resetStaticState();
        final String testName = description.getDisplayName();
        Log.i(TAG, "Starting " + testName);
        TestNameUtils.setCurrentTestName(testName);
    }

    @Override
    protected void finished(Description description) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        final String testName = description.getDisplayName();
        cleanAllActivities();
        Log.i(TAG, "Finished " + testName);
        TestNameUtils.setCurrentTestName(null);
    }

    private void resetStaticState() {
        synchronized (sUnfinishedBusiness) {
            sUnfinishedBusiness.clear();
        }
        synchronized (sAllActivities) {
            sAllActivities.clear();
        }
    }

    /**
     * Registers an activity so it's automatically finished (if necessary) after the test.
     */
    public static void registerActivity(@NonNull String where,
            @NonNull AbstractAutoFillActivity activity) {
        synchronized (sUnfinishedBusiness) {
            if (sUnfinishedBusiness.contains(activity)) {
                throw new IllegalStateException("Already registered " + activity);
            }
            Log.v(TAG, "registering activity on " + where + ": " + activity);
            sUnfinishedBusiness.add(activity);
            sAllActivities.add(activity);
        }
        synchronized (sAllActivities) {
            sAllActivities.add(activity);

        }
    }

    /**
     * Unregisters an activity so it's not automatically finished after the test.
     */
    public static void unregisterActivity(@NonNull String where,
            @NonNull AbstractAutoFillActivity activity) {
        synchronized (sUnfinishedBusiness) {
            final boolean unregistered = sUnfinishedBusiness.remove(activity);
            if (unregistered) {
                Log.d(TAG, "unregistered activity on " + where + ": " + activity);
            } else {
                Log.v(TAG, "ignoring already unregistered activity on " + where + ": " + activity);
            }
        }
    }

    /**
     * Gets the instance of a previously registered activity.
     */
    @Nullable
    public static <A extends AbstractAutoFillActivity> A getActivity(@NonNull Class<A> clazz) {
        @SuppressWarnings("unchecked")
        final A activity = (A) sAllActivities.stream().filter(a -> a.getClass().equals(clazz))
                .findFirst()
                .get();
        return activity;
    }

    private void finishActivities() {
        synchronized (sUnfinishedBusiness) {
            if (sUnfinishedBusiness.isEmpty()) {
                return;
            }
            Log.d(TAG, "Manually finishing " + sUnfinishedBusiness.size() + " activities");
            for (AbstractAutoFillActivity activity : sUnfinishedBusiness) {
                if (activity.isFinishing()) {
                    Log.v(TAG, "Ignoring activity that isFinishing(): " + activity);
                } else {
                    Log.d(TAG, "Finishing activity: " + activity);
                    activity.finishOnly();
                }
            }
        }
    }

    private void waitUntilAllDestroyed() {
        synchronized (sAllActivities) {
            if (sAllActivities.isEmpty()) return;

            Log.d(TAG, "Waiting until " + sAllActivities.size() + " activities are destroyed");
            for (AbstractAutoFillActivity activity : sAllActivities) {
                Log.d(TAG, "Waiting for " + activity);
                try {
                    activity.waintUntilDestroyed(Timeouts.ACTIVITY_RESURRECTION);
                } catch (InterruptedException e) {
                    Log.e(TAG, "interrupted waiting for " + activity + " to be destroyed");
                    Thread.currentThread().interrupt();
                }
            }
        }
    }
}
