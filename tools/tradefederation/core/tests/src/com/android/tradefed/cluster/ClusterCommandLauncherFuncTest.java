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
package com.android.tradefed.cluster;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;

/** Functional tests for {@link ClusterCommandLauncher}. */
@RunWith(MockitoJUnitRunner.class)
public class ClusterCommandLauncherFuncTest {

    private static final String LEGACY_TRADEFED_JAR = "/testdata/tradefed-prebuilt-cts-8.0_r21.jar";
    private static final String LEGACY_TRADEFED_COMMAND =
            "host --null-device --class com.android.tradefed.device.DeviceDiagTest";

    private File mRootDir;
    private IConfiguration mConfiguration;
    private IInvocationContext mInvocationContext;
    private OptionSetter mOptionSetter;

    @Spy private ClusterCommandLauncher mLauncher;
    @Mock private ITestInvocationListener mListener;

    @Before
    public void setUp() throws Exception {
        mRootDir = FileUtil.createTempDir(getClass().getName() + "_RootDir");
        mConfiguration = new Configuration("name", "description");
        mConfiguration.getCommandOptions().setInvocationTimeout(60_000L); // 1 minute
        mInvocationContext = new InvocationContext();
        mLauncher.setConfiguration(mConfiguration);
        mLauncher.setInvocationContext(mInvocationContext);
        mOptionSetter = new OptionSetter(mLauncher);
        mOptionSetter.setOptionValue("cluster:root-dir", mRootDir.getAbsolutePath());
        mOptionSetter.setOptionValue("cluster:env-var", "TF_WORK_DIR", mRootDir.getAbsolutePath());
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mRootDir);
    }

    @Ignore
    @Test
    public void testRun_withLegacyTradefed()
            throws IOException, ConfigurationException, DeviceNotAvailableException {
        File tfJar = new File(mRootDir, "tradefed.jar");
        FileUtil.writeToFile(getClass().getResourceAsStream(LEGACY_TRADEFED_JAR), tfJar);
        mOptionSetter.setOptionValue("cluster:env-var", "TF_PATH", mRootDir.getAbsolutePath());
        mOptionSetter.setOptionValue("cluster:use-subprocess-reporting", "true");
        mOptionSetter.setOptionValue("cluster:command-line", LEGACY_TRADEFED_COMMAND);

        mLauncher.run(mListener);

        HashMap<String, MetricMeasurement.Metric> emptyMap = new HashMap<>();
        verify(mListener).testRunStarted(anyString(), anyInt(), anyInt(), anyLong());
        verify(mListener).testStarted(any(TestDescription.class), anyLong());
        verify(mListener).testEnded(any(TestDescription.class), anyLong(), eq(emptyMap));
        verify(mListener).testRunEnded(anyLong(), eq(emptyMap));
    }
}
