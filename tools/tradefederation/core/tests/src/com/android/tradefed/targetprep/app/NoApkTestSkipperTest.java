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
package com.android.tradefed.targetprep.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.AppBuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.StubTargetPreparer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;

/** Unit tests for {@link NoApkTestSkipper}. */
@RunWith(JUnit4.class)
public class NoApkTestSkipperTest {

    private NoApkTestSkipper mSkipper;
    private ITestDevice mMockDevice;
    private AppBuildInfo mAppBuildInfo;
    private IConfiguration mConfig;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mSkipper = new NoApkTestSkipper();
        mMockDevice = Mockito.mock(ITestDevice.class);
        mAppBuildInfo = new AppBuildInfo("buildid", "buildname");
        mConfig = new Configuration("name", "desc");
        InvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mAppBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testApksPresent() throws Exception {
        mAppBuildInfo.addAppPackageFile(new File("fakepackage"), "v2");
        mSkipper.setUp(mTestInfo);
    }

    @Test
    public void testNoApkPresent() throws Exception {
        StubTargetPreparer preparer = new StubTargetPreparer();
        assertFalse(preparer.isDisabled());
        mConfig.setTargetPreparer(preparer);
        assertEquals(1, mConfig.getTests().size());

        mSkipper.setConfiguration(mConfig);
        mSkipper.setUp(mTestInfo);
        // Verify preparer is disabled
        assertTrue(preparer.isDisabled());
        assertTrue(mConfig.getTests().isEmpty());
    }
}
