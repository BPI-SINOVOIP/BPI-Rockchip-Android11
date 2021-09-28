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

package com.android.car.stats;

import android.app.StatsManager;
import android.app.StatsManager.PullAtomMetadata;
import android.content.Context;
import android.content.pm.PackageManager;
import android.util.ArrayMap;
import android.util.Log;
import android.util.StatsEvent;

import com.android.car.CarStatsLog;
import com.android.car.stats.VmsClientLogger.ConnectionState;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.util.ConcurrentUtils;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.function.Consumer;
import java.util.function.Function;

/**
 * Registers pulled atoms with statsd via StatsManager.
 *
 * Also implements collection and dumpsys reporting of atoms in CSV format.
 */
public class CarStatsService {
    private static final boolean DEBUG = false;
    private static final String TAG = "CarStatsService";
    private static final String VMS_CONNECTION_STATS_DUMPSYS_HEADER =
            "uid,packageName,attempts,connected,disconnected,terminated,errors";

    private static final Function<VmsClientLogger, String> VMS_CONNECTION_STATS_DUMPSYS_FORMAT =
            entry -> String.format(Locale.US,
                    "%d,%s,%d,%d,%d,%d,%d",
                    entry.getUid(), entry.getPackageName(),
                    entry.getConnectionStateCount(ConnectionState.CONNECTING),
                    entry.getConnectionStateCount(ConnectionState.CONNECTED),
                    entry.getConnectionStateCount(ConnectionState.DISCONNECTED),
                    entry.getConnectionStateCount(ConnectionState.TERMINATED),
                    entry.getConnectionStateCount(ConnectionState.CONNECTION_ERROR));

    private static final String VMS_CLIENT_STATS_DUMPSYS_HEADER =
            "uid,layerType,layerChannel,layerVersion,"
                    + "txBytes,txPackets,rxBytes,rxPackets,droppedBytes,droppedPackets";

    private static final Function<VmsClientStats, String> VMS_CLIENT_STATS_DUMPSYS_FORMAT =
            entry -> String.format(
                    "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                    entry.getUid(),
                    entry.getLayerType(), entry.getLayerChannel(), entry.getLayerVersion(),
                    entry.getTxBytes(), entry.getTxPackets(),
                    entry.getRxBytes(), entry.getRxPackets(),
                    entry.getDroppedBytes(), entry.getDroppedPackets());

    private static final Comparator<VmsClientStats> VMS_CLIENT_STATS_ORDER =
            Comparator.comparingInt(VmsClientStats::getUid)
                    .thenComparingInt(VmsClientStats::getLayerType)
                    .thenComparingInt(VmsClientStats::getLayerChannel)
                    .thenComparingInt(VmsClientStats::getLayerVersion);

    private final Context mContext;
    private final PackageManager mPackageManager;
    private final StatsManager mStatsManager;

    @GuardedBy("mVmsClientStats")
    private final Map<Integer, VmsClientLogger> mVmsClientStats = new ArrayMap<>();

    public CarStatsService(Context context) {
        mContext = context;
        mPackageManager = context.getPackageManager();
        mStatsManager = (StatsManager) mContext.getSystemService(Context.STATS_MANAGER);
    }

    /**
     * Registers VmsClientStats puller with StatsManager.
     */
    public void init() {
        PullAtomMetadata metadata = new PullAtomMetadata.Builder()
                .setAdditiveFields(new int[] {5, 6, 7, 8, 9, 10})
                .build();
        mStatsManager.setPullAtomCallback(
                CarStatsLog.VMS_CLIENT_STATS,
                metadata,
                ConcurrentUtils.DIRECT_EXECUTOR,
                (atomTag, data) -> pullVmsClientStats(atomTag, data)
        );
    }

    /**
     * Gets a logger for the VMS client with a given UID.
     */
    public VmsClientLogger getVmsClientLogger(int clientUid) {
        synchronized (mVmsClientStats) {
            return mVmsClientStats.computeIfAbsent(
                    clientUid,
                    uid -> {
                        String packageName = mPackageManager.getNameForUid(uid);
                        if (DEBUG) {
                            Log.d(TAG, "Created VmsClientLog: " + packageName);
                        }
                        return new VmsClientLogger(uid, packageName);
                    });
        }
    }

    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        List<String> flags = Arrays.asList(args);
        if (args.length == 0 || flags.contains("--vms-client")) {
            dumpVmsStats(writer);
        }
    }

    private void dumpVmsStats(PrintWriter writer) {
        synchronized (mVmsClientStats) {
            writer.println(VMS_CONNECTION_STATS_DUMPSYS_HEADER);
            mVmsClientStats.values().stream()
                    // Unknown UID will not have connection stats
                    .filter(entry -> entry.getUid() > 0)
                    // Sort stats by UID
                    .sorted(Comparator.comparingInt(VmsClientLogger::getUid))
                    .forEachOrdered(entry -> writer.println(
                            VMS_CONNECTION_STATS_DUMPSYS_FORMAT.apply(entry)));
            writer.println();

            writer.println(VMS_CLIENT_STATS_DUMPSYS_HEADER);
            dumpVmsClientStats(entry -> writer.println(
                    VMS_CLIENT_STATS_DUMPSYS_FORMAT.apply(entry)));
        }
    }

    private int pullVmsClientStats(int atomTag, List<StatsEvent> pulledData) {
        if (atomTag != CarStatsLog.VMS_CLIENT_STATS) {
            Log.w(TAG, "Unexpected atom tag: " + atomTag);
            return StatsManager.PULL_SKIP;
        }

        dumpVmsClientStats((entry) -> {
            StatsEvent e = StatsEvent.newBuilder()
                    .setAtomId(atomTag)
                    .writeInt(entry.getUid())
                    .addBooleanAnnotation(CarStatsLog.ANNOTATION_ID_IS_UID, true)

                    .writeInt(entry.getLayerType())
                    .writeInt(entry.getLayerChannel())
                    .writeInt(entry.getLayerVersion())

                    .writeLong(entry.getTxBytes())
                    .writeLong(entry.getTxPackets())
                    .writeLong(entry.getRxBytes())
                    .writeLong(entry.getRxPackets())
                    .writeLong(entry.getDroppedBytes())
                    .writeLong(entry.getDroppedPackets())
                    .build();
            pulledData.add(e);
        });
        return StatsManager.PULL_SUCCESS;
    }

    private void dumpVmsClientStats(Consumer<VmsClientStats> dumpFn) {
        synchronized (mVmsClientStats) {
            mVmsClientStats.values().stream()
                    .flatMap(log -> log.getLayerEntries().stream())
                    .sorted(VMS_CLIENT_STATS_ORDER)
                    .forEachOrdered(dumpFn);
        }
    }
}
