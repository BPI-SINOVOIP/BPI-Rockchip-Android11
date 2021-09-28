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

package com.android.networkstack.metrics;

import android.net.captiveportal.CaptivePortalProbeResult;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.internal.util.HexDump;
import com.android.server.connectivity.nano.DataStallEventProto;

/**
 * Collection of utilities for data stall metrics.
 *
 * To see if the logs are properly sent to statsd, execute following command.
 *
 * $ adb shell cmd stats print-logs
 * $ adb logcat | grep statsd  OR $ adb logcat -b stats
 *
 * @hide
 */
public class DataStallStatsUtils {
    private static final String TAG = DataStallStatsUtils.class.getSimpleName();
    private static final boolean DBG = false;

    /**
     * Map {@link CaptivePortalProbeResult} to {@link DataStallEventProto}.
     */
    @VisibleForTesting
    public static int probeResultToEnum(@Nullable final CaptivePortalProbeResult result) {
        if (result == null) return DataStallEventProto.INVALID;

        if (result.isSuccessful()) {
            return DataStallEventProto.VALID;
        } else if (result.isPortal()) {
            return DataStallEventProto.PORTAL;
        } else if (result.isPartialConnectivity()) {
            return DataStallEventProto.PARTIAL;
        } else {
            return DataStallEventProto.INVALID;
        }
    }

    /**
     * Write the metric to {@link StatsLog}.
     */
    public static void write(@NonNull final DataStallDetectionStats stats,
            @NonNull final CaptivePortalProbeResult result) {
        int validationResult = probeResultToEnum(result);
        if (DBG) {
            Log.d(TAG, "write: " + stats + " with result: " + validationResult
                    + ", dns: " + HexDump.toHexString(stats.mDns));
        }
        NetworkStackStatsLog.write(NetworkStackStatsLog.DATA_STALL_EVENT,
                stats.mEvaluationType,
                validationResult,
                stats.mNetworkType,
                stats.mWifiInfo,
                stats.mCellularInfo,
                stats.mDns,
                stats.mTcpFailRate,
                stats.mTcpSentSinceLastRecv);
    }
}
