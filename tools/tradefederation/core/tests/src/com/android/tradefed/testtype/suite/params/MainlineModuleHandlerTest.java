/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.testtype.suite.params;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.targetprep.InstallApexModuleTargetPreparer;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IAbi;

import org.easymock.EasyMock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link MainlineModuleHandler}. */
@RunWith(JUnit4.class)
public final class MainlineModuleHandlerTest {

    private MainlineModuleHandler mHandler;
    private IConfiguration mConfig;
    private IBuildInfo mMockBuildInfo;
    private IInvocationContext mContext;
    private IAbi mAbi;

    @Before
    public void setUp() {
        mConfig = new Configuration("test", "test");
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        mContext = new InvocationContext();
        mAbi = new Abi("armeabi-v7a", "32");
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andStubReturn("flavor");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andStubReturn("id");
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockBuildInfo);
    }

    /** Test that when a module configuration go through the handler it gets tuned properly. */
    @Test
    public void testApplySetup() {
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andStubReturn("branch");
        EasyMock.replay(mMockBuildInfo);
        mHandler = new MainlineModuleHandler("mod1.apk", mAbi, mContext);
        mHandler.applySetup(mConfig);
        assertTrue(mConfig.getTargetPreparers().get(0) instanceof InstallApexModuleTargetPreparer);
        InstallApexModuleTargetPreparer preparer =
                (InstallApexModuleTargetPreparer) mConfig.getTargetPreparers().get(0);
        assertEquals(preparer.getTestsFileName().size(),  1);
        assertEquals(preparer.getTestsFileName().get(0).getName(), "mod1.apk");
        EasyMock.verify(mMockBuildInfo);
    }

    /** Test that when a module configuration go through the handler it gets tuned properly. */
    @Test
    public void testApplySetup_MultipleMainlineModules() {
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andStubReturn("branch");
        EasyMock.replay(mMockBuildInfo);
        mHandler = new MainlineModuleHandler("mod1.apk+mod2.apex", mAbi, mContext);
        mHandler.applySetup(mConfig);
        assertTrue(mConfig.getTargetPreparers().get(0) instanceof InstallApexModuleTargetPreparer);
        InstallApexModuleTargetPreparer preparer =
                (InstallApexModuleTargetPreparer) mConfig.getTargetPreparers().get(0);
        assertEquals(preparer.getTestsFileName().size(),  2);
        assertEquals(preparer.getTestsFileName().get(0).getName(), "mod1.apk");
        assertEquals(preparer.getTestsFileName().get(1).getName(), "mod2.apex");
        EasyMock.verify(mMockBuildInfo);
    }

    /**
     * Test for {@link MainlineModuleHandler#buildDynamicBaseLink(IBuildInfo)}
     * implementation to throw an exception when the build information isn't correctly set.
     */
    @Test
    public void testBuildDynamicBaseLink_BranchIsNotSet() throws Exception {
        try {
            EasyMock.expect(mMockBuildInfo.getBuildBranch()).andStubReturn(null);
            EasyMock.replay(mMockBuildInfo);
            mHandler = new MainlineModuleHandler("mod1.apk+mod2.apex", mAbi, mContext);
            fail("Should have thrown an exception.");
        } catch (IllegalArgumentException expected) {
            // expected
            assertEquals(
                    "Missing required information to build the dynamic base link.",
                    expected.getMessage());
        } finally {
            EasyMock.verify(mMockBuildInfo);
        }
    }
}
