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

import android.automotive.computepipe.runner.PipeInputConfig;
import android.automotive.computepipe.runner.PipeOffloadConfig;
import android.automotive.computepipe.runner.PipeTerminationConfig;
import android.automotive.computepipe.runner.PipeOutputConfig;

/**
 * Pipe Descriptor
 *
 * This is the descriptor for use case that describes all the supported
 * a) input options
 * b) termination options
 * c) offload options
 * d) output streams
 *
 * This is returned by the HIDL implementation to the client to describe a given graph.
 * The client selects the config for a given graph run from the available
 * choices advertised. Note the output stream that a client wants to subscribes
 * to, require the client to subscribe to each stream individually.
 *
 * This descriptor is returned by the HAL to the client.
 */
@VintfStability
parcelable PipeDescriptor {
    /**
     * input configurations supported by the graph.
     */
    PipeInputConfig[] inputConfig;
    /**
     * Offload options supported by the graph.
     */
    PipeOffloadConfig[] offloadConfig;
    /**
     * Termination options supported by the graph.
     */
    PipeTerminationConfig[] terminationConfig;
    /**
     * Output streams supported by the graph.
     */
    PipeOutputConfig[] outputConfig;
}

