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

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.AssetManager;
import android.os.Bundle;

import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.ConfigMetricsReportList.ConfigKey;
import com.google.common.collect.ImmutableMap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.Description;
import org.junit.runner.Result;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Map;

/** Unit tests for {@link StatsdListener}. */
public class StatsdListenerTest {
    private StatsdListener mListener;

    private static final String CONFIG_NAME_1 = "config-1";
    private static final String CONFIG_NAME_2 = "config-2";

    private static final long CONFIG_ID_1 = 1;
    private static final long CONFIG_ID_2 = 2;

    private static class DummyTest {}

    private static final Class<?> TEST_CLASS = DummyTest.class;
    private static final String TEST_METHOD_NAME_1 = "testMethodOne";
    private static final String TEST_METHOD_NAME_2 = "testMethodTwo";

    private static final StatsdConfig CONFIG_1 =
            StatsdConfig.newBuilder().setId(CONFIG_ID_1).build();
    private static final StatsdConfig CONFIG_2 =
            StatsdConfig.newBuilder().setId(CONFIG_ID_2).build();

    private static final ConfigMetricsReportList REPORT_1 =
            ConfigMetricsReportList.newBuilder()
                    .setConfigKey(ConfigKey.newBuilder().setUid(0).setId(CONFIG_ID_1))
                    .build();
    private static final ConfigMetricsReportList REPORT_2 =
            ConfigMetricsReportList.newBuilder()
                    .setConfigKey(ConfigKey.newBuilder().setUid(0).setId(CONFIG_ID_2))
                    .build();

    private static final ImmutableMap<String, StatsdConfig> CONFIG_MAP =
            ImmutableMap.of(CONFIG_NAME_1, CONFIG_1, CONFIG_NAME_2, CONFIG_2);

    @Rule public ExpectedException mExpectedException = ExpectedException.none();

    @Before
    public void setUp() throws Exception {
        mListener = spy(new StatsdListener());
        // Stub the report collection to isolate collector from StatsManager.
        doNothing().when(mListener).addStatsConfig(anyLong(), any());
        doReturn(REPORT_1.toByteArray()).when(mListener).getStatsReports(eq(CONFIG_ID_1));
        doReturn(REPORT_2.toByteArray()).when(mListener).getStatsReports(eq(CONFIG_ID_2));
        doNothing().when(mListener).removeStatsConfig(anyLong());
        // Stub calls to permission APIs.
        doNothing().when(mListener).adoptShellPermissionIdentity();
        doNothing().when(mListener).dropShellPermissionIdentity();
        // Stub calls to StatsLog APIs.
        doReturn(true).when(mListener).logStart(anyInt());
        doReturn(true).when(mListener).logStop(anyInt());
        // Stub file I/O.
        doAnswer(invocation -> invocation.getArgument(0)).when(mListener).writeToFile(any(), any());
        // Stub randome UUID generation.
        doReturn(CONFIG_ID_1).when(mListener).getUniqueIdForConfig(eq(CONFIG_1));
        doReturn(CONFIG_ID_2).when(mListener).getUniqueIdForConfig(eq(CONFIG_2));
    }

