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
package com.android.collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.device.metric.FilePullerDeviceMetricCollector;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.URLConnection;
import java.util.Arrays;

/**
 * Host side tests for the screenshot collector, this ensure that we are able to use the collectors
 * in a similar way as the infra.
 *
 * <p>Command: mm CollectorHostsideLibTest CollectorDeviceLibTest -j16 tradefed.sh run
 * commandAndExit template/local_min --template:map test=CollectorHostsideLibTest
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ScreenshotOnFailureCollectorHostTest extends BaseHostJUnit4Test {
    private static final String TEST_APK = "CollectorDeviceLibTest.apk";
    private static final String PACKAGE_NAME = "android.device.collectors";
    private static final String AJUR_RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    private static final String SCREENSHOT_COLLECTOR =
            "android.device.collectors.ScreenshotOnFailureCollector";

    private RemoteAndroidTestRunner mTestRunner;
    private IInvocationContext mContext;

    @Before
    public void setUp() throws Exception {
        installPackage(TEST_APK);
        assertTrue(isPackageInstalled(PACKAGE_NAME));
        mTestRunner =
                new RemoteAndroidTestRunner(PACKAGE_NAME, AJUR_RUNNER, getDevice().getIDevice());
        mTestRunner.setClassName("android.device.tests.TestEvents");
        mContext = mock(IInvocationContext.class);
        doReturn(Arrays.asList(getDevice())).when(mContext).getDevices();
        doReturn(Arrays.asList(getBuild())).when(mContext).getBuildInfos();
    }

    /**
     * Test that ScreenshotOnFailureCollector collects screenshot and records to a file per test.
     */
    @Test
    public void testScreenshotListener() throws Exception {
        mTestRunner.addInstrumentationArg("listener", SCREENSHOT_COLLECTOR);
        mTestRunner.addInstrumentationArg("screenshot-format", "file:screenshot-log");

        CollectingTestListener listener = new CollectingTestListener();
        FilePullerDeviceMetricCollector collector =
                new FilePullerDeviceMetricCollector() {
                    @Override
                    public void processMetricFile(
                            String key, File metricFile, DeviceMetricData runData) {
                        try {
                            assertTrue(metricFile.getName().contains("png"));
                            assertTrue(metricFile.length() > 0);
                            String mime =
                                    URLConnection.guessContentTypeFromStream(
                                            new FileInputStream(metricFile));
                            assertEquals("image/png", mime);
                        } catch (IOException e) {
                            throw new RuntimeException(e);
                        } finally {
                            assertTrue(metricFile.delete());
                        }
                    }

                    @Override
                    public void processMetricDirectory(
                            String key, File metricDirectory, DeviceMetricData runData) {}
                };
        OptionSetter optionSetter = new OptionSetter(collector);
        String pattern = String.format("%s_.*", SCREENSHOT_COLLECTOR);
        optionSetter.setOptionValue("pull-pattern-keys", pattern);
        collector.init(mContext, listener);
        assertTrue(getDevice().runInstrumentationTests(mTestRunner, collector));
        assertFalse(listener.getRunResults().iterator().next().isRunFailure());
    }
}
