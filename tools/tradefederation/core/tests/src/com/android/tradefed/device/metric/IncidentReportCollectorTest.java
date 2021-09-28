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
package com.android.tradefed.device.metric;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.IncidentProto;

import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.proto.TfMetricProtoUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link IncidentReportCollector}. */
@RunWith(JUnit4.class)
public class IncidentReportCollectorTest {
    private static final TestDescription FAKE_TEST = new TestDescription("class", "test");
    private static final String FAKE_REPORT_PATH = "/sdcard/incidents/final.pb";
    private static final long FAKE_TIME = 10000;

    // The {@code IncidentReportCollector} under test.
    private IncidentReportCollector mIncidentCollector;

    // Invocation context referencing the test device.
    private IInvocationContext mInvocationContext;

    // Mock objects to enable the incident collector.
    @Mock ITestDevice mMockTestDevice;
    @Mock ITestInvocationListener mInvocationListener;

    // List of files created for this test to remove.
    private List<File> mCurrentTestFiles = new ArrayList<>();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // Disable expected calls to log raw and processed files.
        doNothing()
                .when(mInvocationListener)
                .testLog(anyString(), any(LogDataType.class), any(InputStreamSource.class));
        // Set up invocation context that refers to a mock device.
        mInvocationContext = new InvocationContext();
        mInvocationContext.addAllocatedDevice(
                ConfigurationDef.DEFAULT_DEVICE_NAME, mMockTestDevice);
        // Setup the collector for testing with necessary mocks.
        mIncidentCollector = new IncidentReportCollector();
        mIncidentCollector.init(mInvocationContext, mInvocationListener);
    }

    @After
    public void tearDown() throws Exception {
        // Clear all files created at test end.
        for (File file : mCurrentTestFiles) {
            FileUtil.deleteFile(file);
        }
        mCurrentTestFiles.clear();
    }

    /**
     * Creates a {@link File} with the specified byte {@code contents}.
     *
     * @param contents the file contents in bytes
     * @return a file with the supplied contents
     */
    private File createFileWithBytes(byte[] contents) {
        try {
            File result = FileUtil.createTempFile("fake-incident", ".pb");
            mCurrentTestFiles.add(result);
            Files.write(result.toPath(), contents, StandardOpenOption.WRITE);
            return result;
        } catch (IOException e) {
            throw new RuntimeException("Failed to create file for test.", e);
        }
    }

    /** Tests that the raw incident report is pulled, processed, and both results are reported. */
    @Test
    public void testPullWithIncidentPrefix() throws Exception {
        // Set up the device metrics and fake file to pull from the device.
        Map<String, String> deviceMetrics = new HashMap<String, String>();
        deviceMetrics.put("is-incident-report", FAKE_REPORT_PATH.toString());
        File fakeReport = createFileWithBytes(IncidentProto.getDefaultInstance().toByteArray());
        when(mMockTestDevice.pullFile(Mockito.eq(FAKE_REPORT_PATH.toString())))
                .thenReturn(fakeReport);
        // Replay the test started and test ended events to simulate a test.
        mIncidentCollector.testStarted(FAKE_TEST);
        mIncidentCollector.testEnded(FAKE_TEST, TfMetricProtoUtil.upgradeConvert(deviceMetrics));
        // Ensure the file was processed and contains the expected report.
        verify(mInvocationListener)
                .testLog(
                        matches(".*incident.*"),
                        eq(LogDataType.PB),
                        any(FileInputStreamSource.class));
        verify(mInvocationListener)
                .testLog(
                        matches(".*-processed"),
                        eq(LogDataType.PB),
                        any(ByteArrayInputStreamSource.class));
    }

    /** Tests that the files reported without the proper matching key are ignored entirely. */
    @Test
    public void testIgnoreOtherFilesProcessed() throws Exception {
        // Set up the device metrics and fake file to pull from the device.
        Map<String, String> deviceMetrics = new HashMap<String, String>();
        deviceMetrics.put("not-that-type-of-report", FAKE_REPORT_PATH);
        deviceMetrics.put("/system/bin/incidentd#anon-avg", FAKE_REPORT_PATH);
        // Replay the test started and test ended events to simulate a test.
        mIncidentCollector.testStarted(FAKE_TEST);
        mIncidentCollector.testEnded(FAKE_TEST, TfMetricProtoUtil.upgradeConvert(deviceMetrics));
        // Ensure the metrics/files were not pulled or processed.
        verify(mInvocationListener, never()).testLog(any(), any(), any());
        verify(mMockTestDevice, never()).pullFile(any(String.class));
    }

    /** Tests that the corrupt reports are pulled and reported only in their raw format. */
    @Test
    public void testIgnoreAndLogFailures() throws Exception {
        // Set up the device metrics and fake file to pull from the device.
        Map<String, String> deviceMetrics = new HashMap<String, String>();
        deviceMetrics.put("is-incident-report", FAKE_REPORT_PATH.toString());
        File fakeReport = createFileWithBytes("can't parse this".getBytes());
        when(mMockTestDevice.pullFile(Mockito.eq(FAKE_REPORT_PATH.toString())))
                .thenReturn(fakeReport);
        // Replay the test started and test ended events to simulate a test.
        mIncidentCollector.testStarted(FAKE_TEST);
        mIncidentCollector.testEnded(FAKE_TEST, TfMetricProtoUtil.upgradeConvert(deviceMetrics));
        // Ensure the raw file was pulled and contained in the metrics.
        verify(mMockTestDevice).pullFile(FAKE_REPORT_PATH.toString());
        verify(mInvocationListener).testLog(matches(".*incident.*"), any(), any());
        // Ensure the processed file was not successfully processed or reported.
        verify(mInvocationListener, never()).testLog(matches(".*processed.*"), any(), any());
    }

    /** Tests that a report is collected and reported if the test run end option is set. */
    @Test
    public void testCollectOnTestRunEnd() throws Exception {
        OptionSetter setter = new OptionSetter(mIncidentCollector);
        setter.setOptionValue("incident-on-test-run-end", "true");
        // Replay the test started and test ended events to simulate a test.
        mIncidentCollector.testStarted(FAKE_TEST);
        mIncidentCollector.testEnded(
                FAKE_TEST, TfMetricProtoUtil.upgradeConvert(new HashMap<String, String>()));
        // Call test run end and ensure something is collected.
        when(mMockTestDevice.executeShellV2Command(
                        eq(IncidentReportCollector.INCIDENT_REPORT_CMD),
                        any(FileOutputStream.class)))
                .thenReturn(new CommandResult(CommandStatus.SUCCESS));
        mIncidentCollector.testRunEnded(FAKE_TIME, new HashMap<String, Metric>());
        // Ensure the expected "on-test-run-end" files were logged and processed.
        verify(mInvocationListener)
                .testLog(
                        matches(".*incident-on-test-run-end.*"),
                        eq(LogDataType.PB),
                        any(FileInputStreamSource.class));
        verify(mInvocationListener)
                .testLog(
                        matches(".*incident-on-test-run-end.*-processed"),
                        eq(LogDataType.PB),
                        any(ByteArrayInputStreamSource.class));
    }
}
