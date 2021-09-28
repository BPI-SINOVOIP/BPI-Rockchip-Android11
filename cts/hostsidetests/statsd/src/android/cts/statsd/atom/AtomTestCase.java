/*
 * Copyright (C) 2017 The Android Open Source Project
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
package android.cts.statsd.atom;

import static android.cts.statsd.atom.DeviceAtomTestCase.DEVICE_SIDE_TEST_APK;
import static android.cts.statsd.atom.DeviceAtomTestCase.DEVICE_SIDE_TEST_PACKAGE;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.os.BatteryStatsProto;
import android.os.StatsDataDumpProto;
import android.service.battery.BatteryServiceDumpProto;
import android.service.batterystats.BatteryStatsServiceDumpProto;
import android.service.procstats.ProcessStatsServiceDumpProto;

import com.android.annotations.Nullable;
import com.android.internal.os.StatsdConfigProto.AtomMatcher;
import com.android.internal.os.StatsdConfigProto.EventMetric;
import com.android.internal.os.StatsdConfigProto.FieldFilter;
import com.android.internal.os.StatsdConfigProto.FieldMatcher;
import com.android.internal.os.StatsdConfigProto.FieldValueMatcher;
import com.android.internal.os.StatsdConfigProto.GaugeMetric;
import com.android.internal.os.StatsdConfigProto.Predicate;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.SimplePredicate;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.internal.os.StatsdConfigProto.TimeUnit;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.ProcessStatsPackageProto;
import com.android.os.AtomsProto.ProcessStatsProto;
import com.android.os.AtomsProto.ProcessStatsStateProto;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.DurationMetricData;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.GaugeBucketInfo;
import com.android.os.StatsLog.GaugeMetricData;
import com.android.os.StatsLog.CountMetricData;
import com.android.os.StatsLog.StatsLogReport;
import com.android.os.StatsLog.ValueMetricData;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import com.google.common.collect.Range;
import com.google.common.io.Files;
import com.google.protobuf.ByteString;

import perfetto.protos.PerfettoConfig.DataSourceConfig;
import perfetto.protos.PerfettoConfig.FtraceConfig;
import perfetto.protos.PerfettoConfig.TraceConfig;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.Random;
import java.util.Set;
import java.util.function.Function;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

/**
 * Base class for testing Statsd atoms.
 * Validates reporting of statsd logging based on different events
 */
public class AtomTestCase extends BaseTestCase {

    /**
     * Run tests that are optional; they are not valid CTS tests per se, since not all devices can
     * be expected to pass them, but can be run, if desired, to ensure they work when appropriate.
     */
    public static final boolean OPTIONAL_TESTS_ENABLED = false;

    public static final String UPDATE_CONFIG_CMD = "cmd stats config update";
    public static final String DUMP_REPORT_CMD = "cmd stats dump-report";
    public static final String DUMP_BATTERY_CMD = "dumpsys battery";
    public static final String DUMP_BATTERYSTATS_CMD = "dumpsys batterystats";
    public static final String DUMPSYS_STATS_CMD = "dumpsys stats";
    public static final String DUMP_PROCSTATS_CMD = "dumpsys procstats";
    public static final String REMOVE_CONFIG_CMD = "cmd stats config remove";
    /** ID of the config, which evaluates to -1572883457. */
    public static final long CONFIG_ID = "cts_config".hashCode();

    public static final String FEATURE_AUDIO_OUTPUT = "android.hardware.audio.output";
    public static final String FEATURE_AUTOMOTIVE = "android.hardware.type.automotive";
    public static final String FEATURE_BLUETOOTH = "android.hardware.bluetooth";
    public static final String FEATURE_BLUETOOTH_LE = "android.hardware.bluetooth_le";
    public static final String FEATURE_CAMERA = "android.hardware.camera";
    public static final String FEATURE_CAMERA_FLASH = "android.hardware.camera.flash";
    public static final String FEATURE_CAMERA_FRONT = "android.hardware.camera.front";
    public static final String FEATURE_LEANBACK_ONLY = "android.software.leanback_only";
    public static final String FEATURE_LOCATION_GPS = "android.hardware.location.gps";
    public static final String FEATURE_PC = "android.hardware.type.pc";
    public static final String FEATURE_PICTURE_IN_PICTURE = "android.software.picture_in_picture";
    public static final String FEATURE_TELEPHONY = "android.hardware.telephony";
    public static final String FEATURE_WATCH = "android.hardware.type.watch";
    public static final String FEATURE_WIFI = "android.hardware.wifi";
    public static final String FEATURE_INCREMENTAL_DELIVERY =
            "android.software.incremental_delivery";

    // Telephony phone types
    public static final int PHONE_TYPE_GSM = 1;
    public static final int PHONE_TYPE_CDMA = 2;
    public static final int PHONE_TYPE_CDMA_LTE = 6;

    protected static final int WAIT_TIME_SHORT = 500;
    protected static final int WAIT_TIME_LONG = 2_000;

    protected static final long SCREEN_STATE_CHANGE_TIMEOUT = 4000;
    protected static final long SCREEN_STATE_POLLING_INTERVAL = 500;

