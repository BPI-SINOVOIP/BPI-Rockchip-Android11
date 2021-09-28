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
package com.android.tradefed.sandbox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.sandbox.SandboxedInvocationExecution;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link SandboxedInvocationExecution}. */
@RunWith(JUnit4.class)
public class SandboxedInvocationExecutionTest {

    private SandboxedInvocationExecution mExecution;
    private IInvocationContext mContext;
    private IConfiguration mConfig;

    @Before
    public void setUp() {
        mExecution = new SandboxedInvocationExecution();
        mContext = new InvocationContext();
        mConfig = new Configuration("name", "desc");
        mConfig.getConfigurationDescription().setSandboxed(true);
    }

    @Test
    public void testBuildInfo_testTag() throws Exception {
        IBuildInfo info = new BuildInfo();
        assertEquals("stub", info.getTestTag());
        info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, new File("doesnt_matter_testsdir"), "tests");
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, info);
        mConfig.getCommandOptions().setTestTag("test");
        TestInformation testInfo =
                TestInformation.newBuilder().setInvocationContext(mContext).build();
        assertNull(testInfo.executionFiles().get(FilesKey.TESTS_DIRECTORY));
        mExecution.fetchBuild(testInfo, mConfig, null, null);
        // Build test tag was updated
        assertEquals("test", info.getTestTag());
        // Execution file was back filled
        assertNotNull(testInfo.executionFiles().get(FilesKey.TESTS_DIRECTORY));
    }
}
