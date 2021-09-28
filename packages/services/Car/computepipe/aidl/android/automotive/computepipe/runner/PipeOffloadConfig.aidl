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

import android.automotive.computepipe.runner.PipeOffloadConfigOffloadType;
import android.automotive.computepipe.runner.PipeOffloadConfigOffloadDesc;

/**
 * Offload configs
 *
 * Structure that describes the offload options that a graph can use.
 * This is determined at graph creation time by the developer.
 * A graph can advertise different combinations of offload options that it
 * can use. A client can choose amongst the combinations of offload options, for
 * any given iteration of the graph execution.
 *
 * This is provided by the HIDL implementation to the client
 */
@VintfStability
parcelable PipeOffloadConfig {
    /**
     * Offload descriptor that the graph can support.
     */
    PipeOffloadConfigOffloadDesc desc;
    /**
     * identifier for the option.
     */
    String configId;
}

