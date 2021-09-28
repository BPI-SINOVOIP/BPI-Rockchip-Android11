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
package com.android.tradefed.device.cloud;

import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.TestDeviceOptions.InstanceType;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.ZipUtil;

import java.io.File;
import java.io.IOException;
import java.util.List;

/**
 * This utility allows to avoid code duplication across the different remote device representation
 * for the remote log fetching logic of common files.
 */
public class CommonLogRemoteFileUtil {

    /** The directory where to find debug logs for a nested remote instance. */
    public static final String NESTED_REMOTE_LOG_DIR = "/home/%s/cuttlefish_runtime/";
    /** The directory where to find debug logs for an emulator instance. */
    public static final String EMULATOR_REMOTE_LOG_DIR = "/home/%s/log/";
    public static final String TOMBSTONES_ZIP_NAME = "tombstones-zip";

    public static final MultiMap<InstanceType, KnownLogFileEntry> KNOWN_FILES_TO_FETCH =
            new MultiMap<>();

    static {
        // Cuttlefish known files to collect
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.CUTTLEFISH,
                new KnownLogFileEntry(
                        NESTED_REMOTE_LOG_DIR + "kernel.log", null, LogDataType.TEXT));
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.CUTTLEFISH,
                new KnownLogFileEntry(
                        NESTED_REMOTE_LOG_DIR + "logcat", "full_gce_logcat", LogDataType.LOGCAT));
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.CUTTLEFISH,
                new KnownLogFileEntry(
                        NESTED_REMOTE_LOG_DIR + "cuttlefish_config.json", null, LogDataType.TEXT));
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.CUTTLEFISH,
                new KnownLogFileEntry(
                        NESTED_REMOTE_LOG_DIR + "launcher.log",
                        "cuttlefish_launcher.log",
                        LogDataType.TEXT));
        // Emulator known files to collect
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.EMULATOR,
                new KnownLogFileEntry(
                        EMULATOR_REMOTE_LOG_DIR + "logcat.log",
                        "full_gce_emulator_logcat",
                        LogDataType.LOGCAT));
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.EMULATOR,
                new KnownLogFileEntry(EMULATOR_REMOTE_LOG_DIR + "adb.log", null, LogDataType.TEXT));
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.EMULATOR,
                new KnownLogFileEntry(
                        EMULATOR_REMOTE_LOG_DIR + "kernel.log", null, LogDataType.TEXT));
        KNOWN_FILES_TO_FETCH.put(
                InstanceType.EMULATOR,
                new KnownLogFileEntry("/var/log/daemon.log", null, LogDataType.TEXT));
    }

    /** A representation of a known log entry for remote devices. */
    public static class KnownLogFileEntry {
        public String path;
        public String logName;
        public LogDataType type;

        KnownLogFileEntry(String path, String logName, LogDataType type) {
            this.path = path;
            this.logName = logName;
            this.type = type;
        }
    }

    /**
     * Fetch and log the commonly known files from remote instances.
     *
     * @param testLogger The {@link ITestLogger} where to log the files.
     * @param gceAvd The descriptor of the remote instance.
     * @param options The {@link TestDeviceOptions} describing the device options
     * @param runUtil A {@link IRunUtil} to execute commands.
     */
    public static void fetchCommonFiles(
            ITestLogger testLogger,
            GceAvdInfo gceAvd,
            TestDeviceOptions options,
            IRunUtil runUtil) {
        if (gceAvd == null) {
            CLog.e("GceAvdInfo was null, cannot collect remote files.");
            return;
        }
        // Capture known extra files
        List<KnownLogFileEntry> toFetch = KNOWN_FILES_TO_FETCH.get(options.getInstanceType());
        if (toFetch != null) {
            for (KnownLogFileEntry entry : toFetch) {
                LogRemoteFile(
                        testLogger,
                        gceAvd,
                        options,
                        runUtil,
                        // Default fetch rely on main user
                        String.format(entry.path, options.getInstanceUser()),
                        entry.type,
                        entry.logName);
            }
        }

        if (options.getRemoteFetchFilePattern().isEmpty()) {
            return;
        }
        for (String file : options.getRemoteFetchFilePattern()) {
            // TODO: Improve type of files.
            LogRemoteFile(testLogger, gceAvd, options, runUtil, file, LogDataType.TEXT, null);
        }
    }

    /**
     * Fetch and log the tombstones from the remote instance.
     *
     * @param testLogger The {@link ITestLogger} where to log the files.
     * @param gceAvd The descriptor of the remote instance.
     * @param options The {@link TestDeviceOptions} describing the device options
     * @param runUtil A {@link IRunUtil} to execute commands.
     */
    public static void fetchTombstones(
            ITestLogger testLogger,
            GceAvdInfo gceAvd,
            TestDeviceOptions options,
            IRunUtil runUtil) {
        if (gceAvd == null) {
            CLog.e("GceAvdInfo was null, cannot collect remote files.");
            return;
        }
        InstanceType type = options.getInstanceType();
        if (!InstanceType.CUTTLEFISH.equals(type) && !InstanceType.REMOTE_AVD.equals(type)) {
            return;
        }
        String pattern =
                String.format(
                        "/home/%s/cuttlefish_runtime/tombstones/*", options.getInstanceUser());
        CommandResult resultList =
                GceManager.remoteSshCommandExecution(
                        gceAvd, options, runUtil, 60000, "ls", "-A1", pattern);
        if (!CommandStatus.SUCCESS.equals(resultList.getStatus())) {
            CLog.e("Failed to list the tombstones: %s", resultList.getStderr());
            return;
        }
        if (resultList.getStdout().split("\n").length <= 0) {
            return;
        }
        File tombstonesDir =
                RemoteFileUtil.fetchRemoteDir(
                        gceAvd,
                        options,
                        runUtil,
                        120000,
                        String.format(
                                "/home/%s/cuttlefish_runtime/tombstones",
                                options.getInstanceUser()));
        if (tombstonesDir == null) {
            CLog.w("No tombstones directory was pulled.");
            return;
        }
        try {
            File zipTombstones = ZipUtil.createZip(tombstonesDir);
            try (InputStreamSource source = new FileInputStreamSource(zipTombstones, true)) {
                testLogger.testLog(TOMBSTONES_ZIP_NAME, LogDataType.ZIP, source);
            }
        } catch (IOException e) {
            CLog.e("Failed to zip the tombstones:");
            CLog.e(e);
        }
    }

    /**
     * Captures a log from the remote destination.
     *
     * @param testLogger The {@link ITestLogger} where to log the files.
     * @param gceAvd The descriptor of the remote instance.
     * @param options The {@link TestDeviceOptions} describing the device options
     * @param runUtil A {@link IRunUtil} to execute commands.
     * @param fileToRetrieve The remote path to the file to pull.
     * @param logType The expected type of the pulled log.
     * @param baseName The base name that will be used to log the file, if null the actually file
     *     name will be used.
     */
    private static void LogRemoteFile(
            ITestLogger testLogger,
            GceAvdInfo gceAvd,
            TestDeviceOptions options,
            IRunUtil runUtil,
            String fileToRetrieve,
            LogDataType logType,
            String baseName) {
        GceManager.logNestedRemoteFile(
                testLogger, gceAvd, options, runUtil, fileToRetrieve, logType, baseName);
    }
}
