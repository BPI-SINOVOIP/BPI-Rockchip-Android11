/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing perNmissions and
 * limitations under the License.
 */

package android.net;

parcelable NetworkTestResultParcelable {
    /**
     * Timestamp of the evaluation, as determined by to SystemClock.elapsedRealtime().
     */
    long timestampMillis;

    /**
     * Result of the evaluation, as a bitmask of INetworkMonitor.NETWORK_VALIDATION_RESULT_*.
     */
    int result;

    /**
     * List of succeeded probes, as a bitmask of INetworkMonitor.NETWORK_VALIDATION_PROBE_* flags.
     */
    int probesSucceeded;

    /**
     * List of attempted probes, as a bitmask of INetworkMonitor.NETWORK_VALIDATION_PROBE_* flags.
     */
    int probesAttempted;

    /**
     * If the evaluation detected a captive portal, the URL that can be used to login to that
     * portal. Otherwise null.
     */
    String redirectUrl;
}