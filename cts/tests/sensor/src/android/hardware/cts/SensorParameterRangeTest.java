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
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.hardware.cts.helpers.SensorCtsHelper;
import android.os.Build;
import android.text.TextUtils;

import com.android.compatibility.common.util.ApiLevelUtil;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Test min-max frequency, max range parameters for sensors.
 *
 * <p>To execute these test cases, the following command can be used:</p>
 * <pre>
 * adb shell am instrument -e class android.hardware.cts.SensorParameterRangeTest \
 *     -w android.hardware.cts/android.test.AndroidJUnitRunner
 * </pre>
 */
public class SensorParameterRangeTest extends SensorTestCase {

    private static final double ACCELEROMETER_MAX_RANGE = 4 * 9.80; // 4g - ε
    private static final double ACCELEROMETER_MAX_FREQUENCY = 50.0;
    private static final double ACCELEROMETER_HIFI_MAX_RANGE = 8 * 9.80; // 8g - ε
    private static final double ACCELEROMETER_HIFI_MIN_FREQUENCY = 12.50;
    private static final double ACCELEROMETER_HIFI_MAX_FREQUENCY = 400.0;
    private static final double ACCELEROMETER_HIFI_MAX_FREQUENCY_BEFORE_N = 200.0;

    private static final double GYRO_MAX_RANGE = 1000 / 57.295 - 1.0; // 1000 degrees/sec - ε
    private static final double GYRO_MAX_FREQUENCY = 50.0;
    private static final double GYRO_HIFI_MAX_RANGE = 1000 / 57.295 - 1.0; // 1000 degrees/sec - ε
    private static final double GYRO_HIFI_MIN_FREQUENCY = 12.50;
    private static final double GYRO_HIFI_MAX_FREQUENCY = 400.0;
    private static final double GYRO_HIFI_MAX_FREQUENCY_BEFORE_N = 200.0;

    private static final double MAGNETOMETER_MAX_RANGE = 900.0;   // micro telsa
    private static final double MAGNETOMETER_MAX_FREQUENCY = 50.0;
    private static final double MAGNETOMETER_HIFI_MAX_RANGE = 900.0;   // micro telsa
    private static final double MAGNETOMETER_HIFI_MIN_FREQUENCY = 5.0;
    private static final double MAGNETOMETER_HIFI_MAX_FREQUENCY = 50.0;

    private static final double PRESSURE_MAX_RANGE = 0.0;     // hecto-pascal
    private static final double PRESSURE_MAX_FREQUENCY = 5.0;
    private static final double PRESSURE_HIFI_MAX_RANGE = 1100.0;     // hecto-pascal
    private static final double PRESSURE_HIFI_MIN_FREQUENCY = 1.0;
    private static final double PRESSURE_HIFI_MAX_FREQUENCY = 10.0;

    // Note these FIFO minimum constants come from the CCD.  In version
    // 6.0 of the CCD, these are in section 7.3.9.
    private static final int ACCELEROMETER_MIN_FIFO_LENGTH = 3000;
    private static final int UNCAL_MAGNETOMETER_MIN_FIFO_LENGTH = 600;
    private static final int PRESSURE_MIN_FIFO_LENGTH = 300;
    private static final int PROXIMITY_SENSOR_MIN_FIFO_LENGTH = 100;
    private static final int STEP_DETECTOR_MIN_FIFO_LENGTH = 100;

    private boolean mHasHifiSensors;
    private boolean mHasProximitySensor;
    private boolean mVrModeHighPerformance;
    private SensorManager mSensorManager;

    @Override
    public void setUp() {
        PackageManager pm = getContext().getPackageManager();
        mSensorManager = (SensorManager) getContext().getSystemService(Context.SENSOR_SERVICE);
        mHasHifiSensors = pm.hasSystemFeature(PackageManager.FEATURE_HIFI_SENSORS);
        mHasProximitySensor = pm.hasSystemFeature(PackageManager.FEATURE_SENSOR_PROXIMITY);
        mVrModeHighPerformance = pm.hasSystemFeature(PackageManager.FEATURE_VR_MODE_HIGH_PERFORMANCE);
    }

    public void testAccelerometerRange() {
        double hifiMaxFrequency = ApiLevelUtil.isAtLeast(Build.VERSION_CODES.N) ?
                ACCELEROMETER_HIFI_MAX_FREQUENCY :
                ACCELEROMETER_HIFI_MAX_FREQUENCY_BEFORE_N;

        checkSensorRangeAndFrequency(
                Sensor.TYPE_ACCELEROMETER,
                ACCELEROMETER_MAX_RANGE,
                ACCELEROMETER_MAX_FREQUENCY,
                ACCELEROMETER_HIFI_MAX_RANGE,
                ACCELEROMETER_HIFI_MIN_FREQUENCY,
                hifiMaxFrequency);
    }

    public void testGyroscopeRange() {
        double hifiMaxFrequency = ApiLevelUtil.isAtLeast(Build.VERSION_CODES.N) ?
                GYRO_HIFI_MAX_FREQUENCY :
                GYRO_HIFI_MAX_FREQUENCY_BEFORE_N;

        checkSensorRangeAndFrequency(
                Sensor.TYPE_GYROSCOPE,
                GYRO_MAX_RANGE,
                GYRO_MAX_FREQUENCY,
                GYRO_HIFI_MAX_RANGE,
                GYRO_HIFI_MIN_FREQUENCY,
                hifiMaxFrequency);
    }

