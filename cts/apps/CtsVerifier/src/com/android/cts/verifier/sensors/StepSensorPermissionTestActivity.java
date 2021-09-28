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

package com.android.cts.verifier.sensors;

import com.android.cts.verifier.sensors.base.SensorCtsVerifierTestActivity;

import android.Manifest;
import com.android.cts.verifier.R;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.net.Uri;
import android.provider.Settings;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/**
 * Test cases to verify step sensor permissions
 */
public class StepSensorPermissionTestActivity extends SensorCtsVerifierTestActivity
        implements SensorEventListener {
    private static final int STEP_DETECT_DELAY_SECONDS = 30;
    private static final int STEP_COUNT_DELAY_SECONDS = 30;
    // Additional amount of time to give for receiving either a step detect or
    // count event in case the user hasn't started walking at the time the test
    // starts.
    private static final int ADDITIONAL_EVENT_DELAY_SECONDS = 2;

    private SensorManager mSensorManager;

    private boolean mHasAccelFeature = false;
    private boolean mHasStepCounterFeature = false;
    private boolean mHasStepDetectorFeature = false;
    private CountDownLatch mEventReceivedLatch = null;
    private Sensor mSensorUnderTest = null;
    private AccelRecorder mAccelRecorder = null;

    public StepSensorPermissionTestActivity() {
        super(StepSensorPermissionTestActivity.class);
    }

    @Override
    protected void activitySetUp() {
        mSensorManager = (SensorManager) getApplicationContext()
                .getSystemService(Context.SENSOR_SERVICE);

        mHasAccelFeature = getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_SENSOR_ACCELEROMETER);
        mHasStepCounterFeature = getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_SENSOR_STEP_COUNTER);
        mHasStepDetectorFeature = getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_SENSOR_STEP_DETECTOR);

        mAccelRecorder = new AccelRecorder(mSensorManager);
    }

    @Override
    protected void activityCleanUp() {
        mSensorManager.unregisterListener(this);
        mAccelRecorder.stop();

        // Notify the user that the test has concluded. This will play in both the pass
        // and fail case
        try {
            playSound();
        } catch (InterruptedException e) {
            // Ignored
        }
    }

    /**
     * Test cases.
     */
    @SuppressWarnings("unused")
    public String testStepEvents() throws Throwable {
        if (mHasStepCounterFeature || mHasStepDetectorFeature) {
            // Verify that sensors cannot be registered when the permission is not granted
            runTest(false /* permissionGranted */);

            // Signal to the user that the first part of the test is over
            try {
                playSound();
            } catch (InterruptedException e) {
                // Ignored
            }

            // Verify that the sensors can be registered when the permission is not granted
            runTest(true /* permissionGranted */);
        }

        return "Pass";
    }

    private void runTest(boolean permissionGranted) throws InterruptedException {
        if (hasPermission(Manifest.permission.ACTIVITY_RECOGNITION) != permissionGranted) {
            requestChangePermission(permissionGranted);
        }

        waitForUser(R.string.snsr_step_permission_walk);
        checkPermission(Manifest.permission.ACTIVITY_RECOGNITION, permissionGranted);

        if (mHasStepDetectorFeature) {
            checkSensor(Sensor.TYPE_STEP_DETECTOR, permissionGranted, STEP_DETECT_DELAY_SECONDS);
        }

        if (mHasStepCounterFeature) {
            checkSensor(Sensor.TYPE_STEP_COUNTER, permissionGranted, STEP_COUNT_DELAY_SECONDS);
        }
    }

    private void requestChangePermission(boolean enable) throws InterruptedException {
        SensorTestLogger logger = getTestLogger();
        if (enable) {
            logger.logInstructions(R.string.snsr_step_permission_enable);
        } else {
            logger.logInstructions(R.string.snsr_step_permission_disable);
        }
        waitForUserToContinue();
        Intent intent = new Intent();
        intent.setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        Uri uri = Uri.fromParts("package", getPackageName(), null);
        intent.setData(uri);
        startActivity(intent);
    }

    private boolean hasPermission(String permission) {
        return getApplicationContext().checkSelfPermission(permission) ==
                PackageManager.PERMISSION_GRANTED;
    }

    private void checkPermission(String permission, boolean expected) {
        if (expected && !hasPermission(permission)) {
            throw new AssertionError(String.format("Should not have '%s' permission", permission));
        } else if (!expected && hasPermission(permission)) {
            throw new AssertionError(String.format("Should have '%s' permission", permission));
        }
    }

    private void checkSensor(int sensorType, boolean expected, int eventDelaySeconds)
            throws InterruptedException {
        if (mHasAccelFeature && !expected) {
            // Record accel when no events are expected to ensure that the device is moving during
            // the test.
            mAccelRecorder.start();
        }

        mEventReceivedLatch = new CountDownLatch(1);
        Sensor sensor = mSensorManager.getDefaultSensor(sensorType);
        mSensorUnderTest = sensor;

        String msg = String.format("Should %sbe able to register for '%s' events",
                expected ? "" : "not ", sensor.getName());
        assertTrue(msg, mSensorManager.registerListener(this, sensor, 0, 0) == expected);

        boolean eventReceived = mEventReceivedLatch.await(
                eventDelaySeconds + ADDITIONAL_EVENT_DELAY_SECONDS, TimeUnit.SECONDS);

        // Ensure that events are only received when the application has permission
        if (expected) {
            assertTrue("Failed to receive event for " + sensor.getName(), eventReceived);
        } else {
            assertFalse("Should not have received event for " + sensor.getName(), eventReceived);

            if (mHasAccelFeature) {
                // Verify that the device was moving during the test
                mAccelRecorder.stop();
                assertTrue("Walking not detected", mAccelRecorder.variance() > 1.0f);
            }
        }
    }

    @Override
    public void onSensorChanged(SensorEvent sensorEvent) {
        if (sensorEvent.sensor == mSensorUnderTest) {
            mEventReceivedLatch.countDown();
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int i) {
    }

    class AccelRecorder implements SensorEventListener {

        private SensorManager mSensorManager;
        private Sensor mAccel;
        private ArrayList<Float> mMagnitudes = new ArrayList<>();

        public AccelRecorder(SensorManager sensorManager) {
            mSensorManager = sensorManager;
            mAccel = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        }

        public void start() {
            mMagnitudes.clear();
            mSensorManager.registerListener(
                    this, mAccel, (int)TimeUnit.MILLISECONDS.toMicros(20), 0);
        }

        public void stop() {
            mSensorManager.unregisterListener(this);
        }

        public float variance() {
            if (mMagnitudes.size() <= 1) {
                return 0.0f;
            }

            float mean = 0;
            for (float val : mMagnitudes) {
                mean += val;
            }
            mean /= mMagnitudes.size();

            float var = 0;
            for (float val : mMagnitudes) {
                var += (val - mean) * (val - mean);
            }
            return var / (mMagnitudes.size() - 1);
        }

        @Override
        public void onSensorChanged(SensorEvent sensorEvent) {
            if (sensorEvent.sensor == mAccel) {
                float magnitude = sensorEvent.values[0] * sensorEvent.values[0] +
                        sensorEvent.values[1] * sensorEvent.values[1] +
                        sensorEvent.values[2] * sensorEvent.values[2];
                mMagnitudes.add((float)Math.sqrt(magnitude));
            }
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int i) {

        }
    }
}
