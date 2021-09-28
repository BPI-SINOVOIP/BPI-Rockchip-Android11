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

import android.car.experimental.IDriverAwarenessSupplierCallback;

/**
 * Binder API for a driver awareness supplier.
 *
 * @hide
 */
oneway interface IDriverAwarenessSupplier {

    /** Called once the distraction service is ready to receive events */
    void onReady() = 0;

    /** Set the callback to be used for this supplier */
    void setCallback(IDriverAwarenessSupplierCallback callback) = 1;
}