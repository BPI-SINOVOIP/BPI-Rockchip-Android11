/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.cts.devicepolicy.metrics;

import static junit.framework.Assert.assertTrue;

import com.android.internal.os.StatsdConfigProto.AtomMatcher;
import com.android.internal.os.StatsdConfigProto.EventMetric;
import com.android.internal.os.StatsdConfigProto.FieldValueMatcher;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsLogReport;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.google.common.io.Files;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;
import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.function.Predicate;
import java.util.stream.Collectors;

/**
 * Tests Statsd atoms.
 * <p/>
 * Uploads statsd event configs, retrieves logs from host side and validates them
 * against specified criteria.
 */
class AtomMetricTester {
    private static final String UPDATE_CONFIG_CMD = "cat %s | cmd stats config update %d";
    private static final String DUMP_REPORT_CMD =
            "cmd stats dump-report %d --include_current_bucket --proto";
    private static final String REMOVE_CONFIG_CMD = "cmd stats config remove %d";
    /** ID of the config, which evaluates to -1572883457. */
    private static final long CONFIG_ID = "cts_config".hashCode();

    private final ITestDevice mDevice;

    AtomMetricTester(ITestDevice device) {
        mDevice = device;
    }

    void cleanLogs() throws Exception {
        if (isStatsdDisabled()) {
            return;
        }
        removeConfig(CONFIG_ID);
        getReportList(); // Clears data.
    }

    private static StatsdConfig.Builder createConfigBuilder() {
        return StatsdConfig.newBuilder().setId(CONFIG_ID)
                .addAllowedLogSource("AID_SYSTEM");
    }

    void createAndUploadConfig(int atomTag) throws Exception {
        StatsdConfig.Builder conf = createConfigBuilder();
        addAtomEvent(conf, atomTag);
        uploadConfig(conf);
    }

    private void uploadConfig(StatsdConfig.Builder config) throws Exception {
        uploadConfig(config.build());
    }

    private void uploadConfig(StatsdConfig config) throws Exception {
        CLog.d("Uploading the following config:\n" + config.toString());
        File configFile = File.createTempFile("statsdconfig", ".config");
        configFile.deleteOnExit();
        Files.write(config.toByteArray(), configFile);
        String remotePath = "/data/local/tmp/" + configFile.getName();
        mDevice.pushFile(configFile, remotePath);
        mDevice.executeShellCommand(String.format(UPDATE_CONFIG_CMD, remotePath, CONFIG_ID));
        mDevice.executeShellCommand("rm " + remotePath);
    }

    private void removeConfig(long configId) throws Exception {
        mDevice.executeShellCommand(String.format(REMOVE_CONFIG_CMD, configId));
    }

