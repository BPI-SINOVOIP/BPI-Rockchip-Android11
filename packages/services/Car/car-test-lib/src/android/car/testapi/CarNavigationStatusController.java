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

package android.car.testapi;

import android.os.Bundle;

/**
 * Controller to manipulate and verify {@link android.car.navigation.CarNavigationStatusManager} in
 * unit tests.
 */
public interface CarNavigationStatusController {
    /** Returns the last state sent to the manager. */
    Bundle getCurrentNavState();

    /** Sets the current internal cluster state to use a cluster that can accept custom images. */
    void setCustomImageClusterInfo(
            int minIntervalMillis,
            int imageWidth,
            int imageHeight,
            int imageColorDepthBits);

    /** Sets the current internal cluster state to use a cluster that can accept image codes. */
    void setImageCodeClusterInfo(int minIntervalMillis);
}
