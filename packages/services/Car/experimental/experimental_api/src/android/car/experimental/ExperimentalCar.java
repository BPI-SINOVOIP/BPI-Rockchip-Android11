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

package android.car.experimental;

import android.car.annotation.ExperimentalFeature;

/**
 * Top level class for experimental Car features
 *
 * @hide
 */
public final class ExperimentalCar {
    /**
     * Service for testing experimental feature
     *
     * @hide
     */
    @ExperimentalFeature
    public static final String TEST_EXPERIMENTAL_FEATURE_SERVICE =
            "android.car.experimental.test_demo_experimental_feature_service";

    /**
     * Service for monitoring the driver distraction level.
     *
     * @hide
     */
    @ExperimentalFeature
    public static final String DRIVER_DISTRACTION_EXPERIMENTAL_FEATURE_SERVICE =
            "android.car.experimental.driver_distraction_experimental_feature_service";


    /**
     * Permission necessary to observe the driver distraction level through {@link
     * CarDriverDistractionManager}.
     *
     * @hide
     */
    public static final String PERMISSION_READ_CAR_DRIVER_DISTRACTION =
            "android.car.permission.CAR_DRIVER_DISTRACTION";
}
