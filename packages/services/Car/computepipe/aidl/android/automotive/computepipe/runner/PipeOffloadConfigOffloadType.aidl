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

/**
 * Types of offload options for graph computations available to the runner
 * All of the offload options maybe virtualized for different execution
 * environments.
 */
@VintfStability
@Backing(type="int")
enum PipeOffloadConfigOffloadType {
    /**
     * Default cpu only execution
     */
    CPU = 0,
    /**
     * GPU, Open GLES based acceleration
     */
    GPU,
    /**
     * Dedicated neural engine provided by SOC vendor
     */
    NEURAL_ENGINE,
    /**
     * Computer Vision engine provided by SOC vendor
     */
    CV_ENGINE,
}
