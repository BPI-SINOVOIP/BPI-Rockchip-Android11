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

package com.android.car.pm;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.app.ActivityOptions;
import android.car.Car;
import android.car.drivingstate.CarDrivingStateEvent;
import android.car.drivingstate.CarDrivingStateManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.view.Display;

import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class ActivityBlockingActivityTest {
    private static final String ACTIVITY_BLOCKING_ACTIVITY_TEXTVIEW_ID =
            "com.android.car:id/blocking_text";

    // cf_x86_auto is very slow, so uses very long timeout.
    private static final int UI_TIMEOUT_MS = 20_000;
    private static final int NOT_FOUND_UI_TIMEOUT_MS = 10_000;
    private static final long ACTIVITY_TIMEOUT_MS = 5000;

    private CarDrivingStateManager mCarDrivingStateManager;

    private UiDevice mDevice;

    // NOTE: Assume there is only one testing Activity.
    private static final AtomicReference<TempActivity> sTestingActivity = new AtomicReference<>();

    @Before
    public void setUp() throws Exception {
        Car car = Car.createCar(getContext());
        mCarDrivingStateManager = (CarDrivingStateManager)
                car.getCarManager(Car.CAR_DRIVING_STATE_SERVICE);
        assertNotNull(mCarDrivingStateManager);

        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        setDrivingStateMoving();
    }

    @After
    public void tearDown() throws Exception {
        setDrivingStateParked();

        TempActivity testingActivity = sTestingActivity.get();
        if (testingActivity != null) {
            testingActivity.finishCompletely();
        }
    }

    @Test
    public void testBlockingActivity_doActivity_isNotBlocked() throws Exception {
        startActivity(toComponentName(getTestContext(), DoActivity.class));

        assertThat(mDevice.wait(Until.findObject(By.text(
                DoActivity.class.getSimpleName())),
                UI_TIMEOUT_MS)).isNotNull();
        assertBlockingActivityNotFound();
    }

    @Test
    public void testBlockingActivity_nonDoActivity_isBlocked() throws Exception {
        startNonDoActivity(NonDoActivity.EXTRA_DO_NOTHING);

        assertThat(mDevice.wait(Until.findObject(By.res(ACTIVITY_BLOCKING_ACTIVITY_TEXTVIEW_ID)),
                UI_TIMEOUT_MS)).isNotNull();
    }

    @Test
    public void testBlockingActivity_nonDoFinishesOnCreate_noBlockingActivity()
            throws Exception {
        startNonDoActivity(NonDoActivity.EXTRA_ONCREATE_FINISH_IMMEDIATELY);

        assertBlockingActivityNotFound();
    }

    @Test
    public void testBlockingActivity_nonDoLaunchesDoOnCreate_noBlockingActivity()
            throws Exception {
        startNonDoActivity(NonDoActivity.EXTRA_ONCREATE_LAUNCH_DO_IMMEDIATELY);

        assertBlockingActivityNotFound();
    }

    @Test
    public void testBlockingActivity_nonDoFinishesOnResume_noBlockingActivity()
            throws Exception {
        startNonDoActivity(NonDoActivity.EXTRA_ONRESUME_FINISH_IMMEDIATELY);

        assertBlockingActivityNotFound();
    }

    @Test
    public void testBlockingActivity_nonDoLaunchesDoOnResume_noBlockingActivity()
            throws Exception {
        startNonDoActivity(NonDoActivity.EXTRA_ONRESUME_LAUNCH_DO_IMMEDIATELY);

        assertBlockingActivityNotFound();
    }

    @Test
    public void testBlockingActivity_nonDoNoHistory_isBlocked() throws Exception {
        startActivity(toComponentName(getTestContext(), NonDoNoHistoryActivity.class));

        assertThat(mDevice.wait(Until.findObject(By.res(ACTIVITY_BLOCKING_ACTIVITY_TEXTVIEW_ID)),
                UI_TIMEOUT_MS)).isNotNull();
    }

    private void assertBlockingActivityNotFound() {
        assertThat(mDevice.wait(Until.gone(By.res(ACTIVITY_BLOCKING_ACTIVITY_TEXTVIEW_ID)),
                NOT_FOUND_UI_TIMEOUT_MS)).isNotNull();
    }

    private void startActivity(ComponentName name) {
        Intent intent = new Intent();
        intent.setComponent(name);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(Display.DEFAULT_DISPLAY);

        getContext().startActivity(intent, options.toBundle());
    }

    private void startNonDoActivity(int firstActionFlag) {
        ComponentName activity = toComponentName(getTestContext(), NonDoActivity.class);
        Intent intent = new Intent();
        intent.setComponent(activity);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(NonDoActivity.EXTRA_FIRST_ACTION, firstActionFlag);

        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(Display.DEFAULT_DISPLAY);

        getContext().startActivity(intent, options.toBundle());
    }


    private void setDrivingStateMoving() {
        mCarDrivingStateManager.injectDrivingState(CarDrivingStateEvent.DRIVING_STATE_MOVING);
    }

    private void setDrivingStateParked() {
        mCarDrivingStateManager.injectDrivingState(CarDrivingStateEvent.DRIVING_STATE_PARKED);
    }

    private static ComponentName toComponentName(Context ctx, Class<?> cls) {
        return ComponentName.createRelative(ctx, cls.getName());
    }

    public static class NonDoActivity extends TempActivity {

        static final String EXTRA_FIRST_ACTION = "first_action";

        static final int EXTRA_DO_NOTHING = 0;
        static final int EXTRA_ONCREATE_FINISH_IMMEDIATELY = 1;
        static final int EXTRA_ONCREATE_LAUNCH_DO_IMMEDIATELY = 2;
        static final int EXTRA_ONRESUME_FINISH_IMMEDIATELY = 3;
        static final int EXTRA_ONRESUME_LAUNCH_DO_IMMEDIATELY = 4;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Bundle extras = getIntent().getExtras();
            if (extras != null) {
                switch (extras.getInt(EXTRA_FIRST_ACTION, EXTRA_DO_NOTHING)) {
                    case EXTRA_ONCREATE_LAUNCH_DO_IMMEDIATELY:
                        startActivity(new Intent(this, DoActivity.class));
                        finish();
                        break;
                    case EXTRA_ONCREATE_FINISH_IMMEDIATELY:
                        finish();
                        break;
                    default:
                        // do nothing
                }
            }
        }

        @Override
        protected void onResume() {
            super.onResume();
            Bundle extras = getIntent().getExtras();
            if (extras != null) {
                switch (extras.getInt(EXTRA_FIRST_ACTION, EXTRA_DO_NOTHING)) {
                    case EXTRA_ONRESUME_LAUNCH_DO_IMMEDIATELY:
                        startActivity(new Intent(this, DoActivity.class));
                        finish();
                        break;
                    case EXTRA_ONRESUME_FINISH_IMMEDIATELY:
                        finish();
                        break;
                    default:
                        // do nothing
                }
            }
        }
    }

    public static class NonDoNoHistoryActivity extends TempActivity {
    }

    public static class DoActivity extends TempActivity {
    }

    /** Activity that closes itself after some timeout to clean up the screen. */
    public static class TempActivity extends Activity {
        private final CountDownLatch mDestroyed = new CountDownLatch(1);
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            sTestingActivity.set(this);
        }

        @Override
        protected void onDestroy() {
            sTestingActivity.set(null);
            super.onDestroy();
            mDestroyed.countDown();
        }

        void finishCompletely() throws InterruptedException {
            finish();
            mDestroyed.await(ACTIVITY_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }
    }

    private Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    private Context getTestContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }
}
