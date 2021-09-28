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
package com.google.android.chre.test.crossvalidator;

import android.hardware.Sensor;
import android.hardware.SensorEvent;

/*package*/
class ApSensorDatapoint extends SensorDatapoint {

    public Sensor sensor;

    /*
    * This is the AP datapoint ctor. Construct a datapoint using the timestamp and floats observed
    * from Android framework.
    *
    * @param sensorEvent The sensor event that this datapoint info comes from.
    */
    /*package*/
    ApSensorDatapoint(SensorEvent sensorEvent) {
        timestamp = sensorEvent.timestamp;
        values = sensorEvent.values.clone();
        sensor = sensorEvent.sensor;
    }
}
