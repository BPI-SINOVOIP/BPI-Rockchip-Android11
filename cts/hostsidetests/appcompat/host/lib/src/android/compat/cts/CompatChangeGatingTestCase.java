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

package android.compat.cts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.internal.os.StatsdConfigProto;
import com.android.os.AtomsProto;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

import com.google.common.io.Files;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

import javax.annotation.Nonnull;

// Shamelessly plagiarised from incident's ProtoDumpTestCase and statsd's BaseTestCase family
public class CompatChangeGatingTestCase extends DeviceTestCase implements IBuildReceiver {
    protected IBuildInfo mCtsBuild;

    private static final String UPDATE_CONFIG_CMD = "cat %s | cmd stats config update %d";
    private static final String DUMP_REPORT_CMD =
            "cmd stats dump-report %d --include_current_bucket --proto";
    private static final String REMOVE_CONFIG_CMD = "cmd stats config remove %d";

    private static final String TEST_RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        assertThat(mCtsBuild).isNotNull();
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    /**
     * Install a device side test package.
     *
     * @param appFileName      Apk file name, such as "CtsNetStatsApp.apk".
     * @param grantPermissions whether to give runtime permissions.
     */
    protected void installPackage(String appFileName, boolean grantPermissions)
            throws FileNotFoundException, DeviceNotAvailableException {
        CLog.d("Installing app " + appFileName);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        final String result = getDevice().installPackage(buildHelper.getTestFile(appFileName), true,
                grantPermissions);
        assertWithMessage("Failed to install %s: %s", appFileName, result).that(result).isNull();
    }

    /**
     * Uninstall a device side test package.
     *
     * @param appFileName      Apk file name, such as "CtsNetStatsApp.apk".
     * @param shouldSucceed    Whether to assert on failure.
     */
    protected void uninstallPackage(String packageName, boolean shouldSucceed)
            throws DeviceNotAvailableException {
        final String result = getDevice().uninstallPackage(packageName);
        if (shouldSucceed) {
            assertWithMessage("uninstallPackage(%s) failed: %s", packageName, result)
                .that(result).isNull();
        }
    }

    /**
     * Run a device side compat test.
     *
     * @param pkgName         Test package name, such as
     *                        "com.android.server.cts.netstats".
     * @param testClassName   Test class name; either a fully qualified name, or "."
     *                        + a class name.
     * @param testMethodName  Test method name.
     * @param enabledChanges  Set of compat changes to enable.
     * @param disabledChanges Set of compat changes to disable.
     */
    protected void runDeviceCompatTest(@Nonnull String pkgName, @Nonnull String testClassName,
            @Nonnull String testMethodName,
            Set<Long> enabledChanges, Set<Long> disabledChanges)
            throws DeviceNotAvailableException {
      runDeviceCompatTestReported(pkgName, testClassName, testMethodName, enabledChanges,
          disabledChanges, enabledChanges, disabledChanges);
    }

