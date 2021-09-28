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

package android.car;

import android.car.IExperimentalCarHelper;

/** @hide */
oneway interface IExperimentalCar {
    /**
     * Initialize the experimental car service.
     * @param helper After init, experimental car service should return binder, class names for
     *               enabled features with all features available through helper.onInitComplete().
     * @param enabledFeatures Currently enabled features. If this feature is not available any more,
     *                        helper.onInitComplete should drop it from started features.
     */
    void init(in IExperimentalCarHelper helper, in List<String> enabledFeatures);
}
