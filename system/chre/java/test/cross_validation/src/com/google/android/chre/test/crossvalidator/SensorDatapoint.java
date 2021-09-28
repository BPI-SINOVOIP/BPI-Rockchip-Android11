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
package com.google.android.chre.test.crossvalidator;

/*
 * Class that all types of sensor datapoints inherit from which supports comparison to another
 * datapoint via a static method. Objects of this type are used in the sensor CHRE cross validator.
 */
/*package*/
abstract class SensorDatapoint {
    // The chreGetTimeOffset() function promises +/-10ms accuracy to actual AP time so allow this
    // much leeway for datapoint comparison.

    public long timestamp;
    public float[] values;

    /*
    * @return The human readable string representing this datapoint.
    */
    @Override
    public String toString() {
        String str = String.format("<SensorDatapoint timestamp: %d, values: [ ", timestamp);
        for (float val : values) {
            str += String.format("%f ", val);
        }
        str += "]";
        return str;
    }
}
