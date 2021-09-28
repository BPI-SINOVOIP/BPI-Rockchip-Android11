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

import com.google.android.chre.nanoapp.proto.ChreCrossValidationSensor;
import com.google.common.primitives.Floats;

/*package*/
class ChreSensorDatapoint extends SensorDatapoint {

    /*
    * This is the CHRE datapoint ctor. Construct datapoint using a timestamp and float values
    * that were collected by CHRE.
    *
    *
    * @param datapoint The chre cross validation proto message for datapoint from CHRE.
    */
    /*package*/
    ChreSensorDatapoint(ChreCrossValidationSensor.SensorDatapoint datapoint) {
        timestamp = datapoint.getTimestampInNs();
        values = Floats.toArray(datapoint.getValuesList());
    }
}