    public void testMagnetometerRange() {
        checkSensorRangeAndFrequency(
                Sensor.TYPE_MAGNETIC_FIELD,
                MAGNETOMETER_MAX_RANGE,
                MAGNETOMETER_MAX_FREQUENCY,
                MAGNETOMETER_HIFI_MAX_RANGE,
                MAGNETOMETER_HIFI_MIN_FREQUENCY,
                MAGNETOMETER_HIFI_MAX_FREQUENCY);
    }

    public void testPressureRange() {
        checkSensorRangeAndFrequency(
                Sensor.TYPE_PRESSURE,
                PRESSURE_MAX_RANGE,
                PRESSURE_MAX_FREQUENCY,
                PRESSURE_HIFI_MAX_RANGE,
                PRESSURE_HIFI_MIN_FREQUENCY,
                PRESSURE_HIFI_MAX_FREQUENCY);
    }

    private void checkSensorRangeAndFrequency(
            int sensorType, double maxRange, double maxFrequency, double hifiMaxRange,
            double hifiMinFrequency, double hifiMaxFrequency) {
        boolean mustMeetHiFi = mHasHifiSensors;

        // CDD 7.9.2/C-1-21: High Performance VR must meet accel, gyro, and mag HiFi requirements
        if (mVrModeHighPerformance && (sensorType == Sensor.TYPE_ACCELEROMETER ||
                sensorType == Sensor.TYPE_GYROSCOPE || sensorType == Sensor.TYPE_MAGNETIC_FIELD)) {
            mustMeetHiFi = true;
        }

        Sensor sensor = mSensorManager.getDefaultSensor(sensorType);
        if (sensor == null) {
            if (mustMeetHiFi) {
                fail(String.format("Must support sensor type %d", sensorType));
            } else {
                // Sensor is not required
                return;
            }
        }

        double range = mustMeetHiFi ? hifiMaxRange : maxRange;
        double frequency = mustMeetHiFi ? hifiMaxFrequency : maxFrequency;

        assertTrue(String.format("%s Range actual=%.2f expected=%.2f %s",
                    sensor.getName(), sensor.getMaximumRange(), range,
                    SensorCtsHelper.getUnitsForSensor(sensor)),
                sensor.getMaximumRange() >= (range - 0.1));

        double actualMaxFrequency = SensorCtsHelper.getFrequency(sensor.getMinDelay(),
                TimeUnit.MICROSECONDS);
        assertTrue(String.format("%s Max Frequency actual=%.2f expected=%.2fHz",
                    sensor.getName(), actualMaxFrequency, frequency), actualMaxFrequency >=
                frequency - 0.1);

        if (mustMeetHiFi) {
            double actualMinFrequency = SensorCtsHelper.getFrequency(sensor.getMaxDelay(),
                    TimeUnit.MICROSECONDS);
            assertTrue(String.format("%s Min Frequency actual=%.2f expected=%.2fHz",
                        sensor.getName(), actualMinFrequency, hifiMinFrequency),
                    actualMinFrequency <=  hifiMinFrequency + 0.1);
        }
    }

    public void testAccelerometerFifoLength() throws Throwable {
        if (!mHasHifiSensors) return;
        checkMinFifoLength(Sensor.TYPE_ACCELEROMETER, ACCELEROMETER_MIN_FIFO_LENGTH);
    }

    public void testUncalMagnetometerFifoLength() throws Throwable {
        if (!mHasHifiSensors) return;
        checkMinFifoLength(
                Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED,
                UNCAL_MAGNETOMETER_MIN_FIFO_LENGTH);
    }

    public void testPressureFifoLength() throws Throwable {
        if (!mHasHifiSensors) return;
        checkMinFifoLength(Sensor.TYPE_PRESSURE, PRESSURE_MIN_FIFO_LENGTH);
    }

    public void testProximityFifoLength() throws Throwable {
        if (!mHasHifiSensors || !mHasProximitySensor) return;
        checkMinFifoLength(Sensor.TYPE_PROXIMITY, PROXIMITY_SENSOR_MIN_FIFO_LENGTH);
    }

    public void testStepDetectorFifoLength() throws Throwable {
        if (!mHasHifiSensors) return;
        checkMinFifoLength(Sensor.TYPE_STEP_DETECTOR, STEP_DETECTOR_MIN_FIFO_LENGTH);
    }

    private void checkMinFifoLength(int sensorType, int minRequiredLength) {
        Sensor sensor = mSensorManager.getDefaultSensor(sensorType);
        assertTrue(String.format("sensor of type=%d (null)", sensorType), sensor != null);
        int reservedLength = sensor.getFifoReservedEventCount();
        assertTrue(String.format("Sensor=%s, min required fifo length=%d actual=%d",
                    sensor.getName(), minRequiredLength, reservedLength),
                    reservedLength >= minRequiredLength);
    }

    public void testStaticSensorId() {
        // all static sensors should have id of 0
        List<Sensor> sensors = mSensorManager.getSensorList(Sensor.TYPE_ALL);
        List<String> errors = new ArrayList<>();
        for (Sensor s : sensors) {
            int id = s.getId();
            if (id != 0) {
                errors.add(String.format("sensor \"%s\" has id %d", s.getName(), id));
            }
        }
        if (errors.size() > 0) {
            String message = "Static sensors should have id of 0, violations: < " +
                    TextUtils.join(", ", errors) + " >";
            fail(message);
        }
    }
}
