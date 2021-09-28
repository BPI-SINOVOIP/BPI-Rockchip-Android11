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
package com.android.car;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertTrue;

import android.annotation.NonNull;
import android.app.Activity;
import android.app.ActivityOptions;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.os.SystemClock;
import android.util.Log;
import android.view.Display;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import com.android.car.SystemActivityMonitoringService.TopTaskInfoContainer;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class SystemActivityMonitoringServiceTest {
    private static final String TAG = "SystemActivityMonitoringServiceTest";

    private static final long ACTIVITY_TIMEOUT_MS = 5000;
    private static final long DEFAULT_TIMEOUT_MS = 10_000;
    private static final int SLEEP_MS = 50;

    private SystemActivityMonitoringService mService;

    @Before
    public void setUp() throws Exception {
        mService = new SystemActivityMonitoringService(getContext());
        mService.init();
    }

    @After
    public void tearDown() {
        mService.registerActivityLaunchListener(null);
        mService.release();
        mService = null;
    }

    @Test
    public void testActivityLaunch() throws Exception {
        ComponentName activityA = toComponentName(getTestContext(), ActivityA.class);
        FilteredLaunchListener listenerA = new FilteredLaunchListener(activityA);
        mService.registerActivityLaunchListener(listenerA);
        startActivity(activityA);
        listenerA.assertTopTaskActivityLaunched();

        ComponentName activityB = toComponentName(getTestContext(), ActivityB.class);
        FilteredLaunchListener listenerB = new FilteredLaunchListener(activityB);
        mService.registerActivityLaunchListener(listenerB);
        startActivity(activityB);
        listenerB.assertTopTaskActivityLaunched();
    }

    @Test
    public void testActivityBlocking() throws Exception {
        ComponentName blackListedActivity = toComponentName(getTestContext(), ActivityC.class);
        ComponentName blockingActivity = toComponentName(getTestContext(), BlockingActivity.class);
        Intent blockingIntent = new Intent();
        blockingIntent.setComponent(blockingActivity);

        // start a black listed activity
        FilteredLaunchListener listenerBlackListed =
                new FilteredLaunchListener(blackListedActivity);
        mService.registerActivityLaunchListener(listenerBlackListed);
        startActivity(blackListedActivity);
        listenerBlackListed.assertTopTaskActivityLaunched();

        // Instead of start activity, invoke blockActivity.
        FilteredLaunchListener listenerBlocking = new FilteredLaunchListener(blockingActivity);
        mService.registerActivityLaunchListener(listenerBlocking);
        mService.blockActivity(listenerBlackListed.mTopTask, blockingIntent);
        listenerBlocking.assertTopTaskActivityLaunched();
    }

    @Test
    public void testRemovesFromTopTasks() throws Exception {
        ComponentName activityA = toComponentName(getTestContext(), ActivityA.class);
        FilteredLaunchListener listenerA = new FilteredLaunchListener(activityA);
        mService.registerActivityLaunchListener(listenerA);
        Activity launchedActivity = startActivity(activityA);
        listenerA.assertTopTaskActivityLaunched();
        assertTrue(topTasksHasComponent(activityA));

        getInstrumentation().runOnMainSync(launchedActivity::finish);
        waitUntil(() -> !topTasksHasComponent(activityA));
    }

    @Test
    public void testGetTopTasksOnMultiDisplay() throws Exception {
        String virtualDisplayName = "virtual_display";
        DisplayManager displayManager = getContext().getSystemService(DisplayManager.class);
        VirtualDisplay virtualDisplay = displayManager.createVirtualDisplay(
                virtualDisplayName, 10, 10, 10, null, 0);

        ComponentName activityA = toComponentName(getTestContext(), ActivityA.class);
        FilteredLaunchListener listenerA = new FilteredLaunchListener(activityA);
        mService.registerActivityLaunchListener(listenerA);
        startActivity(activityA, Display.DEFAULT_DISPLAY);
        listenerA.assertTopTaskActivityLaunched();
        assertTrue(topTasksHasComponent(activityA));

        ComponentName activityB = toComponentName(getTestContext(), ActivityB.class);
        FilteredLaunchListener listenerB = new FilteredLaunchListener(activityB);
        mService.registerActivityLaunchListener(listenerB);
        startActivity(activityB, virtualDisplay.getDisplay().getDisplayId());
        listenerB.assertTopTaskActivityLaunched();
        assertTrue(topTasksHasComponent(activityB));

        virtualDisplay.release();
    }

    private void waitUntil(BooleanSupplier condition) {
        for (long i = DEFAULT_TIMEOUT_MS / SLEEP_MS; !condition.getAsBoolean() && i > 0; --i) {
            SystemClock.sleep(SLEEP_MS);
        }
        if (!condition.getAsBoolean()) {
            throw new RuntimeException("failed while waiting for condition to become true");
        }
    }

    private boolean topTasksHasComponent(ComponentName component) {
        for (TopTaskInfoContainer topTaskInfoContainer : mService.getTopTasks()) {
            if (topTaskInfoContainer.topActivity.equals(component)) {
                return true;
            }
        }
        return false;
    }

    /** Activity that closes itself after some timeout to clean up the screen. */
    public static class TempActivity extends Activity {
        @Override
        protected void onResume() {
            super.onResume();
            getMainThreadHandler().postDelayed(this::finish, ACTIVITY_TIMEOUT_MS);
        }
    }

    public static class ActivityA extends TempActivity {}

    public static class ActivityB extends TempActivity {}

    public static class ActivityC extends TempActivity {}

    public static class BlockingActivity extends TempActivity {}

    private Context getContext() {
        return getInstrumentation().getTargetContext();
    }

    private Context getTestContext() {
        return getInstrumentation().getContext();
    }

    private static ComponentName toComponentName(Context ctx, Class<?> cls) {
        return ComponentName.createRelative(ctx, cls.getName());
    }

    private Activity startActivity(ComponentName name) {
        return startActivity(name, Display.DEFAULT_DISPLAY);
    }

    private Activity startActivity(ComponentName name, int displayId) {
        ActivityMonitor monitor = new ActivityMonitor(name.getClassName(), null, false);
        getInstrumentation().addMonitor(monitor);

        Intent intent = new Intent();
        intent.setComponent(name);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(displayId);

        getContext().startActivity(intent, options.toBundle());
        return monitor.waitForActivityWithTimeout(ACTIVITY_TIMEOUT_MS);
    }

    private class FilteredLaunchListener
            implements SystemActivityMonitoringService.ActivityLaunchListener {

        private final ComponentName mDesiredComponent;
        private final CountDownLatch mActivityLaunched = new CountDownLatch(1);
        private TopTaskInfoContainer mTopTask;

        /**
         * Creates an instance of an
         * {@link com.android.car.SystemActivityMonitoringService.ActivityLaunchListener}
         * that filters based on the component name or does not filter if component name is null.
         */
        FilteredLaunchListener(@NonNull ComponentName desiredComponent) {
            mDesiredComponent = desiredComponent;
        }

        @Override
        public void onActivityLaunch(TopTaskInfoContainer topTask) {
            // Ignore activities outside of this test case
            if (!getTestContext().getPackageName().equals(topTask.topActivity.getPackageName())) {
                Log.d(TAG, "Component launched from other package: "
                        + topTask.topActivity.getClassName());
                return;
            }
            if (!topTask.topActivity.equals(mDesiredComponent)) {
                Log.d(TAG, String.format("Unexpected component: %s. Expected: %s",
                        topTask.topActivity.getClassName(), mDesiredComponent));
                return;
            }
            if (mTopTask == null) {  // We are interested in the first one only.
                mTopTask = topTask;
            }
            mActivityLaunched.countDown();
        }

        void assertTopTaskActivityLaunched() throws InterruptedException {
            assertTrue(mActivityLaunched.await(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        }
    }
}
