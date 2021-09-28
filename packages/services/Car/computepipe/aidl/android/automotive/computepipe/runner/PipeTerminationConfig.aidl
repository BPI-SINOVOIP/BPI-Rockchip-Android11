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

import android.automotive.computepipe.runner.PipeTerminationConfigTerminationDesc;
import android.automotive.computepipe.runner.PipeTerminationConfigTerminationType;

/**
 * Termination configs
 *
 * Structure that describes the termination options a graph can advertise
 *
 * Provided by HIDL implementation to the client as part of GraphDescriptor
 *
 * The client has the option of choosing one of the provided options supported
 * by the graph or calling calling stopPipe() explicitly.
 */
@VintfStability
parcelable PipeTerminationConfig {
    /**
     * termination option supported by graph.
     */
    PipeTerminationConfigTerminationDesc desc;
    /**
     * identifiers for the option.
     */
    int configId;
}