    /**
     * Run a device side compat test where not all changes are reported through statsd.
     *
     * @param pkgName        Test package name, such as
     *                       "com.android.server.cts.netstats".
     * @param testClassName  Test class name; either a fully qualified name, or "."
     *                       + a class name.
     * @param testMethodName Test method name.
     * @param enabledChanges  Set of compat changes to enable.
     * @param disabledChanges Set of compat changes to disable.
     * @param reportedEnabledChanges Expected enabled changes in statsd report.
     * @param reportedDisabledChanges Expected disabled changes in statsd report.
     */
    protected void runDeviceCompatTestReported(@Nonnull String pkgName, @Nonnull String testClassName,
            @Nonnull String testMethodName,
            Set<Long> enabledChanges, Set<Long> disabledChanges,
            Set<Long> reportedEnabledChanges, Set<Long> reportedDisabledChanges)
            throws DeviceNotAvailableException {
        // Set compat overrides
        setCompatConfig(enabledChanges, disabledChanges, pkgName);

        // Send statsd config
        final long configId = getClass().getCanonicalName().hashCode();
        createAndUploadStatsdConfig(configId, pkgName);

        // Run device-side test
        if (testClassName.startsWith(".")) {
            testClassName = pkgName + testClassName;
        }
        RemoteAndroidTestRunner testRunner = new RemoteAndroidTestRunner(pkgName, TEST_RUNNER,
                getDevice().getIDevice());
        testRunner.setMethodName(testClassName, testMethodName);
        CollectingTestListener listener = new CollectingTestListener();
        assertThat(getDevice().runInstrumentationTests(testRunner, listener)).isTrue();

        // Clear overrides.
        resetCompatChanges(enabledChanges, pkgName);
        resetCompatChanges(disabledChanges, pkgName);

        // Clear statsd report data and remove config
        Map<Long, Boolean> reportedChanges = getReportedChanges(configId, pkgName);
        removeStatsdConfig(configId);

        // Check that device side test occurred as expected
        final TestRunResult result = listener.getCurrentRunResults();
        assertWithMessage("Failed to successfully run device tests for %s: %s",
                          result.getName(), result.getRunFailureMessage())
                .that(result.isRunFailure()).isFalse();
        assertWithMessage("Should run only exactly one test method!")
                .that(result.getNumTests()).isEqualTo(1);
        if (result.hasFailedTests()) {
            // build a meaningful error message
            StringBuilder errorBuilder = new StringBuilder("On-device test failed:\n");
            for (Map.Entry<TestDescription, TestResult> resultEntry :
                    result.getTestResults().entrySet()) {
                if (!resultEntry.getValue().getStatus().equals(TestStatus.PASSED)) {
                    errorBuilder.append(resultEntry.getKey().toString());
                    errorBuilder.append(":\n");
                    errorBuilder.append(resultEntry.getValue().getStackTrace());
                }
            }
            throw new AssertionError(errorBuilder.toString());
        }

        // Validate statsd report
        validatePostRunStatsdReport(reportedChanges, reportedEnabledChanges,
            reportedDisabledChanges);
    }

    /**
     * Gets the statsd report. Note that this also deletes that report from statsd.
     */
    private List<ConfigMetricsReport> getReportList(long configId) throws DeviceNotAvailableException {
        try {
            final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
            getDevice().executeShellCommand(String.format(DUMP_REPORT_CMD, configId), receiver);
            return ConfigMetricsReportList.parser()
                    .parseFrom(receiver.getOutput())
                    .getReportsList();
        } catch (InvalidProtocolBufferException e) {
            throw new IllegalStateException("Failed to fetch and parse the statsd output report.",
                    e);
        }
    }

    /**
     * Creates and uploads a statsd config that matches the AppCompatibilityChangeReported atom
     * logged by a given package name.
     *
     * @param configId A unique config id.
     * @param pkgName  The package name of the app that is expected to report the atom. It will be
     *                 the only allowed log source.
     */
    private void createAndUploadStatsdConfig(long configId, String pkgName)
            throws DeviceNotAvailableException {
        final String atomName = "Atom" + System.nanoTime();
        final String eventName = "Event" + System.nanoTime();
        final ITestDevice device = getDevice();

        StatsdConfigProto.StatsdConfig.Builder configBuilder =
                StatsdConfigProto.StatsdConfig.newBuilder()
                        .setId(configId)
                        .addAllowedLogSource(pkgName)
                        .addWhitelistedAtomIds(Atom.APP_COMPATIBILITY_CHANGE_REPORTED_FIELD_NUMBER);
        StatsdConfigProto.SimpleAtomMatcher.Builder simpleAtomMatcherBuilder =
                StatsdConfigProto.SimpleAtomMatcher
                        .newBuilder().setAtomId(
                        Atom.APP_COMPATIBILITY_CHANGE_REPORTED_FIELD_NUMBER);
        configBuilder.addAtomMatcher(
                StatsdConfigProto.AtomMatcher.newBuilder()
                        .setId(atomName.hashCode())
                        .setSimpleAtomMatcher(simpleAtomMatcherBuilder));
        configBuilder.addEventMetric(
                StatsdConfigProto.EventMetric.newBuilder()
                        .setId(eventName.hashCode())
                        .setWhat(atomName.hashCode()));
        StatsdConfigProto.StatsdConfig config = configBuilder.build();
        try {
            File configFile = File.createTempFile("statsdconfig", ".config");
            configFile.deleteOnExit();
            Files.write(config.toByteArray(), configFile);
            String remotePath = "/data/local/tmp/" + configFile.getName();
            device.pushFile(configFile, remotePath);
            device.executeShellCommand(String.format(UPDATE_CONFIG_CMD, remotePath, configId));
            device.executeShellCommand("rm " + remotePath);
        } catch (IOException e) {
            throw new RuntimeException("IO error when writing to temp file.", e);
        }
    }

