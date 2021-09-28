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

package android.hardware.cts;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorAdditionalInfo;
import android.hardware.SensorEventCallback;
import android.hardware.SensorManager;
import android.hardware.cts.helpers.SensorCtsHelper;
import android.util.Log;
import android.content.pm.PackageManager;

import java.lang.Math;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Checks Sensor Additional Information feature functionality.
 */
public class SensorAdditionalInfoTest extends SensorTestCase {
    private static final String TAG = "SensorAdditionalInfoTest";
    private static final int ALLOWED_ADDITIONAL_INFO_DELIVER_SEC = 3;
    private static final int REST_PERIOD_BEFORE_TEST_SEC = 3;
    private static final double EPSILON = 1E-6;

    private SensorManager mSensorManager;

    @Override
    protected void setUp() throws Exception {
        mSensorManager = (SensorManager) getContext().getSystemService(Context.SENSOR_SERVICE);
    }

    public void testSensorAdditionalInfo() {
        if (mSensorManager == null) {
            return;
        }

        List<Sensor> list = mSensorManager.getSensorList(Sensor.TYPE_ALL);
        List<String> errors = new ArrayList<String>();
        for (Sensor s : list) {
            // Only test sensor additional info for Accelerometer, Gyroscope and Magnetometer.
            if (s.getType() != Sensor.TYPE_ACCELEROMETER &&
                    s.getType() != Sensor.TYPE_GYROSCOPE &&
                    s.getType() != Sensor.TYPE_MAGNETIC_FIELD) {
                continue;
            }
            if (!s.isAdditionalInfoSupported()) {
                // check SensorAdditionalInfo is supported for Automotive sensors.
                if (getContext().getPackageManager().hasSystemFeature(
                        PackageManager.FEATURE_AUTOMOTIVE)) {
                    errors.add("Sensor: " + s.getName() +
                        ", error: AdditionalSensorInfo not supported for Automotive sensor.");
                }
                continue;
            }

            try {
                runSensorAdditionalInfoTest(s);
            } catch (AssertionError e) {
                errors.add("Sensor: " + s.getName() + ", error: " + e.getMessage());
            }
        }
        if (errors.size() > 0) {
            StringBuilder sb = new StringBuilder();
            sb.append("Failed for following reasons: [");
            int i = 0;
            for (String error : errors) {
                sb.append(String.format("%d. %s; ", i++, error));
            }
            sb.append("]");
            fail(sb.toString());
        }
    }

    private void runSensorAdditionalInfoTest(Sensor s) throws AssertionError {
        waitBeforeTestStarts();

        AdditionalInfoVerifier verifier = new AdditionalInfoVerifier(s);
        verifier.reset(false /*flushPending*/);

        assertTrue(String.format("Register sensor listener for %s failed.", s.getName()),
                mSensorManager.registerListener(verifier, s, SensorManager.SENSOR_DELAY_NORMAL));
        try {
            assertTrue("Missing additional info at registration: (" + verifier.getState() + ")",
                    verifier.verify());

            assertFalse("Duplicate TYPE_FRAME_BEGIN at: (" +
                    verifier.getState() + ")", verifier.beginFrameDuplicate());

            if (verifier.internalTemperature()) {
                assertFalse("Duplicate TYPE_INTERNAL_TEMPERATURE at: (" +
                        verifier.getState() + ")", verifier.internalTemperatureDuplicate());
            }

            if (verifier.sampling()) {
                assertFalse("Duplicate TYPE_SAMPLING_TEMPERATURE at: (" +
                        verifier.getState() + ")", verifier.samplingDuplicate());
            }

            // verify TYPE_SENSOR_PLACEMENT for Automotive.
            if (getContext().getPackageManager().
                    hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
                assertTrue("Missing TYPE_SENSOR_PLACEMENT at: (" + verifier.getState() + ")",
                        verifier.sensorPlacement());

            }
            if(verifier.sensorPlacement()) {
                assertFalse("Duplicate TYPE_SENSOR_PLACEMENT at: (" +
                        verifier.getState() + ")", verifier.sensorPlacementDuplicate());

                assertTrue("Incorrect size of TYPE_SENSOR_PLACEMENT at: (" +
                        verifier.getState() + ")", verifier.sensorPlacementSizeValid());

                if (verifier.sensorPlacementSizeValid()) {
                    assertTrue("Incorrect rotation matrix of TYPE_SENSOR_PLACEMENT at: (" +
                            verifier.getState() + ")", verifier.sensorPlacementRotationValid());
                }
            }

            if (verifier.untrackedDelay()) {
                assertFalse("Duplicate TYPE_UNTRACKED_DELAY at: (" +
                        verifier.getState() + ")", verifier.untrackedDelayDuplicate());
            }

            if (verifier.vec3Calibration()) {
                assertFalse("Duplicate TYPE_VEC3_CALIBRATION at: (" +
                        verifier.getState() + ")", verifier.vec3CalibrationDuplicate());
            }

            verifier.reset(true /*flushPending*/);
            assertTrue("Flush sensor failed.", mSensorManager.flush(verifier));
            assertTrue("Missing additional info after flushing: (" + verifier.getState() + ")",
                    verifier.verify());
        } finally {
            mSensorManager.unregisterListener(verifier);
        }
    }

