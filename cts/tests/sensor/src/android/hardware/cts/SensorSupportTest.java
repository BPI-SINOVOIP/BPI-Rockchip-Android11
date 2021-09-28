/*
 * Copyright (C) 2015 The Android Open Source Project
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
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.hardware.Sensor;
import android.hardware.SensorDirectChannel;
import android.hardware.SensorManager;
import android.os.Build;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.PropertyUtil;

/**
 * Checks if required sensor types are present, for example sensors required
 * when Hifi sensors or VR High performance mode features are enabled. Also
 * checks that required composite sensor types are present if the underlying
 * physical sensors are present.
 *
 * <p>To execute these test cases, the following command can be used:</p>
 * <pre>
 * adb shell am instrument -e class android.hardware.cts.SensorSupportTest \
 *     -w android.hardware.cts/android.test.AndroidJUnitRunner
 * </pre>
 */
public class SensorSupportTest extends SensorTestCase {
    private SensorManager mSensorManager;
    private boolean mAreHifiSensorsSupported;
    private boolean mVrHighPerformanceModeSupported;
    private boolean mIsVrHeadset;
    private boolean mHasAccel;
    private boolean mHasGyro;
    private boolean mHasMag;

    @Override
    public void setUp() {
        PackageManager pm = getContext().getPackageManager();
        // Some tests will only run if either HIFI_SENSORS or VR high performance mode is supported.
        mAreHifiSensorsSupported = pm.hasSystemFeature(PackageManager.FEATURE_HIFI_SENSORS);
        mVrHighPerformanceModeSupported = pm.hasSystemFeature(PackageManager.FEATURE_VR_MODE_HIGH_PERFORMANCE);
        mIsVrHeadset = (getContext().getResources().getConfiguration().uiMode
            & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_VR_HEADSET;
        mSensorManager =
                (SensorManager) getContext().getSystemService(Context.SENSOR_SERVICE);

        mHasAccel = hasSensorType(Sensor.TYPE_ACCELEROMETER);
        mHasGyro = hasSensorType(Sensor.TYPE_GYROSCOPE);
        mHasMag = hasSensorType(Sensor.TYPE_MAGNETIC_FIELD);
    }

    @CddTest(requirement="7.3.9/C-2-1")
    public void testSupportsAccelerometer() {
        checkHifiVrSensorSupport(Sensor.TYPE_ACCELEROMETER);
    }

    @CddTest(requirement="7.3.9/C-2-2")
    public void testSupportsAccelerometerUncalibrated() {
        // Uncalibrated accelerometer was not required before Android O
        if (PropertyUtil.getFirstApiLevel() >= Build.VERSION_CODES.O) {
            checkHifiVrSensorSupport(Sensor.TYPE_ACCELEROMETER_UNCALIBRATED);
        }
    }

    @CddTest(requirement="7.3.9/C-2-3")
    public void testSupportsGyroscope() {
        checkHifiVrSensorSupport(Sensor.TYPE_GYROSCOPE);
    }

    @CddTest(requirement="7.3.9/C-2-4")
    public void testSupportsGyroscopeUncalibrated() {
        checkHifiVrSensorSupport(Sensor.TYPE_GYROSCOPE_UNCALIBRATED);
    }

    @CddTest(requirement="7.3.9/C-2-5")
    public void testSupportsGeoMagneticField() {
        checkHifiVrSensorSupport(Sensor.TYPE_MAGNETIC_FIELD);
    }

    @CddTest(requirement="7.3.9/C-2-6")
    public void testSupportsMagneticFieldUncalibrated() {
        checkHifiVrSensorSupport(Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED);
    }

    @CddTest(requirement="7.3.9/C-2-7")
    public void testSupportsPressure() {
        checkHifiVrSensorSupport(Sensor.TYPE_PRESSURE);
    }

    @CddTest(requirement="7.3.9/C-2-8")
    public void testSupportsGameRotationVector() {
        checkHifiVrSensorSupport(Sensor.TYPE_GAME_ROTATION_VECTOR);
    }

    @CddTest(requirement="7.3.9/C-2-9")
    public void testSupportsSignificantMotion() {
        checkHifiVrSensorSupport(Sensor.TYPE_SIGNIFICANT_MOTION);
    }

    @CddTest(requirement="7.3.9/C-2-10")
    public void testSupportsStepDetector() {
        checkHifiVrSensorSupport(Sensor.TYPE_STEP_DETECTOR);
    }

    @CddTest(requirement="7.3.9/C-2-11")
    public void testSupportsStepCounter() {
        checkHifiVrSensorSupport(Sensor.TYPE_STEP_COUNTER);
    }

    @CddTest(requirement="7.3.9/C-2-12")
    public void testSupportsTiltDetector() {
        final int TYPE_TILT_DETECTOR = 22;
        checkHifiVrSensorSupport(TYPE_TILT_DETECTOR);
    }

    @CddTest(requirement="7.3.1/C-3-1")
    public void testSupportsGravityAndLinearAccelIfHasAG() {
        if (mHasAccel && mHasGyro) {
            assertTrue(hasSensorType(Sensor.TYPE_GRAVITY));
            assertTrue(hasSensorType(Sensor.TYPE_LINEAR_ACCELERATION));
        }
    }

    @CddTest(requirement="7.3.1/C-4-1")
    public void testSupportsRotationVectorIfHasAGM() {
        if (mHasAccel && mHasGyro && mHasMag) {
            assertTrue(hasSensorType(Sensor.TYPE_ROTATION_VECTOR));
        }
    }

    private boolean sensorRequiredForVrHighPerformanceMode(int sensorType) {
        if (sensorType == Sensor.TYPE_ACCELEROMETER ||
            sensorType == Sensor.TYPE_ACCELEROMETER_UNCALIBRATED ||
            sensorType == Sensor.TYPE_GYROSCOPE ||
            sensorType == Sensor.TYPE_GYROSCOPE_UNCALIBRATED ||
            sensorType == Sensor.TYPE_MAGNETIC_FIELD ||
            sensorType == Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED) {
            return true;
        } else {
            return false;
        }
    }

    private void checkHifiVrSensorSupport(int sensorType) {
        boolean isVrSensor = mVrHighPerformanceModeSupported &&
            sensorRequiredForVrHighPerformanceMode(sensorType);
        if (mAreHifiSensorsSupported || isVrSensor) {
            Sensor sensor = mSensorManager.getDefaultSensor(sensorType);
            assertTrue(sensor != null);
            if (isVrSensor && mIsVrHeadset) {
                assertTrue(sensor.isDirectChannelTypeSupported(SensorDirectChannel.TYPE_HARDWARE_BUFFER));
            }
        }
    }

    private boolean hasSensorType(int sensorType) {
        return (mSensorManager != null && mSensorManager.getDefaultSensor(sensorType) != null);
    }
}
