/**
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

package com.android.car.companiondevicesupport.api.external;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;

/** Callback for triggered associated device related events. */
oneway interface IDeviceAssociationCallback {
    /** Triggered when an associated device has been added */
    void onAssociatedDeviceAdded(in AssociatedDevice device);

    /** Triggered when an associated device has been removed.  */
    void onAssociatedDeviceRemoved(in AssociatedDevice device);

    /** Triggered when an associated device has been updated. */
    void onAssociatedDeviceUpdated(in AssociatedDevice device);
}