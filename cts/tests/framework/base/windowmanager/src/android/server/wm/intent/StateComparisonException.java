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

package android.server.wm.intent;

import org.json.JSONException;

/**
 * Exception to report differences in the {@link Persistence.StateDump} when verifying a
 * {@link Persistence.TestCase} using the {@linkLaunchRunner#verify} method
 */
public class StateComparisonException extends RuntimeException {
    private Persistence.StateDump mExpected;
    private Persistence.StateDump mActual;
    private String stage;

    public StateComparisonException(Persistence.StateDump expected,
            Persistence.StateDump actual, String stage) {
        mExpected = expected;
        mActual = actual;
        this.stage = stage;
    }


    @Override
    public String getMessage() {
        try {
            String newLine = System.lineSeparator();
            return new StringBuilder()
                    .append(newLine)
                    .append(stage)
                    .append(newLine)
                    .append("Expected:")
                    .append(newLine)
                    .append(mExpected.toJson().toString(2))
                    .append(newLine)
                    .append(newLine)
                    .append("Actual:")
                    .append(newLine)
                    .append(mActual.toJson().toString(2))
                    .append(newLine)
                    .append(newLine)
                    .toString();
        } catch (JSONException e) {
            e.printStackTrace();
            return "json parse exception during error message creation";
        }
    }

    public static void assertEndStatesEqual(Persistence.StateDump expected,
            Persistence.StateDump actual) {
        compareAndThrow(expected, actual, "Different endSates");
    }

    public static void assertInitialStateEqual(Persistence.StateDump expected,
            Persistence.StateDump actual) {
        compareAndThrow(expected, actual, "Different initial states");
    }

    private static void compareAndThrow(Persistence.StateDump expected,
            Persistence.StateDump actual, String stage) {
        if (!expected.equals(actual)) {
            throw new StateComparisonException(expected, actual, stage);
        }
    }
}