    protected static final long NS_PER_SEC = (long) 1E+9;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Uninstall to clear the history in case it's still on the device.
        removeConfig(CONFIG_ID);
        getReportList(); // Clears data.
    }

    @Override
    protected void tearDown() throws Exception {
        removeConfig(CONFIG_ID);
        getDevice().uninstallPackage(DEVICE_SIDE_TEST_PACKAGE);
        super.tearDown();
    }

    /**
     * Determines whether logcat indicates that incidentd fired since the given device date.
     */
    protected boolean didIncidentdFireSince(String date) throws Exception {
        final String INCIDENTD_TAG = "incidentd";
        final String INCIDENTD_STARTED_STRING = "reportIncident";
        // TODO: Do something more robust than this in case of delayed logging.
        Thread.sleep(1000);
        String log = getLogcatSince(date, String.format(
                "-s %s -e %s", INCIDENTD_TAG, INCIDENTD_STARTED_STRING));
        return log.contains(INCIDENTD_STARTED_STRING);
    }

    protected boolean checkDeviceFor(String methodName) throws Exception {
        try {
            installPackage(DEVICE_SIDE_TEST_APK, true);
            runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".Checkers", methodName);
            // Test passes, meaning that the answer is true.
            LogUtil.CLog.d(methodName + "() indicates true.");
            return true;
        } catch (AssertionError e) {
            // Method is designed to fail if the answer is false.
            LogUtil.CLog.d(methodName + "() indicates false.");
            return false;
        }
    }

    /**
     * Returns a protobuf-encoded perfetto config that enables the kernel
     * ftrace tracer with sched_switch for 10 seconds.
     */
    protected ByteString getPerfettoConfig() {
        TraceConfig.Builder builder = TraceConfig.newBuilder();

        TraceConfig.BufferConfig buffer = TraceConfig.BufferConfig
            .newBuilder()
            .setSizeKb(128)
            .build();
        builder.addBuffers(buffer);

        FtraceConfig ftraceConfig = FtraceConfig.newBuilder()
            .addFtraceEvents("sched/sched_switch")
            .build();
        DataSourceConfig dataSourceConfig = DataSourceConfig.newBuilder()
            .setName("linux.ftrace")
            .setTargetBuffer(0)
            .setFtraceConfig(ftraceConfig)
            .build();
        TraceConfig.DataSource dataSource = TraceConfig.DataSource
            .newBuilder()
            .setConfig(dataSourceConfig)
            .build();
        builder.addDataSources(dataSource);

        builder.setDurationMs(10000);
        builder.setAllowUserBuildTracing(true);

        // To avoid being hit with guardrails firing in multiple test runs back
        // to back, we set a unique session key for each config.
        Random random = new Random();
        StringBuilder sessionNameBuilder = new StringBuilder("statsd-cts-");
        sessionNameBuilder.append(random.nextInt() & Integer.MAX_VALUE);
        builder.setUniqueSessionName(sessionNameBuilder.toString());

        return builder.build().toByteString();
    }

    /**
     * Resets the state of the Perfetto guardrails. This avoids that the test fails if it's
     * run too close of for too many times and hits the upload limit.
     */
    protected void resetPerfettoGuardrails() throws Exception {
        final String cmd = "perfetto --reset-guardrails";
        CommandResult cr = getDevice().executeShellV2Command(cmd);
        if (cr.getStatus() != CommandStatus.SUCCESS)
            throw new Exception(String.format("Error while executing %s: %s %s", cmd, cr.getStdout(), cr.getStderr()));
    }

    private String probe(String path) throws Exception {
        return getDevice().executeShellCommand("if [ -e " + path + " ] ; then"
                + " cat " + path + " ; else echo -1 ; fi");
    }

    /**
     * Determines whether perfetto enabled the kernel ftrace tracer.
     */
    protected boolean isSystemTracingEnabled() throws Exception {
        final String traceFsPath = "/sys/kernel/tracing/tracing_on";
        String tracing_on = probe(traceFsPath);
        if (tracing_on.startsWith("0")) return false;
        if (tracing_on.startsWith("1")) return true;

        // fallback to debugfs
        LogUtil.CLog.d("Unexpected state for %s = %s. Falling back to debugfs", traceFsPath,
                tracing_on);

        final String debugFsPath = "/sys/kernel/debug/tracing/tracing_on";
        tracing_on = probe(debugFsPath);
        if (tracing_on.startsWith("0")) return false;
        if (tracing_on.startsWith("1")) return true;
        throw new Exception(String.format("Unexpected state for %s = %s", traceFsPath, tracing_on));
    }

    protected static StatsdConfig.Builder createConfigBuilder() {
      return StatsdConfig.newBuilder()
          .setId(CONFIG_ID)
          .addAllowedLogSource("AID_SYSTEM")
          .addAllowedLogSource("AID_BLUETOOTH")
          // TODO(b/134091167): Fix bluetooth source name issue in Auto platform.
          .addAllowedLogSource("com.android.bluetooth")
          .addAllowedLogSource("AID_LMKD")
          .addAllowedLogSource("AID_RADIO")
          .addAllowedLogSource("AID_ROOT")
          .addAllowedLogSource("AID_STATSD")
          .addAllowedLogSource(DeviceAtomTestCase.DEVICE_SIDE_TEST_PACKAGE)
          .addDefaultPullPackages("AID_RADIO")
          .addDefaultPullPackages("AID_SYSTEM")
          .addWhitelistedAtomIds(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER);
    }

    protected void createAndUploadConfig(int atomTag) throws Exception {
        StatsdConfig.Builder conf = createConfigBuilder();
        addAtomEvent(conf, atomTag);
        uploadConfig(conf);
    }

    protected void uploadConfig(StatsdConfig.Builder config) throws Exception {
        uploadConfig(config.build());
    }

    protected void uploadConfig(StatsdConfig config) throws Exception {
        LogUtil.CLog.d("Uploading the following config:\n" + config.toString());
        File configFile = File.createTempFile("statsdconfig", ".config");
        configFile.deleteOnExit();
        Files.write(config.toByteArray(), configFile);
        String remotePath = "/data/local/tmp/" + configFile.getName();
        getDevice().pushFile(configFile, remotePath);
        getDevice().executeShellCommand(
                String.join(" ", "cat", remotePath, "|", UPDATE_CONFIG_CMD,
                        String.valueOf(CONFIG_ID)));
        getDevice().executeShellCommand("rm " + remotePath);
    }

    protected void removeConfig(long configId) throws Exception {
        getDevice().executeShellCommand(
                String.join(" ", REMOVE_CONFIG_CMD, String.valueOf(configId)));
    }

    /** Gets the statsd report and sorts it. Note that this also deletes that report from statsd. */
    protected List<EventMetricData> getEventMetricDataList() throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        return getEventMetricDataList(reportList);
    }

    /**
     *  Gets a List of sorted ConfigMetricsReports from ConfigMetricsReportList.
     */
    protected List<ConfigMetricsReport> getSortedConfigMetricsReports(
            ConfigMetricsReportList configMetricsReportList) {
        return configMetricsReportList.getReportsList().stream()
                .sorted(Comparator.comparing(ConfigMetricsReport::getCurrentReportWallClockNanos))
                .collect(Collectors.toList());
    }

    /**
     * Extracts and sorts the EventMetricData from the given ConfigMetricsReportList (which must
     * contain a single report).
     */
    protected List<EventMetricData> getEventMetricDataList(ConfigMetricsReportList reportList)
            throws Exception {
        assertThat(reportList.getReportsCount()).isEqualTo(1);
        ConfigMetricsReport report = reportList.getReports(0);

        List<EventMetricData> data = new ArrayList<>();
        for (StatsLogReport metric : report.getMetricsList()) {
            data.addAll(metric.getEventMetrics().getDataList());
        }
        data.sort(Comparator.comparing(EventMetricData::getElapsedTimestampNanos));

        LogUtil.CLog.d("Get EventMetricDataList as following:\n");
        for (EventMetricData d : data) {
            LogUtil.CLog.d("Atom at " + d.getElapsedTimestampNanos() + ":\n" + d.getAtom().toString());
        }
        return data;
    }

    protected List<Atom> getGaugeMetricDataList() throws Exception {
        return getGaugeMetricDataList(/*checkTimestampTruncated=*/false);
    }

    protected List<Atom> getGaugeMetricDataList(boolean checkTimestampTruncated) throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        assertThat(reportList.getReportsCount()).isEqualTo(1);

        // only config
        ConfigMetricsReport report = reportList.getReports(0);
        assertThat(report.getMetricsCount()).isEqualTo(1);

        List<Atom> data = new ArrayList<>();
        for (GaugeMetricData gaugeMetricData :
                report.getMetrics(0).getGaugeMetrics().getDataList()) {
            assertThat(gaugeMetricData.getBucketInfoCount()).isEqualTo(1);
            GaugeBucketInfo bucketInfo = gaugeMetricData.getBucketInfo(0);
            for (Atom atom : bucketInfo.getAtomList()) {
                data.add(atom);
            }
            if (checkTimestampTruncated) {
                for (long timestampNs: bucketInfo.getElapsedTimestampNanosList()) {
                    assertTimestampIsTruncated(timestampNs);
                }
            }
        }

        LogUtil.CLog.d("Get GaugeMetricDataList as following:\n");
        for (Atom d : data) {
            LogUtil.CLog.d("Atom:\n" + d.toString());
        }
        return data;
    }

    /**
     * Gets the statsd report and extract duration metric data.
     * Note that this also deletes that report from statsd.
     */
    protected List<DurationMetricData> getDurationMetricDataList() throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        assertThat(reportList.getReportsCount()).isEqualTo(1);
        ConfigMetricsReport report = reportList.getReports(0);

        List<DurationMetricData> data = new ArrayList<>();
        for (StatsLogReport metric : report.getMetricsList()) {
            data.addAll(metric.getDurationMetrics().getDataList());
        }

        LogUtil.CLog.d("Got DurationMetricDataList as following:\n");
        for (DurationMetricData d : data) {
            LogUtil.CLog.d("Duration " + d);
        }
        return data;
    }

    /**
     * Gets the statsd report and extract count metric data.
     * Note that this also deletes that report from statsd.
     */
    protected List<CountMetricData> getCountMetricDataList() throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        assertThat(reportList.getReportsCount()).isEqualTo(1);
        ConfigMetricsReport report = reportList.getReports(0);

        List<CountMetricData> data = new ArrayList<>();
        for (StatsLogReport metric : report.getMetricsList()) {
            data.addAll(metric.getCountMetrics().getDataList());
        }

        LogUtil.CLog.d("Got CountMetricDataList as following:\n");
        for (CountMetricData d : data) {
            LogUtil.CLog.d("Count " + d);
        }
        return data;
    }

    /**
     * Gets the statsd report and extract value metric data.
     * Note that this also deletes that report from statsd.
     */
    protected List<ValueMetricData> getValueMetricDataList() throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        assertThat(reportList.getReportsCount()).isEqualTo(1);
        ConfigMetricsReport report = reportList.getReports(0);

        List<ValueMetricData> data = new ArrayList<>();
        for (StatsLogReport metric : report.getMetricsList()) {
            data.addAll(metric.getValueMetrics().getDataList());
        }

        LogUtil.CLog.d("Got ValueMetricDataList as following:\n");
        for (ValueMetricData d : data) {
            LogUtil.CLog.d("Value " + d);
        }
        return data;
    }

    protected StatsLogReport getStatsLogReport() throws Exception {
        ConfigMetricsReport report = getConfigMetricsReport();
        assertThat(report.hasUidMap()).isTrue();
        assertThat(report.getMetricsCount()).isEqualTo(1);
        return report.getMetrics(0);
    }

    protected ConfigMetricsReport getConfigMetricsReport() throws Exception {
        ConfigMetricsReportList reportList = getReportList();
        assertThat(reportList.getReportsCount()).isEqualTo(1);
        return reportList.getReports(0);
    }

    /** Gets the statsd report. Note that this also deletes that report from statsd. */
    protected ConfigMetricsReportList getReportList() throws Exception {
        try {
            ConfigMetricsReportList reportList = getDump(ConfigMetricsReportList.parser(),
                    String.join(" ", DUMP_REPORT_CMD, String.valueOf(CONFIG_ID),
                            "--include_current_bucket", "--proto"));
            return reportList;
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to fetch and parse the statsd output report. "
                    + "Perhaps there is not a valid statsd config for the requested "
                    + "uid=" + getHostUid() + ", id=" + CONFIG_ID + ".");
            throw (e);
        }
    }

    protected BatteryStatsProto getBatteryStatsProto() throws Exception {
        try {
            BatteryStatsProto batteryStatsProto = getDump(BatteryStatsServiceDumpProto.parser(),
                    String.join(" ", DUMP_BATTERYSTATS_CMD,
                            "--proto")).getBatterystats();
            LogUtil.CLog.d("Got batterystats:\n " + batteryStatsProto.toString());
            return batteryStatsProto;
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to dump batterystats proto");
            throw (e);
        }
    }

    /** Gets reports from the statsd data incident section from the stats dumpsys. */
    protected List<ConfigMetricsReportList> getReportsFromStatsDataDumpProto() throws Exception {
        try {
            StatsDataDumpProto statsProto = getDump(StatsDataDumpProto.parser(),
                    String.join(" ", DUMPSYS_STATS_CMD, "--proto"));
            // statsProto holds repeated bytes, which we must parse into ConfigMetricsReportLists.
            List<ConfigMetricsReportList> reports
                    = new ArrayList<>(statsProto.getConfigMetricsReportListCount());
            for (ByteString reportListBytes : statsProto.getConfigMetricsReportListList()) {
                reports.add(ConfigMetricsReportList.parseFrom(reportListBytes));
            }
            LogUtil.CLog.d("Got dumpsys stats output:\n " + reports.toString());
            return reports;
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to dumpsys stats proto");
            throw (e);
        }
    }

    protected List<ProcessStatsProto> getProcStatsProto() throws Exception {
        try {

            List<ProcessStatsProto> processStatsProtoList =
                new ArrayList<ProcessStatsProto>();
            android.service.procstats.ProcessStatsSectionProto sectionProto = getDump(
                    ProcessStatsServiceDumpProto.parser(),
                    String.join(" ", DUMP_PROCSTATS_CMD,
                            "--proto")).getProcstatsNow();
            for (android.service.procstats.ProcessStatsProto stats :
                    sectionProto.getProcessStatsList()) {
                ProcessStatsProto procStats = ProcessStatsProto.parser().parseFrom(
                    stats.toByteArray());
                processStatsProtoList.add(procStats);
            }
            LogUtil.CLog.d("Got procstats:\n ");
            for (ProcessStatsProto processStatsProto : processStatsProtoList) {
                LogUtil.CLog.d(processStatsProto.toString());
            }
            return processStatsProtoList;
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to dump procstats proto");
            throw (e);
        }
    }

    /*
     * Get all procstats package data in proto
     */
    protected List<ProcessStatsPackageProto> getAllProcStatsProto() throws Exception {
        try {
            android.service.procstats.ProcessStatsSectionProto sectionProto = getDump(
                    ProcessStatsServiceDumpProto.parser(),
                    String.join(" ", DUMP_PROCSTATS_CMD,
                            "--proto")).getProcstatsOver24Hrs();
            List<ProcessStatsPackageProto> processStatsProtoList =
                new ArrayList<ProcessStatsPackageProto>();
            for (android.service.procstats.ProcessStatsPackageProto pkgStast :
                sectionProto.getPackageStatsList()) {
              ProcessStatsPackageProto pkgAtom =
                  ProcessStatsPackageProto.parser().parseFrom(pkgStast.toByteArray());
                processStatsProtoList.add(pkgAtom);
            }
            LogUtil.CLog.d("Got procstats:\n ");
            for (ProcessStatsPackageProto processStatsProto : processStatsProtoList) {
                LogUtil.CLog.d(processStatsProto.toString());
            }
            return processStatsProtoList;
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to dump procstats proto");
            throw (e);
        }
    }

    /*
     * Get all processes' procstats statsd data in proto
     */
    protected List<android.service.procstats.ProcessStatsProto> getAllProcStatsProtoForStatsd()
            throws Exception {
        try {
            android.service.procstats.ProcessStatsSectionProto sectionProto = getDump(
                    android.service.procstats.ProcessStatsSectionProto.parser(),
                    String.join(" ", DUMP_PROCSTATS_CMD,
                            "--statsd"));
            List<android.service.procstats.ProcessStatsProto> processStatsProtoList
                    = sectionProto.getProcessStatsList();
            LogUtil.CLog.d("Got procstats:\n ");
            for (android.service.procstats.ProcessStatsProto processStatsProto
                    : processStatsProtoList) {
                LogUtil.CLog.d(processStatsProto.toString());
            }
            return processStatsProtoList;
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to dump procstats proto");
            throw (e);
        }
    }

    protected boolean hasBattery() throws Exception {
        try {
            BatteryServiceDumpProto batteryProto = getDump(BatteryServiceDumpProto.parser(),
                    String.join(" ", DUMP_BATTERY_CMD, "--proto"));
            LogUtil.CLog.d("Got battery service dump:\n " + batteryProto.toString());
            return batteryProto.getIsPresent();
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            LogUtil.CLog.e("Failed to dump batteryservice proto");
            throw (e);
        }
    }

    /** Creates a FieldValueMatcher.Builder corresponding to the given field. */
    protected static FieldValueMatcher.Builder createFvm(int field) {
        return FieldValueMatcher.newBuilder().setField(field);
    }

    protected void addAtomEvent(StatsdConfig.Builder conf, int atomTag) throws Exception {
        addAtomEvent(conf, atomTag, new ArrayList<FieldValueMatcher.Builder>());
    }

    /**
     * Adds an event to the config for an atom that matches the given key.
     *
     * @param conf    configuration
     * @param atomTag atom tag (from atoms.proto)
     * @param fvm     FieldValueMatcher.Builder for the relevant key
     */
    protected void addAtomEvent(StatsdConfig.Builder conf, int atomTag,
            FieldValueMatcher.Builder fvm)
            throws Exception {
        addAtomEvent(conf, atomTag, Arrays.asList(fvm));
    }

    /**
     * Adds an event to the config for an atom that matches the given keys.
     *
     * @param conf   configuration
     * @param atomId atom tag (from atoms.proto)
     * @param fvms   list of FieldValueMatcher.Builders to attach to the atom. May be null.
     */
    protected void addAtomEvent(StatsdConfig.Builder conf, int atomId,
            List<FieldValueMatcher.Builder> fvms) throws Exception {

        final String atomName = "Atom" + System.nanoTime();
        final String eventName = "Event" + System.nanoTime();

        SimpleAtomMatcher.Builder sam = SimpleAtomMatcher.newBuilder().setAtomId(atomId);
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
     * Adds an atom to a gauge metric of a config
     *
     * @param conf        configuration
     * @param atomId      atom id (from atoms.proto)
     * @param gaugeMetric the gauge metric to add
     */
    protected void addGaugeAtom(StatsdConfig.Builder conf, int atomId,
            GaugeMetric.Builder gaugeMetric) throws Exception {
        final String atomName = "Atom" + System.nanoTime();
        final String gaugeName = "Gauge" + System.nanoTime();
        final String predicateName = "APP_BREADCRUMB";
        SimpleAtomMatcher.Builder sam = SimpleAtomMatcher.newBuilder().setAtomId(atomId);
        conf.addAtomMatcher(AtomMatcher.newBuilder()
                .setId(atomName.hashCode())
                .setSimpleAtomMatcher(sam));
        final String predicateTrueName = "APP_BREADCRUMB_1";
        final String predicateFalseName = "APP_BREADCRUMB_2";
        conf.addAtomMatcher(AtomMatcher.newBuilder()
                .setId(predicateTrueName.hashCode())
                .setSimpleAtomMatcher(SimpleAtomMatcher.newBuilder()
                        .setAtomId(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                        .addFieldValueMatcher(FieldValueMatcher.newBuilder()
                                .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER)
                                .setEqInt(1)
                        )
                )
        )
                // Used to trigger predicate
                .addAtomMatcher(AtomMatcher.newBuilder()
                        .setId(predicateFalseName.hashCode())
                        .setSimpleAtomMatcher(SimpleAtomMatcher.newBuilder()
                                .setAtomId(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                                .addFieldValueMatcher(FieldValueMatcher.newBuilder()
                                        .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER)
                                        .setEqInt(2)
                                )
                        )
                );
        conf.addPredicate(Predicate.newBuilder()
                .setId(predicateName.hashCode())
                .setSimplePredicate(SimplePredicate.newBuilder()
                        .setStart(predicateTrueName.hashCode())
                        .setStop(predicateFalseName.hashCode())
                        .setCountNesting(false)
                )
        );
        gaugeMetric
                .setId(gaugeName.hashCode())
                .setWhat(atomName.hashCode())
                .setCondition(predicateName.hashCode());
        conf.addGaugeMetric(gaugeMetric.build());
    }

    /**
     * Adds an atom to a gauge metric of a config
     *
     * @param conf      configuration
     * @param atomId    atom id (from atoms.proto)
     * @param dimension dimension is needed for most pulled atoms
     */
    protected void addGaugeAtomWithDimensions(StatsdConfig.Builder conf, int atomId,
            @Nullable FieldMatcher.Builder dimension) throws Exception {
        GaugeMetric.Builder gaugeMetric = GaugeMetric.newBuilder()
                .setGaugeFieldsFilter(FieldFilter.newBuilder().setIncludeAll(true).build())
                .setSamplingType(GaugeMetric.SamplingType.CONDITION_CHANGE_TO_TRUE)
                .setMaxNumGaugeAtomsPerBucket(10000)
                .setBucket(TimeUnit.CTS);
        if (dimension != null) {
            gaugeMetric.setDimensionsInWhat(dimension.build());
        }
        addGaugeAtom(conf, atomId, gaugeMetric);
    }

    /**
     * Asserts that each set of states in stateSets occurs at least once in data.
     * Asserts that the states in data occur in the same order as the sets in stateSets.
     *
     * @param stateSets        A list of set of states, where each set represents an equivalent
     *                         state of the device for the purpose of CTS.
     * @param data             list of EventMetricData from statsd, produced by
     *                         getReportMetricListData()
     * @param wait             expected duration (in ms) between state changes; asserts that the
     *                         actual wait
     *                         time was wait/2 <= actual_wait <= 5*wait. Use 0 to ignore this
     *                         assertion.
     * @param getStateFromAtom expression that takes in an Atom and returns the state it contains
     */
    public void assertStatesOccurred(List<Set<Integer>> stateSets, List<EventMetricData> data,
            int wait, Function<Atom, Integer> getStateFromAtom) {
        // Sometimes, there are more events than there are states.
        // Eg: When the screen turns off, it may go into OFF and then DOZE immediately.
        assertWithMessage("Too few states found").that(data.size()).isAtLeast(stateSets.size());
        int stateSetIndex = 0; // Tracks which state set we expect the data to be in.
        for (int dataIndex = 0; dataIndex < data.size(); dataIndex++) {
            Atom atom = data.get(dataIndex).getAtom();
            int state = getStateFromAtom.apply(atom);
            // If state is in the current state set, we do not assert anything.
            // If it is not, we expect to have transitioned to the next state set.
            if (stateSets.get(stateSetIndex).contains(state)) {
                // No need to assert anything. Just log it.
                LogUtil.CLog.i("The following atom at dataIndex=" + dataIndex + " is "
                        + "in stateSetIndex " + stateSetIndex + ":\n"
                        + data.get(dataIndex).getAtom().toString());
            } else {
                stateSetIndex += 1;
                LogUtil.CLog.i("Assert that the following atom at dataIndex=" + dataIndex + " is"
                        + " in stateSetIndex " + stateSetIndex + ":\n"
                        + data.get(dataIndex).getAtom().toString());
                assertWithMessage("Missed first state").that(dataIndex).isNotEqualTo(0);
                assertWithMessage("Too many states").that(stateSetIndex)
                    .isLessThan(stateSets.size());
                assertWithMessage(String.format("Is in wrong state (%d)", state))
                    .that(stateSets.get(stateSetIndex)).contains(state);
                if (wait > 0) {
                    assertTimeDiffBetween(data.get(dataIndex - 1), data.get(dataIndex),
                            wait / 2, wait * 5);
                }
            }
        }
        assertWithMessage("Too few states").that(stateSetIndex).isEqualTo(stateSets.size() - 1);
    }

    /**
     * Removes all elements from data prior to the first occurrence of an element of state. After
     * this method is called, the first element of data (if non-empty) is guaranteed to be an
     * element in state.
     *
     * @param getStateFromAtom expression that takes in an Atom and returns the state it contains
     */
    public void popUntilFind(List<EventMetricData> data, Set<Integer> state,
            Function<Atom, Integer> getStateFromAtom) {
        int firstStateIdx;
        for (firstStateIdx = 0; firstStateIdx < data.size(); firstStateIdx++) {
            Atom atom = data.get(firstStateIdx).getAtom();
            if (state.contains(getStateFromAtom.apply(atom))) {
                break;
            }
        }
        if (firstStateIdx == 0) {
            // First first element already is in state, so there's nothing to do.
            return;
        }
        data.subList(0, firstStateIdx).clear();
    }

    /**
     * Removes all elements from data after to the last occurrence of an element of state. After
     * this method is called, the last element of data (if non-empty) is guaranteed to be an
     * element in state.
     *
     * @param getStateFromAtom expression that takes in an Atom and returns the state it contains
     */
    public void popUntilFindFromEnd(List<EventMetricData> data, Set<Integer> state,
        Function<Atom, Integer> getStateFromAtom) {
        int lastStateIdx;
        for (lastStateIdx = data.size() - 1; lastStateIdx >= 0; lastStateIdx--) {
            Atom atom = data.get(lastStateIdx).getAtom();
            if (state.contains(getStateFromAtom.apply(atom))) {
                break;
            }
        }
        if (lastStateIdx == data.size()-1) {
            // Last element already is in state, so there's nothing to do.
            return;
        }
        data.subList(lastStateIdx+1, data.size()).clear();
    }

    /** Returns the UID of the host, which should always either be SHELL (2000) or ROOT (0). */
    protected int getHostUid() throws DeviceNotAvailableException {
        String strUid = "";
        try {
            strUid = getDevice().executeShellCommand("id -u");
            return Integer.parseInt(strUid.trim());
        } catch (NumberFormatException e) {
            LogUtil.CLog.e("Failed to get host's uid via shell command. Found " + strUid);
            // Fall back to alternative method...
            if (getDevice().isAdbRoot()) {
                return 0; // ROOT
            } else {
                return 2000; // SHELL
            }
        }
    }

    protected String getProperty(String prop) throws Exception {
        return getDevice().executeShellCommand("getprop " + prop).replace("\n", "");
    }

    protected void turnScreenOn() throws Exception {
        getDevice().executeShellCommand("input keyevent KEYCODE_WAKEUP");
        getDevice().executeShellCommand("wm dismiss-keyguard");
    }

    protected void turnScreenOff() throws Exception {
        getDevice().executeShellCommand("input keyevent KEYCODE_SLEEP");
    }

    protected void setChargingState(int state) throws Exception {
        getDevice().executeShellCommand("cmd battery set status " + state);
    }

    protected void unplugDevice() throws Exception {
        // On batteryless devices on Android P or above, the 'unplug' command
        // alone does not simulate the really unplugged state.
        //
        // This is because charging state is left as "unknown". Unless a valid
        // state like 3 = BatteryManager.BATTERY_STATUS_DISCHARGING is set,
        // framework does not consider the device as running on battery.
        setChargingState(3);

        getDevice().executeShellCommand("cmd battery unplug");
    }

    protected void plugInAc() throws Exception {
        getDevice().executeShellCommand("cmd battery set ac 1");
    }

    protected void plugInUsb() throws Exception {
        getDevice().executeShellCommand("cmd battery set usb 1");
    }

    protected void plugInWireless() throws Exception {
        getDevice().executeShellCommand("cmd battery set wireless 1");
    }

    protected void enableLooperStats() throws Exception {
        getDevice().executeShellCommand("cmd looper_stats enable");
    }

    protected void resetLooperStats() throws Exception {
        getDevice().executeShellCommand("cmd looper_stats reset");
    }

    protected void disableLooperStats() throws Exception {
        getDevice().executeShellCommand("cmd looper_stats disable");
    }

    protected void enableBinderStats() throws Exception {
        getDevice().executeShellCommand("dumpsys binder_calls_stats --enable");
    }

    protected void resetBinderStats() throws Exception {
        getDevice().executeShellCommand("dumpsys binder_calls_stats --reset");
    }

    protected void disableBinderStats() throws Exception {
        getDevice().executeShellCommand("dumpsys binder_calls_stats --disable");
    }

    protected void binderStatsNoSampling() throws Exception {
        getDevice().executeShellCommand("dumpsys binder_calls_stats --no-sampling");
    }

    protected void setUpLooperStats() throws Exception {
        getDevice().executeShellCommand("cmd looper_stats enable");
        getDevice().executeShellCommand("cmd looper_stats sampling_interval 1");
        getDevice().executeShellCommand("cmd looper_stats reset");
    }

    protected void cleanUpLooperStats() throws Exception {
        getDevice().executeShellCommand("cmd looper_stats disable");
    }

    public void setAppBreadcrumbPredicate() throws Exception {
        doAppBreadcrumbReportedStart(1);
    }

    public void clearAppBreadcrumbPredicate() throws Exception {
        doAppBreadcrumbReportedStart(2);
    }

    public void doAppBreadcrumbReportedStart(int label) throws Exception {
        doAppBreadcrumbReported(label, AppBreadcrumbReported.State.START.ordinal());
    }

    public void doAppBreadcrumbReportedStop(int label) throws Exception {
        doAppBreadcrumbReported(label, AppBreadcrumbReported.State.STOP.ordinal());
    }

    public void doAppBreadcrumbReported(int label) throws Exception {
        doAppBreadcrumbReported(label, AppBreadcrumbReported.State.UNSPECIFIED.ordinal());
    }

    public void doAppBreadcrumbReported(int label, int state) throws Exception {
        getDevice().executeShellCommand(String.format(
                "cmd stats log-app-breadcrumb %d %d", label, state));
    }

    protected void setBatteryLevel(int level) throws Exception {
        getDevice().executeShellCommand("cmd battery set level " + level);
    }

    protected void resetBatteryStatus() throws Exception {
        getDevice().executeShellCommand("cmd battery reset");
    }

    protected int getScreenBrightness() throws Exception {
        return Integer.parseInt(
                getDevice().executeShellCommand("settings get system screen_brightness").trim());
    }

    protected void setScreenBrightness(int brightness) throws Exception {
        getDevice().executeShellCommand("settings put system screen_brightness " + brightness);
    }

    // Gets whether "Always on Display" setting is enabled.
    // In rare cases, this is different from whether the device can enter SCREEN_STATE_DOZE.
    protected String getAodState() throws Exception {
        return getDevice().executeShellCommand("settings get secure doze_always_on");
    }

    protected void setAodState(String state) throws Exception {
        getDevice().executeShellCommand("settings put secure doze_always_on " + state);
    }

    protected boolean isScreenBrightnessModeManual() throws Exception {
        String mode = getDevice().executeShellCommand("settings get system screen_brightness_mode");
        return Integer.parseInt(mode.trim()) == 0;
    }

    protected void setScreenBrightnessMode(boolean manual) throws Exception {
        getDevice().executeShellCommand(
                "settings put system screen_brightness_mode " + (manual ? 0 : 1));
    }

    protected void enterDozeModeLight() throws Exception {
        getDevice().executeShellCommand("dumpsys deviceidle force-idle light");
    }

    protected void enterDozeModeDeep() throws Exception {
        getDevice().executeShellCommand("dumpsys deviceidle force-idle deep");
    }

    protected void leaveDozeMode() throws Exception {
        getDevice().executeShellCommand("dumpsys deviceidle unforce");
        getDevice().executeShellCommand("dumpsys deviceidle disable");
        getDevice().executeShellCommand("dumpsys deviceidle enable");
    }

    protected void turnBatterySaverOn() throws Exception {
        unplugDevice();
        getDevice().executeShellCommand("settings put global low_power 1");
    }

    protected void turnBatterySaverOff() throws Exception {
        getDevice().executeShellCommand("settings put global low_power 0");
        getDevice().executeShellCommand("cmd battery reset");
    }

    protected void rebootDevice() throws Exception {
        getDevice().rebootUntilOnline();
    }

    /**
     * Asserts that the two events are within the specified range of each other.
     *
     * @param d0        the event that should occur first
     * @param d1        the event that should occur second
     * @param minDiffMs d0 should precede d1 by at least this amount
     * @param maxDiffMs d0 should precede d1 by at most this amount
     */
    public static void assertTimeDiffBetween(EventMetricData d0, EventMetricData d1,
            int minDiffMs, int maxDiffMs) {
        long diffMs = (d1.getElapsedTimestampNanos() - d0.getElapsedTimestampNanos()) / 1_000_000;
        assertWithMessage("Illegal time difference")
            .that(diffMs).isIn(Range.closed((long) minDiffMs, (long) maxDiffMs));
    }

    protected String getCurrentLogcatDate() throws Exception {
        // TODO: Do something more robust than this for getting logcat markers.
        long timestampMs = getDevice().getDeviceDate();
        return new SimpleDateFormat("MM-dd HH:mm:ss.SSS")
                .format(new Date(timestampMs));
    }

    protected String getLogcatSince(String date, String logcatParams) throws Exception {
        return getDevice().executeShellCommand(String.format(
                "logcat -v threadtime -t '%s' -d %s", date, logcatParams));
    }

    // TODO: Remove this and migrate all usages to createConfigBuilder()
    protected StatsdConfig.Builder getPulledConfig() {
        return createConfigBuilder();
    }
    /**
     * Determines if the device has the given feature.
     * Prints a warning if its value differs from requiredAnswer.
     */
    protected boolean hasFeature(String featureName, boolean requiredAnswer) throws Exception {
        final String features = getDevice().executeShellCommand("pm list features");
        boolean hasIt = features.contains(featureName);
        if (hasIt != requiredAnswer) {
            LogUtil.CLog.w("Device does " + (requiredAnswer ? "not " : "") + "have feature "
                    + featureName);
        }
        return hasIt == requiredAnswer;
    }

    /**
     * Determines if the device has |file|.
     */
    protected boolean doesFileExist(String file) throws Exception {
        return getDevice().doesFileExist(file);
    }

    protected void turnOnAirplaneMode() throws Exception {
        getDevice().executeShellCommand("cmd connectivity airplane-mode enable");
    }

    protected void turnOffAirplaneMode() throws Exception {
        getDevice().executeShellCommand("cmd connectivity airplane-mode disable");
    }

    /**
     * Returns a list of fields and values for {@code className} from {@link TelephonyDebugService}
     * output.
     *
     * <p>Telephony dumpsys output does not support proto at the moment. This method provides
     * limited support for parsing its output. Specifically, it does not support arrays or
     * multi-line values.
     */
    private List<Map<String, String>> getTelephonyDumpEntries(String className) throws Exception {
        // Matches any line with indentation, except for lines with only spaces
        Pattern indentPattern = Pattern.compile("^(\\s*)[^ ].*$");
        // Matches pattern for class, e.g. "    Phone:"
        Pattern classNamePattern = Pattern.compile("^(\\s*)" + Pattern.quote(className) + ":.*$");
        // Matches pattern for key-value pairs, e.g. "     mPhoneId=1"
        Pattern keyValuePattern = Pattern.compile("^(\\s*)([a-zA-Z]+[a-zA-Z0-9_]*)\\=(.+)$");
        String response =
                getDevice().executeShellCommand("dumpsys activity service TelephonyDebugService");
        Queue<String> responseLines = new LinkedList<>(Arrays.asList(response.split("[\\r\\n]+")));

        List<Map<String, String>> results = new ArrayList<>();
        while (responseLines.peek() != null) {
            Matcher matcher = classNamePattern.matcher(responseLines.poll());
            if (matcher.matches()) {
                final int classIndentLevel = matcher.group(1).length();
                final Map<String, String> instanceEntries = new HashMap<>();
                while (responseLines.peek() != null) {
                    // Skip blank lines
                    matcher = indentPattern.matcher(responseLines.peek());
                    if (responseLines.peek().length() == 0 || !matcher.matches()) {
                        responseLines.poll();
                        continue;
                    }
                    // Finish (without consuming the line) if already parsed past this instance
                    final int indentLevel = matcher.group(1).length();
                    if (indentLevel <= classIndentLevel) {
                        break;
                    }
                    // Parse key-value pair if it belongs to the instance directly
                    matcher = keyValuePattern.matcher(responseLines.poll());
                    if (indentLevel == classIndentLevel + 1 && matcher.matches()) {
                        instanceEntries.put(matcher.group(2), matcher.group(3));
                    }
                }
                results.add(instanceEntries);
            }
        }
        return results;
    }

    protected int getActiveSimSlotCount() throws Exception {
        List<Map<String, String>> slots = getTelephonyDumpEntries("UiccSlot");
        long count = slots.stream().filter(slot -> "true".equals(slot.get("mActive"))).count();
        return Math.toIntExact(count);
    }

    /**
     * Returns the upper bound of active SIM profile count.
     *
     * <p>The value is an upper bound as eSIMs without profiles are also counted in.
     */
    protected int getActiveSimCountUpperBound() throws Exception {
        List<Map<String, String>> slots = getTelephonyDumpEntries("UiccSlot");
        long count = slots.stream().filter(slot ->
                "true".equals(slot.get("mActive"))
                && "CARDSTATE_PRESENT".equals(slot.get("mCardState"))).count();
        return Math.toIntExact(count);
    }

    /**
     * Returns the upper bound of active eSIM profile count.
     *
     * <p>The value is an upper bound as eSIMs without profiles are also counted in.
     */
    protected int getActiveEsimCountUpperBound() throws Exception {
        List<Map<String, String>> slots = getTelephonyDumpEntries("UiccSlot");
        long count = slots.stream().filter(slot ->
                "true".equals(slot.get("mActive"))
                && "CARDSTATE_PRESENT".equals(slot.get("mCardState"))
                && "true".equals(slot.get("mIsEuicc"))).count();
        return Math.toIntExact(count);
    }

    protected boolean hasGsmPhone() throws Exception {
        // Not using log entries or ServiceState in the dump since they may or may not be present,
        // which can make the test flaky
        return getTelephonyDumpEntries("Phone").stream()
                .anyMatch(phone ->
                        String.format("%d", PHONE_TYPE_GSM).equals(phone.get("getPhoneType()")));
    }

    protected boolean hasCdmaPhone() throws Exception {
        // Not using log entries or ServiceState in the dump due to the same reason as hasGsmPhone()
        return getTelephonyDumpEntries("Phone").stream()
                .anyMatch(phone ->
                        String.format("%d", PHONE_TYPE_CDMA).equals(phone.get("getPhoneType()"))
                        || String.format("%d", PHONE_TYPE_CDMA_LTE)
                                .equals(phone.get("getPhoneType()")));
    }

    // Checks that a timestamp has been truncated to be a multiple of 5 min
    protected void assertTimestampIsTruncated(long timestampNs) {
        long fiveMinutesInNs = NS_PER_SEC * 5 * 60;
        assertWithMessage("Timestamp is not truncated")
                .that(timestampNs % fiveMinutesInNs).isEqualTo(0);
    }
}
