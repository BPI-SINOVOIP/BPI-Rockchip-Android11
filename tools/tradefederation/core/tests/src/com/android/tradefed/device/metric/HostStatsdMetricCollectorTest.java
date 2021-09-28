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
 * limitations under the License
 */
package com.android.tradefed.device.metric;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.TestDescription;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Spy;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** Unit Tests for {@link HostStatsdMetricCollector}. */
@RunWith(JUnit4.class)
public class HostStatsdMetricCollectorTest {
    private static final String STATSD_CONFIG = "statsd.config";
    private static final long CONFIG_ID = 54321L;

    @Mock private IInvocationContext mContext;
    @Mock private ITestInvocationListener mListener;
    @Spy private TestHostStatsdMetricCollector mCollector;
    @Rule public TemporaryFolder mFolder = new TemporaryFolder();

    private TestDescription mTest = new TestDescription("Foo", "Bar");
    private List<ITestDevice> mDevices =
            Stream.of("device_1", "device_2").map(this::mockDevice).collect(Collectors.toList());
    private HashMap<String, Metric> mMetrics = new HashMap<>();

    @Before
    public void setUp() throws IOException, ConfigurationException, DeviceNotAvailableException {
        initMocks(this);
        OptionSetter options = new OptionSetter(mCollector);
        options.setOptionValue(
                "binary-stats-config", mFolder.newFile(STATSD_CONFIG).getAbsolutePath());

        when(mContext.getDevices()).thenReturn(mDevices);
        doReturn(CONFIG_ID)
                .when(mCollector)
                .pushBinaryStatsConfig(any(ITestDevice.class), any(File.class));
    }

    /** Test at per-test level that a binary config is pushed and report is dumped */
    @Test
    public void testCollect_perTest()
            throws IOException, DeviceNotAvailableException, ConfigurationException {
        OptionSetter options = new OptionSetter(mCollector);
        options.setOptionValue("per-run", "false");

        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("collect-metrics", 2);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, mMetrics);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, mMetrics);
        mCollector.testRunEnded(0L, mMetrics);

        for (ITestDevice device : mDevices) {
            verify(mCollector, times(2)).pushBinaryStatsConfig(eq(device), any(File.class));
            verify(mCollector, times(2)).getReportByteStream(eq(device), anyLong());
            verify(mCollector, times(2)).removeConfig(eq(device), anyLong());
            assertEquals(2, mCollector.numberOfReportsProcessed(device));
        }
    }

    /** Test the behavior of collector when actual test failed */
    @Test
    public void testCollect_testFail()
            throws IOException, DeviceNotAvailableException, ConfigurationException {
        OptionSetter options = new OptionSetter(mCollector);
        options.setOptionValue("per-run", "false");

        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("collect-metrics", 2);
        mCollector.testStarted(mTest);
        mCollector.testFailed(mTest, "Test Failed");
        mCollector.testEnded(mTest, mMetrics);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, mMetrics);
        mCollector.testRunEnded(0L, mMetrics);

        for (ITestDevice device : mDevices) {
            verify(mCollector, times(2)).pushBinaryStatsConfig(eq(device), any(File.class));
            verify(mCollector, times(2)).getReportByteStream(eq(device), anyLong());
            verify(mCollector, times(2)).removeConfig(eq(device), anyLong());
            assertEquals(1, mCollector.numberOfReportsProcessed(device));
        }
    }

    /** Test at per-run level that a binary config is pushed and report is dumped at run level. */
    @Test
    public void testCollect_perRun() throws IOException, DeviceNotAvailableException {
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("collect-metrics", 2);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, mMetrics);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, mMetrics);
        mCollector.testRunEnded(0L, mMetrics);

        for (ITestDevice device : mDevices) {
            verify(mCollector).pushBinaryStatsConfig(eq(device), any(File.class));
            verify(mCollector).getReportByteStream(eq(device), anyLong());
            verify(mCollector).removeConfig(eq(device), anyLong());
            assertEquals(1, mCollector.numberOfReportsProcessed(device));
        }
    }

    private ITestDevice mockDevice(String serial) {
        ITestDevice device = mock(ITestDevice.class);
        when(device.getSerialNumber()).thenReturn(serial);
        return device;
    }

    private static class TestHostStatsdMetricCollector extends HostStatsdMetricCollector {

        private Map<ITestDevice, Integer> reportsPerDevice = new HashMap<>();

        int numberOfReportsProcessed(ITestDevice device) {
            return reportsPerDevice.get(device);
        }

        @Override
        protected void processStatsReport(
                ITestDevice device, InputStreamSource dataStream, DeviceMetricData runData) {
            reportsPerDevice.computeIfPresent(device, (k, v) -> v + 1);
            reportsPerDevice.putIfAbsent(device, 1);
        }
    }
}
