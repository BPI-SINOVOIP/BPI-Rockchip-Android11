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
package android.automotive.computepipe.runner;

/**
 * State of the remote graph
 */
@VintfStability
@Backing(type="int")
enum PipeInputConfigCameraType {
    /**
     * Driver focused Camera stream
     */
    DRIVER_VIEW_CAMERA = 0,
    /**
     * Camera with wider field of view that can capture
     * occupants in the car.
     */
    OCCUPANT_VIEW_CAMERA,
    /**
     * External Camera
     */
    EXTERNAL_CAMERA,
    /**
     * Surround view
     */
    SURROUND_VIEW_CAMERA,
}