    /** Test that the collector has correct interactions with statsd for per-run collection. */
    @Test
    public void testRunLevelCollection_statsdInteraction() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_RUN_LEVEL));

        DataRecord runData = new DataRecord();
        Description description = Description.createSuiteDescription("TestRun");

        mListener.onTestRunStart(runData, description);
        verify(mListener, times(1)).addStatsConfig(eq(CONFIG_ID_1), eq(CONFIG_1.toByteArray()));
        verify(mListener, times(1)).addStatsConfig(eq(CONFIG_ID_2), eq(CONFIG_2.toByteArray()));
        verify(mListener, times(1)).logStart(eq(StatsdListener.RUN_EVENT_LABEL));

        mListener.onTestRunEnd(runData, new Result());
        verify(mListener, times(1)).logStop(eq(StatsdListener.RUN_EVENT_LABEL));
        verify(mListener, times(1)).getStatsReports(eq(CONFIG_ID_1));
        verify(mListener, times(1)).getStatsReports(eq(CONFIG_ID_2));
        verify(mListener, times(1)).removeStatsConfig(eq(CONFIG_ID_1));
        verify(mListener, times(1)).removeStatsConfig(eq(CONFIG_ID_2));
    }

    /** Test that the collector dumps reports and report them as metrics. */
    @Test
    public void testRunLevelCollection_metrics() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_RUN_LEVEL));

        // Mock the DataRecord class as its content is not directly visible.
        DataRecord runData = mock(DataRecord.class);
        Description description = Description.createSuiteDescription("TestRun");

        mListener.onTestRunStart(runData, description);
        mListener.onTestRunEnd(runData, new Result());

        verify(mListener, times(1))
                .writeToFile(
                        getExactFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_RUN_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX
                                        + CONFIG_NAME_1
                                        + StatsdListener.PROTO_EXTENSION),
                        any());
        verify(runData, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getExactFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_RUN_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX
                                        + CONFIG_NAME_1
                                        + StatsdListener.PROTO_EXTENSION));
        verify(mListener, times(1))
                .writeToFile(
                        getExactFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_RUN_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX
                                        + CONFIG_NAME_2
                                        + StatsdListener.PROTO_EXTENSION),
                        any());
        verify(runData, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_2),
                        getExactFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_RUN_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX
                                        + CONFIG_NAME_2
                                        + StatsdListener.PROTO_EXTENSION));
    }

    /** Test that the collector has correct interactions with statsd for per-test collection. */
    @Test
    public void testTestLevelCollection_statsdInteraction() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_TEST_LEVEL));

        DataRecord testData = new DataRecord();
        Description description = Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_1);

        // onTestRunStart(...) has to be called because the arguments are parsed here.
        mListener.onTestRunStart(
                new DataRecord(), Description.createSuiteDescription("Placeholder"));

        mListener.onTestStart(testData, description);
        verify(mListener, times(1)).addStatsConfig(eq(CONFIG_ID_1), eq(CONFIG_1.toByteArray()));
        verify(mListener, times(1)).addStatsConfig(eq(CONFIG_ID_2), eq(CONFIG_2.toByteArray()));
        verify(mListener, times(1)).logStart(eq(StatsdListener.TEST_EVENT_LABEL));

        mListener.onTestEnd(testData, description);
        verify(mListener, times(1)).logStop(eq(StatsdListener.TEST_EVENT_LABEL));
        verify(mListener, times(1)).getStatsReports(eq(CONFIG_ID_1));
        verify(mListener, times(1)).getStatsReports(eq(CONFIG_ID_2));
        verify(mListener, times(1)).removeStatsConfig(eq(CONFIG_ID_1));
        verify(mListener, times(1)).removeStatsConfig(eq(CONFIG_ID_2));

        mListener.onTestRunEnd(new DataRecord(), new Result());
    }

    /** Test that the collector dumps report and reports them as metric per test. */
    @Test
    public void testTestLevelCollection_metrics() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_TEST_LEVEL));

        DataRecord testData = mock(DataRecord.class);
        Description description = Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_1);

        // onTestRunStart(...) has to be called because the arguments are parsed here.
        mListener.onTestRunStart(
                new DataRecord(), Description.createSuiteDescription("Placeholder"));

        mListener.onTestStart(testData, description);
        mListener.onTestEnd(testData, description);

        verify(mListener, times(1))
                .writeToFile(
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX,
                                description.getClassName(),
                                TEST_METHOD_NAME_1,
                                CONFIG_NAME_1,
                                StatsdListener.PROTO_EXTENSION),
                        any());
        verify(testData, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX,
                                description.getClassName(),
                                TEST_METHOD_NAME_1,
                                CONFIG_NAME_1,
                                StatsdListener.PROTO_EXTENSION));
        verify(mListener, times(1))
                .writeToFile(
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX,
                                description.getClassName(),
                                TEST_METHOD_NAME_1,
                                CONFIG_NAME_2,
                                StatsdListener.PROTO_EXTENSION),
                        any());
        verify(testData, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_2),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX,
                                description.getClassName(),
                                TEST_METHOD_NAME_1,
                                CONFIG_NAME_2,
                                StatsdListener.PROTO_EXTENSION));

        mListener.onTestRunEnd(new DataRecord(), new Result());
    }

    /** Test that the collector handles multiple test correctly for per-test collection. */
    @Test
    public void testTestLevelCollection_multipleTests() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_TEST_LEVEL));

        // onTestRunStart(...) has to be called because the arguments are parsed here.
        mListener.onTestRunStart(
                new DataRecord(), Description.createSuiteDescription("Placeholder"));

        DataRecord testData1 = mock(DataRecord.class);
        Description description1 =
                Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_1);

        mListener.onTestStart(testData1, description1);
        mListener.onTestEnd(testData1, description1);

        verify(testData1, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                description1.getClassName(),
                                TEST_METHOD_NAME_1,
                                String.valueOf(1)));

        DataRecord testData2 = mock(DataRecord.class);
        Description description2 =
                Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_2);

        mListener.onTestStart(testData2, description2);
        mListener.onTestEnd(testData2, description2);

        verify(testData2, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                description2.getClassName(),
                                TEST_METHOD_NAME_2,
                                String.valueOf(1)));

        mListener.onTestRunEnd(new DataRecord(), new Result());
    }

    /** Test that the collector handles multiple iterations correctly for per-test collection. */
    @Test
    public void testTestLevelCollection_multipleIterations() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_TEST_LEVEL));

        // onTestRunStart(...) has to be called because the arguments are parsed here.
        mListener.onTestRunStart(
                new DataRecord(), Description.createSuiteDescription("Placeholder"));

        DataRecord testData1 = mock(DataRecord.class);
        Description description1 =
                Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_1);

        mListener.onTestStart(testData1, description1);
        mListener.onTestEnd(testData1, description1);

        // The metric file name should contain the iteration number (1).
        verify(testData1, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                description1.getClassName(),
                                TEST_METHOD_NAME_1,
                                String.valueOf(1)));

        DataRecord testData2 = mock(DataRecord.class);
        Description description2 =
                Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_1);

        mListener.onTestStart(testData2, description2);
        mListener.onTestEnd(testData2, description2);

        // The metric file name should contain the iteration number (2).
        verify(testData2, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                description2.getClassName(),
                                TEST_METHOD_NAME_1,
                                String.valueOf(2)));

        mListener.onTestRunEnd(new DataRecord(), new Result());
    }

    /** Test that the collector can perform both run- and test-level collection in the same run. */
    @Test
    public void testRunAndTestLevelCollection() throws Exception {
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_RUN_LEVEL));
        doReturn(CONFIG_MAP)
                .when(mListener)
                .getConfigsFromOption(eq(StatsdListener.OPTION_CONFIGS_TEST_LEVEL));

        DataRecord runData = mock(DataRecord.class);
        Description runDescription = Description.createSuiteDescription("TestRun");

        mListener.onTestRunStart(runData, runDescription);

        DataRecord testData = mock(DataRecord.class);
        Description testDescription =
                Description.createTestDescription(TEST_CLASS, TEST_METHOD_NAME_1);

        mListener.onTestStart(testData, testDescription);
        mListener.onTestEnd(testData, testDescription);

        verify(testData, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getPartialFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_TEST_LEVEL)
                                        .toString(),
                                testDescription.getClassName(),
                                TEST_METHOD_NAME_1,
                                String.valueOf(1)));

        mListener.onTestRunEnd(runData, new Result());

        verify(runData, times(1))
                .addFileMetric(
                        eq(StatsdListener.REPORT_KEY_PREFIX + CONFIG_NAME_1),
                        getExactFileNameMatcher(
                                Paths.get(
                                                StatsdListener.REPORT_PATH_ROOT,
                                                StatsdListener.REPORT_PATH_RUN_LEVEL)
                                        .toString(),
                                StatsdListener.REPORT_FILENAME_PREFIX
                                        + CONFIG_NAME_1
                                        + StatsdListener.PROTO_EXTENSION));
    }

    /** Test that the collector parses the configs from arguments correctly for valid configs. */
    @Test
    public void testParsingConfigFromArguments_validConfig() throws Exception {
        // Stub two configs for testing.
        ByteArrayInputStream config1Stream = new ByteArrayInputStream(CONFIG_1.toByteArray());
        doReturn(config1Stream)
                .when(mListener)
                .openConfigWithAssetManager(any(AssetManager.class), eq(CONFIG_NAME_1));

        ByteArrayInputStream config2Stream = new ByteArrayInputStream(CONFIG_2.toByteArray());
        doReturn(config2Stream)
                .when(mListener)
                .openConfigWithAssetManager(any(AssetManager.class), eq(CONFIG_NAME_2));

        Bundle args = new Bundle();
        args.putString(
                StatsdListener.OPTION_CONFIGS_RUN_LEVEL,
                String.join(",", CONFIG_NAME_1, CONFIG_NAME_2));
        doReturn(args).when(mListener).getArguments();

        Map<String, StatsdConfig> configs =
                mListener.getConfigsFromOption(StatsdListener.OPTION_CONFIGS_RUN_LEVEL);
        Assert.assertTrue(configs.containsKey(CONFIG_NAME_1));
        Assert.assertEquals(configs.get(CONFIG_NAME_1).getId(), CONFIG_ID_1);
        Assert.assertTrue(configs.containsKey(CONFIG_NAME_2));
        Assert.assertEquals(configs.get(CONFIG_NAME_2).getId(), CONFIG_ID_2);
        Assert.assertEquals(configs.size(), 2);
    }

    /** Test that the colletor fails and throws the right exception for an invalid config. */
    @Test
    public void testParsingConfigFromArguments_malformedConfig() throws Exception {
        // Set up an invalid config for testing.
        ByteArrayInputStream configStream = new ByteArrayInputStream("not a config".getBytes());
        doReturn(configStream)
                .when(mListener)
                .openConfigWithAssetManager(any(AssetManager.class), eq(CONFIG_NAME_1));

        Bundle args = new Bundle();
        args.putString(StatsdListener.OPTION_CONFIGS_RUN_LEVEL, CONFIG_NAME_1);
        doReturn(args).when(mListener).getArguments();

        mExpectedException.expectMessage("Cannot parse");
        Map<String, StatsdConfig> configs =
                mListener.getConfigsFromOption(StatsdListener.OPTION_CONFIGS_RUN_LEVEL);
    }

    /** Test that the collector fails and throws the right exception for a nonexistent config. */
    @Test
    public void testParsingConfigFromArguments_nonexistentConfig() {
        Bundle args = new Bundle();
        args.putString(StatsdListener.OPTION_CONFIGS_RUN_LEVEL, "nah");
        doReturn(args).when(mListener).getArguments();

        mExpectedException.expectMessage("does not exist");
        Map<String, StatsdConfig> configs =
                mListener.getConfigsFromOption(StatsdListener.OPTION_CONFIGS_RUN_LEVEL);
    }

    /** Test that the collector has no effect when no config arguments are supplied. */
    @Test
    public void testNoConfigArguments() throws Exception {
        doReturn(new Bundle()).when(mListener).getArguments();

        // Mock the DataRecord class as its content is not directly visible.
        DataRecord runData = mock(DataRecord.class);
        Description description = Description.createSuiteDescription("TestRun");

        mListener.onTestRunStart(runData, description);
        mListener.onTestRunEnd(runData, new Result());

        verify(runData, never()).addFileMetric(any(), any());
        verify(mListener, never()).addStatsConfig(anyLong(), any());
        verify(mListener, never()).getStatsReports(anyLong());
        verify(mListener, never()).removeStatsConfig(anyLong());
    }

    /**
     * Test that the collector can work with arbitrarily constructed test descriptions.
     *
     * <p>This test was created as some runners will create new descriptions on the fly.
     */
    @Test
    public void testArbitraryTestDescription() {
        // Creates a description with no test class and method.
        Description arbitraryDescription = Description.createSuiteDescription("some_test");

        // The test passes if no exceptions are thrown in these callbacks.
        mListener.onTestStart(mock(DataRecord.class), arbitraryDescription);
        mListener.onTestEnd(mock(DataRecord.class), arbitraryDescription);
    }

    /** Returns a Mockito argument matcher that matches the exact file name. */
    private File getExactFileNameMatcher(String parentName, String filename) {
        return argThat(f -> f.getParent().contains(parentName) && f.getName().equals(filename));
    }

    /** Returns a Mockito argument matcher that matche a file name to one or more substrings. */
    private File getPartialFileNameMatcher(
            String parentName, String component, String... moreComponents) {
        return argThat(
                f ->
                        f.getParent().contains(parentName)
                                && f.getName().contains(component)
                                && Arrays.stream(moreComponents)
                                        .allMatch(c -> f.getName().contains(c)));
    }

    /** Test that configs are parsed and applied with correct permission fixes. */
    @Test
    public void testConfigsHavePermissionFixes() throws Exception {
        // Stub a config for testing.
        ByteArrayInputStream configStream = new ByteArrayInputStream(CONFIG_1.toByteArray());
        doReturn(configStream)
                .when(mListener)
                .openConfigWithAssetManager(any(AssetManager.class), eq(CONFIG_NAME_1));

        Bundle args = new Bundle();
        args.putString(StatsdListener.OPTION_CONFIGS_RUN_LEVEL, CONFIG_NAME_1);
        doReturn(args).when(mListener).getArguments();

        Map<String, StatsdConfig> configs =
                mListener.getConfigsFromOption(StatsdListener.OPTION_CONFIGS_RUN_LEVEL);
        Assert.assertTrue(configs.containsKey(CONFIG_NAME_1));
        Assert.assertTrue(
                configs.get(CONFIG_NAME_1)
                        .getWhitelistedAtomIdsList()
                        .stream()
                        .anyMatch(id -> id == Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER));
        Assert.assertTrue(
                configs.get(CONFIG_NAME_1)
                        .getDefaultPullPackagesList()
                        .stream()
                        .anyMatch(name -> "AID_SYSTEM".equals(name)));
    }
}
