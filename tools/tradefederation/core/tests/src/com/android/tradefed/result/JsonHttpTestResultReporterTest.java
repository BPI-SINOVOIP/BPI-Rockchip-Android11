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
package com.android.tradefed.result;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link JsonHttpTestResultReporter}. */
@RunWith(JUnit4.class)
public class JsonHttpTestResultReporterTest {
    // Corresponds to the option name in JsonHttpTestResultReporter.
    private static final String SKIP_FAILED_RUNS_OPTION = "skip-failed-runs";
    private static final String COLLECT_DEVICE_DETAILS_OPTION = "include-device-details";

    // Corresponds to the KEY_METRICS field in JsonHttpTestResultReporter.
    private static final String JSON_METRIC_KEY = "metrics";

    private JsonHttpTestResultReporter mReporter;
    private IInvocationContext mContext;

    @Before
    public void setUp() {
        mReporter = spy(new JsonHttpTestResultReporter());
        doNothing().when(mReporter).postResults(any(JSONObject.class));
        mContext = new InvocationContext();
        mContext.addDeviceBuildInfo("fakeDevice", new BuildInfo());
    }

    /** Test that failed runs are skipped when skip-failed-runs is not set. */
    @Test
    public void testSkipFailedRuns_notSet() throws JSONException {
        mReporter.invocationStarted(mContext);
        injectTestRun(mReporter, "run1", "test", "metric1", 0, true);
        injectTestRun(mReporter, "run2", "test", "metric2", 1, false);
        mReporter.invocationEnded(0);
        ArgumentCaptor<JSONObject> jsonCaptor = ArgumentCaptor.forClass(JSONObject.class);
        verify(mReporter).postResults(jsonCaptor.capture());
        // Both runs should be in the posted metrics.
        Assert.assertTrue(jsonCaptor.getValue().getJSONObject(JSON_METRIC_KEY).has("run1"));
        Assert.assertTrue(jsonCaptor.getValue().getJSONObject(JSON_METRIC_KEY).has("run2"));
    }

    /** Test that failed runs are skipped when skip-failed-runs is set. */
    @Test
    public void testSkipFailedRuns_set() throws ConfigurationException, JSONException {
        OptionSetter optionSetter = new OptionSetter(mReporter);
        optionSetter.setOptionValue(SKIP_FAILED_RUNS_OPTION, String.valueOf(true));
        mReporter.invocationStarted(mContext);
        injectTestRun(mReporter, "run1", "test", "metric1", 0, true);
        injectTestRun(mReporter, "run2", "test", "metric2", 1, false);
        mReporter.invocationEnded(0);
        ArgumentCaptor<JSONObject> jsonCaptor = ArgumentCaptor.forClass(JSONObject.class);
        verify(mReporter).postResults(jsonCaptor.capture());
        // Only the first run should be in the posted metrics.
        Assert.assertTrue(jsonCaptor.getValue().getJSONObject(JSON_METRIC_KEY).has("run1"));
        Assert.assertFalse(jsonCaptor.getValue().getJSONObject(JSON_METRIC_KEY).has("run2"));
    }

    /** Test for parsing additional device details when collect device details is enabled. */
    @Test
    public void testIncludeAdditionalTestDetails() throws ConfigurationException {
        OptionSetter optionSetter = new OptionSetter(mReporter);
        optionSetter.setOptionValue(COLLECT_DEVICE_DETAILS_OPTION, String.valueOf(true));
        // getDevice and parseAdditionalDeviceDetails method calls happen when the
        // additional device details flag is enabled.
        Mockito.doReturn(null).when(mReporter).getDevice(any(InvocationContext.class));
        Mockito.doNothing().when(mReporter).parseAdditionalDeviceDetails(any());
        mReporter.invocationStarted(mContext);
        injectTestRun(mReporter, "run1", "test", "metric1", 0, true);
        mReporter.invocationEnded(0);
    }

    /**
     * Injects a single test run with 1 test into the {@link JsonHttpTestResultReporter} under test.
     *
     * @return the {@link TestDescription} of added test
     */
    private TestDescription injectTestRun(
            CollectingTestListener target,
            String runName,
            String testName,
            String metricValue,
            int attempt,
            boolean failtest) {
        Map<String, String> runMetrics = new HashMap<String, String>(1);
        runMetrics.put("run_metric", metricValue);
        Map<String, String> testMetrics = new HashMap<String, String>(1);
        testMetrics.put("test_metric", metricValue);

        target.testRunStarted(runName, 1, attempt);
        final TestDescription test = new TestDescription("FooTest", testName);
        target.testStarted(test);
        if (failtest) {
            target.testFailed(test, "trace");
        }
        target.testEnded(test, TfMetricProtoUtil.upgradeConvert(testMetrics));
        target.testRunEnded(0, TfMetricProtoUtil.upgradeConvert(runMetrics));
        return test;
    }
}
