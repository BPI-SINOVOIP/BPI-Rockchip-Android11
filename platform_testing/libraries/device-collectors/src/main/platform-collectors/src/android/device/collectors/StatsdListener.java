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
package android.device.collectors;

import android.app.StatsManager;
import android.app.StatsManager.StatsUnavailableException;
import android.content.Context;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.SystemClock;
import android.util.Log;
import android.util.StatsLog;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.runner.Description;
import org.junit.runner.Result;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;
import java.util.stream.Collectors;

/** A device-side metric listener that collects statsd-based metrics using bundled config files. */
public class StatsdListener extends BaseMetricListener {
    private static final String LOG_TAG = StatsdListener.class.getSimpleName();

    static final String OPTION_CONFIGS_RUN_LEVEL = "statsd-configs-run-level";
    static final String OPTION_CONFIGS_TEST_LEVEL = "statsd-configs-test-level";

    // Sub-directory within the test APK's assets/ directory to look for configs.
    static final String CONFIG_SUB_DIRECTORY = "statsd-configs";
    // File extension for all statsd configs.
    static final String PROTO_EXTENSION = ".pb";

    // Parent directory for all statsd reports.
    static final String REPORT_PATH_ROOT = "statsd-reports";
    // Sub-directory for test run reports.
    static final String REPORT_PATH_RUN_LEVEL = "run-level";
    // Sub-directory for test-level reports.
    static final String REPORT_PATH_TEST_LEVEL = "test-level";
    // Suffix template for test-level metric report files.
    static final String TEST_SUFFIX_TEMPLATE = "_%s-%d";

    // Common prefix for the metric key pointing to the report path.
    static final String REPORT_KEY_PREFIX = "statsd-";
    // Common prefix for the metric file.
    static final String REPORT_FILENAME_PREFIX = "statsd-";

    // Labels used to signify test events to statsd with the AppBreadcrumbReported atom.
    static final int RUN_EVENT_LABEL = 7;
    static final int TEST_EVENT_LABEL = 11;
    // A short delay after pushing the AppBreadcrumbReported event so that metrics can be dumped.
    static final long METRIC_PULL_DELAY = TimeUnit.SECONDS.toMillis(1);

    // Configs used for the test run and each test, respectively.
    private Map<String, StatsdConfig> mRunLevelConfigs = new HashMap<String, StatsdConfig>();
    private Map<String, StatsdConfig> mTestLevelConfigs = new HashMap<String, StatsdConfig>();

    // Map to associate config names with their config Ids.
    private Map<String, Long> mRunLevelConfigIds = new HashMap<String, Long>();
    private Map<String, Long> mTestLevelConfigIds = new HashMap<String, Long>();

    // "Counter" for test iterations, keyed by the display name of each test's description.
    private Map<String, Integer> mTestIterations = new HashMap<String, Integer>();

    // Cached stats manager instance.
    private StatsManager mStatsManager;

    /** Register the test run configs with {@link StatsManager} before the test run starts. */
    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        // The argument parsing has to be performed here as the instrumentation has not yet been
        // registered when the constructor of this class is called.
        mRunLevelConfigs.putAll(getConfigsFromOption(OPTION_CONFIGS_RUN_LEVEL));
        mTestLevelConfigs.putAll(getConfigsFromOption(OPTION_CONFIGS_TEST_LEVEL));

        mRunLevelConfigIds = registerConfigsWithStatsManager(mRunLevelConfigs);

