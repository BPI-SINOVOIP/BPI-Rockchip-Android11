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

package android.car.experimental;

import android.car.experimental.DriverDistractionChangeEvent;
import android.car.experimental.IDriverDistractionChangeListener;

/** @hide */
interface IDriverDistractionManager {
    /**
     * Gets the current driver distraction level based on the last change event that is not stale.
     */
    DriverDistractionChangeEvent getLastDistractionEvent() = 0;
   /**
    * Registers a listener to be called when the driver's distraction level has changed.
    *
    * Listener immediately receives a callback.
    */
    void addDriverDistractionChangeListener(in IDriverDistractionChangeListener listener) = 1;
    /**
     * Removes the provided listener from receiving callbacks.
     */
    void removeDriverDistractionChangeListener(in IDriverDistractionChangeListener listener) = 2;
}