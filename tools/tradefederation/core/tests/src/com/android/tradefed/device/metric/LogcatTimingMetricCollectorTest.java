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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.LogcatReceiver;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.TestDescription;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Spy;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A unit test for {@link LogcatTimingMetricCollector} */
@RunWith(JUnit4.class)
public class LogcatTimingMetricCollectorTest {

    private static final String DEVICE_NAME_FORMAT_KEY = "{%s}:%s";
    private static final String RUN_NAME = "HelloTestRun";

    @Spy private LogcatTimingMetricCollector mCollector;
    @Mock private IInvocationContext mContext;
    @Mock private ITestInvocationListener mListener;
    @Mock private LogcatReceiver mMockReceiver;

    private List<ITestDevice> mDevices;
    private TestDescription mTest;

    @Before
    public void setup() throws ConfigurationException {
        initMocks(this);

        mTest = new TestDescription("HelloTest", "testDrive");

        ITestDevice mockDevice = mock(ITestDevice.class);
        when(mockDevice.getSerialNumber()).thenReturn("serial1");
        mDevices = new ArrayList<>();
        mDevices.add(mockDevice);
        when(mContext.getDevices()).thenReturn(mDevices);
        when(mContext.getDeviceName(same(mockDevice))).thenReturn("device1");

        when(mMockReceiver.getLogcatData()).thenReturn(createInputStreamSource());

        doReturn(mMockReceiver)
                .when(mCollector)
                .createLogcatReceiver(same(mockDevice), anyString());
        doReturn(createFakeTimingMetrics()).when(mCollector).parse(any(InputStreamSource.class));
        mCollector.init(mContext, mListener);
        setPatterns(mCollector);
    }

    @Test
    public void testCollect_oneDevice_perRun() {
        HashMap<String, Metric> metrics = new HashMap<>();
        mCollector.testRunStarted(RUN_NAME, 1);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, metrics);
        mCollector.testRunEnded(0, metrics);

        verify(mMockReceiver).start();
        verify(mMockReceiver).stop();
        verify(mMockReceiver).clear();

        assertThat(metrics).hasSize(2);
        assertThat(metrics).containsKey("metric1");
        assertThat(metrics).containsKey("metric2");

        Metric metric1 = metrics.get("metric1");

        String stringValue = metric1.getMeasurements().getSingleString();
        assertThat(stringValue).isEqualTo("1.0");

        Metric metric2 = metrics.get("metric2");

        stringValue = metric2.getMeasurements().getSingleString();
        assertThat(stringValue).isEqualTo("1.0,2.0");
    }

    @Test
    public void testCollect_multipleDevice_perRun() {
        ITestDevice mockDevice = mock(ITestDevice.class);
        LogcatReceiver mockReceiver = mock(LogcatReceiver.class);
        when(mockDevice.getSerialNumber()).thenReturn("serial2");
        when(mockReceiver.getLogcatData()).thenReturn(createInputStreamSource());
        mDevices.add(mockDevice);
        when(mContext.getDeviceName(same(mockDevice))).thenReturn("device2");
        doReturn(mockReceiver).when(mCollector).createLogcatReceiver(same(mockDevice), anyString());

        HashMap<String, Metric> metrics = new HashMap<>();
        mCollector.testRunStarted(RUN_NAME, 1);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, metrics);
        mCollector.testRunEnded(0, metrics);

        verify(mMockReceiver).start();
        verify(mMockReceiver).stop();
        verify(mMockReceiver).clear();
        verify(mockReceiver).start();
        verify(mockReceiver).stop();
        verify(mockReceiver).clear();

        assertThat(metrics).hasSize(4);
        assertThat(metrics)
                .containsKey(String.format(DEVICE_NAME_FORMAT_KEY, "device1", "metric1"));
        assertThat(metrics)
                .containsKey(String.format(DEVICE_NAME_FORMAT_KEY, "device2", "metric2"));
    }

    @Test
    public void testCollect_onDevice_twoTests() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mCollector);
        setter.setOptionValue("per-run", "false");
        HashMap<String, Metric> metrics = new HashMap<>();
        mCollector.testRunStarted(RUN_NAME, 1);
        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, metrics);

        verify(mMockReceiver).start();
        verify(mMockReceiver).stop();
        verify(mMockReceiver).clear();

        assertThat(metrics).hasSize(2);
        assertThat(metrics).containsKey("metric1");
        assertThat(metrics).containsKey("metric2");

        mCollector.testStarted(mTest);
        mCollector.testEnded(mTest, metrics);

        verify(mMockReceiver, times(2)).start();
        verify(mMockReceiver, times(2)).stop();
        verify(mMockReceiver, times(2)).clear();

        assertThat(metrics).hasSize(2);
        assertThat(metrics).containsKey("metric1");
        assertThat(metrics).containsKey("metric2");

        Metric metric1 = metrics.get("metric1");

        String stringValue = metric1.getMeasurements().getSingleString();
        assertThat(stringValue).isEqualTo("1.0");

        Metric metric2 = metrics.get("metric2");

        stringValue = metric2.getMeasurements().getSingleString();
        assertThat(stringValue).isEqualTo("1.0,2.0");

        mCollector.testRunEnded(0, metrics);
    }

    @Test
    public void testCollect_testFail() {
        mCollector.testRunStarted(RUN_NAME, 1);
        mCollector.testStarted(mTest);
        mCollector.testFailed(mTest, "Test Fail");

        verify(mMockReceiver).start();
        verify(mMockReceiver).stop();
        verify(mMockReceiver).clear();
    }

    private void setPatterns(LogcatTimingMetricCollector collector) throws ConfigurationException {
        OptionSetter setter = new OptionSetter(collector);
        setter.setOptionValue("logcat-buffer", "all");
        setter.setOptionValue("start-pattern", "metric1", "boot up");
        setter.setOptionValue("end-pattern", "metric1", "boot complete");
        setter.setOptionValue("start-pattern", "metric2", "service start");
        setter.setOptionValue("end-pattern", "metric2", "service complete");
    }

    private InputStreamSource createInputStreamSource() {
        return new ByteArrayInputStreamSource("".getBytes());
    }

    private Map<String, List<Double>> createFakeTimingMetrics() {
        Map<String, List<Double>> metrics = new HashMap<>();
        metrics.put("metric1", new ArrayList<>());
        metrics.put("metric2", new ArrayList<>());
        metrics.get("metric1").add(1.0);
        metrics.get("metric2").add(1.0);
        metrics.get("metric2").add(2.0);
        return metrics;
    }
}
