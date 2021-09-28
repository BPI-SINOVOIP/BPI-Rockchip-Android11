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

import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.DriverAwarenessSupplierConfig;

/**
 * Binder callback for IDriverAwarenessSupplier.
 *
 * @hide
 */
interface IDriverAwarenessSupplierCallback {
   /**
    * Sets the awareness level for the driver. Determining sufficient data quality is the
    * responsibility of the AwarenessSupplier and events with insufficient data quality should not
    * be sent.
    *
    * <p>Suppliers could crash in the background or fail to send continuously high data and
    * therefore should push events, even if the Awareness level hasn't changed, with a frequency
    * greater than their specified AwarenessSupplier#getMaxStalenessMillis().
    *
    * <p>Should be called once when first registered.
    *
    * @param event a snapshot of the driver's awareness at a certain point in time.
    */
   void onDriverAwarenessUpdated(in DriverAwarenessEvent event) = 0;

   /**
    * Sends the configuration for IDriverAwarenessSupplier configuration that this is a callback
    * for.
    *
    * @param config for the IDriverAwarenessSupplier
    */
   void onConfigLoaded(in DriverAwarenessSupplierConfig config) = 1;
}
