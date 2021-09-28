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

package android.hardware.cts.helpers.sensorverification;

import android.hardware.cts.helpers.SensorStats;
import android.hardware.cts.helpers.TestSensorEnvironment;
import android.hardware.cts.helpers.TestSensorEvent;

import java.util.ArrayList;
import java.util.Collection;
import junit.framework.TestCase;

/**
 * Tests for {@link OffsetVerification}.
 */
public class OffsetVerificationTest extends TestCase {

    /**
     * Test {@link OffsetVerification#verify(TestSensorEnvironment, SensorStats)}.
     * The test verifies that the indices of the offsets within a sensor event are
     * correctly used to determine if the offsets exceed the allowed limit.
     * The format of the test array mirrors that of a {@link SensorEvent}:
     *  v[0] : value 1
     *  v[1] : value 2
     *  v[2] : value 3
     *  v[3] : value 1 offset
     *  v[4] : value 2 offset
     *  v[5] : value 3 offset
     */
    public void testSingleEventVerify() {
        float[][] values = null;

        // Verify that the norm is calculated correctly
        values = new float[][]{ {10, 10, 10, 2, 2, 2} };
        runStats(3.5f /* threshold */, values, true /* pass */);
        runStats(3.4f /* threshold */, values, false /* pass */);

        // Verify that the first value from the offsets is used
        values = new float[][]{ {10, 10, 10, 2, 0, 0} };
        runStats(2.0f /* threshold */, values, true /* pass */);
        runStats(1.9f /* threshold */, values, false /* pass */);

        // Verify that the last value from the offsets is used
        values = new float[][]{ {10, 10, 10, 0, 0, 2} };
        runStats(2.0f /* threshold */, values, true /* pass */);
        runStats(1.9f /* threshold */, values, false /* pass */);

        // Verify that no values outside the offsets is used
        values = new float[][]{ {10, 10, 10, 0, 0, 0, 1} };
        runStats(0.1f /* threshold */, values, true /* pass */);
    }

    /**
     * Test {@link OffsetVerification#verify(TestSensorEnvironment, SensorStats)}.
     * This test verifies that multiple sensor events are correctly recorded and
     * verified.
     */
    public void testMultipleEventVerify() {
        float[][] values = null;

        values = new float[][] {
                {10, 10, 10, 2, 2, 2},
                {10, 10, 10, 2, 2, 2}
        };
        runStats(3.5f /* threshold */, values, true /* pass */);
        runStats(3.4f /* threshold */, values, false /* pass */);

        // Verify when the first event exceeds the threshold and the second does not
        values = new float[][] {
                {0, 0, 0, 0, 0, 0},
                {0, 0, 0, 2, 2, 2}
        };
        runStats(3.0f /* threshold */, values, false /* pass */);

        // Verify when the second event exceeds the threshold and the first does not
        values = new float[][] {
                {0, 0, 0, 0, 0, 0},
                {0, 0, 0, 10, 10, 10}
        };
        runStats(3.0f /* threshold */, values, false /* pass */);
    }

    private void runStats(float threshold, float[][] values, boolean pass) {
        SensorStats stats = new SensorStats();
        OffsetVerification verification = getVerification(threshold, values);
        if (pass) {
            verification.verify(stats);
        } else {
            try {
                verification.verify(stats);
                throw new Error("Expected an AssertionError");
            } catch (AssertionError e) {
                // Expected;
            }
        }
        assertEquals(pass, stats.getValue(OffsetVerification.PASSED_KEY));
    }

    private static OffsetVerification getVerification(
            float threshold, float[] ... values) {
        Collection<TestSensorEvent> events = new ArrayList<>(values.length);
        for (float[] value : values) {
            events.add(new TestSensorEvent(null /* sensor */, 0 /* timestamp */,
                    0 /* receivedTimestamp */, value));
        }
        OffsetVerification verification = new OffsetVerification(threshold);
        verification.addSensorEvents(events);
        return verification;
    }
}
