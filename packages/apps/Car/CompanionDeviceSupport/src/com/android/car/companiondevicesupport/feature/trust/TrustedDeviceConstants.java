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

package com.android.car.companiondevicesupport.feature.trust;

import android.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Constants for trusted device feature. */
public class TrustedDeviceConstants {

    private TrustedDeviceConstants() { }

    /**
     * Intent extra key for a boolean signalling a new escrow token is being enrolled.
     */
    public static final String INTENT_EXTRA_ENROLL_NEW_TOKEN = "trusted.device.enrolling.token";

    /** Intent action used to start a {@link TrustedDeviceActivity}. */
    public static final String INTENT_ACTION_TRUSTED_DEVICE_SETTING =
            "com.android.car.companiondevicesupport.feature.trust.TRUSTED_DEVICE_ACTIVITY";

    @IntDef(prefix = { "TRUSTED_DEVICE_ERROR_" }, value = {
            TRUSTED_DEVICE_ERROR_MESSAGE_TYPE_UNKNOWN,
            TRUSTED_DEVICE_ERROR_DEVICE_NOT_SECURED,
            TRUSTED_DEVICE_ERROR_UNKNOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    /** Errors that may happen in trusted device enrollment. */
    public @interface TrustedDeviceError {}
    public static final int TRUSTED_DEVICE_ERROR_MESSAGE_TYPE_UNKNOWN = 0;
    public static final int TRUSTED_DEVICE_ERROR_DEVICE_NOT_SECURED = 1;
    public static final int TRUSTED_DEVICE_ERROR_UNKNOWN = 2;
}