    private void waitBeforeTestStarts() {
        // wait for sensor system to come to a rest after previous test to avoid flakiness.
        try {
            SensorCtsHelper.sleep(REST_PERIOD_BEFORE_TEST_SEC, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private class AdditionalInfoVerifier extends SensorEventCallback {
        private boolean mBeginFrame = false;
        private boolean mBeginFrameDuplicate = false;
        private boolean mEndFrame = false;
        private boolean mFlushPending = false;
        private boolean mInternalTemperature = false;
        private boolean mInternalTemperatureDuplicate = false;
        private boolean mSampling = false;
        private boolean mSamplingDuplicate = false;
        private boolean mSensorPlacement = false;
        private boolean mSensorPlacementDuplicate = false;
        private boolean mIsSensorPlacementSizeValid = false;
        private boolean mIsSensorPlacementRotationValid = false;
        private boolean mUntrackedDelay = false;
        private boolean mUntrackedDelayDuplicate = false;
        private boolean mVec3Calibration = false;
        private boolean mVec3CalibrationDuplicate = false;
        private CountDownLatch mDone;
        private final Sensor mSensor;

        public AdditionalInfoVerifier(Sensor s) {
            mSensor = s;
        }

        @Override
        public void onFlushCompleted(Sensor sensor) {
            if (sensor == mSensor) {
                mFlushPending = false;
            }
        }

        @Override
        public void onSensorAdditionalInfo(SensorAdditionalInfo info) {
            if (info.sensor == mSensor && !mFlushPending) {
                if (info.type == SensorAdditionalInfo.TYPE_FRAME_BEGIN) {
                    if (mBeginFrame) {
                        mBeginFrameDuplicate = true;
                        return;
                    }
                    mBeginFrame = true;
                } else if (mBeginFrame &&
                            info.type == SensorAdditionalInfo.TYPE_INTERNAL_TEMPERATURE) {
                    if (mInternalTemperature) {
                        mInternalTemperatureDuplicate = true;
                        return;
                    }
                    mInternalTemperature = true;
                } else if (mBeginFrame && info.type == SensorAdditionalInfo.TYPE_SAMPLING) {
                    if (mSampling) {
                        mSamplingDuplicate = true;
                        return;
                    }
                    mSampling = true;
                } else if (mBeginFrame && info.type == SensorAdditionalInfo.TYPE_SENSOR_PLACEMENT) {
                    if (mSensorPlacement) {
                        mSensorPlacementDuplicate = true;
                        return;
                    }
                    mSensorPlacement = true;
                    verifySensorPlacementData(info.floatValues);
                } else if (mBeginFrame && info.type == SensorAdditionalInfo.TYPE_UNTRACKED_DELAY) {
                    if (mUntrackedDelay) {
                        mUntrackedDelayDuplicate = true;
                        return;
                    }
                    mUntrackedDelay = true;
                } else if (mBeginFrame && info.type == SensorAdditionalInfo.TYPE_VEC3_CALIBRATION) {
                    if (mVec3Calibration) {
                        mVec3CalibrationDuplicate = true;
                        return;
                    }
                    mVec3Calibration = true;
                } else if (info.type == SensorAdditionalInfo.TYPE_FRAME_END && mBeginFrame) {
                    mEndFrame = true;
                    mDone.countDown();
                }
            }
        }

        public void reset(boolean flushPending) {
            mFlushPending = flushPending;
            mBeginFrame = false;
            mEndFrame = false;
            mSensorPlacement = false;
            mDone = new CountDownLatch(1);
        }

        public boolean verify() {
            boolean ret;
            try {
                ret = mDone.await(ALLOWED_ADDITIONAL_INFO_DELIVER_SEC, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                ret = false;
            }
            return ret;
        }

        public String getState() {
            return "fp=" + mFlushPending +", b=" + mBeginFrame + ", e=" + mEndFrame;
        }

        // Checks sensor placement data length and determinant of rotation matrix is 1.
        private void verifySensorPlacementData(float[] m) {
            if(m.length < 12) {
                mIsSensorPlacementSizeValid = false;
                return;
            }
            mIsSensorPlacementSizeValid = true;
            double determinant = m[0] * (m[5] * m[10] - m[6] * m[9] ) -
                                 m[1] * (m[4] * m[10] - m[6] * m[8] ) +
                                 m[2] * (m[4] * m[9]  - m[5] * m[8] );
            mIsSensorPlacementRotationValid = (Math.abs(determinant - 1) < EPSILON);
        }

        public boolean beginFrameDuplicate() {
            return mBeginFrameDuplicate;
        }

        public boolean internalTemperature() {
            return mInternalTemperature;
        }
        public boolean internalTemperatureDuplicate() {
            return mInternalTemperatureDuplicate;
        }

        public boolean sampling() {
            return mSampling;
        }
        public boolean samplingDuplicate() {
            return mSamplingDuplicate;
        }

        public boolean sensorPlacement() {
            return mSensorPlacement;
        }
        public boolean sensorPlacementDuplicate() {
            return mSensorPlacementDuplicate;
        }
        public boolean sensorPlacementSizeValid() {
            return mIsSensorPlacementSizeValid;
        }
        public boolean sensorPlacementRotationValid() {
            return mIsSensorPlacementRotationValid;
        }

        public boolean untrackedDelay() {
            return mUntrackedDelay;
        }
        public boolean untrackedDelayDuplicate() {
            return mUntrackedDelayDuplicate;
        }

        public boolean vec3Calibration() {
            return mVec3Calibration;
        }
        public boolean vec3CalibrationDuplicate() {
            return mVec3CalibrationDuplicate;
        }
    }
}