    /**
     * Gets the statsd report and sorts it.
     * Note that this also deletes that report from statsd.
     */
    List<EventMetricData> getEventMetricDataList() throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        return getEventMetricDataList(reportList);
    }

    /**
     * Extracts and sorts the EventMetricData from the given ConfigMetricsReportList (which must
     * contain a single report).
     */
    private List<EventMetricData> getEventMetricDataList(ConfigMetricsReportList reportList)
            throws Exception {
        assertTrue("Expected one report", reportList.getReportsCount() == 1);
        final ConfigMetricsReport report = reportList.getReports(0);
        final List<StatsLogReport> metricsList = report.getMetricsList();
        return metricsList.stream()
                .flatMap(statsLogReport -> statsLogReport.getEventMetrics().getDataList().stream())
                .sorted(Comparator.comparing(EventMetricData::getElapsedTimestampNanos))
                .peek(eventMetricData -> {
                    CLog.d("Atom at " + eventMetricData.getElapsedTimestampNanos()
                            + ":\n" + eventMetricData.getAtom().toString());
                })
                .collect(Collectors.toList());
    }

    /** Gets the statsd report. Note that this also deletes that report from statsd. */
    private ConfigMetricsReportList getReportList() throws Exception {
        try {
            return getDump(ConfigMetricsReportList.parser(),
                    String.format(DUMP_REPORT_CMD, CONFIG_ID));
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            CLog.e("Failed to fetch and parse the statsd output report. "
                    + "Perhaps there is not a valid statsd config for the requested "
                    + "uid=" + getHostUid() + ", id=" + CONFIG_ID + ".");
            throw (e);
        }
    }

    /** Creates a FieldValueMatcher.Builder corresponding to the given field. */
    private static FieldValueMatcher.Builder createFvm(int field) {
        return FieldValueMatcher.newBuilder().setField(field);
    }

    private void addAtomEvent(StatsdConfig.Builder conf, int atomTag) throws Exception {
        addAtomEvent(conf, atomTag, new ArrayList<FieldValueMatcher.Builder>());
    }

    /**
     * Adds an event to the config for an atom that matches the given keys.
     *
     * @param conf   configuration
     * @param atomTag atom tag (from atoms.proto)
     * @param fvms   list of FieldValueMatcher.Builders to attach to the atom. May be null.
     */
    private void addAtomEvent(StatsdConfig.Builder conf, int atomTag,
            List<FieldValueMatcher.Builder> fvms) throws Exception {

        final String atomName = "Atom" + System.nanoTime();
        final String eventName = "Event" + System.nanoTime();

        SimpleAtomMatcher.Builder sam = SimpleAtomMatcher.newBuilder().setAtomId(atomTag);
        if (fvms != null) {
            for (FieldValueMatcher.Builder fvm : fvms) {
                sam.addFieldValueMatcher(fvm);
            }
        }
        conf.addAtomMatcher(AtomMatcher.newBuilder()
                .setId(atomName.hashCode())
                .setSimpleAtomMatcher(sam));
        conf.addEventMetric(EventMetric.newBuilder()
                .setId(eventName.hashCode())
                .setWhat(atomName.hashCode()));
    }

    /**
     * Removes all elements from data prior to the first occurrence of an element for which
     * the <code>atomMatcher</code> predicate returns <code>true</code>.
     * After this method is called, the first element of data (if non-empty) is guaranteed to be
     * an element in state.
     *
     * @param atomMatcher predicate that takes an Atom and returns <code>true</code> if it
     * fits criteria.
     */
    static void dropWhileNot(List<EventMetricData> metricData, Predicate<Atom> atomMatcher) {
        int firstStateIdx;
        for (firstStateIdx = 0; firstStateIdx < metricData.size(); firstStateIdx++) {
            final Atom atom = metricData.get(firstStateIdx).getAtom();
            if (atomMatcher.test(atom)) {
                break;
            }
        }
        if (firstStateIdx == 0) {
            // First first element already is in state, so there's nothing to do.
            return;
        }
        metricData.subList(0, firstStateIdx).clear();
    }

    /** Returns the UID of the host, which should always either be SHELL (2000) or ROOT (0). */
    private int getHostUid() throws DeviceNotAvailableException {
        String strUid = "";
        try {
            strUid = mDevice.executeShellCommand("id -u");
            return Integer.parseInt(strUid.trim());
        } catch (NumberFormatException e) {
            CLog.e("Failed to get host's uid via shell command. Found " + strUid);
            // Fall back to alternative method...
            if (mDevice.isAdbRoot()) {
                return 0; // ROOT
            } else {
                return 2000; // SHELL
            }
        }
    }

    /**
     * Execute a shell command on device and get the results of
     * that as a proto of the given type.
     *
     * @param parser A protobuf parser object. e.g. MyProto.parser()
     * @param command The adb shell command to run. e.g. "dumpsys fingerprint --proto"
     *
     * @throws DeviceNotAvailableException If there was a problem communicating with
     *      the test device.
     * @throws InvalidProtocolBufferException If there was an error parsing
     *      the proto. Note that a 0 length buffer is not necessarily an error.
     */
    private <T extends MessageLite> T getDump(Parser<T> parser, String command)
            throws DeviceNotAvailableException, InvalidProtocolBufferException {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        mDevice.executeShellCommand(command, receiver);
        return parser.parseFrom(receiver.getOutput());
    }

    boolean isStatsdDisabled() throws DeviceNotAvailableException {
        // if ro.statsd.enable doesn't exist, statsd is running by default.
        if ("false".equals(mDevice.getProperty("ro.statsd.enable"))
                && "true".equals(mDevice.getProperty("ro.config.low_ram"))) {
            CLog.d("Statsd is not enabled on the device");
            return true;
        }
        return false;
    }
}