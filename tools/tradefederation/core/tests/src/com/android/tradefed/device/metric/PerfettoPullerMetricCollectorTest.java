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

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.Pair;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/** Unit tests for {@link PerfettoPullerMetricCollector}. */
@RunWith(JUnit4.class)
public class PerfettoPullerMetricCollectorTest {



    private PerfettoPullerMetricCollector mPerfettoMetricCollector;
    @Mock
    private ITestInvocationListener mMockListener;
    @Mock
    private ITestDevice mMockDevice;
    private IInvocationContext mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = new InvocationContext();
        mContext.addAllocatedDevice("default", mMockDevice);
        mPerfettoMetricCollector = Mockito.spy(new PerfettoPullerMetricCollector());
        mPerfettoMetricCollector.init(mContext, mMockListener);
    }

    @Test
    public void testNoProcessingFlow() throws Exception {

        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        HashMap<String, Metric> currentMetrics = new HashMap<>();

        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(new File("trace"));

        TestDescription testDesc = new TestDescription("xyz", "abc");
        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);

        assertTrue("Trace duration available but not expected.",
                currentMetrics.size() == 0);
    }

    @Test
    public void testProcessingFlow() throws Exception {

        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        setter.setOptionValue("perfetto-binary-path", "trx");
        setter.setOptionValue("convert-metric-file", "false");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("perfettofile", TfMetricProtoUtil.stringToMetric("/data/trace.pb"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(new File("trace"));

        TestDescription testDesc = new TestDescription("xyz", "abc");
        CommandResult cr = new CommandResult();
        cr.setStatus(CommandStatus.SUCCESS);
        cr.setStdout("abc:efg");

        Mockito.doReturn(cr).when(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());

        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);

        Mockito.verify(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());
        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("trace"), Mockito.eq(LogDataType.PERFETTO), Mockito.any());
        assertTrue("Expected two metrics that includes success status",
                currentMetrics.get("perfetto_trace_extractor_status").getMeasurements()
                        .getSingleString().equals("1"));
        assertTrue("Trace duration metrics not available but expected.",
                currentMetrics.get("perfetto_trace_extractor_runtime").getMeasurements()
                        .getSingleDouble() >= 0);
        assertTrue("Trace file size metric is not available.",
                currentMetrics.get("perfetto_trace_file_size_bytes").getMeasurements()
                        .getSingleDouble() >= 0);
    }

    @Test
    public void testCompressedProcessingFlow() throws Exception {
        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        setter.setOptionValue("perfetto-binary-path", "trx");
        setter.setOptionValue("compress-perfetto", "true");
        setter.setOptionValue("convert-metric-file", "false");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("perfettofile", TfMetricProtoUtil.stringToMetric("/data/trace.pb"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(null);

        TestDescription testDesc = new TestDescription("xyz", "abc");
        CommandResult cr = new CommandResult();
        cr.setStatus(CommandStatus.SUCCESS);
        cr.setStdout("abc:efg");

        Mockito.doReturn(cr).when(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());

        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);

        Mockito.verify(mMockDevice, times(0)).pullFile(Mockito.eq("/data/trace.pb"));
        Mockito.verify(mPerfettoMetricCollector, times(2)).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());
        Mockito.verify(mMockListener).testLog(Mockito.contains("compressed"),
                Mockito.eq(LogDataType.GZIP), Mockito.any());
        assertTrue("Expected two metrics that includes success status",
                currentMetrics.get("perfetto_trace_extractor_status").getMeasurements()
                        .getSingleString().equals("1"));
        assertTrue("Trace duration metrics not available but expected.",
                currentMetrics.get("perfetto_trace_extractor_runtime").getMeasurements()
                        .getSingleDouble() >= 0);
    }

    @Test
    public void testScriptFailureStatus() throws Exception {

        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        setter.setOptionValue("perfetto-binary-path", "trx");
        setter.setOptionValue("convert-metric-file", "false");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("perfettofile", TfMetricProtoUtil.stringToMetric("/data/trace.pb"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(new File("trace"));

        TestDescription testDesc = new TestDescription("xyz", "abc");
        CommandResult cr = new CommandResult();
        cr.setStatus(CommandStatus.FAILED);
        cr.setStdout("abc:efg");

        Mockito.doReturn(cr).when(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());

        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);

        Mockito.verify(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());
        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("trace"), Mockito.eq(LogDataType.PERFETTO), Mockito.any());
        assertTrue("Expected two metrics that includes failure status",
                currentMetrics.get("perfetto_trace_extractor_status").getMeasurements()
                        .getSingleString().equals("0"));
        assertTrue("Trace duration metrics not available but expected.",
                currentMetrics.get("perfetto_trace_extractor_runtime").getMeasurements()
                        .getSingleDouble() >= 0);
    }

    @Test
    public void testBinaryArgs() throws Exception {
        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        setter.setOptionValue("perfetto-binary-path", "trx");
        setter.setOptionValue("perfetto-binary-args", "--uno");
        setter.setOptionValue("perfetto-binary-args", "--dos");
        setter.setOptionValue("perfetto-binary-args", "--tres");
        setter.setOptionValue("convert-metric-file", "false");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("perfettofile", TfMetricProtoUtil.stringToMetric("/data/trace.pb"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(new File("trace"));

        TestDescription testDesc = new TestDescription("xyz", "abc");
        CommandResult cr = new CommandResult();
        cr.setStatus(CommandStatus.SUCCESS);
        cr.setStdout("abc:efg");

        Mockito.doReturn(cr).when(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());

        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);

        ArgumentCaptor<String[]> captor = ArgumentCaptor.forClass(String[].class);
        Mockito.verify(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                captor.capture(), Mockito.any(), Mockito.any());
        List<String> args = Arrays.asList(captor.getValue());
        Assert.assertTrue(args.contains("--uno"));
        Assert.assertTrue(args.contains("--dos"));
        Assert.assertTrue(args.contains("--tres"));
    }

    /**
     * Test the defaults args for trace processor shell.
     * @throws Exception
     */
    @Test
    public void testTraceProcessorArgsDefaults() throws Exception {
        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        HashMap<String, Metric> currentMetrics = new HashMap<>();

        currentMetrics.put("perfettofile", TfMetricProtoUtil.stringToMetric("/data/trace.pb"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(new File("trace"));

        TestDescription testDesc = new TestDescription("xyz", "abc");
        CommandResult cr = new CommandResult();
        cr.setStatus(CommandStatus.SUCCESS);
        cr.setStdout("sometext");

        File tmpFile = File.createTempFile("trace_processor_shell", "");

        // Verifies the trace processor shell lookup in test artifacts file path map.
        Mockito.doReturn(tmpFile).when(
                mPerfettoMetricCollector).getFileFromTestArtifacts("trace_processor_shell");
        Mockito.doReturn(cr).when(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());

        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);

        String path = tmpFile.getAbsolutePath();
        tmpFile.delete();

        ArgumentCaptor<String[]> captor = ArgumentCaptor.forClass(String[].class);
        Mockito.verify(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                captor.capture(), Mockito.any(), Mockito.any());
        List<String> args = Arrays.asList(captor.getValue());
        Assert.assertTrue(args.contains(path));
        Assert.assertTrue(args.contains("--run-metrics"));
        Assert.assertTrue(args.contains("--metrics-output=text"));

    }

    /**
     * Test that trace processor run metrics are used when running the trace
     * processor shell command.
     *
     * @throws Exception
     */
    @Test
    public void testTraceProcessorAdditionalArgs() throws Exception {
        File tmpFile = File.createTempFile("trace_processor_binary", "");
        OptionSetter setter = new OptionSetter(mPerfettoMetricCollector);
        setter.setOptionValue("trace-processor-name", "trace_processor_shell");
        setter.setOptionValue("pull-pattern-keys", "perfettofile");
        setter.setOptionValue("trace-processor-run-metrics", "android_cpu,android_mem");
        setter.setOptionValue("trace-processor-binary", tmpFile.getAbsolutePath());

        HashMap<String, Metric> currentMetrics = new HashMap<>();

        currentMetrics.put("perfettofile", TfMetricProtoUtil.stringToMetric("/data/trace.pb"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/trace.pb")))
                .thenReturn(new File("trace"));

        TestDescription testDesc = new TestDescription("xyz", "abc");
        CommandResult cr = new CommandResult();
        cr.setStatus(CommandStatus.SUCCESS);
        cr.setStdout("abc:efg");

        Mockito.doReturn(cr).when(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                Mockito.any(), Mockito.any(), Mockito.any());

        mPerfettoMetricCollector.testStarted(testDesc);
        mPerfettoMetricCollector.testEnded(testDesc, currentMetrics);
        tmpFile.delete();

        ArgumentCaptor<String[]> captor = ArgumentCaptor.forClass(String[].class);
        Mockito.verify(mPerfettoMetricCollector).runHostCommand(Mockito.anyLong(),
                captor.capture(), Mockito.any(), Mockito.any());
        Mockito.verify(mPerfettoMetricCollector).getCompressedFile(Mockito.any());
        List<String> args = Arrays.asList(captor.getValue());
        // Verifies all the overridden values are taken into account and trace processor
        // binary is given preference over trace processor shell name.
        Assert.assertTrue(args.contains(tmpFile.getAbsolutePath()));
        Assert.assertTrue(args.contains("android_cpu,android_mem"));
        Assert.assertTrue(args.contains("--metrics-output=text"));
    }

    @Test
    public void testSplitKeyValue() {
        Assert.assertNull(PerfettoPullerMetricCollector.splitKeyValue("a:"));
        Assert.assertNull(PerfettoPullerMetricCollector.splitKeyValue(""));
        Assert.assertNull(PerfettoPullerMetricCollector.splitKeyValue(":a"));
        Assert.assertEquals(
                PerfettoPullerMetricCollector.splitKeyValue("abc:xyz"), new Pair<>("abc", "xyz"));
        Assert.assertEquals(
                PerfettoPullerMetricCollector.splitKeyValue("a:b:c:xyz"),
                new Pair<>("a:b:c", "xyz"));
    }

}
