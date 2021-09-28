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
package com.android.tradefed.config;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.testtype.StubTest;
import com.android.tradefed.testtype.suite.retry.ITestSuiteResultLoader;
import com.android.tradefed.testtype.suite.retry.RetryRescheduler;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.keystore.StubKeyStoreClient;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.ArrayList;

/** Unit tests for {@link RetryConfigurationFactory}. */
@RunWith(JUnit4.class)
public class RetryConfigurationFactoryTest {

    private RetryConfigurationFactory mFactory;
    private File mConfig;
    private ITestSuiteResultLoader mMockLoader;
    private CollectingTestListener mFakeRecord;

    @Before
    public void setUp() throws Exception {
        mFactory = new RetryConfigurationFactory();
        mConfig = FileUtil.createTempFile("config-retry-factory", ".xml");
        mMockLoader = EasyMock.createMock(ITestSuiteResultLoader.class);
    }

    @After
    public void tearDown() {
        FileUtil.deleteFile(mConfig);
    }

    /** Test returning the original config when it's not a new retry type of config. */
    @Test
    public void testLoad_compatible() throws Exception {
        IConfiguration config =
                mFactory.createConfigurationFromArgs(
                        new String[] {"empty"}, new ArrayList<>(), new StubKeyStoreClient());

        assertFalse(config.getTests().get(0) instanceof RetryRescheduler);
        // This is our original empty config.
        assertTrue(config.getTests().get(0) instanceof StubTest);
    }

    /** Test returning the config to be re-run when it supports the reschedule type. */
    @Test
    public void testLoadRescheduler() throws Exception {
        populateFakeResults();
        mFactory = new RetryConfigurationFactory();

        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("suite/apct");
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);
        mMockLoader.customizeConfiguration(EasyMock.anyObject());
        mMockLoader.cleanUp();

        IConfiguration retryConfig = new Configuration("retry", "retry");
        retryConfig.setConfigurationObject(RetryRescheduler.PREVIOUS_LOADER_NAME, mMockLoader);
        retryConfig.setTest(new RetryRescheduler());

        EasyMock.replay(mMockLoader);
        IConfiguration config = mFactory.createRetryConfiguration(retryConfig);
        EasyMock.verify(mMockLoader);

        // It's not our original config anymore
        assertFalse(config.getTests().get(0) instanceof RetryRescheduler);
    }

    private void populateFakeResults() {
        CollectingTestListener reporter = new CollectingTestListener();
        IInvocationContext context = new InvocationContext();
        context.setConfigurationDescriptor(new ConfigurationDescriptor());
        context.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());
        reporter.invocationStarted(context);
        reporter.invocationEnded(0L);
        mFakeRecord = reporter;
    }
}
