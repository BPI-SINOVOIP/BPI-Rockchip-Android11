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

parcelable DataStallReportParcelable {
    /**
     * Timestamp of the report, relative to SystemClock.elapsedRealtime().
     */
    long timestampMillis = 0;

    /**
     * Detection method of the data stall, one of DataStallReport.DETECTION_METHOD_*.
     */
    int detectionMethod = 1;

    /**
     * @see android.net.ConnectivityDiagnosticsManager.DataStallReport.KEY_TCP_PACKET_FAIL_RATE.
     * Only set if the detection method is TCP, otherwise 0.
     */
    int tcpPacketFailRate = 2;

    /**
     * @see android.net.ConnectivityDiagnosticsManager.DataStallReport
     *           .KEY_TCP_METRICS_COLLECTION_PERIOD_MILLIS.
     * Only set if the detection method is TCP, otherwise 0.
     */
    int tcpMetricsCollectionPeriodMillis = 3;

    /**
     * @see android.net.ConnectivityDiagnosticsManager.DataStallReport.KEY_DNS_CONSECUTIVE_TIMEOUTS.
     * Only set if the detection method is DNS, otherwise 0.
     */
    int dnsConsecutiveTimeouts = 4;
}