        if (!logStart(RUN_EVENT_LABEL)) {
            Log.w(LOG_TAG, "Failed to log a test run start event. Metrics might be incomplete.");
        }
    }

    /**
     * Dump the test run stats reports to the test run subdirectory after the test run ends.
     *
     * <p>Dumps the stats regardless of whether all the tests pass.
     */
    @Override
    public void onTestRunEnd(DataRecord runData, Result result) {
        if (!logStop(RUN_EVENT_LABEL)) {
            Log.w(LOG_TAG, "Failed to log a test run end event. Metrics might be incomplete.");
        }
        SystemClock.sleep(METRIC_PULL_DELAY);

        Map<String, File> configReports =
                pullReportsAndRemoveConfigs(
                        mRunLevelConfigIds, Paths.get(REPORT_PATH_ROOT, REPORT_PATH_RUN_LEVEL), "");
        for (String configName : configReports.keySet()) {
            runData.addFileMetric(REPORT_KEY_PREFIX + configName, configReports.get(configName));
        }
    }

    /** Register the test-level configs with {@link StatsManager} before each test starts. */
    @Override
    public void onTestStart(DataRecord testData, Description description) {
        mTestIterations.computeIfPresent(description.getDisplayName(), (name, count) -> count + 1);
        mTestIterations.computeIfAbsent(description.getDisplayName(), name -> 1);
        mTestLevelConfigIds = registerConfigsWithStatsManager(mTestLevelConfigs);

        if (!logStart(TEST_EVENT_LABEL)) {
            Log.w(LOG_TAG, "Failed to log a test start event. Metrics might be incomplete.");
        }
    }

    /**
     * Dump the test-level stats reports to the test-specific subdirectory after the test ends.
     *
     * <p>Dumps the stats regardless of whether the test passes.
     */
    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        if (!logStop(TEST_EVENT_LABEL)) {
            Log.w(LOG_TAG, "Failed to log a test end event. Metrics might be incomplete.");
        }
        SystemClock.sleep(METRIC_PULL_DELAY);

        Map<String, File> configReports =
                pullReportsAndRemoveConfigs(
                        mTestLevelConfigIds,
                        Paths.get(REPORT_PATH_ROOT, REPORT_PATH_TEST_LEVEL),
                        getTestSuffix(description));
        for (String configName : configReports.keySet()) {
            testData.addFileMetric(REPORT_KEY_PREFIX + configName, configReports.get(configName));
        }
    }

    /**
     * Register a set of statsd configs and return their config IDs in a {@link Map}.
     *
     * @param configs Map of (config name, config proto message)
     * @return Map of (config name, config id)
     */
    private Map<String, Long> registerConfigsWithStatsManager(
            final Map<String, StatsdConfig> configs) {
        Map<String, Long> configIds = new HashMap<String, Long>();
        adoptShellPermissionIdentity();
        for (String configName : configs.keySet()) {
            long configId = getUniqueIdForConfig(configs.get(configName));
            StatsdConfig newConfig = configs.get(configName).toBuilder().setId(configId).build();
            try {
                Log.i(LOG_TAG, String.format("Adding config %s with ID %d.", configName, configId));
                addStatsConfig(configId, newConfig.toByteArray());
                configIds.put(configName, configId);
            } catch (StatsUnavailableException e) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Failed to add statsd config %s due to %s.",
                                configName, e.toString()));
            }
        }
        dropShellPermissionIdentity();
        return configIds;
    }

    /**
     * For a set of statsd config ids, retrieve the config reports from {@link StatsManager}, remove
     * the config and dump the reports into the designated directory on the device's external
     * storage.
     *
     * @param configIds Map of (config name, config Id)
     * @param directory relative directory on external storage to dump the report in. Each report
     *     will be named after its config.
     * @param suffix a suffix to append to the metric report file name, used to differentiate
     *     between tests and left empty for the test run.
     * @return Map of (config name, config report file)
     */
    private Map<String, File> pullReportsAndRemoveConfigs(
            final Map<String, Long> configIds, Path directory, String suffix) {
        File externalStorage = Environment.getExternalStorageDirectory();
        File saveDirectory = new File(externalStorage, directory.toString());
        if (!saveDirectory.isDirectory()) {
            saveDirectory.mkdirs();
        }
        Map<String, File> savedConfigFiles = new HashMap<String, File>();
        adoptShellPermissionIdentity();
        for (String configName : configIds.keySet()) {
            // Dump the metric report to external storage.
            ConfigMetricsReportList reportList;
            try {
                Log.i(
                        LOG_TAG,
                        String.format(
                                "Pulling metrics for config %s with ID %d.",
                                configName, configIds.get(configName)));
                reportList =
                        ConfigMetricsReportList.parseFrom(
                                getStatsReports(configIds.get(configName)));
                Log.i(
                        LOG_TAG,
                        String.format(
                                "Found %d metric %s from config %s.",
                                reportList.getReportsCount(),
                                reportList.getReportsCount() == 1 ? "report" : "reports",
                                configName));
                File reportFile =
                        new File(
                                saveDirectory,
                                REPORT_FILENAME_PREFIX + configName + suffix + PROTO_EXTENSION);
                writeToFile(reportFile, reportList.toByteArray());
                savedConfigFiles.put(configName, reportFile);
            } catch (StatsUnavailableException e) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Failed to retrieve metrics for config %s due to %s.",
                                configName, e.toString()));
            } catch (InvalidProtocolBufferException e) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Unable to parse report for config %s. Details: %s.",
                                configName, e.toString()));
            } catch (IOException e) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Failed to write metric report for config %s to device. "
                                        + "Details: %s.",
                                configName, e.toString()));
            }

            // Remove the statsd config.
            try {
                Log.i(
                        LOG_TAG,
                        String.format(
                                "Removing config %s with ID %d.",
                                configName, configIds.get(configName)));
                removeStatsConfig(configIds.get(configName));
            } catch (StatsUnavailableException e) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Unable to remove config %s due to %s.", configName, e.toString()));
            }
        }
        dropShellPermissionIdentity();
        return savedConfigFiles;
    }

    /**
     * Adopt shell permission identity to communicate with {@link StatsManager}.
     *
     * @hide
     */
    @VisibleForTesting
    protected void adoptShellPermissionIdentity() {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .adoptShellPermissionIdentity();
    }

    /**
     * Drop shell permission identity once communication with {@link StatsManager} is done.
     *
     * @hide
     */
    @VisibleForTesting
    protected void dropShellPermissionIdentity() {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /** Returns the cached {@link StatsManager} instance; if none exists, request and cache it. */
    private StatsManager getStatsManager() {
        if (mStatsManager == null) {
            mStatsManager =
                    (StatsManager)
                            InstrumentationRegistry.getTargetContext()
                                    .getSystemService(Context.STATS_MANAGER);
        }
        return mStatsManager;
    }

    /** Get the suffix for a test + iteration combination to differentiate it from other files. */
    @VisibleForTesting
    String getTestSuffix(Description description) {
        return String.format(
                TEST_SUFFIX_TEMPLATE,
                formatDescription(description),
                mTestIterations.get(description.getDisplayName()));
    }

    /** Format a JUnit {@link Description} to a desired string format. */
    @VisibleForTesting
    String formatDescription(Description description) {
        // Use String.valueOf() to guard agaist a null class name. This normally should not happen
        // but the Description class does not explicitly guarantee it.
        String className = String.valueOf(description.getClassName());
        String methodName = description.getMethodName();
        return methodName == null ? className : String.join("#", className, methodName);
    }

    /**
     * Forwarding logic for {@link StatsManager} as it is final and cannot be mocked.
     *
     * @hide
     */
    @VisibleForTesting
    protected void addStatsConfig(long configKey, byte[] config) throws StatsUnavailableException {
        getStatsManager().addConfig(configKey, config);
    }

    /**
     * Forwarding logic for {@link StatsManager} as it is final and cannot be mocked.
     *
     * @hide
     */
    @VisibleForTesting
    protected void removeStatsConfig(long configKey) throws StatsUnavailableException {
        mStatsManager.removeConfig(configKey);
    }

    /**
     * Forwarding logic for {@link StatsManager} as it is final and cannot be mocked.
     *
     * @hide
     */
    @VisibleForTesting
    protected byte[] getStatsReports(long configKey) throws StatsUnavailableException {
        return mStatsManager.getReports(configKey);
    }

    /**
     * Allow tests to stub out getting instrumentation arguments.
     *
     * @hide
     */
    @VisibleForTesting
    protected Bundle getArguments() {
        return InstrumentationRegistry.getArguments();
    }

    /**
     * Allow tests to stub out file I/O.
     *
     * @hide
     */
    @VisibleForTesting
    protected File writeToFile(File f, byte[] content) throws IOException {
        Files.write(f.toPath(), content);
        return f;
    }

    /**
     * Allow tests to override the random ID generation. The config is passed in to allow a specific
     * ID to be associated with a config in the test.
     *
     * @hide
     */
    @VisibleForTesting
    protected long getUniqueIdForConfig(StatsdConfig config) {
        return (long) UUID.randomUUID().hashCode();
    }

    /**
     * Allow tests to stub out {@link AssetManager} interactions as that class is final and cannot .
     * be mocked.
     *
     * @hide
     */
    @VisibleForTesting
    protected InputStream openConfigWithAssetManager(AssetManager manager, String configName)
            throws IOException {
        String configFilePath =
                Paths.get(CONFIG_SUB_DIRECTORY, configName + PROTO_EXTENSION).toString();
        return manager.open(configFilePath);
    }

    /**
     * Parse a config from its name using {@link AssetManager}.
     *
     * <p>The option name is passed in for better error messaging.
     */
    private StatsdConfig parseConfigFromName(
            final AssetManager manager, String optionName, String configName) {
        try (InputStream configStream = openConfigWithAssetManager(manager, configName)) {
            try {
                return fixPermissions(StatsdConfig.parseFrom(configStream));
            } catch (IOException e) {
                throw new RuntimeException(
                        String.format(
                                "Cannot parse config %s in option %s.", configName, optionName),
                        e);
            }
        } catch (IOException e) {
            throw new IllegalArgumentException(
                    String.format(
                            "Config name %s in option %s does not exist", configName, optionName));
        }
    }

    /**
     * Parse the suppplied option to get a set of statsd configs keyed by their names.
     *
     * @hide
     */
    @VisibleForTesting
    protected Map<String, StatsdConfig> getConfigsFromOption(String optionName) {
        List<String> configNames =
                Arrays.asList(getArguments().getString(optionName, "").split(","))
                        .stream()
                        .map(s -> s.trim())
                        .filter(s -> !s.isEmpty())
                        .distinct()
                        .collect(Collectors.toList());
        // Look inside the APK assets for the configuration file.
        final AssetManager manager = InstrumentationRegistry.getContext().getAssets();
        return configNames
                .stream()
                .collect(
                        Collectors.toMap(
                                Function.identity(),
                                configName ->
                                        parseConfigFromName(manager, optionName, configName)));
    }

    /**
     * Log a "start" AppBreadcrumbReported event to statsd. Wraps a static method for testing.
     *
     * @hide
     */
    @VisibleForTesting
    protected boolean logStart(int label) {
        return StatsLog.logStart(label);
    }

    /**
     * Log a "stop" AppBreadcrumbReported event to statsd. Wraps a static method for testing.
     *
     * @hide
     */
    @VisibleForTesting
    protected boolean logStop(int label) {
        return StatsLog.logStop(label);
    }

    /**
     * Add a few permission-related options to the statsd config.
     *
     * <p>This is related to some new permission restrictions in RVC.
     */
    private StatsdConfig fixPermissions(StatsdConfig config) {
        StatsdConfig.Builder builder = config.toBuilder();
        // Allow system power stats to be pulled.
        builder.addDefaultPullPackages("AID_SYSTEM");
        // Gauge metrics rely on AppBreadcrumbReported as metric dump triggers.
        builder.addWhitelistedAtomIds(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER);

        return builder.build();
    }
}
