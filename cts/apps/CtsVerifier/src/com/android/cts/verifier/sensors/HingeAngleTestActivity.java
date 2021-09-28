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

package com.android.cts.verifier.sensors;

import com.android.cts.verifier.R;
import com.android.cts.verifier.sensors.base.SensorCtsVerifierTestActivity;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.hardware.cts.helpers.TestSensorEnvironment;
import android.hardware.cts.helpers.sensoroperations.TestSensorOperation;
import android.hardware.cts.helpers.sensorverification.HingeAngleVerification;

public class HingeAngleTestActivity extends SensorCtsVerifierTestActivity {
    private SensorManager mSensorManager;

    public HingeAngleTestActivity() {
        super(HingeAngleTestActivity.class, true /* enableRetry */);
    }

    @Override
    protected void activitySetUp() throws InterruptedException {
        mSensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);

        getTestLogger().logInstructions(R.string.snsr_hinge_angle_test_instructions);
        waitForUserToContinue();
    }

    public String testExerciseHinge() throws Throwable {
        return verifyMeasurements(R.string.snsr_hinge_angle_test_exercise_hinge);
    }

    /**
     * Helper method that verifies hinge sensor measurements meet the following criteria:
     *  - Maximum range of the sensor isn't exceeded.
     *  - Sensor resolution is respected
     *  - Duplicate values are not seen back-to-back.
     */
    private String verifyMeasurements(int instructionsResId) throws Throwable {
        Sensor wakeUpSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_HINGE_ANGLE);
        TestSensorEnvironment environment = new TestSensorEnvironment(
                getApplicationContext(),
                wakeUpSensor,
                0, /* samplingPeriod */
                SensorManager.SENSOR_DELAY_FASTEST);

        TestSensorOperation sensorOperation = TestSensorOperation
                .createOperation(environment, () -> waitForUser(instructionsResId));

        HingeAngleVerification verification = new HingeAngleVerification();
        sensorOperation.addVerification(verification);
        sensorOperation.execute(getCurrentTestNode());

        return null;
    }
}