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
package android.media.cts;

import static com.google.common.truth.Truth.assertThat;

import android.stats.mediametrics.Mediametrics;

import com.android.internal.os.StatsdConfigProto;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto;
import com.android.os.StatsLog;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.tradefed.device.CollectingByteOutputReceiver;

import com.google.common.io.Files;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

/** Host-side tests for MediaExtractor. */
public class MediaExtractorHostSideTest extends BaseMediaHostSideTest {
    /** Package name of the device-side tests. */
    private static final String DEVICE_SIDE_TEST_PACKAGE = "android.media.cts";
    /** Name of the APK that contains the device-side tests. */
    private static final String DEVICE_SIDE_TEST_APK = "CtsMediaExtractorHostTestApp.apk";
    /** Fully qualified class name for the device-side tests. */
    private static final String DEVICE_SIDE_TEST_CLASS =
            "android.media.cts.MediaExtractorDeviceSideTest";

    private static final long CONFIG_ID = "cts_config".hashCode();

    @Override
    public void setUp() throws Exception {
        super.setUp();
        getDevice().uninstallPackage(DEVICE_SIDE_TEST_PACKAGE);
        installApp(DEVICE_SIDE_TEST_APK);
        removeConfig(); // Clear existing configs.
        createAndUploadConfig();
        getAndClearReportList(); // Clear existing reports.
    }

    @Override
    public void tearDown() throws Exception {
        removeConfig();
        getDevice().uninstallPackage(DEVICE_SIDE_TEST_PACKAGE);
    }

    // Tests.

    public void testMediaMetricsEntryPointSdk() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, DEVICE_SIDE_TEST_CLASS, "testEntryPointSdk");
        assertThat(getMediaExtractorReportedEntryPoint())
                .isEqualTo(Mediametrics.ExtractorData.EntryPoint.SDK);
    }

    public void testMediaMetricsEntryPointNdkNoJvm() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, DEVICE_SIDE_TEST_CLASS, "testEntryPointNdkNoJvm");
        assertThat(getMediaExtractorReportedEntryPoint())
                .isEqualTo(Mediametrics.ExtractorData.EntryPoint.NDK_NO_JVM);
    }

    public void testMediaMetricsEntryPointNdkWithJvm() throws Exception {
        runDeviceTests(
                DEVICE_SIDE_TEST_PACKAGE, DEVICE_SIDE_TEST_CLASS, "testEntryPointNdkWithJvm");
        assertThat(getMediaExtractorReportedEntryPoint())
                .isEqualTo(Mediametrics.ExtractorData.EntryPoint.NDK_WITH_JVM);
    }

    // Internal methods.

    /** Removes any existing config with id {@link #CONFIG_ID}. */
    private void removeConfig() throws Exception {
        getDevice().executeShellCommand("cmd stats config remove " + CONFIG_ID);
    }

    /** Creates the statsd config and passes it to statsd. */
    private void createAndUploadConfig() throws Exception {
        StatsdConfig.Builder configBuilder =
                StatsdConfigProto.StatsdConfig.newBuilder()
                        .setId(CONFIG_ID)
                        .addAllowedLogSource(DEVICE_SIDE_TEST_PACKAGE)
                        .addWhitelistedAtomIds(
                                AtomsProto.Atom.MEDIAMETRICS_EXTRACTOR_REPORTED_FIELD_NUMBER);
        addAtomEvent(configBuilder);
        uploadConfig(configBuilder.build());
    }

    /** Writes the given config into a file and passes is to statsd via standard input. */
    private void uploadConfig(StatsdConfig config) throws Exception {
        File configFile = File.createTempFile("statsdconfig", ".config");
        configFile.deleteOnExit();
        Files.write(config.toByteArray(), configFile);
        String remotePath = "/data/local/tmp/" + configFile.getName();
        // Make sure a config file with the same name doesn't exist already.
        getDevice().deleteFile(remotePath);
        assertThat(getDevice().pushFile(configFile, remotePath)).isTrue();
        getDevice()
                .executeShellCommand(
                        "cat " + remotePath + " | cmd stats config update " + CONFIG_ID);
        getDevice().deleteFile(remotePath);
    }

    /** Adds an event to the config in order to match MediaParser reported atoms. */
    private static void addAtomEvent(StatsdConfig.Builder config) {
        String atomName = "Atom" + System.nanoTime();
        String eventName = "Event" + System.nanoTime();
        SimpleAtomMatcher.Builder sam =
                SimpleAtomMatcher.newBuilder()
                        .setAtomId(AtomsProto.Atom.MEDIAMETRICS_EXTRACTOR_REPORTED_FIELD_NUMBER);
        config.addAtomMatcher(
                StatsdConfigProto.AtomMatcher.newBuilder()
                        .setId(atomName.hashCode())
                        .setSimpleAtomMatcher(sam));
        config.addEventMetric(
                StatsdConfigProto.EventMetric.newBuilder()
                        .setId(eventName.hashCode())
                        .setWhat(atomName.hashCode()));
    }

    /**
     * Returns all MediaParser reported metric events sorted by timestamp.
     *
     * <p>Note: Calls {@link #getAndClearReportList()} to obtain the statsd report.
     */
    private Mediametrics.ExtractorData.EntryPoint getMediaExtractorReportedEntryPoint()
            throws Exception {
        ConfigMetricsReportList reportList = getAndClearReportList();
        assertThat(reportList.getReportsCount()).isEqualTo(1);
        StatsLog.ConfigMetricsReport report = reportList.getReports(0);
        ArrayList<StatsLog.EventMetricData> data = new ArrayList<>();
        report.getMetricsList()
                .forEach(
                        statsLogReport ->
                                data.addAll(statsLogReport.getEventMetrics().getDataList()));
        List<AtomsProto.MediametricsExtractorReported> mediametricsExtractorReported =
                data.stream()
                        .map(element -> element.getAtom().getMediametricsExtractorReported())
                        .collect(Collectors.toList());
        // During device boot, services may extract media files. We ensure we only pick up metric
        // events from our device-side test.
        mediametricsExtractorReported.removeIf(
                entry -> !DEVICE_SIDE_TEST_PACKAGE.equals(entry.getPackageName()));
        assertThat(mediametricsExtractorReported).hasSize(1);
        return mediametricsExtractorReported.get(0).getExtractorData().getEntryPoint();
    }

    /** Gets a statsd report and removes it from the device. */
    private ConfigMetricsReportList getAndClearReportList() throws Exception {
        CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        getDevice()
                .executeShellCommand(
                        "cmd stats dump-report " + CONFIG_ID + " --include_current_bucket --proto",
                        receiver);
        return ConfigMetricsReportList.parser().parseFrom(receiver.getOutput());
    }
}
