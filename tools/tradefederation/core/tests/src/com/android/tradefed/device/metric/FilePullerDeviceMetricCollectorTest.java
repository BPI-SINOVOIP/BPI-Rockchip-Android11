package com.android.tradefed.device.metric;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.util.HashMap;

/** Unit tests for {@link FilePullerDeviceMetricCollector}. */
@RunWith(JUnit4.class)
public class FilePullerDeviceMetricCollectorTest {
    private FilePullerDeviceMetricCollector mFilePuller;
    @Mock private ITestInvocationListener mMockListener;
    @Mock private ITestDevice mMockDevice;
    @Mock private ITestDevice mStubDevice;
    private IInvocationContext mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = new InvocationContext();
        mContext.addAllocatedDevice("default", mMockDevice);
        mContext.addAllocatedDevice("stub", mStubDevice);
        Mockito.when(mStubDevice.getIDevice()).thenReturn(new StubDevice("serial-stub"));
        mFilePuller =
                new FilePullerDeviceMetricCollector() {
                    @Override
                    public void processMetricFile(
                            String key, File metricFile, DeviceMetricData data) {
                        try (FileInputStreamSource source = new FileInputStreamSource(metricFile)) {
                            testLog(key, LogDataType.TEXT, source);
                        }
                    }
                    @Override
                    public void processMetricDirectory(
                            String key, File metricDirectory, DeviceMetricData data) {
                        try (FileInputStreamSource source = new FileInputStreamSource(
                                metricDirectory)) {
                            testLog(key, LogDataType.TEXT, source);
                        } finally {
                            FileUtil.deleteFile(metricDirectory);
                        }
                    }
                };
        mFilePuller.init(mContext, mMockListener);
    }

    /** Test when no keys have been requested, nothing should be queried anywhere. */
    @Test
    public void testNoMatchingKey() {
        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testRunEnded(500, new HashMap<String, Metric>());
    }

    /**
     * Test when a file is found matching the key, then pulled and {@link
     * FilePullerDeviceMetricCollector#processMetricFile(String, File, DeviceMetricData)} is called.
     */
    @Test
    public void testPullMatchingKey() throws Exception {
        OptionSetter setter = new OptionSetter(mFilePuller);
        setter.setOptionValue("pull-pattern-keys", "coverageFile");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("coverageFile", TfMetricProtoUtil.stringToMetric("/data/coverage"));

        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/coverage")))
                .thenReturn(new File("fake"));

        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testRunEnded(500, currentMetrics);

        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("coverageFile"), Mockito.eq(LogDataType.TEXT), Mockito.any());
    }

    /**
     * Test when a multiple file is found matching the matching key, then pulled and {@link
     * FilePullerDeviceMetricCollector#processMetricFile(String, File, DeviceMetricData)} is called
     * multiple times.
     */
    @Test
    public void testPullMultipleMatchingKeyInMetrics() throws Exception {
        OptionSetter setter = new OptionSetter(mFilePuller);
        setter.setOptionValue("pull-pattern-keys", "coverageFile");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("coverageFile", TfMetricProtoUtil.stringToMetric("/data/coverage1"));
        currentMetrics.put("coverageFileAnother",
                TfMetricProtoUtil.stringToMetric("/data/coverage2"));

        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/coverage1")))
                .thenReturn(new File("fake1"));
        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/coverage2")))
                .thenReturn(new File("fake2"));

        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testRunEnded(500, currentMetrics);

        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("coverageFile"), Mockito.eq(LogDataType.TEXT), Mockito.any());
        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("coverageFileAnother"), Mockito.eq(LogDataType.TEXT),
                        Mockito.any());
    }

    /**
     * Test when a file is found matching the key using a pattern matching, then pulled and {@link
     * FilePullerDeviceMetricCollector#processMetricFile(String, File, DeviceMetricData)} is called.
     */
    @Test
    public void testPullMatchingKeyPattern() throws Exception {
        OptionSetter setter = new OptionSetter(mFilePuller);
        // Using a pattern to find the file
        setter.setOptionValue("pull-pattern-keys", "coverage.*");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("coverageFile", TfMetricProtoUtil.stringToMetric("/data/coverage"));

        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/coverage")))
                .thenReturn(new File("fake"));

        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testRunEnded(500, currentMetrics);

        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("coverageFile"), Mockito.eq(LogDataType.TEXT), Mockito.any());
    }

    /**
     * Test {@link FilePullerDeviceMetricCollector#processMetricFile(String, File,
     * DeviceMetricData)} is called on test case end and test run ended.
     */
    @Test
    public void testMetricFileProcessingFlow() throws Exception {
        OptionSetter setter = new OptionSetter(mFilePuller);
        setter.setOptionValue("pull-pattern-keys", "coverageFile");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("coverageFile", TfMetricProtoUtil.stringToMetric("/data/coverage"));

        Mockito.when(mMockDevice.pullFile(Mockito.eq("/data/coverage")))
                .thenReturn(new File("fake"));

        TestDescription testDesc = new TestDescription("xyz", "abc");

        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testStarted(testDesc);
        mFilePuller.testEnded(testDesc, currentMetrics);
        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("coverageFile"), Mockito.eq(LogDataType.TEXT), Mockito.any());
        verify(mMockListener, times(1)).testLog(Mockito.eq("coverageFile"),
                Mockito.eq(LogDataType.TEXT), Mockito.any());
        mFilePuller.testRunEnded(500, currentMetrics);
        verify(mMockListener, times(2)).testLog(Mockito.eq("coverageFile"),
                Mockito.eq(LogDataType.TEXT), Mockito.any());

    }

    /** Test when a file exists in the metrics but the pattern searching does not match it. */
    @Test
    public void testPatternNotMatching() throws Exception {
        OptionSetter setter = new OptionSetter(mFilePuller);
        // Using a pattern to find the file but no file matches
        setter.setOptionValue("pull-pattern-keys", "wrongPattern.*");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("coverageFile", TfMetricProtoUtil.stringToMetric("/data/coverage"));

        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testRunEnded(500, currentMetrics);
        // Nothing gets pulled from the device and logged.
        Mockito.verify(mMockDevice, Mockito.times(0)).pullFile(Mockito.eq("/data/coverage"));
        Mockito.verify(mMockListener, Mockito.times(0))
                .testLog(Mockito.eq("coverageFile"), Mockito.eq(LogDataType.TEXT), Mockito.any());
    }

    /**
     * Test when a directory is found matching the key, then pulled and {@link
     * FilePullerDeviceMetricCollector#processMetricDirectory(String key,
     * File metricDirectory, DeviceMetricData runData)} is called.
     */
    @Test
    public void testPullMatchingDirectory() throws Exception {
        OptionSetter setter = new OptionSetter(mFilePuller);
        setter.setOptionValue("directory-keys", "coverageDirectory");
        HashMap<String, Metric> currentMetrics = new HashMap<>();
        currentMetrics.put("coverageDirectory", TfMetricProtoUtil.stringToMetric("/data/coverage"));

        Mockito.when(mMockDevice.pullDir(Mockito.eq("coverageDirectory"),
                Mockito.any(File.class))).thenReturn(true);

        mFilePuller.testRunStarted("fakeRun", 5);
        mFilePuller.testRunEnded(500, currentMetrics);

        Mockito.verify(mMockListener)
                .testLog(Mockito.eq("coverageDirectory"), Mockito.eq(LogDataType.TEXT),
                        Mockito.any());
    }
}
