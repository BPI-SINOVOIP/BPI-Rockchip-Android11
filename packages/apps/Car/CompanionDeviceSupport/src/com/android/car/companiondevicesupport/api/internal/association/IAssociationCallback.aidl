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

package com.android.car.companiondevicesupport.api.internal.association;

/** Callback for triggered association events. */
oneway interface IAssociationCallback {

    /** Triggered when IHU starts advertising for association successfully. */
    void onAssociationStartSuccess(in String deviceName);

    /** Triggered when IHU failed to start advertising for association. */
    void onAssociationStartFailure();

    /** Triggered when an error has been encountered during assocition with a new device. */
    void onAssociationError(in int error);

    /**  Triggered when a pairing code is available to be present. */
    void onVerificationCodeAvailable(in String code);

    /** Triggered when the assocition has completed */
    void onAssociationCompleted();
}