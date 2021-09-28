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
package com.android.compatibility.common.tradefed.result.suite;

import static org.junit.Assert.*;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.targetprep.BuildFingerPrintPreparer;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInvocation;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.util.FileUtil;

import com.google.protobuf.Any;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.FileOutputStream;

/**
 * Unit tests for {@link PreviousResultLoader}.
 */
@RunWith(JUnit4.class)
public class PreviousResultLoaderTest {

    public static final String RUN_HISTORY_KEY = "run_history";

    private PreviousResultLoader mLoader;
    private IInvocationContext mContext;
    private File mRootDir;
    private File mProtoFile;

    private ITestDevice mMockDevice;
    private IBuildProvider mMockProvider;

    @Before
    public void setUp() throws Exception {
        mMockProvider = EasyMock.createMock(IBuildProvider.class);
        mLoader = new PreviousResultLoader();
        mLoader.setProvider(mMockProvider);
        OptionSetter setter = new OptionSetter(mLoader);
        setter.setOptionValue("retry", "0");
        mContext = new InvocationContext();
        mContext.setConfigurationDescriptor(new ConfigurationDescriptor());
        mContext.addInvocationAttribute(TestInvocation.COMMAND_ARGS_KEY,
                "cts -m CtsGesture --skip-all-system-status-check");
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());
        mMockDevice = EasyMock.createMock(ITestDevice.class);
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mRootDir);
    }

    /**
     * Test the loader properly fails when the results are not loaded.
     */
    @Test
    public void testReloadTests_failed() throws Exception {
        EasyMock.expect(mMockProvider.getBuild()).andReturn(createFakeBuild("", false));
        // Delete the proto file
        mProtoFile.delete();
        try {
            EasyMock.replay(mMockProvider);
            mLoader.init();
            fail("Should have thrown an exception.");
        } catch (RuntimeException expected) {
            // expected
            assertEquals("Could not find any test-record.pb to load.", expected.getMessage());
        }
        EasyMock.verify(mMockProvider);
    }

    /**
     * Test that the loader can provide the results information back.
     */
    @Test
    public void testReloadTests() throws Exception {
        final String EXPECTED_RUN_HISTORY =
                "[{\"startTime\":1530218251501,"
                        + "\"endTime\":1530218261061,"
                        + "\"passedTests\":0,"
                        + "\"failedTests\":0,"
                        + "\"commandLineArgs\":\"cts -m CtsGesture "
                        + "--skip-all-system-status-check\","
                        + "\"hostName\":\"user.android.com\"}]";
        EasyMock.expect(mMockProvider.getBuild())
                .andReturn(createFakeBuild(createBasicResults(), false));
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);

        EasyMock.replay(mMockDevice, mMockProvider);
        mLoader.init();
        assertEquals("cts -m CtsGesture --skip-all-system-status-check", mLoader.getCommandLine());
        IConfiguration config = new Configuration("name", "desc");
        assertEquals(0, config.getTargetPreparers().size());
        mLoader.customizeConfiguration(config);
        // A special preparer was added for fingerprint
        assertEquals(1, config.getTargetPreparers().size());
        ITargetPreparer preparer = config.getTargetPreparers().get(0);
        assertTrue(preparer instanceof BuildFingerPrintPreparer);
        assertEquals(
                "testfingerprint", ((BuildFingerPrintPreparer) preparer).getExpectedFingerprint());
        String runHistory =
                config.getCommandOptions().getInvocationData().getUniqueMap().get(RUN_HISTORY_KEY);
        assertEquals(EXPECTED_RUN_HISTORY, runHistory);
        EasyMock.verify(mMockDevice, mMockProvider);
    }

    @Test
    public void testReloadTests_withMultiProto() throws Exception {
        final String EXPECTED_RUN_HISTORY =
                "[{\"startTime\":1530218251501,"
                        + "\"endTime\":1530218261061,"
                        + "\"passedTests\":0,"
                        + "\"failedTests\":0,"
                        + "\"commandLineArgs\":\"cts -m CtsGesture "
                        + "--skip-all-system-status-check\","
                        + "\"hostName\":\"user.android.com\"}]";
        EasyMock.expect(mMockProvider.getBuild())
                .andReturn(createFakeBuild(createBasicResults(), true));
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);

        EasyMock.replay(mMockDevice, mMockProvider);
        mLoader.init();
        assertEquals("cts -m CtsGesture --skip-all-system-status-check", mLoader.getCommandLine());
        CollectingTestListener listener = mLoader.loadPreviousResults();
        assertNotNull(listener);
        IConfiguration config = new Configuration("name", "desc");
        assertEquals(0, config.getTargetPreparers().size());
        mLoader.customizeConfiguration(config);
        // A special preparer was added for fingerprint
        assertEquals(1, config.getTargetPreparers().size());
        ITargetPreparer preparer = config.getTargetPreparers().get(0);
        assertTrue(preparer instanceof BuildFingerPrintPreparer);
        assertEquals("testfingerprint",
                ((BuildFingerPrintPreparer) preparer).getExpectedFingerprint());
        String runHistory =
                config.getCommandOptions().getInvocationData().getUniqueMap().get(RUN_HISTORY_KEY);
        assertEquals(EXPECTED_RUN_HISTORY, runHistory);
        EasyMock.verify(mMockDevice, mMockProvider);
    }

    /** Test that the loader can correctly provide the run history back. */
    @Test
    public void testReloadTests_withRunHistory() throws Exception {
        final String RUN_HISTORY_1 =
                "{\"startTime\":1000000000000,"
                        + "\"endTime\":1000000010000,"
                        + "\"passedTests\":10,"
                        + "\"failedTests\":5,"
                        + "\"commandLineArgs\":\"cts -m CtsGesture --skip-all-system-status-check\","
                        + "\"hostName\":\"user1.android.com\"}";
        final String RUN_HISTORY_2 =
                "{\"startTime\":1530218251501,"
                        + "\"endTime\":1530218261061,"
                        + "\"passedTests\":0,"
                        + "\"failedTests\":0,"
                        + "\"commandLineArgs\":\"cts -m CtsGesture --skip-all-system-status-check "
                        + "--shard-count 5\","
                        + "\"hostName\":\"user2.android.com\"}";
        final String OLD_RUN_HISTORY = String.format("[%s]", RUN_HISTORY_1);
        final String EXPECTED_RUN_HISTORY = String.format("[%s,%s]", RUN_HISTORY_1, RUN_HISTORY_2);
        mContext.addInvocationAttribute(RUN_HISTORY_KEY, OLD_RUN_HISTORY);
        EasyMock.expect(mMockProvider.getBuild())
                .andReturn(createFakeBuild(createResultsWithRunHistory(), false));
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        EasyMock.replay(mMockDevice, mMockProvider);

        mLoader.init();
        IConfiguration config = new Configuration("name", "desc");
        mLoader.customizeConfiguration(config);
        String runHistory =
                config.getCommandOptions().getInvocationData().getUniqueMap().get(RUN_HISTORY_KEY);

        assertEquals(EXPECTED_RUN_HISTORY, runHistory);
        EasyMock.verify(mMockDevice, mMockProvider);
    }

    private IBuildInfo createFakeBuild(String resultContent, boolean index) throws Exception {
        DeviceBuildInfo build = new DeviceBuildInfo();
        build.addBuildAttribute(CompatibilityBuildHelper.SUITE_NAME, "CTS");
        mRootDir = FileUtil.createTempDir("cts-root-dir");
        new File(mRootDir, "android-cts/results/").mkdirs();
        build.addBuildAttribute(CompatibilityBuildHelper.ROOT_DIR, mRootDir.getAbsolutePath());
        // Create fake result dir
        long time = System.currentTimeMillis();
        build.addBuildAttribute(CompatibilityBuildHelper.START_TIME_MS, Long.toString(time));
        new CompatibilityBuildHelper(build).getResultDir().mkdirs();
        // Populate a test_results.xml
        File testResult = new File(new CompatibilityBuildHelper(build).getResultDir(),
                "test_result.xml");
        testResult.createNewFile();
        // Populate a proto result
        File protoDir =
                new File(
                        new CompatibilityBuildHelper(build).getResultDir(),
                        CompatibilityProtoResultReporter.PROTO_DIR);
        protoDir.mkdir();
        if (index) {
            mProtoFile = new File(protoDir, CompatibilityProtoResultReporter.PROTO_FILE_NAME + "0");
        } else {
            mProtoFile = new File(protoDir, CompatibilityProtoResultReporter.PROTO_FILE_NAME);
        }
        TestRecord.Builder builder = TestRecord.newBuilder();
        builder.setDescription(Any.pack(mContext.toProto()));
        builder.build().writeDelimitedTo(new FileOutputStream(mProtoFile));
        FileUtil.writeToFile(resultContent, testResult);
        return build;
    }

    private String createBasicResults() {
        StringBuilder sb = new StringBuilder();
        sb.append("<?xml version='1.0' encoding='UTF-8' standalone='no' ?>\n");
        sb.append("<?xml-stylesheet type=\"text/xsl\" href=\"compatibility_result.xsl\"?>\n");
        sb.append(
                "<Result start=\"1530218251501\" end=\"1530218261061\" "
                        + "start_display=\"Thu Jun 28 13:37:31 PDT 2018\" "
                        + "end_display=\"Thu Jun 28 13:37:41 PDT 2018\" "
                        + "command_line_args=\"cts -m CtsGesture --skip-all-system-status-check\" "
                        + "suite_name=\"CTS\" suite_version=\"9.0_r1\" "
                        + "suite_plan=\"cts\" suite_build_number=\"8888\" report_version=\"5.0\" "
                        + "devices=\"HT6570300047\" "
                        + "host_name=\"user.android.com\">\n");
        sb.append(
                "  <Build command_line_args=\"cts -m CtsGesture --skip-all-system-status-check\""
                        + " build_vendor_fingerprint=\"vendorFingerprint\" "
                        + " build_reference_fingerprint=\"\" "
                        + " build_fingerprint=\"testfingerprint\"/>\n");
        // Summary
        sb.append("  <Summary pass=\"0\" failed=\"0\" modules_done=\"2\" modules_total=\"2\" />\n");
        // Each module results
        sb.append("  <Module name=\"CtsGestureTestCases\" abi=\"arm64-v8a\" runtime=\"2776\" "
                + "done=\"true\" pass=\"0\" total_tests=\"0\" />\n");
        sb.append("  <Module name=\"CtsGestureTestCases\" abi=\"armeabi-v7a\" runtime=\"2776\" "
                + "done=\"true\" pass=\"0\" total_tests=\"0\" />\n");
        // End
        sb.append("</Result>");
        return sb.toString();
    }

    private String createResultsWithRunHistory() {
        // This method is the same as createBasicResults() except that it contains run history.
        StringBuilder sb = new StringBuilder();
        sb.append("<?xml version='1.0' encoding='UTF-8' standalone='no' ?>\n");
        sb.append("<?xml-stylesheet type=\"text/xsl\" href=\"compatibility_result.xsl\"?>\n");
        sb.append(
                "<Result start=\"1530218251501\" end=\"1530218261061\" "
                        + "start_display=\"Thu Jun 28 13:37:31 PDT 2018\" "
                        + "end_display=\"Thu Jun 28 13:37:41 PDT 2018\" "
                        + "command_line_args=\"cts -m CtsGesture --skip-all-system-status-check "
                        + "--shard-count 5\" "
                        + "suite_name=\"CTS\" suite_version=\"9.0_r1\" "
                        + "suite_plan=\"cts\" suite_build_number=\"8888\" report_version=\"5.0\" "
                        + "devices=\"HT6570300047\" "
                        + "host_name=\"user2.android.com\" >\n");
        final String RUN_HISTORY_JSON =
                "[{'startTime':1000000000000,'endTime':1000000010000,"
                        + "'pass':10,'failed':5,"
                        + "'commandLineArgs':'cts -m CtsGesture --skip-all-system-status-check',"
                        + "'hostName':'user1.android.com'}]";
        sb.append(
                "  <Build command_line_args=\"cts -m CtsGesture --skip-all-system-status-check\""
                        + " build_vendor_fingerprint=\"vendorFingerprint\" "
                        + " build_reference_fingerprint=\"\" "
                        + " build_fingerprint=\"testfingerprint\""
                        + " run_history=\""
                        + RUN_HISTORY_JSON
                        + "\"/>\n");
        // Run history
        sb.append(
                "  <RunHistory>\n"
                        + "    <Run start=\"1000000000000\" end=\"1000000010000\" "
                        + "pass=\"10\" failed=\"5\" "
                        + "command_line_args=\"cts -m CtsGesture --skip-all-system-status-check\" "
                        + "hostName=\"user1.android.com\" />\n"
                        + "  </RunHistory>\n");
        // Summary
        sb.append("  <Summary pass=\"0\" failed=\"0\" modules_done=\"2\" modules_total=\"2\" />\n");
        // Each module results
        sb.append(
                "  <Module name=\"CtsGestureTestCases\" abi=\"arm64-v8a\" runtime=\"2776\" "
                        + "done=\"true\" pass=\"0\" total_tests=\"0\" />\n");
        sb.append(
                "  <Module name=\"CtsGestureTestCases\" abi=\"armeabi-v7a\" runtime=\"2776\" "
                        + "done=\"true\" pass=\"0\" total_tests=\"0\" />\n");
        // End
        sb.append("</Result>");
        return sb.toString();
    }
}
