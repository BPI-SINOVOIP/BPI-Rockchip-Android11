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

/** @hide */
interface IExperimentalCarHelper {
    /**
     * Notify the completion of init to car service.
     * @param allAvailableFeatures All features available in the package.
     * @param startedFeatures Started features. This is a subset of enabledFeatures passed through
     *        IExperimentalCar.init(..). Only available features should be started.
     * @param classNames Car*Manager class names for all started experimental features. Class name
     *        can be null if the feature does not have Car*Manager (=internal feature).
     * @param binders Car*Manager binders for all started experimental features. Binder can be null
     *        if the feature does not have Car*Manager.
     */
    void onInitComplete(in List<String> allAvailableFeatures, in List<String> startedFeatures,
            in List<String> classNames, in List<IBinder> binders);
}
