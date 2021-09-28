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
package android.automotive.computepipe.runner;

import android.automotive.computepipe.runner.PipeOutputConfig;
import android.automotive.computepipe.runner.PipeTerminationConfigTerminationType;

/**
 * structure that describes the different termination options supported
 * by the graph
 */
@VintfStability
parcelable PipeTerminationConfigTerminationDesc {
    /**
     * type of termination criteria
     */
    PipeTerminationConfigTerminationType type;
    /**
     * type based qualifier, could be run time, packet count, or usecase
     * specific event identifier.
     */
    int qualifier;
    /**
     * Output stream config. This will only be used when type is MAX_PACKET_COUNT
     */
     PipeOutputConfig streamConfig;
}