    /**
     * Gets the uid of the test app.
     */
    protected int getUid(@Nonnull String packageName) throws DeviceNotAvailableException {
        int currentUser = getDevice().getCurrentUser();
        String uidLine = getDevice()
                .executeShellCommand(
                        "cmd package list packages -U --user " + currentUser + " "
                                + packageName);
        String[] uidLineParts = uidLine.split(":");
        // 3rd entry is package uid
        assertThat(uidLineParts.length).isGreaterThan(2);
        int uid = Integer.parseInt(uidLineParts[2].trim());
        assertThat(uid).isGreaterThan(10000);
        return uid;
    }

    /**
     * Set the compat config using adb.
     *
     * @param enabledChanges  Changes to be enabled.
     * @param disabledChanges Changes to be disabled.
     * @param packageName     Package name for the app whose config is being changed.
     */
    private void setCompatConfig(Set<Long> enabledChanges, Set<Long> disabledChanges,
            @Nonnull String packageName) throws DeviceNotAvailableException {
        for (Long enabledChange : enabledChanges) {
            runCommand("am compat enable " + enabledChange + " " + packageName);
        }
        for (Long disabledChange : disabledChanges) {
            runCommand("am compat disable " + disabledChange + " " + packageName);
        }
    }

    /**
     * Reset changes to default for a package.
     */
    private void resetCompatChanges(Set<Long> changes, @Nonnull String packageName)
            throws DeviceNotAvailableException {
        for (Long change : changes) {
            runCommand("am compat reset " + change + " " + packageName);
        }
    }

    /**
     * Remove statsd config for a given id.
     */
    private void removeStatsdConfig(long configId) throws DeviceNotAvailableException {
        getDevice().executeShellCommand(
                String.join(" ", REMOVE_CONFIG_CMD, String.valueOf(configId)));
    }

    /**
     * Get the compat changes that were logged.
     */
    private Map<Long, Boolean> getReportedChanges(long configId, String pkgName)
            throws DeviceNotAvailableException {
        final int packageUid = getUid(pkgName);
        return getReportList(configId).stream()
                .flatMap(report -> report.getMetricsList().stream())
                .flatMap(metric -> metric.getEventMetrics().getDataList().stream())
                .filter(eventMetricData -> eventMetricData.hasAtom())
                .map(eventMetricData -> eventMetricData.getAtom())
                .map(atom -> atom.getAppCompatibilityChangeReported())
                .filter(atom -> atom != null && atom.getUid() == packageUid) // Should be redundant
                .collect(Collectors.toMap(
                        atom -> atom.getChangeId(), // Key
                        atom -> atom.getState() ==  // Value
                                AtomsProto.AppCompatibilityChangeReported.State.ENABLED));
    }

    /**
     * Validate that all overridden changes were logged while running the test.
     */
    private void validatePostRunStatsdReport(Map<Long, Boolean> reportedChanges,
            Set<Long> enabledChanges, Set<Long> disabledChanges)
            throws DeviceNotAvailableException {
        for (Long enabledChange : enabledChanges) {
            assertThat(reportedChanges)
                    .containsEntry(enabledChange, true);
        }
        for (Long disabledChange : disabledChanges) {
            assertThat(reportedChanges)
                    .containsEntry(disabledChange, false);
        }
    }

    /**
     * Execute the given command, and returns the output.
     */
    protected String runCommand(String command) throws DeviceNotAvailableException {
        final CollectingOutputReceiver receiver = new CollectingOutputReceiver();
        getDevice().executeShellCommand(command, receiver);
        return receiver.getOutput();
    }
}
