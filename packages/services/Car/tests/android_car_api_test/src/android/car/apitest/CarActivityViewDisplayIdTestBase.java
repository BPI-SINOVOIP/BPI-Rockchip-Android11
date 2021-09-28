/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.car.apitest;

import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Display.INVALID_DISPLAY;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.ActivityView;
import android.app.Instrumentation;
import android.car.Car;
import android.car.app.CarActivityView;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.Display;
import android.view.ViewGroup;

import org.junit.Before;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Base class for all CarActivityViewDisplayId tests.
 */
abstract class CarActivityViewDisplayIdTestBase extends CarApiTestBase {
    protected static final String ACTIVITY_VIEW_TEST_PKG_NAME = "android.car.apitest";

    protected static final String ACTIVITY_VIEW_TEST_PROCESS_NAME =
            ACTIVITY_VIEW_TEST_PKG_NAME + ":activity_view_test";

    protected static final String ACTIVITY_VIEW_DISPLAY_NAME = "TaskVirtualDisplay";

    protected static final int TEST_POLL_MS = 50;
    protected static final int TEST_TIMEOUT_SEC = 5;
    protected static final int TEST_TIMEOUT_MS = TEST_TIMEOUT_SEC * 1000;

    protected DisplayManager mDisplayManager;
    protected ActivityManager mActivityManager;
    protected CarUxRestrictionsManager mCarUxRestrictionsManager;

    @Before
    public void setUp() throws Exception {
        mDisplayManager = getContext().getSystemService(DisplayManager.class);
        mActivityManager = getContext().getSystemService(ActivityManager.class);
        mCarUxRestrictionsManager = (CarUxRestrictionsManager)
                getCar().getCarManager(Car.CAR_UX_RESTRICTION_SERVICE);
    }

    protected int waitForActivityViewDisplayReady(String packageName) {
        for (int i = 0; i < TEST_TIMEOUT_MS / TEST_POLL_MS; ++i) {
            for (Display display : mDisplayManager.getDisplays()) {
                if (display.getName().contains(ACTIVITY_VIEW_DISPLAY_NAME)
                        && display.getOwnerPackageName().equals(packageName)
                        && display.getState() == Display.STATE_ON) {
                    return display.getDisplayId();
                }
            }
            SystemClock.sleep(TEST_POLL_MS);
        }
        return INVALID_DISPLAY;
    }

    protected int getMappedPhysicalDisplayOfVirtualDisplay(int displayId) {
        return mCarUxRestrictionsManager.getMappedPhysicalDisplayOfVirtualDisplay(displayId);
    }

    public static class ActivityViewTestActivity extends CarActivityViewDisplayIdTest.TestActivity {
        private static final class ActivityViewStateCallback extends ActivityView.StateCallback {
            private final CountDownLatch mActivityViewReadyLatch = new CountDownLatch(1);
            private final CountDownLatch mActivityViewDestroyedLatch = new CountDownLatch(1);

            @Override
            public void onActivityViewReady(ActivityView view) {
                mActivityViewReadyLatch.countDown();
            }

            @Override
            public void onActivityViewDestroyed(ActivityView view) {
                mActivityViewDestroyedLatch.countDown();
            }
        }

        private CarActivityView mActivityView;
        private final ActivityViewStateCallback mCallback = new ActivityViewStateCallback();

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            mActivityView = new CarActivityView(this, /*attrs=*/null , /*defStyle=*/0 ,
                    /*singleTaskInstance=*/true);
            mActivityView.setCallback(mCallback);
            setContentView(mActivityView);

            ViewGroup.LayoutParams layoutParams = mActivityView.getLayoutParams();
            layoutParams.width = MATCH_PARENT;
            layoutParams.height = MATCH_PARENT;
            mActivityView.requestLayout();
        }

        @Override
        protected void onStop() {
            super.onStop();
            // Moved the release of the view from onDestroy to onStop since onDestroy was called
            // in non-deterministic timing.
            mActivityView.release();
        }

        ActivityView getActivityView() {
            return mActivityView;
        }

        void waitForActivityViewReady() throws Exception {
            waitForLatch(mCallback.mActivityViewReadyLatch);
        }

        void waitForActivityViewDestroyed() throws Exception {
            waitForLatch(mCallback.mActivityViewDestroyedLatch);
        }
    }

    protected ActivityViewTestActivity startActivityViewTestActivity(int displayId)
            throws Exception {
        return (ActivityViewTestActivity) startTestActivity(ActivityViewTestActivity.class,
                displayId);
    }

    /** Test activity representing a multi-process activity. */
    public static final class MultiProcessActivityViewTestActivity extends
            ActivityViewTestActivity {
    }

    /**
     * Test activity that has {@link android.R.attr#resizeableActivity} attribute set to
     * {@code true}.
     */
    public static final class ActivityInActivityView extends TestActivity {}

    /**
     * Starts the provided activity and returns the started instance.
     */
    protected TestActivity startTestActivity(Class<?> activityClass, int displayId)
            throws Exception {
        Instrumentation.ActivityMonitor monitor = new Instrumentation.ActivityMonitor(
                activityClass.getName(), null, false);
        getInstrumentation().addMonitor(monitor);

        Context context = getContext();
        Intent intent = new Intent(context, activityClass).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ActivityOptions options = ActivityOptions.makeBasic();
        if (displayId != DEFAULT_DISPLAY) {
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            options.setLaunchDisplayId(displayId);
        }
        context.startActivity(intent, options.toBundle());

        TestActivity activity = (TestActivity) monitor.waitForActivityWithTimeout(TEST_TIMEOUT_MS);
        if (activity == null) {
            throw new TimeoutException("Timed out waiting " + TEST_TIMEOUT_MS + " milliseconds "
                    + " waiting for Activity");
        }

        activity.waitForResumeStateChange();
        return activity;
    }

    private static void waitForLatch(CountDownLatch latch) throws Exception {
        boolean result = latch.await(TEST_TIMEOUT_SEC, TimeUnit.SECONDS);
        if (!result) {
            throw new TimeoutException("Timed out waiting " + TEST_TIMEOUT_SEC + " seconds waiting"
                    + " for task stack change notification");
        }
    }

    protected static class TestActivity extends Activity {
        private final CountDownLatch mResumed = new CountDownLatch(1);

        @Override
        protected void onPostResume() {
            super.onPostResume();
            mResumed.countDown();
        }

        void waitForResumeStateChange() throws Exception {
            waitForLatch(mResumed);
        }
    }
}
