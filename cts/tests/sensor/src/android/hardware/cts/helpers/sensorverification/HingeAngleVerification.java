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
 * limitations under the License
 */

package android.hardware.cts.helpers.sensorverification;

import junit.framework.Assert;

import android.hardware.Sensor;
import android.hardware.cts.helpers.SensorStats;
import android.hardware.cts.helpers.TestSensorEnvironment;
import android.hardware.cts.helpers.TestSensorEvent;

import java.lang.Math;
import java.util.LinkedList;
import java.util.List;

/**
 * A {@link ISensorVerification} that verifies the measurements of {@link Sensor#TYPE_HINGE_ANGLE}
 *
 * Its job is to verify that, while the tester manipulates the device:
 * - Back-to-back duplicate values aren’t seen
 * - Maximum range is not exceeded
 * - No event values were less than 0
 * - No updates were given more frequently than the resolution denotes
 */
public class HingeAngleVerification extends AbstractSensorVerification {
    public static final String PASSED_KEY = "hinge_angle_integration_passed";

    private final List<TestSensorEvent> mEvents = new LinkedList<>();

    @Override
    public void verify(TestSensorEnvironment environment, SensorStats stats) {
        float resolution = environment.getSensor().getResolution();
        float maxRange = environment.getSensor().getMaximumRange();
        TestSensorEvent lastEvent = null;

        boolean success = true;
        StringBuilder builder = new StringBuilder("Hinge Angle | failures:");
        Assert.assertTrue("The hinge angle sensor must output at least two sensor events",
                mEvents.size() >= 2);
        for (int i = 0; i < mEvents.size(); i++) {
            TestSensorEvent event = mEvents.get(i);
            if (lastEvent != null && lastEvent.values[0] == event.values[0]) {
                success = false;
                String message = String.format("\nEvent time=%s, value=%s same as prior event" +
                        " time=%s, value=%s", event.timestamp, event.values[0], lastEvent.timestamp,
                        lastEvent.values[0]);
                builder.append(message);
            }

            if (event.values[0] > maxRange) {
                success = false;
                String message = String.format("\nEvent time=%s, value=%s greater than maximum" +
                        " range=%s", event.timestamp, event.values[0], maxRange);
                builder.append(message);
            }

            if (event.values[0] < 0) {
                success = false;
                String message = String.format("\nEvent time=%s, value=%s less than 0",
                        event.timestamp, event.values[0]);
                builder.append(message);
            }

            // Make sure remainder is very close to zero, but not exactly to account for floating
            // point division errors.
            float remainder = event.values[0] == 0.0f ? 0 : Math.abs(event.values[0] % resolution);
            if (remainder > 0.001 && resolution - remainder > 0.001) {
                success = false;
                String message = String.format("\nEvent time=%s, value=%s more fine grained than" +
                        " resolution=%s %s", event.timestamp, event.values[0], resolution,
                        remainder);
                builder.append(message);
            }

            lastEvent = event;
        }

        stats.addValue(PASSED_KEY, success);
        Assert.assertTrue(builder.toString(), success);
    }

    @Override
    public HingeAngleVerification clone() {
        return new HingeAngleVerification();
    }

    @Override
    protected void addSensorEventInternal(TestSensorEvent event) {
        mEvents.add(event);
    }
}
