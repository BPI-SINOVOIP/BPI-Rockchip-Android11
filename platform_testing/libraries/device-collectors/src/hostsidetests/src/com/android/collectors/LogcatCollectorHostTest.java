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
package com.android.collectors;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.device.metric.FilePullerDeviceMetricCollector;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.URLConnection;
import java.util.Arrays;

/**
 * Host side tests for the logcat collector, this ensure that we are able to use the collector in a
 * similar way as the infra.
 *
 * <p>Command: mm CollectorHostsideLibTest CollectorDeviceLibTest -j16
 *
 * <p>tradefed.sh run commandAndExit template/local_min --template:map test=CollectorHostsideLibTest
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class LogcatCollectorHostTest extends BaseHostJUnit4Test {
    private static final String TEST_APK = "CollectorDeviceLibTest.apk";
    private static final String PACKAGE_NAME = "android.device.collectors";
    private static final String AJUR_RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    private static final String LOGCAT_COLLECTOR =
            "android.device.collectors.LogcatCollector";

    private IInvocationContext mContext;
    private DeviceTestRunOptions mOptions = null;

    @Before
    public void setUp() throws Exception {
        installPackage(TEST_APK);
        assertTrue(isPackageInstalled(PACKAGE_NAME));
        mOptions = new DeviceTestRunOptions(PACKAGE_NAME);
        mOptions.setRunner(AJUR_RUNNER);
        mOptions.addInstrumentationArg("listener", LOGCAT_COLLECTOR);
        mOptions.setTestClassName("android.device.tests.TestEvents");
        mOptions.setDisableIsolatedStorage(true);

        mContext = mock(IInvocationContext.class);
        doReturn(Arrays.asList(getDevice())).when(mContext).getDevices();
        doReturn(Arrays.asList(getBuild())).when(mContext).getBuildInfos();
    }

    @Test
    public void testCollect() throws Exception {
        TestFilePullerDeviceMetricCollector collector = new TestFilePullerDeviceMetricCollector();
        OptionSetter optionSetter = new OptionSetter(collector);
        String pattern = String.format("%s_.*", LOGCAT_COLLECTOR);
        optionSetter.setOptionValue("pull-pattern-keys", pattern);
        collector.init(mContext, new CollectingTestListener());
        mOptions.addExtraListener(collector);
        mOptions.setCheckResults(false);
        runDeviceTests(mOptions);
        TestRunResult results = getLastDeviceRunResults();
        assertFalse(results.isRunFailure());
        // Ensure we actually collected something
        assertTrue(collector.mCollectedOnce);
    }

    private class TestFilePullerDeviceMetricCollector extends FilePullerDeviceMetricCollector {

        public boolean mCollectedOnce = false;

        @Override
        public void processMetricFile(String key, File metricFile, DeviceMetricData runData) {
            try {
                assertTrue(metricFile.getName().endsWith(".txt"));
                String mime =
                        URLConnection.guessContentTypeFromStream(new FileInputStream(metricFile));
                assertNull(mime); // guessContentTypeFromStream returns null for text/plain
            } catch (IOException e) {
                throw new RuntimeException(e);
            } finally {
                assertTrue(metricFile.delete());
            }
            mCollectedOnce = true;
        }

        @Override
        public void processMetricDirectory(
                String key, File metricDirectory, DeviceMetricData runData) {}
    }
}
