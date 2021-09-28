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

package com.android.car.companiondevicesupport.feature.trust;

import static com.android.car.connecteddevice.util.SafeLog.logi;

/** Logging class for collecting metrics related to the Trusted Device feature. */
class TrustedDeviceEventLog {

    private static final String TAG = "TrustedDeviceEvent";

    private TrustedDeviceEventLog() { }

    /** Mark in the log that the {@link TrustedDeviceManagerService} has started. */
    static void onTrustedDeviceServiceStarted() {
        logi(TAG, "SERVICE_STARTED");
    }

    /** Mark in the log that the {@link TrustedDeviceAgentService} has started. */
    static void onTrustAgentStarted() {
        logi(TAG, "TRUST_AGENT_STARTED");
    }

    /** Mark in the log that credentials were received from the device. */
    static void onCredentialsReceived() {
        logi(TAG, "CREDENTIALS_RECEIVED");
    }

    /** Mark in the log that the user successfully unlocked. */
    static void onUserUnlocked() {
        logi(TAG, "USER_UNLOCKED");
    }
}
